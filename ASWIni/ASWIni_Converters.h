/* **************************************************************************
ASWIni_Converters.h
Author: Anthony S. West - ASW Software

An INI file reader/writer library.

TASWIniConverter<T>, which converts a typed value to and from the text of an
INI value, as the value options (TASWIniValueOptions) say. The library has
converters for std::string, bool, the integer types, float and double; an
application can add its own by specializing TASWIniConverter for its type.
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
#include <cstddef>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>
//---------------------------------------------------------------------------
#include "ASWIni_Options.h"
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

// The longest part of a value's text a message quotes (see MessageText())
constexpr std::size_t MaxMessageTextSize = 40;

// 'value' as TASWIniConverter<double> and TASWIniConverter<float> write it, converted with 'conversion'
std::string FloatingPointText(double value, EFloatConversion conversion);
std::string FloatingPointText(float value, EFloatConversion conversion);

// 'value' as decimal digits, with '-' when it is negative
std::string IntegerText(long long value);
std::string IntegerText(unsigned long long value);

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

// A failed result with 'code' and the message "The value <text> <problem>", e.g. ValueFailure(InvalidValue, "maybe",
// "isn't a bool: it must be true, false, yes, no, on, off, 1 or 0.")
TASWIniResult ValueFailure(EIniCode code, std::string_view text, std::string_view problem);

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
struct TASWIniConverter<T, std::enable_if_t<Detail::IsConvertibleInteger<T> > >
{
    static TASWIniResult FromText(std::string_view text, T& value, TASWIniValueOptions const& /*options*/)
    {
        // Read as the widest type of the same signedness, then checked against T's range
        if constexpr (std::is_signed_v<T>)
        {
            long long read = 0;
            TASWIniResult result = Detail::ReadSignedInteger(text, std::numeric_limits<T>::min(),
                std::numeric_limits<T>::max(), read);
            if (result.IsOk())
                value = static_cast<T>(read);

            return result;
        }
        else
        {
            unsigned long long read = 0;
            TASWIniResult result = Detail::ReadUnsignedInteger(text, std::numeric_limits<T>::max(), read);
            if (result.IsOk())
                value = static_cast<T>(read);

            return result;
        }
    }

    static TASWIniResult ToText(T value, std::string& text, TASWIniValueOptions const& /*options*/)
    {
        if constexpr (std::is_signed_v<T>)
            text = Detail::IntegerText(static_cast<long long>(value));
        else
            text = Detail::IntegerText(static_cast<unsigned long long>(value));

        return TASWIniResult();
    }
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

} // namespace ASWIni

//---------------------------------------------------------------------------
#endif // #ifndef ASWIni_ConvertersH
