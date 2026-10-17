/* **************************************************************************
ASWIni_Utils.h
Author: Anthony S. West - ASW Software

An INI file reader/writer library.

Text helpers of the library: ASCII case-insensitive comparison, trimming,
byte order marks, UTF-8 validation and splitting text into lines that keep
their own line breaks. Internal (namespace ASWIni::Detail): not part of the
public interface, and may change in any release.

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

#ifndef ASWIni_UtilsH
#define ASWIni_UtilsH
//---------------------------------------------------------------------------
#include <cstddef>
#include <string_view>
#include <vector>
//---------------------------------------------------------------------------
#include "ASWIni_Types.h"
//---------------------------------------------------------------------------

namespace ASWIni
{

//---------------------------------------------------------------------------
// Internals of the library. Not part of the public interface.
//---------------------------------------------------------------------------
namespace Detail
{

// A byte order mark at the start of a text
enum class EBom
{
    None,
    Utf8, // EF BB BF
    Utf16LE, // FF FE
    Utf16BE, // FE FF
    Utf32LE, // FF FE 00 00
    Utf32BE, // 00 00 FE FF
};

// One line of a text: its characters, without the line break, and the line break that ended it. The views point into
// the text that was split.
struct TLineSpan
{
    std::string_view Text;
    ELineBreak Break = ELineBreak::None;
};

// The size in bytes of 'bom' (0 for EBom::None)
std::size_t BomSize(EBom bom) noexcept;

// -1, 0 or 1 as 'left' sorts before, the same as, or after 'right', with the ASCII letters compared without case.
// Other bytes (including UTF-8 sequences) are compared as unsigned values.
int CompareNoCase(std::string_view left, std::string_view right) noexcept;

// The byte order mark 'text' starts with, if any. A UTF-32 LE mark (FF FE 00 00) is found before the UTF-16 LE mark
// it starts with.
EBom DetectBom(std::string_view text) noexcept;

// Whether 'left' and 'right' are the same with the ASCII letters compared without case
bool EqualsNoCase(std::string_view left, std::string_view right) noexcept;

// Whether 'c' is a space or a tab: the characters trimmed from keys and values
inline bool IsBlank(char c) noexcept
{
    return c == ' ' || c == '\t';
}

// Whether 'text' is valid UTF-8: no overlong forms, surrogates (U+D800 to U+DFFF), values above U+10FFFF or
// truncated sequences
bool IsValidUtf8(std::string_view text) noexcept;

// The characters of 'lineBreak' ("" for ELineBreak::None)
std::string_view LineBreakText(ELineBreak lineBreak) noexcept;

// The platform's line break: CRLF on Windows, LF elsewhere
ELineBreak NativeLineBreak() noexcept;

// The lines of 'text', each with the line break that ended it, so appending each line's Text and LineBreakText(Break)
// gives 'text' back exactly. A text that ends with a line break has no empty line after it; an empty text has no lines.
// LF, CR and CRLF are each one line break, so "a\r\r\nb" is "a" (CR), "" (CRLF) and "b" (None).
std::vector<TLineSpan> SplitLines(std::string_view text);

// The ASCII letter 'c' in lower case; any other character unchanged
inline char ToLowerAscii(char c) noexcept
{
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

// 'text' without the spaces and tabs at its start and end
std::string_view Trim(std::string_view text) noexcept;

// 'text' without the spaces and tabs at its start
std::string_view TrimLeft(std::string_view text) noexcept;

// 'text' without the spaces and tabs at its end
std::string_view TrimRight(std::string_view text) noexcept;

} // namespace Detail

} // namespace ASWIni

//---------------------------------------------------------------------------
#endif // #ifndef ASWIni_UtilsH
