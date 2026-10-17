/* **************************************************************************
ASWIni_Converters.h
Author: Anthony S. West - ASW Software

An INI file reader/writer library.

TASWIniConverter<T>, which converts a typed value to and from the text of an
INI value, as the value options (TASWIniValueOptions) say. The library has
converters for std::string and bool; an application can add its own by
specializing TASWIniConverter for its type. Conversions don't depend on the
C or C++ locale.

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
#include <string>
#include <string_view>
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

//---------------------------------------------------------------------------

namespace Detail
{

// The longest part of a value's text a message quotes (see MessageText())
constexpr std::size_t MaxMessageTextSize = 40;

// 'text' as a message quotes it: in double quotes, with each control character (below 0x20, and 0x7F) as '?', and,
// when it has more than MaxMessageTextSize bytes, cut to at most that many (not inside a UTF-8 sequence) and
// followed by "..."
std::string MessageText(std::string_view text);

// A failed result with 'code' and the message "The value <text> <problem>", e.g. ValueFailure(InvalidValue, "maybe",
// "isn't a bool: it must be true, false, yes, no, on, off, 1 or 0.")
TASWIniResult ValueFailure(EIniCode code, std::string_view text, std::string_view problem);

} // namespace Detail

} // namespace ASWIni

//---------------------------------------------------------------------------
#endif // #ifndef ASWIni_ConvertersH
