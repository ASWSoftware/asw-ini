/* **************************************************************************
ASWIni_Utils.cpp
Author: Anthony S. West - ASW Software

See header for info.

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

//---------------------------------------------------------------------------
// Module header
#include "ASWIni_Utils.h"
//---------------------------------------------------------------------------

namespace ASWIni
{

namespace Detail
{

//---------------------------------------------------------------------------
std::size_t BomSize(EBom bom) noexcept
{
    switch (bom)
    {
        case EBom::None:
            return 0;

        case EBom::Utf8:
            return 3;

        case EBom::Utf16LE:
        case EBom::Utf16BE:
            return 2;

        case EBom::Utf32LE:
        case EBom::Utf32BE:
            return 4;
    }

    return 0;
}
//---------------------------------------------------------------------------
int CompareNoCase(std::string_view left, std::string_view right) noexcept
{
    std::size_t const commonSize = left.size() < right.size() ? left.size() : right.size();

    for (std::size_t i = 0; i < commonSize; ++i)
    {
        unsigned char const leftChar = static_cast<unsigned char>(ToLowerAscii(left[i]));
        unsigned char const rightChar = static_cast<unsigned char>(ToLowerAscii(right[i]));

        if (leftChar != rightChar)
            return leftChar < rightChar ? -1 : 1;
    }

    if (left.size() == right.size())
        return 0;

    return left.size() < right.size() ? -1 : 1;
}
//---------------------------------------------------------------------------
EBom DetectBom(std::string_view text) noexcept
{
    auto const startsWith = [text](char const* bytes, std::size_t size)
        {
            return text.size() >= size && text.compare(0, size, std::string_view(bytes, size)) == 0;
        };

    // UTF-32 LE before UTF-16 LE, whose mark it starts with
    if (startsWith("\xFF\xFE\x00\x00", 4))
        return EBom::Utf32LE;

    if (startsWith("\x00\x00\xFE\xFF", 4))
        return EBom::Utf32BE;

    if (startsWith("\xEF\xBB\xBF", 3))
        return EBom::Utf8;

    if (startsWith("\xFF\xFE", 2))
        return EBom::Utf16LE;

    if (startsWith("\xFE\xFF", 2))
        return EBom::Utf16BE;

    return EBom::None;
}
//---------------------------------------------------------------------------
bool EqualsNoCase(std::string_view left, std::string_view right) noexcept
{
    return left.size() == right.size() && CompareNoCase(left, right) == 0;
}
//---------------------------------------------------------------------------
bool IsValidUtf8(std::string_view text) noexcept
{
    std::size_t i = 0;

    while (i < text.size())
    {
        unsigned char const lead = static_cast<unsigned char>(text[i]);

        if (lead < 0x80)
        {
            ++i;
            continue;
        }

        // The number of continuation bytes, and the range the second byte must be in to rule out overlong forms,
        // surrogates and values above U+10FFFF (RFC 3629's table)
        std::size_t continuationCount;
        unsigned char secondMin = 0x80;
        unsigned char secondMax = 0xBF;

        if (lead >= 0xC2 && lead <= 0xDF)
        {
            continuationCount = 1;
        }
        else if (lead >= 0xE0 && lead <= 0xEF)
        {
            continuationCount = 2;
            if (lead == 0xE0)
                secondMin = 0xA0; // Overlong below U+0800
            else if (lead == 0xED)
                secondMax = 0x9F; // Surrogates U+D800 to U+DFFF
        }
        else if (lead >= 0xF0 && lead <= 0xF4)
        {
            continuationCount = 3;
            if (lead == 0xF0)
                secondMin = 0x90; // Overlong below U+10000
            else if (lead == 0xF4)
                secondMax = 0x8F; // Above U+10FFFF
        }
        else
        {
            return false; // A continuation byte without a lead byte, an overlong C0/C1 lead, or F5 to FF
        }

        if (text.size() - i <= continuationCount)
            return false; // Truncated

        unsigned char const second = static_cast<unsigned char>(text[i + 1]);
        if (second < secondMin || second > secondMax)
            return false;

        for (std::size_t j = 2; j <= continuationCount; ++j)
        {
            unsigned char const continuation = static_cast<unsigned char>(text[i + j]);
            if (continuation < 0x80 || continuation > 0xBF)
                return false;
        }

        i += continuationCount + 1;
    }

    return true;
}
//---------------------------------------------------------------------------
std::string_view LineBreakText(ELineBreak lineBreak) noexcept
{
    switch (lineBreak)
    {
        case ELineBreak::None:
            return std::string_view();

        case ELineBreak::LF:
            return std::string_view("\n", 1);

        case ELineBreak::CR:
            return std::string_view("\r", 1);

        case ELineBreak::CRLF:
            return std::string_view("\r\n", 2);
    }

    return std::string_view();
}
//---------------------------------------------------------------------------
ELineBreak NativeLineBreak() noexcept
{
#if defined(_WIN32)
    return ELineBreak::CRLF;
#else
    return ELineBreak::LF;
#endif
}
//---------------------------------------------------------------------------
std::vector<TLineSpan> SplitLines(std::string_view text)
{
    std::vector<TLineSpan> lines;
    std::size_t lineStart = 0;
    std::size_t i = 0;

    while (i < text.size())
    {
        char const c = text[i];

        if (c != '\n' && c != '\r')
        {
            ++i;
            continue;
        }

        TLineSpan line;
        line.Text = text.substr(lineStart, i - lineStart);

        if (c == '\r' && i + 1 < text.size() && text[i + 1] == '\n')
        {
            line.Break = ELineBreak::CRLF;
            i += 2;
        }
        else
        {
            line.Break = c == '\n' ? ELineBreak::LF : ELineBreak::CR;
            ++i;
        }

        lines.push_back(line);
        lineStart = i;
    }

    // The last line, when the text doesn't end with a line break
    if (lineStart < text.size())
    {
        TLineSpan line;
        line.Text = text.substr(lineStart);
        lines.push_back(line);
    }

    return lines;
}
//---------------------------------------------------------------------------
std::string_view Trim(std::string_view text) noexcept
{
    return TrimRight(TrimLeft(text));
}
//---------------------------------------------------------------------------
std::string_view TrimLeft(std::string_view text) noexcept
{
    std::size_t start = 0;

    while (start < text.size() && IsBlank(text[start]))
        ++start;

    return text.substr(start);
}
//---------------------------------------------------------------------------
std::string_view TrimRight(std::string_view text) noexcept
{
    std::size_t end = text.size();

    while (end > 0 && IsBlank(text[end - 1]))
        --end;

    return text.substr(0, end);
}
//---------------------------------------------------------------------------

} // namespace Detail

} // namespace ASWIni
