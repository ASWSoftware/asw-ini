/* **************************************************************************
Test_ASWIni_Utils.cpp
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
#include "Test_ASWIni_Utils.h"
//---------------------------------------------------------------------------
#include "ASWUnitTests_Registry.h"
//---------------------------------------------------------------------------
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>
//---------------------------------------------------------------------------
#include "ASWIni_Types.h"
#include "ASWIni_Utils.h"
//---------------------------------------------------------------------------

namespace ASWUnitTests
{

namespace
{

using ASWIni::Detail::EBom;
using ASWIni::Detail::TLineSpan;
using ASWIni::ELineBreak;

std::string Bytes(char const* bytes, std::size_t size);
std::string Join(std::vector<TLineSpan> const& lines);

//---------------------------------------------------------------------------
/*
    Bytes

    A text with embedded NUL bytes or other bytes a C string can't hold, from a literal and its size
*/
std::string Bytes(char const* bytes, std::size_t size)
{
    return std::string(bytes, size);
}
//---------------------------------------------------------------------------
/*
    Join

    'lines' joined back together, each with its line break
*/
//---------------------------------------------------------------------------
std::string Join(std::vector<TLineSpan> const& lines)
{
    std::string text;

    for (TLineSpan const& line : lines)
    {
        text.append(line.Text.data(), line.Text.size());
        std::string_view const lineBreak = ASWIni::Detail::LineBreakText(line.Break);
        text.append(lineBreak.data(), lineBreak.size());
    }

    return text;
}

} // namespace

//---------------------------------------------------------------------------

/////////////////////////////////////////////////////////////////////////////
// TTest_ASWIni_Utils
/////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------
TTest_ASWIni_Utils::TTest_ASWIni_Utils()
    : inherited("ASWIni_Utils_Tests")
{
}
//---------------------------------------------------------------------------
TTest_ASWIni_Utils::~TTest_ASWIni_Utils()
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_Utils::SetUp_Group()
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_Utils::SetUp_Test(ITestCase& /*testCase*/)
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_Utils::TearDown_Group()
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_Utils::TearDown_Test(ITestCase& /*testCase*/)
{
}
//---------------------------------------------------------------------------

// /////// Begin tests after this line ///////////////////////

//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_Utils, BomSize_MatchesEachMark)
{
    CheckEquals(0, static_cast<int>(ASWIni::Detail::BomSize(EBom::None)), __func__, __LINE__, "no mark");
    CheckEquals(3, static_cast<int>(ASWIni::Detail::BomSize(EBom::Utf8)), __func__, __LINE__, "UTF-8");
    CheckEquals(2, static_cast<int>(ASWIni::Detail::BomSize(EBom::Utf16LE)), __func__, __LINE__, "UTF-16 LE");
    CheckEquals(2, static_cast<int>(ASWIni::Detail::BomSize(EBom::Utf16BE)), __func__, __LINE__, "UTF-16 BE");
    CheckEquals(4, static_cast<int>(ASWIni::Detail::BomSize(EBom::Utf32LE)), __func__, __LINE__, "UTF-32 LE");
    CheckEquals(4, static_cast<int>(ASWIni::Detail::BomSize(EBom::Utf32BE)), __func__, __LINE__, "UTF-32 BE");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_Utils, CompareNoCase_IgnoresAsciiCaseOnly)
{
    CheckEquals(0, ASWIni::Detail::CompareNoCase("General", "GENERAL"), __func__, __LINE__,
        "ASCII letters should match in any case");
    CheckEquals(0, ASWIni::Detail::CompareNoCase("Log_Level-2", "log_level-2"), __func__, __LINE__,
        "digits and punctuation should match themselves");

    // U+00C9 and U+00E9 (E and e with acute) are different UTF-8 bytes; only ASCII is folded
    CheckNotEquals(0, ASWIni::Detail::CompareNoCase("\xC3\x89", "\xC3\xA9"), __func__, __LINE__,
        "non-ASCII letters should not be folded");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_Utils, CompareNoCase_OrdersByBytesThenLength)
{
    CheckEquals(-1, ASWIni::Detail::CompareNoCase("apple", "Banana"), __func__, __LINE__,
        "a should sort before B");
    CheckEquals(1, ASWIni::Detail::CompareNoCase("b", "A"), __func__, __LINE__, "b should sort after A");
    CheckEquals(-1, ASWIni::Detail::CompareNoCase("Key", "key2"), __func__, __LINE__,
        "a prefix should sort before the longer text");
    CheckEquals(1, ASWIni::Detail::CompareNoCase("key2", "Key"), __func__, __LINE__,
        "the longer text should sort after its prefix");
    CheckEquals(-1, ASWIni::Detail::CompareNoCase("z", "\xC3\xA9"), __func__, __LINE__,
        "bytes above 0x7F should compare as unsigned, after ASCII");
    CheckEquals(0, ASWIni::Detail::CompareNoCase("", ""), __func__, __LINE__, "two empty texts should be equal");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_Utils, DetectBom_FindsEachMark)
{
    CheckEquals(EBom::Utf8, ASWIni::Detail::DetectBom("\xEF\xBB\xBF[General]"), __func__, __LINE__, "UTF-8");
    CheckEquals(EBom::Utf16LE, ASWIni::Detail::DetectBom(Bytes("\xFF\xFE[\x00", 4)), __func__, __LINE__,
        "UTF-16 LE");
    CheckEquals(EBom::Utf16BE, ASWIni::Detail::DetectBom(Bytes("\xFE\xFF\x00[", 4)), __func__, __LINE__,
        "UTF-16 BE");
    CheckEquals(EBom::Utf32LE, ASWIni::Detail::DetectBom(Bytes("\xFF\xFE\x00\x00[\x00\x00\x00", 8)), __func__,
        __LINE__, "UTF-32 LE should be found before the UTF-16 LE mark it starts with");
    CheckEquals(EBom::Utf32BE, ASWIni::Detail::DetectBom(Bytes("\x00\x00\xFE\xFF\x00\x00\x00[", 8)), __func__,
        __LINE__, "UTF-32 BE");
    CheckEquals(EBom::None, ASWIni::Detail::DetectBom("[General]"), __func__, __LINE__, "no mark");
    CheckEquals(EBom::None, ASWIni::Detail::DetectBom(""), __func__, __LINE__, "an empty text");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_Utils, DetectBom_IgnoresPartialMarks)
{
    CheckEquals(EBom::None, ASWIni::Detail::DetectBom("\xEF\xBB"), __func__, __LINE__,
        "two bytes of the UTF-8 mark");
    CheckEquals(EBom::None, ASWIni::Detail::DetectBom("\xFF"), __func__, __LINE__, "one byte of the UTF-16 LE mark");
    CheckEquals(EBom::None, ASWIni::Detail::DetectBom(Bytes("\x00\x00\xFE", 3)), __func__, __LINE__,
        "three bytes of the UTF-32 BE mark");
    CheckEquals(EBom::None, ASWIni::Detail::DetectBom("a\xEF\xBB\xBF"), __func__, __LINE__,
        "a mark that isn't at the start");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_Utils, EqualsNoCase_ComparesWholeText)
{
    CheckTrue(ASWIni::Detail::EqualsNoCase("LogLevel", "LOGLEVEL"), __func__, __LINE__,
        "the same text in another case should be equal");
    CheckTrue(ASWIni::Detail::EqualsNoCase("", ""), __func__, __LINE__, "two empty texts should be equal");
    CheckFalse(ASWIni::Detail::EqualsNoCase("Log", "LogLevel"), __func__, __LINE__,
        "a prefix should not be equal");
    CheckFalse(ASWIni::Detail::EqualsNoCase("Log Level", "LogLevel"), __func__, __LINE__,
        "a space should not be ignored");
    CheckFalse(ASWIni::Detail::EqualsNoCase(std::string_view("a\0b", 3), std::string_view("a\0c", 3)), __func__,
        __LINE__, "bytes after a NUL should be compared");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_Utils, IsValidUtf8_AcceptsValidText)
{
    CheckTrue(ASWIni::Detail::IsValidUtf8(""), __func__, __LINE__, "an empty text");
    CheckTrue(ASWIni::Detail::IsValidUtf8("Plain ASCII = 1"), __func__, __LINE__, "ASCII");
    CheckTrue(ASWIni::Detail::IsValidUtf8(Bytes("a\0b", 3)), __func__, __LINE__, "a NUL byte is ASCII");
    CheckTrue(ASWIni::Detail::IsValidUtf8("\xC2\x80" "\xDF\xBF"), __func__, __LINE__,
        "two-byte sequences U+0080 and U+07FF");
    CheckTrue(ASWIni::Detail::IsValidUtf8("\xE0\xA0\x80" "\xED\x9F\xBF" "\xEE\x80\x80" "\xEF\xBF\xBF"), __func__,
        __LINE__, "three-byte sequences U+0800, U+D7FF, U+E000 and U+FFFF");
    CheckTrue(ASWIni::Detail::IsValidUtf8("\xF0\x90\x80\x80" "\xF4\x8F\xBF\xBF"), __func__, __LINE__,
        "four-byte sequences U+10000 and U+10FFFF");
    CheckTrue(ASWIni::Detail::IsValidUtf8("caf" "\xC3\xA9" " = \xE2\x82\xAC" "5"), __func__, __LINE__,
        "mixed ASCII and sequences");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_Utils, IsValidUtf8_RejectsInvalidSequences)
{
    CheckFalse(ASWIni::Detail::IsValidUtf8("\x80"), __func__, __LINE__, "a continuation byte without a lead byte");
    CheckFalse(ASWIni::Detail::IsValidUtf8("\xC0\xAF"), __func__, __LINE__, "an overlong two-byte form (C0)");
    CheckFalse(ASWIni::Detail::IsValidUtf8("\xC1\xBF"), __func__, __LINE__, "an overlong two-byte form (C1)");
    CheckFalse(ASWIni::Detail::IsValidUtf8("\xE0\x9F\xBF"), __func__, __LINE__, "an overlong three-byte form");
    CheckFalse(ASWIni::Detail::IsValidUtf8("\xF0\x8F\xBF\xBF"), __func__, __LINE__, "an overlong four-byte form");
    CheckFalse(ASWIni::Detail::IsValidUtf8("\xED\xA0\x80"), __func__, __LINE__, "a surrogate (U+D800)");
    CheckFalse(ASWIni::Detail::IsValidUtf8("\xED\xBF\xBF"), __func__, __LINE__, "a surrogate (U+DFFF)");
    CheckFalse(ASWIni::Detail::IsValidUtf8("\xF4\x90\x80\x80"), __func__, __LINE__, "above U+10FFFF");
    CheckFalse(ASWIni::Detail::IsValidUtf8("\xF5\x80\x80\x80"), __func__, __LINE__, "a lead byte above F4");
    CheckFalse(ASWIni::Detail::IsValidUtf8("\xFF"), __func__, __LINE__, "the byte FF");
    CheckFalse(ASWIni::Detail::IsValidUtf8("ab\xE2\x82"), __func__, __LINE__, "a sequence cut off at the end");
    CheckFalse(ASWIni::Detail::IsValidUtf8("\xE2\x82" "A"), __func__, __LINE__,
        "a sequence with an ASCII byte where a continuation byte should be");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_Utils, LineBreakText_MatchesEachBreak)
{
    CheckEquals("", std::string(ASWIni::Detail::LineBreakText(ELineBreak::None)), __func__, __LINE__,
        "None");
    CheckEquals("\n", std::string(ASWIni::Detail::LineBreakText(ELineBreak::LF)), __func__, __LINE__,
        "LF");
    CheckEquals("\r", std::string(ASWIni::Detail::LineBreakText(ELineBreak::CR)), __func__, __LINE__,
        "CR");
    CheckEquals("\r\n", std::string(ASWIni::Detail::LineBreakText(ELineBreak::CRLF)), __func__,
        __LINE__, "CRLF");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_Utils, NativeLineBreak_MatchesPlatform)
{
    // Act
    ELineBreak const lineBreak = ASWIni::Detail::NativeLineBreak();

    // Assert
#if defined(_WIN32)
    CheckEquals(ELineBreak::CRLF, lineBreak, __func__, __LINE__, "Windows should use CRLF");
#else
    CheckEquals(ELineBreak::LF, lineBreak, __func__, __LINE__, "other platforms should use LF");
#endif
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_Utils, SplitLines_EmptyText)
{
    // Act
    std::vector<TLineSpan> const lines = ASWIni::Detail::SplitLines("");

    // Assert
    CheckEmpty(lines, __func__, __LINE__, "an empty text should have no lines");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_Utils, SplitLines_JoinsBackToOriginal)
{
    // Arrange: texts whose lines must join back to exactly the same bytes
    std::vector<std::string> const texts = {
        "[General]\r\nKey = 1\r\n",
        "a\nb\rc\r\nd",
        "\n\n\r\n\r\r",
        "a\r\r\nb\n\r",
        "no line break",
        Bytes("nul\0inside\nline", 15),
        "\xEF\xBB\xBF" "; comment\n"
    };

    for (std::string const& text : texts)
    {
        // Act
        std::string const joined = Join(ASWIni::Detail::SplitLines(text));

        // Assert
        CheckEquals(text, joined, __func__, __LINE__, "joining the lines should give back the text");
    }
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_Utils, SplitLines_KeepsEachLineBreak)
{
    // Arrange
    // An LF line, a CR line, an empty CRLF line (the CR before it belongs to "b") and a CRLF line
    std::string const text = "a\nb\r\r\nc\r\n";

    // Act
    std::vector<TLineSpan> const lines = ASWIni::Detail::SplitLines(text);

    // Assert
    AssertEquals(4, static_cast<int>(lines.size()), __func__, __LINE__, "there should be four lines");
    CheckEquals("a", std::string(lines[0].Text), __func__, __LINE__, "line 1's text");
    CheckEquals(ELineBreak::LF, lines[0].Break, __func__, __LINE__, "line 1 should end with LF");
    CheckEquals("b", std::string(lines[1].Text), __func__, __LINE__, "line 2's text");
    CheckEquals(ELineBreak::CR, lines[1].Break, __func__, __LINE__, "line 2 should end with CR");
    CheckEquals("", std::string(lines[2].Text), __func__, __LINE__, "line 3 should be empty");
    CheckEquals(ELineBreak::CRLF, lines[2].Break, __func__, __LINE__, "line 3 should end with CRLF");
    CheckEquals("c", std::string(lines[3].Text), __func__, __LINE__, "line 4's text");
    CheckEquals(ELineBreak::CRLF, lines[3].Break, __func__, __LINE__, "line 4 should end with CRLF");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_Utils, SplitLines_LastLineWithoutBreak)
{
    // Act
    std::vector<TLineSpan> const lines = ASWIni::Detail::SplitLines("Key=1\nLast=2");

    // Assert
    AssertEquals(2, static_cast<int>(lines.size()), __func__, __LINE__, "there should be two lines");
    CheckEquals("Last=2", std::string(lines[1].Text), __func__, __LINE__, "the last line's text");
    CheckEquals(ELineBreak::None, lines[1].Break, __func__, __LINE__, "the last line should have no line break");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_Utils, ToLowerAscii_ChangesOnlyUpperCaseLetters)
{
    CheckEquals(static_cast<int>('a'), static_cast<int>(ASWIni::Detail::ToLowerAscii('A')), __func__, __LINE__, "A");
    CheckEquals(static_cast<int>('z'), static_cast<int>(ASWIni::Detail::ToLowerAscii('Z')), __func__, __LINE__, "Z");
    CheckEquals(static_cast<int>('m'), static_cast<int>(ASWIni::Detail::ToLowerAscii('m')), __func__, __LINE__,
        "a lower case letter should stay");
    CheckEquals(static_cast<int>('@'), static_cast<int>(ASWIni::Detail::ToLowerAscii('@')), __func__, __LINE__,
        "the character before A should stay");
    CheckEquals(static_cast<int>('['), static_cast<int>(ASWIni::Detail::ToLowerAscii('[')), __func__, __LINE__,
        "the character after Z should stay");
    CheckEquals(static_cast<int>('\xC3'), static_cast<int>(ASWIni::Detail::ToLowerAscii('\xC3')), __func__, __LINE__,
        "a byte above 0x7F should stay");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_Utils, Trim_RemovesSpacesAndTabsOnly)
{
    CheckEquals("a b", std::string(ASWIni::Detail::Trim(" \t a b\t ")), __func__, __LINE__,
        "spaces and tabs at both ends should go, the inner space should stay");
    CheckEquals("a b\t ", std::string(ASWIni::Detail::TrimLeft(" \t a b\t ")), __func__, __LINE__,
        "TrimLeft should only trim the start");
    CheckEquals(" \t a b", std::string(ASWIni::Detail::TrimRight(" \t a b\t ")), __func__, __LINE__,
        "TrimRight should only trim the end");
    CheckEquals("", std::string(ASWIni::Detail::Trim(" \t\t ")), __func__, __LINE__,
        "only blanks should trim to empty");
    CheckEquals("\r a \n", std::string(ASWIni::Detail::Trim("\r a \n")), __func__, __LINE__,
        "line breaks should not be trimmed");
    CheckEquals("", std::string(ASWIni::Detail::Trim("")), __func__, __LINE__,
        "an empty text should stay empty");
}
//---------------------------------------------------------------------------

} // namespace ASWUnitTests

//---------------------------------------------------------------------------
ASW_REGISTER_TEST_GROUP(ASWUnitTests::TTest_ASWIni_Utils)
