/* **************************************************************************
ASWIni_Converters.h
Author: Anthony S. West - ASW Software

An INI file reader/writer library.

TASWIniConverter<T>, which converts a typed value to and from the text of an
INI value, as the value options (TASWIniValueOptions) say. The library has
converters for std::string, std::filesystem::path, std::u8string (C++20),
bool, the integer types, float, double, enums (by name when the application
gives the enum names, see TASWIniEnumName, else as numbers) and std::vector of
any of them (lists, also of lists); an application can add its own by
specializing TASWIniConverter for its type.
Conversions don't depend on the C or C++ locale.

Requires C++ 17 or higher.

Copyright 2026 ASW Software

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    https://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

************************************************************************** */

#pragma once

#ifndef ASWIni_ConvertersH
#define ASWIni_ConvertersH
//---------------------------------------------------------------------------
#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>
//---------------------------------------------------------------------------
#include "ASWIni_Options.h"
#include "ASWIni_Platform.h"
#include "ASWIni_Types.h"
//---------------------------------------------------------------------------

// Whether the float and double converters use the standard library's std::to_chars() and std::from_chars() (1) or
// the C runtime (0). bcc32c's (RAD Studio Win32) give wrong results for many values, so it uses the C runtime. Define
// it as 0 when building the library with another standard library that has no floating-point <charconv> (e.g.
// libstdc++ before GCC 11).
#if !defined(ASWINI_STD_FLOAT_CHARCONV)
#  if defined(__BORLANDC__) && !defined(_WIN64)
#    define ASWINI_STD_FLOAT_CHARCONV 0
#  else
#    define ASWINI_STD_FLOAT_CHARCONV 1
#  endif
#endif
//---------------------------------------------------------------------------

namespace ASWIni
{

/////////////////////////////////////////////////////////////////////////////
// TASWIniConverter
//
// Converts a value of type T to and from the text of an INI value (the text TASWIniDocumentLine::GetValue() gives and
// TASWIniDocument::SetValue() takes, so without quotes or comments). A converter has two static functions:
//
//     static TASWIniResult FromText(std::string_view text, T& value, TASWIniValueOptions const& options);
//     static TASWIniResult ToText(T const& value, std::string& text, TASWIniValueOptions const& options);
//
// On failure they leave 'value' or 'text' as it was, so a caller can set the default first. A text that isn't a value
// of the type fails with EIniCode::InvalidValue, and a number outside the type's range with EIniCode::ValueOutOfRange,
// each with a message that quotes the text (see Detail::ValueFailure()). An application adds a converter for its own
// type by specializing the template in namespace ASWIni; the second parameter lets one partial specialization cover a
// group of types (e.g. with std::enable_if_t). The template itself is only declared, so a type with no converter is a
// compile error.
/////////////////////////////////////////////////////////////////////////////
template <typename T, typename Enable = void>
struct TASWIniConverter;


/////////////////////////////////////////////////////////////////////////////
// TASWIniEnumName
//
// One name of a value of the enum T, in the names an application gives an enum so that it is converted by name (see
// TASWIniConverter<enums with names>). The application declares a function ASWIniEnumNames() next to the enum, in the
// enum's namespace (or, for an enum declared in a class, as a friend in that class), so argument-dependent lookup
// finds it. Its argument only selects it, and it returns anything a range-based for loop can go over that gives
// TASWIniEnumName<T>s: a reference to an array or a std::array, or a std::vector. A value may have several names: the
// first one listed is written, and the others are read too (e.g. an old name). A name is compared with the ASCII
// letters' case ignored; one that is nullptr is skipped, and one that is empty or has blanks at either end never
// matches.
//
//     enum class EColor { Red, Green, Blue };
//
//     inline auto const& ASWIniEnumNames(EColor)
//     {
//         static constexpr ASWIni::TASWIniEnumName<EColor> names[] = {
//             { EColor::Red, "Red" }, { EColor::Green, "Green" }, { EColor::Blue, "Blue" },
//             { EColor::Green, "Lime" }, // Read as Green, never written
//         };
//
//         return names;
//     }
/////////////////////////////////////////////////////////////////////////////
template <typename T>
struct TASWIniEnumName
{
    static_assert(std::is_enum_v<T>, "TASWIniEnumName is for enums");

    T Value;
    char const* Name;
};

//---------------------------------------------------------------------------

// Helpers of the converters' templates
namespace Detail
{

// The code the float and double converters convert with. Each one reads and writes the same texts (the converters
// check the text's form, its sign, inf and nan themselves); they differ only in how exact they are. All of them are
// built on every compiler, so the tests can compare them.
enum class EFloatConversion : int
{
    Standard = 0, // std::to_chars() and std::from_chars() (the C runtime's when ASWINI_STD_FLOAT_CHARCONV is 0)
    CRuntime = 1, // snprintf() for the digits, and strtold() narrowed to the type
    UniversalCrt = 2, // As CRuntime, but read with the Windows Universal C Runtime's exact _strtod_l() and _strtof_l()
                      // (ucrtbase.dll) when Windows has it; CRuntime elsewhere
};

// Whether argument-dependent lookup finds ASWIniEnumNames() for a T (see TASWIniEnumName); IsNamedEnum says it for
// enums only
template <typename T, typename Enable = void>
struct THasEnumNames : std::false_type
{
};

template <typename T>
struct THasEnumNames<T, std::void_t<decltype(ASWIniEnumNames(std::declval<T>()))> > : std::true_type
{
};

// The conversion the float and double converters use
#if ASWINI_STD_FLOAT_CHARCONV
constexpr EFloatConversion DefaultFloatConversion = EFloatConversion::Standard;
#else
constexpr EFloatConversion DefaultFloatConversion = EFloatConversion::UniversalCrt;
#endif

// Whether T is char8_t (C++20; never before it)
#if defined(__cpp_char8_t)
template <typename T>
constexpr bool IsChar8 = std::is_same_v<T, char8_t>;
#else
template <typename T>
constexpr bool IsChar8 = false;
#endif

// Whether T is one of the integer types TASWIniConverter reads and writes as numbers: an integral type other than bool
// and the wide and Unicode character types. Plain char is a number too: bcc32c's std::int8_t is char.
template <typename T>
constexpr bool IsConvertibleInteger = std::is_integral_v<T> && !std::is_same_v<T, bool> &&
    !std::is_same_v<T, wchar_t> && !std::is_same_v<T, char16_t> && !std::is_same_v<T, char32_t> && !IsChar8<T>;

// Whether T is an enum with names: one for which argument-dependent lookup finds ASWIniEnumNames() (see
// TASWIniEnumName)
template <typename T>
constexpr bool IsNamedEnum = std::is_enum_v<T> && THasEnumNames<T>::value;

// The longest part of a value's text a message quotes (see MessageText())
constexpr std::size_t MaxMessageTextSize = 40;

// 'value' as TASWIniConverter<double> and TASWIniConverter<float> write it, converted with 'conversion'
std::string FloatingPointText(double value, EFloatConversion conversion);
std::string FloatingPointText(float value, EFloatConversion conversion);

// 'value' as decimal digits, with '-' when it is negative
std::string IntegerText(long long value);
std::string IntegerText(unsigned long long value);

// The InvalidValue result of an enum with names reading 'text', which is none of its names (see
// TASWIniConverter<enums with names>): the message lists 'names', the names the enum's values are written as
TASWIniResult InvalidEnumName(std::string_view text, std::vector<char const*> const& names);

// Whether 'text', without the spaces and tabs around it, is 'name' with the ASCII letters' case ignored (never when
// 'name' is nullptr)
bool IsEnumName(std::string_view text, char const* name) noexcept;

// The text of a list with the items 'items' (see TASWIniConverter<std::vector<T> >): the items joined with 'delimiter'
// and a space (no space before an empty item, so the text never ends with one), each in double quotes, with each '"'
// in it as "", when it holds 'delimiter' or a '"', has a space or a tab at either end, or is the only item and empty
std::string JoinListItems(std::vector<std::string> const& items, char delimiter);

// The result of a list's item 'number' (1 for the first) failing to read, with 'itemResult': its code, and a message
// that quotes the list's 'text' and then gives the item's message
TASWIniResult ListItemReadFailure(std::string_view text, std::size_t number, TASWIniResult const& itemResult);

// The result of a list's item 'number' (1 for the first) failing to be written, with 'itemResult': its code, and a
// message that names the item and then gives the item's message
TASWIniResult ListItemWriteFailure(std::size_t number, TASWIniResult const& itemResult);

// 'text' as a message quotes it: in double quotes, with each control character (below 0x20, and 0x7F) as '?', and,
// when it has more than MaxMessageTextSize bytes, cut to at most that many (not inside a UTF-8 sequence) and
// followed by "..."
std::string MessageText(std::string_view text);

// Reads 'text' as TASWIniConverter<double> and TASWIniConverter<float> read it, converted with 'conversion', into
// 'value', which is left as it was on failure
TASWIniResult ReadFloatingPoint(std::string_view text, double& value, EFloatConversion conversion);
TASWIniResult ReadFloatingPoint(std::string_view text, float& value, EFloatConversion conversion);

// Reads 'text' as an integer from 'min' to 'max' (see TASWIniConverter<integer types>) into 'value', which is left as
// it was on failure: InvalidValue when the text isn't an integer, ValueOutOfRange when it is outside the range. The
// range is a type's: 'min' is negative (signed) or 0 (unsigned), and 'max' is positive.
TASWIniResult ReadSignedInteger(std::string_view text, long long min, long long max, long long& value);
TASWIniResult ReadUnsignedInteger(std::string_view text, unsigned long long max, unsigned long long& value);

// Splits 'text', a list (see TASWIniConverter<std::vector<T> >), into 'items' (set only on success): each item's text
// as its converter reads it, without its quotes and with each "" in them as one '"' when it is quoted, else without
// the spaces and tabs around it. An empty text (or only blanks) is no items. Fails with InvalidValue when a quote
// isn't closed, or anything but blanks follows a closing quote before the next 'delimiter'.
TASWIniResult SplitListItems(std::string_view text, char delimiter, std::vector<std::string>& items);

// The InvalidValue result of an enum with names writing a value that has no name, whose number is 'number'
TASWIniResult UnnamedEnumValue(std::string_view number);

// A failed result with 'code' and the message "The value <text> <problem>", e.g. ValueFailure(InvalidValue, "maybe",
// "isn't a bool: it must be true, false, yes, no, on, off, 1 or 0.")
TASWIniResult ValueFailure(EIniCode code, std::string_view text, std::string_view problem);


/////////////////////////////////////////////////////////////////////////////
// TIntegerConverter
//
// Converts any integral type T as a number, as TASWIniConverter<integer types> describes, so bool and the character
// types too: the integer converters are this one, and the enum converters use it for an enum's underlying type
/////////////////////////////////////////////////////////////////////////////
template <typename T>
struct TIntegerConverter
{
    static_assert(std::is_integral_v<T>, "TIntegerConverter is for integral types");

    static TASWIniResult FromText(std::string_view text, T& value, TASWIniValueOptions const& /*options*/)
    {
        // Read as the widest type of the same signedness, then checked against T's range
        if constexpr (std::is_signed_v<T>)
        {
            long long read = 0;
            TASWIniResult result = ReadSignedInteger(text, std::numeric_limits<T>::min(),
                std::numeric_limits<T>::max(), read);
            if (result.IsOk())
                value = static_cast<T>(read);

            return result;
        }
        else
        {
            unsigned long long read = 0;
            TASWIniResult result = ReadUnsignedInteger(text, std::numeric_limits<T>::max(), read);
            if (result.IsOk())
                value = static_cast<T>(read);

            return result;
        }
    }

    static TASWIniResult ToText(T value, std::string& text, TASWIniValueOptions const& /*options*/)
    {
        if constexpr (std::is_signed_v<T>)
            text = IntegerText(static_cast<long long>(value));
        else
            text = IntegerText(static_cast<unsigned long long>(value));

        return TASWIniResult();
    }
};

} // namespace Detail


/////////////////////////////////////////////////////////////////////////////
// TASWIniConverter<bool>
//
// Reads true/false, yes/no, on/off and 1/0, with the ASCII letters' case and the spaces and tabs around the text
// ignored; writes the form TASWIniValueOptions::BoolFormat names ("true"/"false" for a value that isn't an
// EBoolFormat).
/////////////////////////////////////////////////////////////////////////////
template <>
struct TASWIniConverter<bool>
{
    static TASWIniResult FromText(std::string_view text, bool& value, TASWIniValueOptions const& options);
    static TASWIniResult ToText(bool value, std::string& text, TASWIniValueOptions const& options);
};


/////////////////////////////////////////////////////////////////////////////
// TASWIniConverter<std::string>
//
// The text as it is, both ways: it never fails, and keeps any spaces and tabs (a quoted value can have them)
/////////////////////////////////////////////////////////////////////////////
template <>
struct TASWIniConverter<std::string>
{
    static TASWIniResult FromText(std::string_view text, std::string& value, TASWIniValueOptions const& options);
    static TASWIniResult ToText(std::string const& value, std::string& text, TASWIniValueOptions const& options);
};


/////////////////////////////////////////////////////////////////////////////
// TASWIniConverter<std::filesystem::path>
//
// The text, as it is (blanks too), is the path in UTF-8 on every platform, whatever the standard library makes of a
// narrow string (Windows libraries differ: the ANSI code page or UTF-8). Writes the path as it holds it, separators
// included ("C:\Temp\app.ini" on Windows stays so). A text that isn't valid UTF-8 is InvalidValue, and so is a path
// whose UTF-8 form wouldn't read back as the same path (an unpaired surrogate in a Windows path, or bytes that aren't
// UTF-8 in a POSIX one), so what is written always reads back the same.
/////////////////////////////////////////////////////////////////////////////
template <>
struct TASWIniConverter<std::filesystem::path>
{
    static TASWIniResult FromText(std::string_view text, std::filesystem::path& value,
        TASWIniValueOptions const& options);
    static TASWIniResult ToText(std::filesystem::path const& value, std::string& text,
        TASWIniValueOptions const& options);
};

#if ASWINI_HAS_U8STRING

/////////////////////////////////////////////////////////////////////////////
// TASWIniConverter<std::u8string>
//
// C++20 (see ASWINI_HAS_U8STRING). As TASWIniConverter<std::string>: the text's bytes as they are, both ways. Defined
// here, so it is there when an application is compiled at C++20 and the library at C++17.
/////////////////////////////////////////////////////////////////////////////
template <>
struct TASWIniConverter<std::u8string>
{
    static TASWIniResult FromText(std::string_view text, std::u8string& value, TASWIniValueOptions const& /*options*/)
    {
        value.assign(text.begin(), text.end());

        return TASWIniResult();
    }

    static TASWIniResult ToText(std::u8string const& value, std::string& text, TASWIniValueOptions const& /*options*/)
    {
        text.assign(value.begin(), value.end());

        return TASWIniResult();
    }
};

#endif


/////////////////////////////////////////////////////////////////////////////
// TASWIniConverter<integer types>
//
// Every integral type except bool and the wide and Unicode character types (wchar_t, char8_t, char16_t, char32_t), so
// char, signed char and unsigned char (std::int8_t, std::uint8_t) are numbers, not characters (a char's range is
// signed char's or unsigned char's, as the compiler makes char). Reads an optional sign ('+' or '-'), then decimal
// digits, or 0x (or 0X) and hex digits, with the spaces and tabs around the text ignored: "42", "-42", "+42", "0x2A",
// "-0x2A". Leading zeros are decimal ("010" is 10). A number outside the type's range is ValueOutOfRange, a negative
// one for an unsigned type too ("-0" is 0); hex is a value, not a bit pattern ("0xFFFFFFFF" is out of range for
// std::int32_t). Writes decimal digits, with '-' when negative.
/////////////////////////////////////////////////////////////////////////////
template <typename T>
struct TASWIniConverter<T, std::enable_if_t<Detail::IsConvertibleInteger<T> > > : Detail::TIntegerConverter<T>
{
};


/////////////////////////////////////////////////////////////////////////////
// TASWIniConverter<double>
//
// Reads an optional sign, then digits with an optional '.' and exponent ("1.5", ".5", "5.", "1e5", "-2.5E-3"), or inf,
// infinity or nan (with the ASCII letters' case ignored), with the spaces and tabs around the text ignored. A ','
// decimal mark, hex and digit separators are InvalidValue, whatever the C locale says. A number too large for the type
// is ValueOutOfRange; one too small reads as 0. Writes the shortest text that reads back as the same value, as
// std::to_chars() does ("0.1", "3", "1e+21", "-0"), or "inf", "-inf" or "nan" (a NaN's sign isn't kept).
// On RAD Studio Win32 (bcc32c), whose std::to_chars() and std::from_chars() aren't reliable, the C runtime is used: the
// text is the same in nearly every case, and every value written reads back the same; with an older Windows that
// lacks the Universal C Runtime (ucrtbase.dll), about 1 in 12,000 doubles written elsewhere reads back 1 unit in the
// last place off.
/////////////////////////////////////////////////////////////////////////////
template <>
struct TASWIniConverter<double>
{
    static TASWIniResult FromText(std::string_view text, double& value, TASWIniValueOptions const& options);
    static TASWIniResult ToText(double value, std::string& text, TASWIniValueOptions const& options);
};


/////////////////////////////////////////////////////////////////////////////
// TASWIniConverter<float>
//
// As TASWIniConverter<double>, with float's range and precision ("0.1" is the float nearest to 0.1)
/////////////////////////////////////////////////////////////////////////////
template <>
struct TASWIniConverter<float>
{
    static TASWIniResult FromText(std::string_view text, float& value, TASWIniValueOptions const& options);
    static TASWIniResult ToText(float value, std::string& text, TASWIniValueOptions const& options);
};


/////////////////////////////////////////////////////////////////////////////
// TASWIniConverter<enums with names>
//
// An enum for which argument-dependent lookup finds ASWIniEnumNames() (see TASWIniEnumName). Reads one of its names,
// with the ASCII letters' case and the spaces and tabs around the text ignored, or else the number of a listed value
// (read as TASWIniConverter<integer types> reads a number of the underlying type), so a file that stored the number
// still reads. Anything else is InvalidValue, a number that isn't a listed value's too, with a message that lists the
// names. Writes the first name listed for the value; a value with no name is InvalidValue, since it couldn't be read
// back.
/////////////////////////////////////////////////////////////////////////////
template <typename T>
struct TASWIniConverter<T, std::enable_if_t<Detail::IsNamedEnum<T> > >
{
private:
    typedef std::underlying_type_t<T> TNumber;

    // The names the values in 'names' are written as: the first name listed for each value, in the order listed
    template <typename TNames>
    static std::vector<char const*> WrittenNames(TNames const& names)
    {
        std::vector<char const*> written;
        std::vector<T> values;
        for (auto const& name : names)
        {
            if (name.Name != nullptr && std::find(values.begin(), values.end(), name.Value) == values.end())
            {
                written.push_back(name.Name);
                values.push_back(name.Value);
            }
        }

        return written;
    }

public:
    static TASWIniResult FromText(std::string_view text, T& value, TASWIniValueOptions const& options)
    {
        auto const& names = ASWIniEnumNames(T());
        for (auto const& name : names)
        {
            if (Detail::IsEnumName(text, name.Name))
            {
                value = name.Value;
                return TASWIniResult();
            }
        }

        // The number of a listed value
        TNumber number = TNumber();
        if (Detail::TIntegerConverter<TNumber>::FromText(text, number, options).IsOk())
        {
            for (auto const& name : names)
            {
                if (name.Name != nullptr && name.Value == static_cast<T>(number))
                {
                    value = name.Value;
                    return TASWIniResult();
                }
            }
        }

        return Detail::InvalidEnumName(text, WrittenNames(names));
    }

    static TASWIniResult ToText(T value, std::string& text, TASWIniValueOptions const& options)
    {
        for (auto const& name : ASWIniEnumNames(T()))
        {
            if (name.Name != nullptr && name.Value == value)
            {
                text = name.Name;
                return TASWIniResult();
            }
        }

        // The value's number, for the message (writing a number never fails)
        std::string number;
        static_cast<void>(Detail::TIntegerConverter<TNumber>::ToText(static_cast<TNumber>(value), number, options));

        return Detail::UnnamedEnumValue(number);
    }
};


/////////////////////////////////////////////////////////////////////////////
// TASWIniConverter<enums without names>
//
// An enum for which argument-dependent lookup finds no ASWIniEnumNames() converts as a number of its underlying type,
// as TASWIniConverter<integer types> describes (an underlying bool or character type is a number too), whether the
// number is one of its enumerators or not (e.g. bit flags: "5" is the value 5)
/////////////////////////////////////////////////////////////////////////////
template <typename T>
struct TASWIniConverter<T, std::enable_if_t<std::is_enum_v<T> && !Detail::IsNamedEnum<T> > >
{
private:
    typedef std::underlying_type_t<T> TNumber;

public:
    static TASWIniResult FromText(std::string_view text, T& value, TASWIniValueOptions const& options)
    {
        TNumber number = TNumber();
        TASWIniResult result = Detail::TIntegerConverter<TNumber>::FromText(text, number, options);
        if (result.IsOk())
            value = static_cast<T>(number);

        return result;
    }

    static TASWIniResult ToText(T value, std::string& text, TASWIniValueOptions const& options)
    {
        return Detail::TIntegerConverter<TNumber>::ToText(static_cast<TNumber>(value), text, options);
    }
};


/////////////////////////////////////////////////////////////////////////////
// TASWIniConverter<std::vector<T> >
//
// A list of values of any type T that has a converter and a default constructor (each item is made before it is read),
// as items separated by TASWIniValueOptions::ListDelimiter, in the CSV style. Reads an empty text (or only blanks) as
// an empty list; otherwise each item is the text up to the next delimiter, without the spaces and tabs around it, or,
// when it starts with a '"', a quoted item: the text up to the closing '"', with each "" in it as one '"', which keeps
// its blanks and may hold the delimiter. Only blanks may follow a quoted item before the next delimiter, and an
// unclosed quote is InvalidValue; a '"' anywhere else in an item is an ordinary character. Each item is read with
// TASWIniConverter<T>, so "1,,2" fails as a list of int and is "1", "" and "2" as a list of std::string. Writes the
// items joined with the delimiter and a space ("a, b, c"), quoting an item only when it has to be (see
// Detail::JoinListItems()): "a, b" and c are written as "\"a, b\", c". A list of lists needs nothing more: its items
// are lists, quoted ("\"1, 2\", \"3, 4\""). When an item fails, so does the list, with the item's code and a message
// that names the item, leaving the value or text as it was.
// With TASWIniReadingOptions::QuotedValues, the document removes the quotes around a whole value before the converter
// sees it, so a hand-written k = "a, b" is the two items a and b; a list written through TASWIniDocument::SetValue()
// still reads back, since the document quotes a value that starts with a '"' again (k = """a, b""").
/////////////////////////////////////////////////////////////////////////////
template <typename T, typename TAllocator>
struct TASWIniConverter<std::vector<T, TAllocator> >
{
    static_assert(std::is_default_constructible_v<T>, "a list's items need a default constructor");

    static TASWIniResult FromText(std::string_view text, std::vector<T, TAllocator>& value,
        TASWIniValueOptions const& options)
    {
        std::vector<std::string> items;
        TASWIniResult result = Detail::SplitListItems(text, options.ListDelimiter, items);
        if (!result.IsOk())
            return result;

        std::vector<T, TAllocator> read(value.get_allocator());
        read.reserve(items.size());
        for (std::size_t i = 0; i < items.size(); ++i)
        {
            T item = T();
            result = TASWIniConverter<T>::FromText(items[i], item, options);
            if (!result.IsOk())
                return Detail::ListItemReadFailure(text, i + 1, result);

            read.push_back(std::move(item));
        }

        value = std::move(read);

        return TASWIniResult();
    }

    static TASWIniResult ToText(std::vector<T, TAllocator> const& value, std::string& text,
        TASWIniValueOptions const& options)
    {
        // By index, since a std::vector<bool>'s items are proxies
        std::vector<std::string> items(value.size());
        for (std::size_t i = 0; i < value.size(); ++i)
        {
            TASWIniResult const result = TASWIniConverter<T>::ToText(value[i], items[i], options);
            if (!result.IsOk())
                return Detail::ListItemWriteFailure(i + 1, result);
        }

        text = Detail::JoinListItems(items, options.ListDelimiter);

        return TASWIniResult();
    }
};

} // namespace ASWIni

//---------------------------------------------------------------------------
#endif // #ifndef ASWIni_ConvertersH
