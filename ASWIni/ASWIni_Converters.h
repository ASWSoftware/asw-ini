/* **************************************************************************
ASWIni_Converters.h
Author: Anthony S. West - ASW Software

An INI file reader/writer library.

TASWIniConverter<T>, which converts a typed value to and from the text of an
INI value, as the value options (TASWIniValueOptions) say. The library has
converters for std::string, bool and the integer types; an application can
add its own by specializing TASWIniConverter for its type. Conversions don't
depend on the C or C++ locale.

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

// 'value' as decimal digits, with '-' when it is negative
std::string IntegerText(long long value);
std::string IntegerText(unsigned long long value);

// 'text' as a message quotes it: in double quotes, with each control character (below 0x20, and 0x7F) as '?', and,
// when it has more than MaxMessageTextSize bytes, cut to at most that many (not inside a UTF-8 sequence) and
// followed by "..."
std::string MessageText(std::string_view text);

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

} // namespace ASWIni

//---------------------------------------------------------------------------
#endif // #ifndef ASWIni_ConvertersH
