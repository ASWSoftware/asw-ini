/* **************************************************************************
Test_ASWIni_Document.cpp
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
#include "Test_ASWIni_Document.h"
//---------------------------------------------------------------------------
#include "ASWUnitTests_Registry.h"
//---------------------------------------------------------------------------
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>
//---------------------------------------------------------------------------
#include "ASWIni_Document.h"
#include "ASWIni_Options.h"
#include "ASWIni_Types.h"
//---------------------------------------------------------------------------

namespace ASWUnitTests
{

namespace
{

using ASWIni::EIniCode;
using ASWIni::ELineBreak;
using ASWIni::ELineKind;
using ASWIni::TASWIniDocument;
using ASWIni::TASWIniDocumentLine;
using ASWIni::TASWIniMessage;
using ASWIni::TASWIniReadingOptions;
using ASWIni::TASWIniResult;

std::vector<TASWIniDocumentLine> AllLines(TASWIniDocument const& document);
TASWIniDocument ParseText(std::string_view text, TASWIniReadingOptions const& options, TASWIniResult& result,
    std::vector<TASWIniMessage>& messages);

//---------------------------------------------------------------------------
/*
    AllLines

    The lines of all the sections of 'document', in file order.
*/
std::vector<TASWIniDocumentLine> AllLines(TASWIniDocument const& document)
{
    std::vector<TASWIniDocumentLine> lines;
    for (ASWIni::TASWIniDocumentSection const& section : document.GetSections())
        lines.insert(lines.end(), section.GetLines().begin(), section.GetLines().end());

    return lines;
}
//---------------------------------------------------------------------------
/*
    ParseText

    'text' parsed with 'options' into a new document, with Parse()'s result in 'result' and its messages in 'messages'.
*/
TASWIniDocument ParseText(std::string_view text, TASWIniReadingOptions const& options, TASWIniResult& result,
    std::vector<TASWIniMessage>& messages)
{
    TASWIniDocument document;
    result = document.Parse(text, options, messages);

    return document;
}
//---------------------------------------------------------------------------

} // namespace


/////////////////////////////////////////////////////////////////////////////
// TTest_ASWIni_DocumentModel
/////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------
TTest_ASWIni_DocumentModel::TTest_ASWIni_DocumentModel()
    : inherited("ASWIni_DocumentModel_Tests")
{
}
//---------------------------------------------------------------------------
TTest_ASWIni_DocumentModel::~TTest_ASWIni_DocumentModel()
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_DocumentModel::SetUp_Group()
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_DocumentModel::SetUp_Test(ITestCase& /*testCase*/)
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_DocumentModel::TearDown_Group()
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_DocumentModel::TearDown_Test(ITestCase& /*testCase*/)
{
}
//---------------------------------------------------------------------------

// /////// Begin tests after this line ///////////////////////

//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_DocumentModel, Document_CopyIsIndependent)
{
    // Arrange
    TASWIniReadingOptions const options;
    std::vector<TASWIniMessage> messages;
    TASWIniDocument original;
    AssertTrue(original.Parse("[General]\nWidth = 640\n", options, messages).IsOk(), __func__, __LINE__,
        "the original should parse");

    // Act
    TASWIniDocument const copy = original;
    AssertTrue(original.Parse("[Other]\n", options, messages).IsOk(), __func__, __LINE__,
        "the original should parse again");

    // Assert
    AssertEquals(2, static_cast<int>(copy.GetSections().size()), __func__, __LINE__,
        "the copy should keep its preamble and section");
    CheckEquals(std::string("General"), std::string(copy.GetSections()[1].GetName()), __func__, __LINE__,
        "the copy should keep its section's name");
    CheckEquals(std::string("640"), std::string(copy.GetSections()[1].GetLines()[1].GetValue()), __func__, __LINE__,
        "the copy should keep its value");
    CheckEquals(std::string("Other"), std::string(original.GetSections()[1].GetName()), __func__, __LINE__,
        "the original should hold what it parsed last");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_DocumentModel, Document_DefaultIsEmpty)
{
    // Act
    TASWIniDocument const document;

    // Assert
    AssertEquals(1, static_cast<int>(document.GetSections().size()), __func__, __LINE__,
        "a new document should have one section, its preamble");
    CheckFalse(document.GetSections().front().HasHeader(), __func__, __LINE__, "the preamble should have no header");
    CheckEquals(0, static_cast<int>(document.GetLineCount()), __func__, __LINE__,
        "a new document should have no lines");
    CheckFalse(document.HasBom(), __func__, __LINE__, "a new document should have no byte order mark");
#if defined(_WIN32)
    CheckEquals(ELineBreak::CRLF, document.GetLineBreak(), __func__, __LINE__,
        "new lines should get the platform's line break, CRLF on Windows");
#else
    CheckEquals(ELineBreak::LF, document.GetLineBreak(), __func__, __LINE__,
        "new lines should get the platform's line break, LF on other platforms");
#endif
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_DocumentModel, Line_DefaultIsBlank)
{
    // Act
    TASWIniDocumentLine const line;

    // Assert
    CheckEquals(ELineKind::Blank, line.GetKind(), __func__, __LINE__, "a new line should be blank");
    CheckEquals(std::string(), line.GetText(), __func__, __LINE__, "a new line should have no text");
    CheckEquals(ELineBreak::None, line.GetLineBreak(), __func__, __LINE__, "a new line should have no line break");
    CheckEquals(std::string(), std::string(line.GetKey()), __func__, __LINE__, "a blank line should have no key");
    CheckEquals(std::string(), std::string(line.GetValue()), __func__, __LINE__, "a blank line should have no value");
    CheckEquals(std::string(), std::string(line.GetSectionName()), __func__, __LINE__,
        "a blank line should have no section name");
    CheckEquals(0, static_cast<int>(line.GetSeparator()), __func__, __LINE__,
        "a blank line's separator should be '\\0'");
    CheckFalse(line.HasComment(), __func__, __LINE__, "a blank line should have no comment");
    CheckEquals(std::string(), std::string(line.GetComment()), __func__, __LINE__,
        "a blank line's comment should be empty");
    CheckFalse(line.IsQuoted(), __func__, __LINE__, "a blank line should have no quoted value");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_DocumentModel, Section_DefaultIsEmptyPreamble)
{
    // Act
    ASWIni::TASWIniDocumentSection const section;

    // Assert
    CheckFalse(section.HasHeader(), __func__, __LINE__, "a new section should have no header");
    CheckEquals(std::string(), std::string(section.GetName()), __func__, __LINE__,
        "a section with no header should have the name \"\"");
    CheckEquals(0, static_cast<int>(section.GetLines().size()), __func__, __LINE__,
        "a new section should have no lines");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_DocumentModel, ToString_LineKindNames)
{
    CheckEquals(std::string("Blank"), std::string(ASWIni::ToString(ELineKind::Blank)), __func__, __LINE__,
        "Blank's name");
    CheckEquals(std::string("Comment"), std::string(ASWIni::ToString(ELineKind::Comment)), __func__, __LINE__,
        "Comment's name");
    CheckEquals(std::string("Section"), std::string(ASWIni::ToString(ELineKind::Section)), __func__, __LINE__,
        "Section's name");
    CheckEquals(std::string("KeyValue"), std::string(ASWIni::ToString(ELineKind::KeyValue)), __func__, __LINE__,
        "KeyValue's name");
    CheckEquals(std::string("Raw"), std::string(ASWIni::ToString(ELineKind::Raw)), __func__, __LINE__,
        "Raw's name");
    CheckEquals(std::string("Unknown"), std::string(ASWIni::ToString(static_cast<ELineKind>(99))), __func__,
        __LINE__, "a value that isn't an ELineKind should be \"Unknown\"");
}
//---------------------------------------------------------------------------


/////////////////////////////////////////////////////////////////////////////
// TTest_ASWIni_DocumentParse
/////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------
TTest_ASWIni_DocumentParse::TTest_ASWIni_DocumentParse()
    : inherited("ASWIni_DocumentParse_Tests")
{
}
//---------------------------------------------------------------------------
TTest_ASWIni_DocumentParse::~TTest_ASWIni_DocumentParse()
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_DocumentParse::CheckKeyValue(TASWIniDocumentLine const& line, std::string const& key,
    std::string const& value, std::string const& method, int lineNumber, std::string const& msg)
{
    CheckEquals(ELineKind::KeyValue, line.GetKind(), method, lineNumber, msg + ": the line should be a KeyValue");
    CheckEquals(key, std::string(line.GetKey()), method, lineNumber, msg + ": the key");
    CheckEquals(value, std::string(line.GetValue()), method, lineNumber, msg + ": the value");
}
//---------------------------------------------------------------------------
TASWIniDocument TTest_ASWIni_DocumentParse::ParseValid(std::string_view text, TASWIniReadingOptions const& options,
    std::string const& method, int lineNumber)
{
    TASWIniResult result;
    std::vector<TASWIniMessage> messages;
    TASWIniDocument document = ParseText(text, options, result, messages);

    AssertTrue(result.IsOk(), method, lineNumber, "the text should parse: " + result.Message);
    AssertEquals(0, static_cast<int>(messages.size()), method, lineNumber,
        "the text should parse without warnings" + (messages.empty() ? std::string() : ": " + messages[0].Text));

    return document;
}
//---------------------------------------------------------------------------
void TTest_ASWIni_DocumentParse::SetUp_Group()
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_DocumentParse::SetUp_Test(ITestCase& /*testCase*/)
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_DocumentParse::TearDown_Group()
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_DocumentParse::TearDown_Test(ITestCase& /*testCase*/)
{
}
//---------------------------------------------------------------------------

// /////// Begin tests after this line ///////////////////////

//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_DocumentParse, Bom_IsDetectedAndNotPartOfALine)
{
    // Arrange
    TASWIniReadingOptions const options;

    // Act
    TASWIniDocument const withContent = ParseValid("\xEF\xBB\xBF[S]\r\nk=v", options, __func__, __LINE__);
    TASWIniDocument const bomOnly = ParseValid("\xEF\xBB\xBF", options, __func__, __LINE__);
    TASWIniDocument const noBom = ParseValid("[S]", options, __func__, __LINE__);

    // Assert
    CheckTrue(withContent.HasBom(), __func__, __LINE__, "a text with a UTF-8 byte order mark should have one");
    std::vector<TASWIniDocumentLine> const lines = AllLines(withContent);
    AssertEquals(2, static_cast<int>(lines.size()), __func__, __LINE__, "the text should have two lines");
    CheckEquals(std::string("[S]"), lines[0].GetText(), __func__, __LINE__,
        "the byte order mark shouldn't be part of the first line");
    CheckEquals(std::string("S"), std::string(lines[0].GetSectionName()), __func__, __LINE__,
        "the first line should still be the section header");
    CheckTrue(bomOnly.HasBom(), __func__, __LINE__, "a text of only a byte order mark should have one");
    CheckEquals(0, static_cast<int>(bomOnly.GetLineCount()), __func__, __LINE__,
        "a text of only a byte order mark should have no lines");
    CheckFalse(noBom.HasBom(), __func__, __LINE__, "a text without a byte order mark should have none");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_DocumentParse, CommentPrefixes_AreConfigurable)
{
    // Arrange
    TASWIniReadingOptions options;
    options.CommentPrefixes = { ";", ";;", "//" };

    // Act
    std::vector<TASWIniDocumentLine> const lines = AllLines(ParseValid("// note\n//x=1\n#x=1\n;; double\n", options,
        __func__, __LINE__));

    // Assert
    AssertEquals(4, static_cast<int>(lines.size()), __func__, __LINE__, "the text should have four lines");
    CheckEquals(ELineKind::Comment, lines[0].GetKind(), __func__, __LINE__, "\"// note\" should be a comment");
    CheckEquals(std::string("note"), std::string(lines[0].GetComment()), __func__, __LINE__,
        "the comment's text should follow the \"//\"");
    CheckEquals(ELineKind::Comment, lines[1].GetKind(), __func__, __LINE__,
        "\"//x=1\" should be a comment, since comments come before keys");
    CheckKeyValue(lines[2], "#x", "1", __func__, __LINE__, "\"#x=1\" without '#' as a prefix");
    CheckEquals(std::string("double"), std::string(lines[3].GetComment()), __func__, __LINE__,
        "the longest prefix that matches should be the comment's prefix");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_DocumentParse, Comments_KeepTheirText)
{
    // Arrange
    TASWIniReadingOptions const options;

    // Act
    std::vector<TASWIniDocumentLine> const lines = AllLines(ParseValid("; first\n# second \n  \t;indented\n;\n",
        options, __func__, __LINE__));

    // Assert
    AssertEquals(4, static_cast<int>(lines.size()), __func__, __LINE__, "the text should have four lines");
    for (TASWIniDocumentLine const& line : lines)
    {
        CheckEquals(ELineKind::Comment, line.GetKind(), __func__, __LINE__, "\"" + line.GetText() + "\" kind");
        CheckTrue(line.HasComment(), __func__, __LINE__, "\"" + line.GetText() + "\" should have a comment");
    }

    CheckEquals(std::string("first"), std::string(lines[0].GetComment()), __func__, __LINE__, "a ';' comment");
    CheckEquals(std::string("second"), std::string(lines[1].GetComment()), __func__, __LINE__,
        "a '#' comment, without its trailing space");
    CheckEquals(std::string("indented"), std::string(lines[2].GetComment()), __func__, __LINE__,
        "an indented comment");
    CheckEquals(std::string(), std::string(lines[3].GetComment()), __func__, __LINE__, "an empty comment");
    CheckEquals(std::string("  \t;indented"), lines[2].GetText(), __func__, __LINE__,
        "the line's text should be kept as it is");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_DocumentParse, EmptyText_HasNoLines)
{
    // Act
    TASWIniDocument const document = ParseValid("", TASWIniReadingOptions(), __func__, __LINE__);

    // Assert
    CheckEquals(1, static_cast<int>(document.GetSections().size()), __func__, __LINE__,
        "an empty text should have only the preamble");
    CheckEquals(0, static_cast<int>(document.GetLineCount()), __func__, __LINE__, "an empty text should have no lines");
    CheckEquals(TASWIniDocument().GetLineBreak(), document.GetLineBreak(), __func__, __LINE__,
        "an empty text should get the platform's line break");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_DocumentParse, InlineComments_AreOffByDefault)
{
    // Act
    std::vector<TASWIniDocumentLine> const lines = AllLines(ParseValid("k = v ; note\n", TASWIniReadingOptions(),
        __func__, __LINE__));

    // Assert
    AssertEquals(1, static_cast<int>(lines.size()), __func__, __LINE__, "the text should have one line");
    CheckKeyValue(lines[0], "k", "v ; note", __func__, __LINE__, "the prefix should be part of the value");
    CheckFalse(lines[0].HasComment(), __func__, __LINE__, "the line should have no comment");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_DocumentParse, InlineComments_NeedABlankBeforeThePrefix)
{
    // Arrange
    TASWIniReadingOptions options;
    options.InlineComments = true;

    // Act
    std::vector<TASWIniDocumentLine> const lines = AllLines(ParseValid(
        "a = v ; note\nb = v;x\nc = ; note\nd=;note\ne = v\t# tabbed \n", options, __func__, __LINE__));

    // Assert
    AssertEquals(5, static_cast<int>(lines.size()), __func__, __LINE__, "the text should have five lines");
    CheckKeyValue(lines[0], "a", "v", __func__, __LINE__, "a comment after a space");
    CheckEquals(std::string("note"), std::string(lines[0].GetComment()), __func__, __LINE__, "the comment's text");
    CheckKeyValue(lines[1], "b", "v;x", __func__, __LINE__, "a prefix with no blank before it");
    CheckFalse(lines[1].HasComment(), __func__, __LINE__, "a prefix with no blank before it shouldn't be a comment");
    CheckKeyValue(lines[2], "c", "", __func__, __LINE__, "an empty value with a comment");
    CheckTrue(lines[2].HasComment(), __func__, __LINE__, "the empty value should have a comment");
    CheckKeyValue(lines[3], "d", ";note", __func__, __LINE__, "a prefix right after the separator");
    CheckFalse(lines[3].HasComment(), __func__, __LINE__, "a prefix right after the separator shouldn't be a comment");
    CheckKeyValue(lines[4], "e", "v", __func__, __LINE__, "a comment after a tab");
    CheckEquals(std::string("tabbed"), std::string(lines[4].GetComment()), __func__, __LINE__,
        "the comment's text, trimmed");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_DocumentParse, KeyValue_FirstSeparatorEndsTheKey)
{
    // Act
    std::vector<TASWIniDocumentLine> const lines = AllLines(ParseValid(
        "url = http://x\na:b=c\nk = v=w\ncolon: value\n", TASWIniReadingOptions(), __func__, __LINE__));

    // Assert
    AssertEquals(4, static_cast<int>(lines.size()), __func__, __LINE__, "the text should have four lines");
    CheckKeyValue(lines[0], "url", "http://x", __func__, __LINE__, "a ':' in the value");
    CheckEquals(static_cast<int>('='), static_cast<int>(lines[0].GetSeparator()), __func__, __LINE__,
        "the separator of \"url = http://x\"");
    CheckKeyValue(lines[1], "a", "b=c", __func__, __LINE__, "a ':' before a '='");
    CheckEquals(static_cast<int>(':'), static_cast<int>(lines[1].GetSeparator()), __func__, __LINE__,
        "the separator of \"a:b=c\"");
    CheckKeyValue(lines[2], "k", "v=w", __func__, __LINE__, "a '=' in the value");
    CheckKeyValue(lines[3], "colon", "value", __func__, __LINE__, "a ':' separator");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_DocumentParse, KeyValue_KeepsNulAndLongValues)
{
    // Arrange
    std::string const nul("k = a\0b", 7);
    std::string const longValue(100 * 1024, 'x');

    // Act
    std::vector<TASWIniDocumentLine> const nulLines = AllLines(ParseValid(nul, TASWIniReadingOptions(), __func__,
        __LINE__));
    std::vector<TASWIniDocumentLine> const longLines = AllLines(ParseValid("long = " + longValue + "\n",
        TASWIniReadingOptions(), __func__, __LINE__));

    // Assert
    AssertEquals(1, static_cast<int>(nulLines.size()), __func__, __LINE__, "the NUL text should have one line");
    CheckKeyValue(nulLines[0], "k", std::string("a\0b", 3), __func__, __LINE__, "a value with a NUL byte");
    AssertEquals(1, static_cast<int>(longLines.size()), __func__, __LINE__, "the long text should have one line");
    CheckKeyValue(longLines[0], "long", longValue, __func__, __LINE__, "a 100 KiB value");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_DocumentParse, KeyValue_KeyAndValueAreTrimmed)
{
    // Act
    std::vector<TASWIniDocumentLine> const lines = AllLines(ParseValid(
        "\tMy Key \t=\t value with spaces \t\nempty =\nk=v\n", TASWIniReadingOptions(), __func__, __LINE__));

    // Assert
    AssertEquals(3, static_cast<int>(lines.size()), __func__, __LINE__, "the text should have three lines");
    CheckKeyValue(lines[0], "My Key", "value with spaces", __func__, __LINE__, "a key and value with blanks");
    CheckEquals(std::string("\tMy Key \t=\t value with spaces \t"), lines[0].GetText(), __func__, __LINE__,
        "the line's text should be kept as it is");
    CheckKeyValue(lines[1], "empty", "", __func__, __LINE__, "an empty value");
    CheckKeyValue(lines[2], "k", "v", __func__, __LINE__, "no blanks");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_DocumentParse, LineBreak_IsTheMostCommonOne)
{
    // Arrange
    TASWIniReadingOptions const options;
    ELineBreak const native = TASWIniDocument().GetLineBreak();

    // Act & Assert
    CheckEquals(ELineBreak::CRLF, ParseValid("a=1\r\nb=2\r\nc=3\n", options, __func__, __LINE__).GetLineBreak(),
        __func__, __LINE__, "two CRLF and one LF");
    CheckEquals(ELineBreak::LF, ParseValid("a=1\nb=2\r\n", options, __func__, __LINE__).GetLineBreak(), __func__,
        __LINE__, "a tie should go to the line break found first (LF)");
    CheckEquals(ELineBreak::CRLF, ParseValid("a=1\r\nb=2\n", options, __func__, __LINE__).GetLineBreak(), __func__,
        __LINE__, "a tie should go to the line break found first (CRLF)");
    CheckEquals(ELineBreak::CR, ParseValid("a=1\rb=2\rc=3", options, __func__, __LINE__).GetLineBreak(), __func__,
        __LINE__, "CR only, and a last line with no line break");
    CheckEquals(native, ParseValid("a=1", options, __func__, __LINE__).GetLineBreak(), __func__, __LINE__,
        "a text with no line break should get the platform's");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_DocumentParse, LineBreaks_AreKeptForEachLine)
{
    // Act
    std::vector<TASWIniDocumentLine> const lines = AllLines(ParseValid("a=1\nb=2\r\nc=3\rd=4", TASWIniReadingOptions(),
        __func__, __LINE__));

    // Assert
    AssertEquals(4, static_cast<int>(lines.size()), __func__, __LINE__, "the text should have four lines");
    CheckEquals(ELineBreak::LF, lines[0].GetLineBreak(), __func__, __LINE__, "line 1 should end with LF");
    CheckEquals(ELineBreak::CRLF, lines[1].GetLineBreak(), __func__, __LINE__, "line 2 should end with CRLF");
    CheckEquals(ELineBreak::CR, lines[2].GetLineBreak(), __func__, __LINE__, "line 3 should end with CR");
    CheckEquals(ELineBreak::None, lines[3].GetLineBreak(), __func__, __LINE__, "line 4 should have no line break");
    CheckEquals(std::string("b=2"), lines[1].GetText(), __func__, __LINE__,
        "a line's text shouldn't hold its line break");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_DocumentParse, Parse_ReplacesPreviousContents)
{
    // Arrange
    TASWIniReadingOptions const options;
    std::vector<TASWIniMessage> messages;
    TASWIniDocument document;
    AssertTrue(document.Parse("\xEF\xBB\xBF[A]\na=1\n[B]\n", options, messages).IsOk(), __func__, __LINE__,
        "the first text should parse");

    // Act
    TASWIniResult const result = document.Parse("x=1", options, messages);

    // Assert
    AssertTrue(result.IsOk(), __func__, __LINE__, "the second text should parse: " + result.Message);
    CheckEquals(1, static_cast<int>(document.GetSections().size()), __func__, __LINE__,
        "the sections of the first text should be gone");
    CheckEquals(1, static_cast<int>(document.GetLineCount()), __func__, __LINE__,
        "only the second text's line should be left");
    CheckFalse(document.HasBom(), __func__, __LINE__, "the first text's byte order mark should be gone");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_DocumentParse, QuotedValues_AreOffByDefault)
{
    // Act
    std::vector<TASWIniDocumentLine> const lines = AllLines(ParseValid("k = \" spaced \"\n", TASWIniReadingOptions(),
        __func__, __LINE__));

    // Assert
    AssertEquals(1, static_cast<int>(lines.size()), __func__, __LINE__, "the text should have one line");
    CheckKeyValue(lines[0], "k", "\" spaced \"", __func__, __LINE__, "the quotes should be part of the value");
    CheckFalse(lines[0].IsQuoted(), __func__, __LINE__, "the value shouldn't be quoted");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_DocumentParse, QuotedValues_LoseTheirQuotes)
{
    // Arrange
    TASWIniReadingOptions options;
    options.QuotedValues = true;
    TASWIniReadingOptions withComments = options;
    withComments.InlineComments = true;
    std::string const text = "a = \"  spaced  \"  \nb = \"\"\nc = \"x\" junk\nd = \"open\ne = \"x\" ; note\n"
        "f = \"a ; b\" ; note\ng = \"x\";note\n";

    // Act
    std::vector<TASWIniDocumentLine> const lines = AllLines(ParseValid(text, options, __func__, __LINE__));
    std::vector<TASWIniDocumentLine> const commented = AllLines(ParseValid(text, withComments, __func__, __LINE__));

    // Assert
    AssertEquals(7, static_cast<int>(lines.size()), __func__, __LINE__, "the text should have seven lines");
    CheckKeyValue(lines[0], "a", "  spaced  ", __func__, __LINE__, "a quoted value keeps its spaces");
    CheckTrue(lines[0].IsQuoted(), __func__, __LINE__, "\"  spaced  \" should be quoted");
    CheckKeyValue(lines[1], "b", "", __func__, __LINE__, "an empty quoted value");
    CheckTrue(lines[1].IsQuoted(), __func__, __LINE__, "\"\" should be quoted");
    CheckKeyValue(lines[2], "c", "\"x\" junk", __func__, __LINE__, "text after the closing quote");
    CheckFalse(lines[2].IsQuoted(), __func__, __LINE__, "a value with text after its closing quote isn't quoted");
    CheckKeyValue(lines[3], "d", "\"open", __func__, __LINE__, "no closing quote");
    CheckFalse(lines[3].IsQuoted(), __func__, __LINE__, "a value with no closing quote isn't quoted");
    CheckKeyValue(lines[4], "e", "\"x\" ; note", __func__, __LINE__,
        "a comment after the quotes without InlineComments");
    CheckFalse(lines[4].IsQuoted(), __func__, __LINE__, "a comment without InlineComments is text after the quote");

    AssertEquals(7, static_cast<int>(commented.size()), __func__, __LINE__, "the text should have seven lines");
    CheckKeyValue(commented[4], "e", "x", __func__, __LINE__, "a comment after the quotes with InlineComments");
    CheckTrue(commented[4].IsQuoted(), __func__, __LINE__, "\"x\" ; note should be quoted with InlineComments");
    CheckEquals(std::string("note"), std::string(commented[4].GetComment()), __func__, __LINE__,
        "the comment after the quotes");
    CheckKeyValue(commented[5], "f", "a ; b", __func__, __LINE__, "a prefix inside the quotes");
    CheckEquals(std::string("note"), std::string(commented[5].GetComment()), __func__, __LINE__,
        "the comment should be searched for after the closing quote");
    CheckKeyValue(commented[6], "g", "\"x\";note", __func__, __LINE__, "a prefix right after the closing quote");
    CheckFalse(commented[6].HasComment(), __func__, __LINE__,
        "a prefix right after the closing quote shouldn't be a comment");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_DocumentParse, Sections_DuplicatesAreSeparate)
{
    // Arrange
    TASWIniResult result;
    std::vector<TASWIniMessage> messages;

    // Act
    TASWIniDocument const document = ParseText("[A]\nk=1\nk=2\n[A]\nk=3\n", TASWIniReadingOptions(), result,
        messages);

    // Assert
    AssertTrue(result.IsOk(), __func__, __LINE__, "the text should parse: " + result.Message);
    AssertEquals(3, static_cast<int>(document.GetSections().size()), __func__, __LINE__,
        "the preamble and both [A] sections");
    CheckEquals(3, static_cast<int>(document.GetSections()[1].GetLines().size()), __func__, __LINE__,
        "the first [A] should keep both k lines");
    CheckEquals(2, static_cast<int>(document.GetSections()[2].GetLines().size()), __func__, __LINE__,
        "the second [A] should keep its k line");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_DocumentParse, Sections_HoldTheLinesAfterTheirHeader)
{
    // Act
    TASWIniDocument const document = ParseValid("; top\na=1\n\n[S]\nb=2\n; c\n[T]\n", TASWIniReadingOptions(),
        __func__, __LINE__);

    // Assert
    std::vector<ASWIni::TASWIniDocumentSection> const& sections = document.GetSections();
    AssertEquals(3, static_cast<int>(sections.size()), __func__, __LINE__, "the preamble, [S] and [T]");
    CheckFalse(sections[0].HasHeader(), __func__, __LINE__, "the preamble should have no header");
    CheckEquals(3, static_cast<int>(sections[0].GetLines().size()), __func__, __LINE__,
        "the preamble should hold the lines before the first header");
    CheckKeyValue(sections[0].GetLines()[1], "a", "1", __func__, __LINE__, "a key before any section");
    CheckTrue(sections[1].HasHeader(), __func__, __LINE__, "[S] should have a header");
    CheckEquals(std::string("S"), std::string(sections[1].GetName()), __func__, __LINE__, "[S]'s name");
    CheckEquals(3, static_cast<int>(sections[1].GetLines().size()), __func__, __LINE__,
        "[S] should hold its header, b and the comment");
    CheckEquals(ELineKind::Section, sections[1].GetLines()[0].GetKind(), __func__, __LINE__,
        "[S]'s first line should be its header");
    CheckEquals(1, static_cast<int>(sections[2].GetLines().size()), __func__, __LINE__,
        "[T] should hold only its header");
    CheckEquals(7, static_cast<int>(document.GetLineCount()), __func__, __LINE__, "the text has seven lines");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_DocumentParse, Sections_KeepATrailingComment)
{
    // Act
    std::vector<TASWIniDocumentLine> const lines = AllLines(ParseValid("[General] ; note\n[Other];x\n[Plain]  \n",
        TASWIniReadingOptions(), __func__, __LINE__));

    // Assert
    AssertEquals(3, static_cast<int>(lines.size()), __func__, __LINE__, "the text should have three lines");
    CheckEquals(std::string("General"), std::string(lines[0].GetSectionName()), __func__, __LINE__,
        "the name before the comment");
    CheckTrue(lines[0].HasComment(), __func__, __LINE__, "\"[General] ; note\" should have a comment");
    CheckEquals(std::string("note"), std::string(lines[0].GetComment()), __func__, __LINE__, "the comment's text");
    CheckEquals(std::string("x"), std::string(lines[1].GetComment()), __func__, __LINE__,
        "a comment right after the ']' (even with InlineComments off)");
    CheckFalse(lines[2].HasComment(), __func__, __LINE__, "blanks after the ']' aren't a comment");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_DocumentParse, Sections_NamesAreTrimmedAndFlat)
{
    // Act
    std::vector<TASWIniDocumentLine> const lines = AllLines(ParseValid(
        "[ General ]\n  [Indented]\n[a.b]\n[remote \"origin\"]\n[*.md]\n", TASWIniReadingOptions(), __func__,
        __LINE__));

    // Assert
    AssertEquals(5, static_cast<int>(lines.size()), __func__, __LINE__, "the text should have five lines");
    CheckEquals(std::string("General"), std::string(lines[0].GetSectionName()), __func__, __LINE__,
        "the blanks around the name should be trimmed");
    CheckEquals(std::string("Indented"), std::string(lines[1].GetSectionName()), __func__, __LINE__,
        "an indented header");
    CheckEquals(std::string("a.b"), std::string(lines[2].GetSectionName()), __func__, __LINE__,
        "a dot should be an ordinary character");
    CheckEquals(std::string("remote \"origin\""), std::string(lines[3].GetSectionName()), __func__, __LINE__,
        "git's subsection syntax should be a literal name");
    CheckEquals(std::string("*.md"), std::string(lines[4].GetSectionName()), __func__, __LINE__,
        "an .editorconfig glob should be a literal name");
}
//---------------------------------------------------------------------------


/////////////////////////////////////////////////////////////////////////////
// TTest_ASWIni_DocumentMessages
/////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------
TTest_ASWIni_DocumentMessages::TTest_ASWIni_DocumentMessages()
    : inherited("ASWIni_DocumentMessages_Tests")
{
}
//---------------------------------------------------------------------------
TTest_ASWIni_DocumentMessages::~TTest_ASWIni_DocumentMessages()
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_DocumentMessages::CheckParseFails(std::string_view text, TASWIniReadingOptions const& options,
    EIniCode code, std::string const& method, int lineNumber, std::string const& msg)
{
    TASWIniDocument document;
    std::vector<TASWIniMessage> messages;
    AssertTrue(document.Parse("[Old]\nk=1\nk=2\n", TASWIniReadingOptions(), messages).IsOk(), method, lineNumber,
        msg + ": the first text should parse");
    AssertEquals(1, static_cast<int>(messages.size()), method, lineNumber, msg + ": the first text's duplicate");

    TASWIniResult const result = document.Parse(text, options, messages);

    CheckEquals(code, result.Code, method, lineNumber, msg + ": the result's code");
    CheckFalse(result.Message.empty(), method, lineNumber, msg + ": the result should describe the error");
    AssertEquals(2, static_cast<int>(document.GetSections().size()), method, lineNumber,
        msg + ": the document should keep its sections");
    CheckEquals(std::string("Old"), std::string(document.GetSections()[1].GetName()), method, lineNumber,
        msg + ": the document should keep its section");
    CheckEquals(1, static_cast<int>(messages.size()), method, lineNumber,
        msg + ": no message should be added when parsing fails");
}
//---------------------------------------------------------------------------
void TTest_ASWIni_DocumentMessages::CheckWarning(TASWIniMessage const& message, EIniCode code, std::size_t line,
    std::string const& section, std::string const& method, int lineNumber, std::string const& msg)
{
    CheckEquals(ASWIni::ESeverity::Warning, message.Severity, method, lineNumber, msg + ": the severity");
    CheckEquals(code, message.Code, method, lineNumber, msg + ": the code");
    CheckEquals(static_cast<int>(line), static_cast<int>(message.Line), method, lineNumber, msg + ": the line");
    CheckEquals(section, message.Section, method, lineNumber, msg + ": the section");
    CheckFalse(message.Text.empty(), method, lineNumber, msg + ": the message should have a text");
    CheckTrue(message.FilePath.empty(), method, lineNumber, msg + ": a parsed text has no file path");
}
//---------------------------------------------------------------------------
void TTest_ASWIni_DocumentMessages::SetUp_Group()
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_DocumentMessages::SetUp_Test(ITestCase& /*testCase*/)
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_DocumentMessages::TearDown_Group()
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_DocumentMessages::TearDown_Test(ITestCase& /*testCase*/)
{
}
//---------------------------------------------------------------------------

// /////// Begin tests after this line ///////////////////////

//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_DocumentMessages, DuplicateKey_IsReportedWithTheFirstLine)
{
    // Arrange
    TASWIniResult result;
    std::vector<TASWIniMessage> messages;

    // Act
    TASWIniDocument const document = ParseText("[S]\nWidth=1\nheight=2\nWIDTH=3\n", TASWIniReadingOptions(), result,
        messages);

    // Assert
    AssertTrue(result.IsOk(), __func__, __LINE__, "a duplicate key shouldn't fail the parse: " + result.Message);
    AssertEquals(1, static_cast<int>(messages.size()), __func__, __LINE__, "one duplicate");
    CheckWarning(messages[0], EIniCode::DuplicateKey, 4, "S", __func__, __LINE__, "the duplicate key");
    CheckEquals(std::string("WIDTH"), messages[0].Key, __func__, __LINE__, "the key as the duplicate writes it");
    CheckContains(messages[0].Text, "line 2", __func__, __LINE__, "the text should name the first line");
    CheckContains(messages[0].Text, "first one's", __func__, __LINE__, "the text should say the first one wins");
    CheckEquals(4, static_cast<int>(document.GetSections()[1].GetLines().size()), __func__, __LINE__,
        "every line should be kept");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_DocumentMessages, DuplicateKey_MatchesCaseSensitiveAndNamesTheWinner)
{
    // Arrange
    TASWIniReadingOptions caseSensitive;
    caseSensitive.CaseSensitive = true;
    TASWIniReadingOptions lastWins;
    lastWins.DuplicateKeys = ASWIni::EDuplicateKeys::LastWins;
    TASWIniResult result;
    std::vector<TASWIniMessage> caseMessages;
    std::vector<TASWIniMessage> lastMessages;
    std::vector<TASWIniMessage> otherSectionMessages;

    // Act
    ParseText("k=1\nK=2\n", caseSensitive, result, caseMessages);
    ParseText("k=1\nk=2\n", lastWins, result, lastMessages);
    ParseText("[A]\nk=1\n[B]\nk=2\n", TASWIniReadingOptions(), result, otherSectionMessages);

    // Assert
    CheckEquals(0, static_cast<int>(caseMessages.size()), __func__, __LINE__,
        "keys that differ in case are different keys with CaseSensitive");
    AssertEquals(1, static_cast<int>(lastMessages.size()), __func__, __LINE__, "one duplicate with LastWins");
    CheckWarning(lastMessages[0], EIniCode::DuplicateKey, 2, "", __func__, __LINE__, "a duplicate in the preamble");
    CheckContains(lastMessages[0].Text, "last one's", __func__, __LINE__, "the text should say the last one wins");
    CheckEquals(0, static_cast<int>(otherSectionMessages.size()), __func__, __LINE__,
        "the same key in another section isn't a duplicate");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_DocumentMessages, DuplicateSection_IsReportedWithTheFirstLine)
{
    // Arrange
    TASWIniReadingOptions caseSensitive;
    caseSensitive.CaseSensitive = true;
    TASWIniResult result;
    std::vector<TASWIniMessage> messages;
    std::vector<TASWIniMessage> caseMessages;

    // Act
    ParseText("[General]\nk=1\n[general]\nk=2\n", TASWIniReadingOptions(), result, messages);
    ParseText("[General]\n[general]\n", caseSensitive, result, caseMessages);

    // Assert
    AssertEquals(1, static_cast<int>(messages.size()), __func__, __LINE__,
        "one duplicate section (and no duplicate key, which is in another section)");
    CheckWarning(messages[0], EIniCode::DuplicateSection, 3, "general", __func__, __LINE__, "the duplicate section");
    CheckContains(messages[0].Text, "line 1", __func__, __LINE__, "the text should name the first line");
    CheckEquals(0, static_cast<int>(caseMessages.size()), __func__, __LINE__,
        "sections that differ in case are different sections with CaseSensitive");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_DocumentMessages, EmptyKey_IsMalformed)
{
    // Arrange
    TASWIniResult result;
    std::vector<TASWIniMessage> messages;

    // Act
    TASWIniDocument const document = ParseText("[S]\n = value\n", TASWIniReadingOptions(), result, messages);

    // Assert
    AssertTrue(result.IsOk(), __func__, __LINE__, "an empty key shouldn't fail the parse: " + result.Message);
    AssertEquals(1, static_cast<int>(messages.size()), __func__, __LINE__, "one malformed line");
    CheckWarning(messages[0], EIniCode::MalformedLine, 2, "S", __func__, __LINE__, "an empty key");
    CheckEquals(ELineKind::Raw, document.GetSections()[1].GetLines()[1].GetKind(), __func__, __LINE__,
        "the line should be kept as a Raw line");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_DocumentMessages, EmptySectionName_IsMalformed)
{
    // Arrange
    TASWIniResult result;
    std::vector<TASWIniMessage> messages;

    // Act
    TASWIniDocument const document = ParseText("[]\n[  ]\nk=1\n", TASWIniReadingOptions(), result, messages);

    // Assert
    AssertTrue(result.IsOk(), __func__, __LINE__, "an empty section name shouldn't fail the parse: " + result.Message);
    AssertEquals(2, static_cast<int>(messages.size()), __func__, __LINE__, "two malformed lines");
    CheckWarning(messages[0], EIniCode::MalformedLine, 1, "", __func__, __LINE__, "\"[]\"");
    CheckWarning(messages[1], EIniCode::MalformedLine, 2, "", __func__, __LINE__, "\"[  ]\"");
    AssertEquals(1, static_cast<int>(document.GetSections().size()), __func__, __LINE__,
        "an empty name shouldn't start a section");
    CheckEquals(ELineKind::Raw, document.GetSections()[0].GetLines()[0].GetKind(), __func__, __LINE__,
        "\"[]\" should be kept as a Raw line");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_DocumentMessages, InvalidOptions_Fail)
{
    // Arrange
    TASWIniReadingOptions options;
    options.Separators.clear();

    // Act & Assert
    CheckParseFails("k=1\n", options, EIniCode::InvalidOptions, __func__, __LINE__, "empty Separators");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_DocumentMessages, InvalidUtf8_IsReportedOnce)
{
    // Arrange
    TASWIniResult result;
    std::vector<TASWIniMessage> messages;
    std::string const text = "ok=1\n[S]\na=\xFF\nb=\xC0\x80\n";

    // Act
    TASWIniDocument const document = ParseText(text, TASWIniReadingOptions(), result, messages);

    // Assert
    AssertTrue(result.IsOk(), __func__, __LINE__, "invalid UTF-8 shouldn't fail the parse: " + result.Message);
    AssertEquals(1, static_cast<int>(messages.size()), __func__, __LINE__, "one warning for the whole text");
    CheckWarning(messages[0], EIniCode::InvalidUtf8, 3, "S", __func__, __LINE__, "the first invalid line");
    std::vector<TASWIniDocumentLine> const lines = AllLines(document);
    AssertEquals(4, static_cast<int>(lines.size()), __func__, __LINE__, "the text should have four lines");
    CheckEquals(std::string("a=\xFF"), lines[2].GetText(), __func__, __LINE__, "the bytes should be kept");
    CheckEquals(std::string("\xC0\x80"), std::string(lines[3].GetValue()), __func__, __LINE__,
        "an overlong sequence should be kept as a value");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_DocumentMessages, NoSeparator_IsMalformed)
{
    // Arrange
    TASWIniResult result;
    std::vector<TASWIniMessage> messages;

    // Act
    TASWIniDocument const document = ParseText("[mysqld]\nskip-networking\n", TASWIniReadingOptions(), result,
        messages);

    // Assert
    AssertTrue(result.IsOk(), __func__, __LINE__, "a line without a separator shouldn't fail: " + result.Message);
    AssertEquals(1, static_cast<int>(messages.size()), __func__, __LINE__, "one malformed line");
    CheckWarning(messages[0], EIniCode::MalformedLine, 2, "mysqld", __func__, __LINE__, "a line without a separator");
    CheckContains(messages[0].Text, "kept as it is", __func__, __LINE__, "the text should say the line is kept");
    TASWIniDocumentLine const& line = document.GetSections()[1].GetLines()[1];
    CheckEquals(ELineKind::Raw, line.GetKind(), __func__, __LINE__, "the line should be kept as a Raw line");
    CheckEquals(std::string(), std::string(line.GetKey()), __func__, __LINE__, "a Raw line should have no key");
    CheckEquals(std::string("skip-networking"), line.GetText(), __func__, __LINE__, "the text should be kept");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_DocumentMessages, StrictParsing_FailsAtTheFirstMalformedLine)
{
    // Arrange
    TASWIniReadingOptions options;
    options.StrictParsing = true;
    TASWIniResult result;
    std::vector<TASWIniMessage> messages;

    // Act
    ParseText("[S]\nk=1\nbad\n[open\n", options, result, messages);

    // Assert
    CheckEquals(EIniCode::ParseFailed, result.Code, __func__, __LINE__, "a malformed line should fail strict parsing");
    CheckEquals(3, static_cast<int>(result.Line), __func__, __LINE__, "the result should name the first bad line");
    CheckContains(result.Message, "line 3", __func__, __LINE__, "the message should name the line");
    CheckParseFails("[S]\n[open\n", options, EIniCode::ParseFailed, __func__, __LINE__, "an unclosed section");
    CheckParseFails("[S] junk\n", options, EIniCode::ParseFailed, __func__, __LINE__, "text after a header");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_DocumentMessages, StrictParsing_KeepsOtherWarnings)
{
    // Arrange
    TASWIniReadingOptions options;
    options.StrictParsing = true;
    TASWIniResult result;
    std::vector<TASWIniMessage> messages;

    // Act
    ParseText("[S]\nk=1\nk=2\n[S]\nv=\xFF\n", options, result, messages);

    // Assert
    AssertTrue(result.IsOk(), __func__, __LINE__, "duplicates and invalid UTF-8 shouldn't fail strict parsing");
    AssertEquals(3, static_cast<int>(messages.size()), __func__, __LINE__, "three warnings");
    CheckEquals(EIniCode::DuplicateKey, messages[0].Code, __func__, __LINE__, "the duplicate key");
    CheckEquals(EIniCode::DuplicateSection, messages[1].Code, __func__, __LINE__, "the duplicate section");
    CheckEquals(EIniCode::InvalidUtf8, messages[2].Code, __func__, __LINE__, "the invalid UTF-8");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_DocumentMessages, TextAfterSectionHeader_IsMalformedButUsed)
{
    // Arrange
    TASWIniResult result;
    std::vector<TASWIniMessage> messages;

    // Act
    TASWIniDocument const document = ParseText("[General] junk\nk=1\n[a]b]\n", TASWIniReadingOptions(), result,
        messages);

    // Assert
    AssertTrue(result.IsOk(), __func__, __LINE__, "text after a header shouldn't fail the parse: " + result.Message);
    AssertEquals(2, static_cast<int>(messages.size()), __func__, __LINE__, "two malformed headers");
    CheckWarning(messages[0], EIniCode::MalformedLine, 1, "General", __func__, __LINE__, "\"[General] junk\"");
    CheckContains(messages[0].Text, "section is used", __func__, __LINE__, "the text should say the section is used");
    CheckWarning(messages[1], EIniCode::MalformedLine, 3, "a", __func__, __LINE__,
        "\"[a]b]\": the first ']' closes the name");
    AssertEquals(3, static_cast<int>(document.GetSections().size()), __func__, __LINE__,
        "both headers should start a section");
    CheckEquals(std::string("General"), std::string(document.GetSections()[1].GetName()), __func__, __LINE__,
        "the section's name");
    CheckEquals(2, static_cast<int>(document.GetSections()[1].GetLines().size()), __func__, __LINE__,
        "the key should be in [General]");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_DocumentMessages, UnclosedSection_IsRawAndKeysStayInThePreviousSection)
{
    // Arrange
    TASWIniResult result;
    std::vector<TASWIniMessage> messages;

    // Act
    TASWIniDocument const document = ParseText("[A]\n[General\nk=1\n[a=b\n", TASWIniReadingOptions(), result,
        messages);

    // Assert
    AssertTrue(result.IsOk(), __func__, __LINE__, "an unclosed section shouldn't fail the parse: " + result.Message);
    AssertEquals(2, static_cast<int>(messages.size()), __func__, __LINE__, "two unclosed sections");
    CheckWarning(messages[0], EIniCode::UnclosedSection, 2, "A", __func__, __LINE__, "\"[General\"");
    CheckWarning(messages[1], EIniCode::UnclosedSection, 4, "A", __func__, __LINE__,
        "\"[a=b\" is an unclosed section, not a key");
    AssertEquals(2, static_cast<int>(document.GetSections().size()), __func__, __LINE__,
        "an unclosed section shouldn't start a section");
    std::vector<TASWIniDocumentLine> const& lines = document.GetSections()[1].GetLines();
    AssertEquals(4, static_cast<int>(lines.size()), __func__, __LINE__, "every line should stay in [A]");
    CheckEquals(ELineKind::Raw, lines[1].GetKind(), __func__, __LINE__, "\"[General\" should be a Raw line");
    CheckEquals(ELineKind::KeyValue, lines[2].GetKind(), __func__, __LINE__, "k should be a key of [A]");
    CheckEquals(ELineKind::Raw, lines[3].GetKind(), __func__, __LINE__, "\"[a=b\" should be a Raw line");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_DocumentMessages, UnsupportedEncoding_Fails)
{
    // Arrange
    TASWIniReadingOptions const options;

    // Act & Assert
    CheckParseFails(std::string("\xFF\xFE[\x00", 4), options, EIniCode::UnsupportedEncoding, __func__, __LINE__,
        "UTF-16 LE");
    CheckParseFails(std::string("\xFE\xFF\x00[", 4), options, EIniCode::UnsupportedEncoding, __func__, __LINE__,
        "UTF-16 BE");
    CheckParseFails(std::string("\xFF\xFE\x00\x00", 4), options, EIniCode::UnsupportedEncoding, __func__, __LINE__,
        "UTF-32 LE");
    CheckParseFails(std::string("\x00\x00\xFE\xFF", 4), options, EIniCode::UnsupportedEncoding, __func__, __LINE__,
        "UTF-32 BE");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_DocumentMessages, ValidText_HasNoMessages)
{
    // Arrange
    TASWIniResult result;
    std::vector<TASWIniMessage> messages;
    std::string const text = "; comment\r\n\r\ntop = 1\r\n[General]\r\nWidth = 640\r\nName: \xC3\xA9t\xC3\xA9\r\n"
        "  # indented\r\n[Other]\r\n";

    // Act
    ParseText(text, TASWIniReadingOptions(), result, messages);

    // Assert
    CheckTrue(result.IsOk(), __func__, __LINE__, "the text should parse: " + result.Message);
    CheckEquals(0, static_cast<int>(messages.size()), __func__, __LINE__,
        "a valid text should have no messages" + (messages.empty() ? std::string() : ": " + messages[0].Text));
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_DocumentMessages, Warnings_AreAppended)
{
    // Arrange
    TASWIniDocument document;
    std::vector<TASWIniMessage> messages(1);
    messages[0].Text = "earlier";

    // Act
    TASWIniResult const result = document.Parse("bad\n", TASWIniReadingOptions(), messages);

    // Assert
    AssertTrue(result.IsOk(), __func__, __LINE__, "the text should parse: " + result.Message);
    AssertEquals(2, static_cast<int>(messages.size()), __func__, __LINE__, "the earlier message and the new one");
    CheckEquals(std::string("earlier"), messages[0].Text, __func__, __LINE__, "the earlier message should be kept");
    CheckWarning(messages[1], EIniCode::MalformedLine, 1, "", __func__, __LINE__, "the new warning");
}
//---------------------------------------------------------------------------

} // namespace ASWUnitTests

//---------------------------------------------------------------------------
ASW_REGISTER_TEST_GROUP_IN_SUITE(ASWUnitTests::TTest_ASWIni_DocumentMessages, "Document")
ASW_REGISTER_TEST_GROUP_IN_SUITE(ASWUnitTests::TTest_ASWIni_DocumentModel, "Document")
ASW_REGISTER_TEST_GROUP_IN_SUITE(ASWUnitTests::TTest_ASWIni_DocumentParse, "Document")
