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
#include <cstdint>
#include <initializer_list>
#include <random>
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
using ASWIni::TASWIniOptions;
using ASWIni::TASWIniResult;
using ASWIni::TASWIniWritingOptions;

// The fuzz tests' texts: how many, the seed of the first (each next one's is one more), and their largest size
constexpr std::size_t FuzzTextCount = 2000;
constexpr std::uint32_t FuzzFirstSeed = 20261017;
constexpr std::size_t FuzzMaxTextSize = 300;

/////////////////////////////////////////////////////////////////////////////
// TCorpusText
//
// A text of the round-trip corpus: what it shows, the text, how many lines it has, and the codes of the warnings it
// gets with the default reading options, in order
/////////////////////////////////////////////////////////////////////////////
struct TCorpusText
{
    std::string Name;
    std::string Text;
    std::size_t LineCount;
    std::vector<EIniCode> Warnings;
};


/////////////////////////////////////////////////////////////////////////////
// TFuzzToken
//
// A piece the fuzz tests' texts are made of, and how often it is picked compared with the others
/////////////////////////////////////////////////////////////////////////////
struct TFuzzToken
{
    std::string Text;
    std::uint32_t Weight;
};

//---------------------------------------------------------------------------

std::vector<TASWIniDocumentLine> AllLines(TASWIniDocument const& document);
std::string CodeNames(std::vector<EIniCode> const& codes);
std::vector<TCorpusText> Corpus();
std::string JoinedNames(std::vector<std::string> const& names);
std::vector<EIniCode> MessageCodes(std::vector<TASWIniMessage> const& messages);
TASWIniDocument ParseText(std::string_view text, TASWIniOptions const& options, TASWIniResult& result,
    std::vector<TASWIniMessage>& messages);
std::string RandomText(std::uint32_t seed);
std::vector<TASWIniOptions> ReadingOptionSets();

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
    CodeNames

    The names of 'codes', separated by commas, e.g. "MalformedLine, DuplicateKey"; "(none)" when there are none.
*/
std::string CodeNames(std::vector<EIniCode> const& codes)
{
    if (codes.empty())
        return "(none)";

    std::string names;
    for (EIniCode const code : codes)
    {
        if (!names.empty())
            names += ", ";
        names += ASWIni::ToString(code);
    }

    return names;
}
//---------------------------------------------------------------------------
/*
    Corpus

    The round-trip corpus: texts with what a careless parser or writer would change or lose (byte order marks, line
    breaks, blanks, comments, malformed lines, duplicates, odd bytes), each of which parses and must be written back
    byte for byte.
*/
std::vector<TCorpusText> Corpus()
{
    std::string const bom = "\xEF\xBB\xBF";

    return {
        { "an empty text", "", 0, {} },
        { "only a byte order mark", bom, 0, {} },
        { "a byte order mark and content", bom + "[General]\r\nWidth = 640\r\n", 2, {} },
        { "LF line breaks", "[A]\nk=1\n", 2, {} },
        { "CRLF line breaks", "[A]\r\nk=1\r\n", 2, {} },
        { "CR line breaks", "[A]\rk=1\r", 2, {} },
        { "mixed line breaks", "[A]\nk=1\r\nl=2\rm=3\n\r\n", 5, {} },
        { "no final line break", "[A]\nk=1", 2, {} },
        { "blank and whitespace-only lines", "\n  \n\t\n \t \r\n[A]\n\n", 6, {} },
        { "tabs", "\t[A]\t\n\tk\t=\tv\t\n", 2, {} },
        { "keys before any section", "top=1\nnext = 2\n[A]\nk=1\n", 4, {} },
        { "keys with spaces", "[A]\nMy Key = My Value\n  Other Key=x\n", 3, {} },
        { "'=' in a value", "[A]\nformula = a=b+c\n", 2, {} },
        { "the first separator wins", "url = http://example.com:8080/x\na:b=c\n", 2, {} },
        { "comments", "; semicolon\n# hash\n  ; indented\n\t# tabbed\n;\n#\n", 6, {} },
        { "inline comment characters", "k = v ; note\nl = a#b\n[S] ; header note\n", 3, {} },
        { "quoted values", "a = \"  spaced  \"\nb = \"\"\nc = \"open\nd = \"x\" ; note\n", 4, {} },
        { "an unclosed section", "[A]\n[General\nk=1\n", 3, { EIniCode::UnclosedSection } },
        { "a header comment", "[General] ; note\n[Other];x\n", 2, {} },
        { "text after a header", "[General] junk\nk=1\n", 2, { EIniCode::MalformedLine } },
        { "empty section names", "[]\n[ ]\n", 2, { EIniCode::MalformedLine, EIniCode::MalformedLine } },
        { "an empty key", "= value\n", 1, { EIniCode::MalformedLine } },
        { "a line without a separator", "[mysqld]\nskip-networking\n", 2, { EIniCode::MalformedLine } },
        { "duplicate keys", "[A]\nk=1\nK=2\n", 3, { EIniCode::DuplicateKey } },
        { "duplicate sections", "[A]\n[a]\n", 2, { EIniCode::DuplicateSection } },
        { "invalid UTF-8", "k=\xFF\nl=\xC0\x80\nm=\xED\xA0\x80\n", 3, { EIniCode::InvalidUtf8 } },
        { "valid UTF-8", "name = \xC3\xA9t\xC3\xA9 \xE2\x82\xAC \xF0\x9F\x98\x80\n", 1, {} },
        { "NUL bytes", std::string("k = a\0b\n\0\n", 10), 2, { EIniCode::MalformedLine } },
        { "a 100 KiB line", "long = " + std::string(100 * 1024, 'x') + "\n", 1, {} },
        { "a section name with blanks around it", "[ General ]\n", 1, {} },
        { "git's subsection syntax", "[remote \"origin\"]\n\turl = https://x\n", 2, {} },
        { "an .editorconfig glob", "[*.md]\nindent_size = 2\n", 2, {} },
    };
}
//---------------------------------------------------------------------------
/*
    JoinedNames

    'names' separated by '|', e.g. "General|Window.Main"; "" when there are none.
*/
std::string JoinedNames(std::vector<std::string> const& names)
{
    std::string joined;
    for (std::size_t i = 0; i < names.size(); ++i)
    {
        if (i > 0)
            joined += "|";
        joined += names[i];
    }

    return joined;
}
//---------------------------------------------------------------------------
/*
    MessageCodes

    The codes of 'messages', in order.
*/
std::vector<EIniCode> MessageCodes(std::vector<TASWIniMessage> const& messages)
{
    std::vector<EIniCode> codes;
    for (TASWIniMessage const& message : messages)
        codes.push_back(message.Code);

    return codes;
}
//---------------------------------------------------------------------------
/*
    ParseText

    'text' parsed with 'options' into a new document, with Parse()'s result in 'result' and its messages in 'messages'.
*/
TASWIniDocument ParseText(std::string_view text, TASWIniOptions const& options, TASWIniResult& result,
    std::vector<TASWIniMessage>& messages)
{
    TASWIniDocument document;
    result = document.Parse(text, options, messages);

    return document;
}
//---------------------------------------------------------------------------
/*
    RandomText

    A text of 0 to FuzzMaxTextSize bytes made of weighted INI pieces (brackets, separators, comment prefixes, blanks,
    line breaks, quotes, letters, digits, a section header that repeats, a UTF-8 byte order mark, an invalid byte and
    NUL), the same for the same
    'seed' everywhere: std::mt19937's numbers are defined by the standard, and they are used directly, since the
    standard distributions may give different results on different libraries. There is no 0xFE byte, so no text
    starts with a UTF-16 or UTF-32 byte order mark. The last piece may be cut short.
*/
std::string RandomText(std::uint32_t seed)
{
    std::vector<TFuzzToken> const tokens = {
        { "[", 4 }, { "]", 4 }, { "=", 4 }, { ":", 3 }, { ";", 3 }, { "#", 3 }, { "//", 1 }, { " ", 5 },
        { "\t", 2 }, { "\r", 3 }, { "\n", 5 }, { "\r\n", 3 }, { "\"", 2 }, { "a", 4 }, { "K", 3 }, { "z", 2 },
        { "0", 2 }, { "7", 1 }, { ".", 1 }, { "[a]", 2 }, { "\xEF\xBB\xBF", 1 }, { "\xFF", 1 },
        { std::string(1, '\0'), 1 },
    };

    std::uint32_t totalWeight = 0;
    for (TFuzzToken const& token : tokens)
        totalWeight += token.Weight;

    std::mt19937 generator(seed);
    std::size_t const size = static_cast<std::size_t>(generator() % (FuzzMaxTextSize + 1));

    std::string text;
    while (text.size() < size)
    {
        std::uint32_t pick = static_cast<std::uint32_t>(generator() % totalWeight);
        for (TFuzzToken const& token : tokens)
        {
            if (pick < token.Weight)
            {
                text += token.Text;
                break;
            }

            pick -= token.Weight;
        }
    }

    text.resize(size);

    return text;
}
//---------------------------------------------------------------------------
/*
    ReadingOptionSets

    Options whose reading options parse the same text into different parts (the defaults first), none of them
    strict: the text written back must not depend on how it was read.
*/
std::vector<TASWIniOptions> ReadingOptionSets()
{
    std::vector<TASWIniOptions> sets(4);

    sets[1].Reading.InlineComments = true;
    sets[1].Reading.QuotedValues = true;

    sets[2].Reading.CaseSensitive = true;
    sets[2].Reading.DuplicateKeys = ASWIni::EDuplicateKeys::LastWins;

    sets[3].Reading.Separators = ":";
    sets[3].Reading.CommentPrefixes = { ";", "#", "//" };
    sets[3].Writing.Separator = ": ";

    return sets;
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
    TASWIniOptions options;
    options.Reading.CaseSensitive = true;
    std::vector<TASWIniMessage> messages;
    TASWIniDocument original;
    AssertTrue(original.Parse("[General]\nWidth = 640\n", options, messages).IsOk(), __func__, __LINE__,
        "the original should parse");

    // Act
    TASWIniDocument const copy = original;
    AssertTrue(original.Parse("[Other]\n", TASWIniOptions(), messages).IsOk(), __func__, __LINE__,
        "the original should parse again");

    // Assert
    AssertEquals(2, static_cast<int>(copy.GetSections().size()), __func__, __LINE__,
        "the copy should keep its preamble and section");
    CheckEquals(std::string("General"), std::string(copy.GetSections()[1].GetName()), __func__, __LINE__,
        "the copy should keep its section's name");
    CheckEquals(std::string("640"), std::string(copy.GetSections()[1].GetLines()[1].GetValue()), __func__, __LINE__,
        "the copy should keep its value");
    CheckTrue(copy.GetOptions().Reading.CaseSensitive, __func__, __LINE__, "the copy should keep its options");
    CheckEquals(std::string("Other"), std::string(original.GetSections()[1].GetName()), __func__, __LINE__,
        "the original should hold what it parsed last");
    CheckFalse(original.GetOptions().Reading.CaseSensitive, __func__, __LINE__,
        "the original should have the options it parsed with last");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_DocumentModel, Document_DefaultIsEmpty)
{
    // Arrange
    TASWIniOptions const defaults;

    // Act
    TASWIniDocument const document;

    // Assert
    AssertEquals(1, static_cast<int>(document.GetSections().size()), __func__, __LINE__,
        "a new document should have one section, its preamble");
    CheckFalse(document.GetSections().front().HasHeader(), __func__, __LINE__, "the preamble should have no header");
    CheckEquals(0, static_cast<int>(document.GetLineCount()), __func__, __LINE__,
        "a new document should have no lines");
    CheckFalse(document.HasBom(), __func__, __LINE__, "a new document should have no byte order mark");
    CheckFalse(document.IsModified(), __func__, __LINE__, "a new document shouldn't be modified");
#if defined(_WIN32)
    CheckEquals(ELineBreak::CRLF, document.GetLineBreak(), __func__, __LINE__,
        "new lines should get the platform's line break, CRLF on Windows");
#else
    CheckEquals(ELineBreak::LF, document.GetLineBreak(), __func__, __LINE__,
        "new lines should get the platform's line break, LF on other platforms");
#endif
    CheckEquals(defaults.Reading.Separators, document.GetOptions().Reading.Separators, __func__, __LINE__,
        "a new document should have the default reading options");
    CheckEquals(defaults.Writing.Separator, document.GetOptions().Writing.Separator, __func__, __LINE__,
        "a new document should have the default writing options");
    CheckTrue(document.GetOptions().Validate().IsOk(), __func__, __LINE__, "a new document's options should be valid");
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
ASW_TEST(TTest_ASWIni_DocumentModel, Modified_ParseAndResetLeaveItClear)
{
    // Arrange
    TASWIniDocument document;
    std::vector<TASWIniMessage> messages;

    // Act & Assert
    AssertTrue(document.Parse("[A]\nk=1\n", TASWIniOptions(), messages).IsOk(), __func__, __LINE__,
        "the text should parse");
    CheckFalse(document.IsModified(), __func__, __LINE__, "a parsed document shouldn't be modified");
    AssertTrue(document.Reset(TASWIniOptions()).IsOk(), __func__, __LINE__, "the document should reset");
    CheckFalse(document.IsModified(), __func__, __LINE__, "a reset document shouldn't be modified");
    document.ClearModified();
    CheckFalse(document.IsModified(), __func__, __LINE__, "ClearModified() should leave the flag clear");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_DocumentModel, Reset_EmptiesTheDocumentWithNewOptions)
{
    // Arrange
    TASWIniOptions options;
    options.Reading.CaseSensitive = true;
    options.Writing.Separator = "=";
    TASWIniDocument document;
    std::vector<TASWIniMessage> messages;
    AssertTrue(document.Parse("\xEF\xBB\xBF[A]\rk=1\r", TASWIniOptions(), messages).IsOk(), __func__, __LINE__,
        "the text should parse");
    AssertTrue(document.GetLineBreak() == ELineBreak::CR, __func__, __LINE__, "the text's line break should be CR");

    // Act
    TASWIniResult const result = document.Reset(options);

    // Assert
    AssertTrue(result.IsOk(), __func__, __LINE__, "the document should reset: " + result.Message);
    AssertEquals(1, static_cast<int>(document.GetSections().size()), __func__, __LINE__,
        "a reset document should have only its preamble");
    CheckFalse(document.GetSections().front().HasHeader(), __func__, __LINE__, "the preamble should have no header");
    CheckEquals(0, static_cast<int>(document.GetLineCount()), __func__, __LINE__,
        "a reset document should have no lines");
    CheckFalse(document.HasBom(), __func__, __LINE__, "a reset document should have no byte order mark");
    CheckEquals(TASWIniDocument().GetLineBreak(), document.GetLineBreak(), __func__, __LINE__,
        "a reset document should get the platform's line break");
    CheckTrue(document.GetOptions().Reading.CaseSensitive, __func__, __LINE__,
        "a reset document should have the new reading options");
    CheckEquals(std::string("="), document.GetOptions().Writing.Separator, __func__, __LINE__,
        "a reset document should have the new writing options");
    CheckEquals(std::string(), document.ToText(), __func__, __LINE__, "a reset document should be an empty text");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_DocumentModel, Reset_InvalidOptionsChangeNothing)
{
    // Arrange
    // Each group is valid on its own, but a "=" separator wouldn't read back with only ':' read
    TASWIniOptions invalid;
    invalid.Reading.Separators = ":";
    invalid.Writing.Separator = "=";
    TASWIniOptions options;
    options.Reading.CaseSensitive = true;
    TASWIniDocument document;
    std::vector<TASWIniMessage> messages;
    AssertTrue(document.Parse("[A]\nk=1\n", options, messages).IsOk(), __func__, __LINE__, "the text should parse");

    // Act
    TASWIniResult const result = document.Reset(invalid);

    // Assert
    CheckEquals(EIniCode::InvalidOptions, result.Code, __func__, __LINE__, "the options should be rejected");
    CheckFalse(result.Message.empty(), __func__, __LINE__, "the result should describe the problem");
    CheckEquals(2, static_cast<int>(document.GetLineCount()), __func__, __LINE__, "the document should keep its lines");
    CheckTrue(document.GetOptions().Reading.CaseSensitive, __func__, __LINE__,
        "the document should keep its options");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_DocumentModel, Reset_WithItsOwnOptionsOnlyEmptiesIt)
{
    // Arrange
    TASWIniOptions options;
    options.Reading.CommentPrefixes = { "//" };
    options.Writing.CommentPrefix = "// ";
    TASWIniDocument document;
    std::vector<TASWIniMessage> messages;
    AssertTrue(document.Parse("[A]\nk=1\n", options, messages).IsOk(), __func__, __LINE__, "the text should parse");

    // Act
    TASWIniResult const result = document.Reset(document.GetOptions());

    // Assert
    AssertTrue(result.IsOk(), __func__, __LINE__, "the document should reset: " + result.Message);
    CheckEquals(0, static_cast<int>(document.GetLineCount()), __func__, __LINE__, "the document should be empty");
    AssertEquals(1, static_cast<int>(document.GetOptions().Reading.CommentPrefixes.size()), __func__, __LINE__,
        "the document should keep its comment prefixes");
    CheckEquals(std::string("//"), document.GetOptions().Reading.CommentPrefixes[0], __func__, __LINE__,
        "the document should keep its comment prefix");
    CheckEquals(std::string("// "), document.GetOptions().Writing.CommentPrefix, __func__, __LINE__,
        "the document should keep its written comment prefix");
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
TASWIniDocument TTest_ASWIni_DocumentParse::ParseValid(std::string_view text, TASWIniOptions const& options,
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
    TASWIniOptions const options;

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
    TASWIniOptions options;
    options.Reading.CommentPrefixes = { ";", ";;", "//" };

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
    TASWIniOptions const options;

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
    TASWIniDocument const document = ParseValid("", TASWIniOptions(), __func__, __LINE__);

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
    std::vector<TASWIniDocumentLine> const lines = AllLines(ParseValid("k = v ; note\n", TASWIniOptions(),
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
    TASWIniOptions options;
    options.Reading.InlineComments = true;

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
        "url = http://x\na:b=c\nk = v=w\ncolon: value\n", TASWIniOptions(), __func__, __LINE__));

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
    std::vector<TASWIniDocumentLine> const nulLines = AllLines(ParseValid(nul, TASWIniOptions(), __func__,
        __LINE__));
    std::vector<TASWIniDocumentLine> const longLines = AllLines(ParseValid("long = " + longValue + "\n",
        TASWIniOptions(), __func__, __LINE__));

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
        "\tMy Key \t=\t value with spaces \t\nempty =\nk=v\n", TASWIniOptions(), __func__, __LINE__));

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
    TASWIniOptions const options;
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
    std::vector<TASWIniDocumentLine> const lines = AllLines(ParseValid("a=1\nb=2\r\nc=3\rd=4", TASWIniOptions(),
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
ASW_TEST(TTest_ASWIni_DocumentParse, Parse_KeepsTheOptions)
{
    // Arrange
    TASWIniOptions options;
    options.Reading.InlineComments = true;
    options.Writing.LineEnding = ASWIni::ELineEnding::LF;

    // Act
    TASWIniDocument const document = ParseValid("a = 1 ; note\r\nb=2\r\n", options, __func__, __LINE__);

    // Assert
    CheckTrue(document.GetOptions().Reading.InlineComments, __func__, __LINE__,
        "the document should keep the reading options");
    CheckEquals(ASWIni::ELineEnding::LF, document.GetOptions().Writing.LineEnding, __func__, __LINE__,
        "the document should keep the writing options");
    CheckEquals(std::string("note"), std::string(AllLines(document)[0].GetComment()), __func__, __LINE__,
        "the text should be read with the reading options");
    CheckEquals(std::string("a = 1 ; note\nb=2\n"), document.ToText(), __func__, __LINE__,
        "the text should be written with the writing options");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_DocumentParse, Parse_ReplacesPreviousContents)
{
    // Arrange
    TASWIniOptions const options;
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
    std::vector<TASWIniDocumentLine> const lines = AllLines(ParseValid("k = \" spaced \"\n", TASWIniOptions(),
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
    TASWIniOptions options;
    options.Reading.QuotedValues = true;
    TASWIniOptions withComments = options;
    withComments.Reading.InlineComments = true;
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
    TASWIniDocument const document = ParseText("[A]\nk=1\nk=2\n[A]\nk=3\n", TASWIniOptions(), result,
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
    TASWIniDocument const document = ParseValid("; top\na=1\n\n[S]\nb=2\n; c\n[T]\n", TASWIniOptions(),
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
        TASWIniOptions(), __func__, __LINE__));

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
        "[ General ]\n  [Indented]\n[a.b]\n[remote \"origin\"]\n[*.md]\n", TASWIniOptions(), __func__,
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
void TTest_ASWIni_DocumentMessages::CheckParseFails(std::string_view text, TASWIniOptions const& options,
    EIniCode code, std::string const& method, int lineNumber, std::string const& msg)
{
    TASWIniDocument document;
    std::vector<TASWIniMessage> messages;
    AssertTrue(document.Parse("[Old]\nk=1\nk=2\n", TASWIniOptions(), messages).IsOk(), method, lineNumber,
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
    TASWIniDocument const document = ParseText("[S]\nWidth=1\nheight=2\nWIDTH=3\n", TASWIniOptions(), result,
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
    TASWIniOptions caseSensitive;
    caseSensitive.Reading.CaseSensitive = true;
    TASWIniOptions lastWins;
    lastWins.Reading.DuplicateKeys = ASWIni::EDuplicateKeys::LastWins;
    TASWIniResult result;
    std::vector<TASWIniMessage> caseMessages;
    std::vector<TASWIniMessage> lastMessages;
    std::vector<TASWIniMessage> otherSectionMessages;

    // Act
    ParseText("k=1\nK=2\n", caseSensitive, result, caseMessages);
    ParseText("k=1\nk=2\n", lastWins, result, lastMessages);
    ParseText("[A]\nk=1\n[B]\nk=2\n", TASWIniOptions(), result, otherSectionMessages);

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
    TASWIniOptions caseSensitive;
    caseSensitive.Reading.CaseSensitive = true;
    TASWIniResult result;
    std::vector<TASWIniMessage> messages;
    std::vector<TASWIniMessage> caseMessages;

    // Act
    ParseText("[General]\nk=1\n[general]\nk=2\n", TASWIniOptions(), result, messages);
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
    TASWIniDocument const document = ParseText("[S]\n = value\n", TASWIniOptions(), result, messages);

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
    TASWIniDocument const document = ParseText("[]\n[  ]\nk=1\n", TASWIniOptions(), result, messages);

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
    TASWIniOptions reading;
    reading.Reading.Separators.clear();
    TASWIniOptions writing;
    writing.Writing.LineEnding = static_cast<ASWIni::ELineEnding>(99);
    TASWIniOptions saving;
    saving.Saving.Backup = true;
    saving.Saving.BackupSuffix.clear();
    TASWIniOptions crossGroup;
    crossGroup.Reading.Separators = ":";
    crossGroup.Writing.Separator = "=";

    // Act & Assert
    CheckParseFails("k=1\n", reading, EIniCode::InvalidOptions, __func__, __LINE__, "empty Reading.Separators");
    CheckParseFails("k=1\n", writing, EIniCode::InvalidOptions, __func__, __LINE__, "an unknown LineEnding");
    CheckParseFails("k=1\n", saving, EIniCode::InvalidOptions, __func__, __LINE__, "an empty BackupSuffix");
    CheckParseFails("k:1\n", crossGroup, EIniCode::InvalidOptions, __func__, __LINE__,
        "a Writing.Separator that isn't one of the Reading.Separators");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_DocumentMessages, InvalidUtf8_IsReportedOnce)
{
    // Arrange
    TASWIniResult result;
    std::vector<TASWIniMessage> messages;
    std::string const text = "ok=1\n[S]\na=\xFF\nb=\xC0\x80\n";

    // Act
    TASWIniDocument const document = ParseText(text, TASWIniOptions(), result, messages);

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
    TASWIniDocument const document = ParseText("[mysqld]\nskip-networking\n", TASWIniOptions(), result,
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
    TASWIniOptions options;
    options.Reading.StrictParsing = true;
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
    TASWIniOptions options;
    options.Reading.StrictParsing = true;
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
    TASWIniDocument const document = ParseText("[General] junk\nk=1\n[a]b]\n", TASWIniOptions(), result,
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
    TASWIniDocument const document = ParseText("[A]\n[General\nk=1\n[a=b\n", TASWIniOptions(), result,
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
    TASWIniOptions const options;

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
    ParseText(text, TASWIniOptions(), result, messages);

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
    TASWIniResult const result = document.Parse("bad\n", TASWIniOptions(), messages);

    // Assert
    AssertTrue(result.IsOk(), __func__, __LINE__, "the text should parse: " + result.Message);
    AssertEquals(2, static_cast<int>(messages.size()), __func__, __LINE__, "the earlier message and the new one");
    CheckEquals(std::string("earlier"), messages[0].Text, __func__, __LINE__, "the earlier message should be kept");
    CheckWarning(messages[1], EIniCode::MalformedLine, 1, "", __func__, __LINE__, "the new warning");
}
//---------------------------------------------------------------------------


/////////////////////////////////////////////////////////////////////////////
// TTest_ASWIni_DocumentToText
/////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------
TTest_ASWIni_DocumentToText::TTest_ASWIni_DocumentToText()
    : inherited("ASWIni_DocumentToText_Tests")
{
}
//---------------------------------------------------------------------------
TTest_ASWIni_DocumentToText::~TTest_ASWIni_DocumentToText()
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_DocumentToText::SetUp_Group()
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_DocumentToText::SetUp_Test(ITestCase& /*testCase*/)
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_DocumentToText::TearDown_Group()
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_DocumentToText::TearDown_Test(ITestCase& /*testCase*/)
{
}
//---------------------------------------------------------------------------
std::string TTest_ASWIni_DocumentToText::WriteBack(std::string_view text, TASWIniWritingOptions const& writing,
    std::string const& method, int lineNumber)
{
    TASWIniOptions options;
    options.Writing = writing;
    TASWIniResult result;
    std::vector<TASWIniMessage> messages;
    TASWIniDocument const document = ParseText(text, options, result, messages);

    AssertTrue(result.IsOk(), method, lineNumber, "the text should parse: " + result.Message);

    return document.ToText();
}
//---------------------------------------------------------------------------

// /////// Begin tests after this line ///////////////////////

//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_DocumentToText, Bom_FollowsTheMode)
{
    // Arrange
    std::string const bom = "\xEF\xBB\xBF";
    std::string const text = "[S]\nk=v\n";
    TASWIniWritingOptions always;
    always.Bom = ASWIni::EBomMode::Always;
    TASWIniWritingOptions never;
    never.Bom = ASWIni::EBomMode::Never;

    // Act & Assert
    CheckEquals(bom + text, WriteBack(bom + text, TASWIniWritingOptions(), __func__, __LINE__), __func__, __LINE__,
        "Auto should keep the mark of a text that had one");
    CheckEquals(text, WriteBack(text, TASWIniWritingOptions(), __func__, __LINE__), __func__, __LINE__,
        "Auto should write no mark for a text that had none");
    CheckEquals(bom + text, WriteBack(text, always, __func__, __LINE__), __func__, __LINE__,
        "Always should add the mark");
    CheckEquals(bom + text, WriteBack(bom + text, always, __func__, __LINE__), __func__, __LINE__,
        "Always should write the mark once");
    CheckEquals(text, WriteBack(bom + text, never, __func__, __LINE__), __func__, __LINE__,
        "Never should drop the mark");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_DocumentToText, DefaultOptions_WriteTheParsedTextBack)
{
    // Arrange
    // Texts with something that a careless writer would change: breaks, blanks, malformed lines, odd bytes
    std::vector<std::string> const texts = {
        "; comment\r\n\r\n[General]\r\nWidth = 640\r\n",
        "a=1\nb=2\r\nc=3\rd=4",
        "\xEF\xBB\xBF[S]\n\tk \t=\t v \t\n  \t\n",
        "[General] junk\nskip-networking\n[open\n = v\n[]\n",
        "k=1\nk=2\n[S]\n[s]\n",
        "a=\xFF\xFE\nb=\xC0\x80\n",
        std::string("k = a\0b\n\0\n", 10),
        "\n\n\r\n\r\r",
        "url = http://x ; not a comment\n[General] ; note\n",
    };

    // Act & Assert
    for (std::size_t i = 0; i < texts.size(); ++i)
    {
        CheckEquals(texts[i], WriteBack(texts[i], TASWIniWritingOptions(), __func__, __LINE__), __func__, __LINE__,
            "text " + std::to_string(i) + " should be written back byte for byte");
    }
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_DocumentToText, EmptyDocument_IsEmptyText)
{
    // Arrange
    TASWIniOptions always;
    always.Writing.Bom = ASWIni::EBomMode::Always;
    TASWIniDocument alwaysDocument;
    AssertTrue(alwaysDocument.Reset(always).IsOk(), __func__, __LINE__, "the document should reset");

    // Act & Assert
    CheckEquals(std::string(), TASWIniDocument().ToText(), __func__, __LINE__,
        "a new document should be an empty text");
    CheckEquals(std::string("\xEF\xBB\xBF"), alwaysDocument.ToText(), __func__, __LINE__,
        "an empty document with Always should be only the mark");
    CheckEquals(std::string("\xEF\xBB\xBF"), WriteBack("\xEF\xBB\xBF", TASWIniWritingOptions(), __func__,
        __LINE__), __func__, __LINE__, "a text of only the mark should be written back as it was");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_DocumentToText, LineEnding_AutoKeepsEachLinesBreak)
{
    // Arrange
    // Mostly CRLF, so a writer that used the most common break for every line would change the others
    std::string const text = "a=1\r\nb=2\r\nc=3\nd=4\re=5\r\n";
    TASWIniWritingOptions options;
    options.LineEnding = ASWIni::ELineEnding::Auto;

    // Act
    std::string const written = WriteBack(text, options, __func__, __LINE__);

    // Assert
    CheckEquals(text, written, __func__, __LINE__, "each line should keep its own line break");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_DocumentToText, LineEnding_ConvertsEveryLine)
{
    // Arrange
    std::string const text = "a=1\nb=2\r\nc=3\rd=4\n";
    TASWIniWritingOptions lf;
    lf.LineEnding = ASWIni::ELineEnding::LF;
    TASWIniWritingOptions crlf;
    crlf.LineEnding = ASWIni::ELineEnding::CRLF;
    TASWIniWritingOptions native;
    native.LineEnding = ASWIni::ELineEnding::Native;
#if defined(_WIN32)
    std::string const nativeText = "a=1\r\nb=2\r\nc=3\r\nd=4\r\n";
#else
    std::string const nativeText = "a=1\nb=2\nc=3\nd=4\n";
#endif

    // Act & Assert
    CheckEquals(std::string("a=1\nb=2\nc=3\nd=4\n"), WriteBack(text, lf, __func__, __LINE__), __func__, __LINE__,
        "LF should end every line with LF");
    CheckEquals(std::string("a=1\r\nb=2\r\nc=3\r\nd=4\r\n"), WriteBack(text, crlf, __func__, __LINE__), __func__,
        __LINE__, "CRLF should end every line with CRLF");
    CheckEquals(nativeText, WriteBack(text, native, __func__, __LINE__), __func__, __LINE__,
        "Native should end every line with the platform's line break");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_DocumentToText, LineEnding_NeverAddsAFinalLineBreak)
{
    // Arrange
    TASWIniWritingOptions crlf;
    crlf.LineEnding = ASWIni::ELineEnding::CRLF;
    TASWIniWritingOptions lf;
    lf.LineEnding = ASWIni::ELineEnding::LF;

    // Act & Assert
    CheckEquals(std::string("a=1\r\nb=2"), WriteBack("a=1\nb=2", crlf, __func__, __LINE__), __func__, __LINE__,
        "a last line with no line break should get none with CRLF");
    CheckEquals(std::string("a=1"), WriteBack("a=1", lf, __func__, __LINE__), __func__, __LINE__,
        "a single line with no line break should get none with LF");
}
//---------------------------------------------------------------------------


/////////////////////////////////////////////////////////////////////////////
// TTest_ASWIni_DocumentRoundTrip
/////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------
TTest_ASWIni_DocumentRoundTrip::TTest_ASWIni_DocumentRoundTrip()
    : inherited("ASWIni_DocumentRoundTrip_Tests")
{
}
//---------------------------------------------------------------------------
TTest_ASWIni_DocumentRoundTrip::~TTest_ASWIni_DocumentRoundTrip()
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_DocumentRoundTrip::CheckSameLine(TASWIniDocumentLine const& expected,
    TASWIniDocumentLine const& actual, ELineBreak lineBreak, std::string const& method, int lineNumber,
    std::string const& msg)
{
    ELineBreak const expectedBreak = expected.GetLineBreak() == ELineBreak::None ? ELineBreak::None : lineBreak;

    CheckEquals(expected.GetKind(), actual.GetKind(), method, lineNumber, msg + ": the kind");
    CheckEquals(expected.GetText(), actual.GetText(), method, lineNumber, msg + ": the text");
    CheckEquals(expectedBreak, actual.GetLineBreak(), method, lineNumber, msg + ": the line break");
    CheckEquals(std::string(expected.GetKey()), std::string(actual.GetKey()), method, lineNumber, msg + ": the key");
    CheckEquals(std::string(expected.GetValue()), std::string(actual.GetValue()), method, lineNumber,
        msg + ": the value");
    CheckEquals(std::string(expected.GetSectionName()), std::string(actual.GetSectionName()), method, lineNumber,
        msg + ": the section name");
    CheckEquals(expected.HasComment(), actual.HasComment(), method, lineNumber, msg + ": whether it has a comment");
    CheckEquals(std::string(expected.GetComment()), std::string(actual.GetComment()), method, lineNumber,
        msg + ": the comment");
}
//---------------------------------------------------------------------------
void TTest_ASWIni_DocumentRoundTrip::SetUp_Group()
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_DocumentRoundTrip::SetUp_Test(ITestCase& /*testCase*/)
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_DocumentRoundTrip::TearDown_Group()
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_DocumentRoundTrip::TearDown_Test(ITestCase& /*testCase*/)
{
}
//---------------------------------------------------------------------------

// /////// Begin tests after this line ///////////////////////

//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_DocumentRoundTrip, Corpus_ConvertedLineEndingsKeepEachLine)
{
    // Arrange
    std::vector<TCorpusText> const corpus = Corpus();

    for (ELineBreak const lineBreak : { ELineBreak::LF, ELineBreak::CRLF })
    {
        TASWIniOptions options;
        options.Writing.LineEnding = lineBreak == ELineBreak::LF ? ASWIni::ELineEnding::LF :
                ASWIni::ELineEnding::CRLF;

        for (TCorpusText const& corpusText : corpus)
        {
            std::string const name = corpusText.Name + " with " + ASWIni::ToString(lineBreak);
            TASWIniResult result;
            std::vector<TASWIniMessage> messages;
            TASWIniDocument const original = ParseText(corpusText.Text, options, result, messages);
            AssertTrue(result.IsOk(), __func__, __LINE__, name + ": the text should parse: " + result.Message);

            // Act
            std::vector<TASWIniMessage> convertedMessages;
            TASWIniDocument const converted = ParseText(original.ToText(), TASWIniOptions(), result,
                convertedMessages);

            // Assert
            AssertTrue(result.IsOk(), __func__, __LINE__, name + ": the converted text should parse: " +
                result.Message);
            CheckEquals(original.HasBom(), converted.HasBom(), __func__, __LINE__,
                name + ": the byte order mark should be kept");
            CheckEquals(CodeNames(MessageCodes(messages)), CodeNames(MessageCodes(convertedMessages)), __func__,
                __LINE__, name + ": the converted text should get the same warnings");

            std::vector<TASWIniDocumentLine> const originalLines = AllLines(original);
            std::vector<TASWIniDocumentLine> const convertedLines = AllLines(converted);
            AssertEquals(static_cast<int>(originalLines.size()), static_cast<int>(convertedLines.size()), __func__,
                __LINE__, name + ": the converted text should have as many lines");
            for (std::size_t i = 0; i < originalLines.size(); ++i)
            {
                CheckSameLine(originalLines[i], convertedLines[i], lineBreak, __func__, __LINE__,
                    name + ", line " + std::to_string(i + 1));
            }
        }
    }
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_DocumentRoundTrip, Corpus_ParsesWithTheExpectedLinesAndWarnings)
{
    // Arrange
    std::vector<TCorpusText> const corpus = Corpus();

    for (TCorpusText const& corpusText : corpus)
    {
        TASWIniResult result;
        std::vector<TASWIniMessage> messages;

        // Act
        TASWIniDocument const document = ParseText(corpusText.Text, TASWIniOptions(), result, messages);

        // Assert
        AssertTrue(result.IsOk(), __func__, __LINE__, corpusText.Name + ": the text should parse: " +
            result.Message);
        CheckEquals(static_cast<int>(corpusText.LineCount), static_cast<int>(document.GetLineCount()), __func__,
            __LINE__, corpusText.Name + ": the number of lines");
        CheckEquals(CodeNames(corpusText.Warnings), CodeNames(MessageCodes(messages)), __func__, __LINE__,
            corpusText.Name + ": the warnings");
    }
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_DocumentRoundTrip, Corpus_WritesBackByteForByte)
{
    // Arrange
    std::vector<TCorpusText> const corpus = Corpus();
    std::vector<TASWIniOptions> const optionSets = ReadingOptionSets();

    for (std::size_t set = 0; set < optionSets.size(); ++set)
    {
        for (TCorpusText const& corpusText : corpus)
        {
            std::string const name = corpusText.Name + " (reading options " + std::to_string(set) + ")";
            TASWIniResult result;
            std::vector<TASWIniMessage> messages;

            // Act
            TASWIniDocument const document = ParseText(corpusText.Text, optionSets[set], result, messages);
            std::string const written = document.ToText();

            // Assert
            AssertTrue(result.IsOk(), __func__, __LINE__, name + ": the text should parse: " + result.Message);
            CheckTrue(written == corpusText.Text, __func__, __LINE__,
                name + ": the text should be written back byte for byte");
        }
    }
}
//---------------------------------------------------------------------------


/////////////////////////////////////////////////////////////////////////////
// TTest_ASWIni_DocumentFuzz
/////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------
TTest_ASWIni_DocumentFuzz::TTest_ASWIni_DocumentFuzz()
    : inherited("ASWIni_DocumentFuzz_Tests")
{
}
//---------------------------------------------------------------------------
TTest_ASWIni_DocumentFuzz::~TTest_ASWIni_DocumentFuzz()
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_DocumentFuzz::SetUp_Group()
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_DocumentFuzz::SetUp_Test(ITestCase& /*testCase*/)
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_DocumentFuzz::TearDown_Group()
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_DocumentFuzz::TearDown_Test(ITestCase& /*testCase*/)
{
}
//---------------------------------------------------------------------------

// /////// Begin tests after this line ///////////////////////

//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_DocumentFuzz, RandomText_IsTheSameEverywhere)
{
    // Arrange
    // The standard requires the 10000th number of a default-constructed std::mt19937 to be 4123659995, so a seed
    // in a failure message gives the same text on every platform
    std::mt19937 generator;
    generator.discard(9999);

    // Act
    std::uint32_t const tenThousandth = static_cast<std::uint32_t>(generator());

    // Assert
    CheckTrue(tenThousandth == 4123659995u, __func__, __LINE__,
        "std::mt19937 should give the numbers the standard defines");
    CheckEquals(RandomText(FuzzFirstSeed), RandomText(FuzzFirstSeed), __func__, __LINE__,
        "the same seed should give the same text");
    CheckFalse(RandomText(FuzzFirstSeed) == RandomText(FuzzFirstSeed + 1), __func__, __LINE__,
        "different seeds should give different texts");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_DocumentFuzz, RandomTexts_ConvertedLineEndingsKeepTheLines)
{
    // Arrange
    TASWIniOptions options;
    options.Writing.LineEnding = ASWIni::ELineEnding::LF;

    for (std::size_t i = 0; i < FuzzTextCount; ++i)
    {
        std::uint32_t const seed = FuzzFirstSeed + static_cast<std::uint32_t>(i);
        std::string const name = "seed " + std::to_string(seed);
        TASWIniResult result;
        std::vector<TASWIniMessage> messages;
        TASWIniDocument const original = ParseText(RandomText(seed), options, result, messages);
        AssertTrue(result.IsOk(), __func__, __LINE__, name + ": the text should parse: " + result.Message);

        // Act
        TASWIniDocument const converted = ParseText(original.ToText(), TASWIniOptions(), result, messages);

        // Assert
        AssertTrue(result.IsOk(), __func__, __LINE__, name + ": the converted text should parse: " + result.Message);
        std::vector<TASWIniDocumentLine> const originalLines = AllLines(original);
        std::vector<TASWIniDocumentLine> const convertedLines = AllLines(converted);
        AssertEquals(static_cast<int>(originalLines.size()), static_cast<int>(convertedLines.size()), __func__,
            __LINE__, name + ": the converted text should have as many lines");
        for (std::size_t line = 0; line < originalLines.size(); ++line)
        {
            if (originalLines[line].GetKind() != convertedLines[line].GetKind() ||
                originalLines[line].GetText() != convertedLines[line].GetText())
            {
                CheckTrue(false, __func__, __LINE__, name + ", line " + std::to_string(line + 1) +
                    ": the converted line should have the same kind and text");
            }
        }
    }
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_DocumentFuzz, RandomTexts_ParseAndWriteBack)
{
    // Arrange
    std::vector<TASWIniOptions> const optionSets = ReadingOptionSets();

    for (std::size_t i = 0; i < FuzzTextCount; ++i)
    {
        std::uint32_t const seed = FuzzFirstSeed + static_cast<std::uint32_t>(i);
        std::string const text = RandomText(seed);

        for (std::size_t set = 0; set < optionSets.size(); ++set)
        {
            std::string const name = "seed " + std::to_string(seed) + " (reading options " + std::to_string(set) +
                ")";
            TASWIniResult result;
            std::vector<TASWIniMessage> messages;

            // Act
            TASWIniDocument const document = ParseText(text, optionSets[set], result, messages);

            // Assert
            AssertTrue(result.IsOk(), __func__, __LINE__, name + ": the text should parse: " + result.Message);
            CheckTrue(document.ToText() == text, __func__, __LINE__,
                name + ": the text should be written back byte for byte");
        }
    }
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_DocumentFuzz, RandomTexts_StrictParsingFailsCleanly)
{
    // Arrange
    TASWIniOptions strict;
    strict.Reading.StrictParsing = true;
    std::size_t failures = 0;

    for (std::size_t i = 0; i < FuzzTextCount; ++i)
    {
        std::uint32_t const seed = FuzzFirstSeed + static_cast<std::uint32_t>(i);
        std::string const name = "seed " + std::to_string(seed);
        std::string const text = RandomText(seed);
        TASWIniResult lenientResult;
        std::vector<TASWIniMessage> lenientMessages;
        TASWIniDocument const lenient = ParseText(text, TASWIniOptions(), lenientResult, lenientMessages);
        TASWIniDocument document;
        std::vector<TASWIniMessage> messages;

        // Act
        TASWIniResult const result = document.Parse(text, strict, messages);

        // Assert
        if (result.IsOk())
        {
            CheckTrue(document.ToText() == text, __func__, __LINE__,
                name + ": a text that parses strictly should be written back byte for byte");
            continue;
        }

        ++failures;
        CheckEquals(EIniCode::ParseFailed, result.Code, __func__, __LINE__, name + ": a strict failure's code");
        CheckTrue(result.Line >= 1 && result.Line <= lenient.GetLineCount(), __func__, __LINE__,
            name + ": the failure should name one of the text's lines");
        CheckEquals(0, static_cast<int>(document.GetLineCount()), __func__, __LINE__,
            name + ": a failed parse should leave the document as it was");
        CheckEquals(0, static_cast<int>(messages.size()), __func__, __LINE__,
            name + ": a failed parse should add no messages");
    }

    // Most random texts have a malformed line, so this test checks failures, not only successes
    CheckTrue(failures > FuzzTextCount / 2, __func__, __LINE__, "most random texts should fail strict parsing");
}
//---------------------------------------------------------------------------


/////////////////////////////////////////////////////////////////////////////
// TTest_ASWIni_DocumentLookup
/////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------
TTest_ASWIni_DocumentLookup::TTest_ASWIni_DocumentLookup()
    : inherited("ASWIni_DocumentLookup_Tests")
{
}
//---------------------------------------------------------------------------
TTest_ASWIni_DocumentLookup::~TTest_ASWIni_DocumentLookup()
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_DocumentLookup::CheckFoundValue(TASWIniDocumentLine const* line, std::string const& value,
    std::string const& method, int lineNumber, std::string const& msg)
{
    AssertTrue(line != nullptr, method, lineNumber, msg + ": the key should be found");
    CheckEquals(value, std::string(line->GetValue()), method, lineNumber, msg + ": the value");
}
//---------------------------------------------------------------------------
TASWIniDocument TTest_ASWIni_DocumentLookup::Parsed(std::string_view text, TASWIniOptions const& options,
    std::string const& method, int lineNumber)
{
    TASWIniResult result;
    std::vector<TASWIniMessage> messages;
    TASWIniDocument document = ParseText(text, options, result, messages);

    AssertTrue(result.IsOk(), method, lineNumber, "the text should parse: " + result.Message);

    return document;
}
//---------------------------------------------------------------------------
void TTest_ASWIni_DocumentLookup::SetUp_Group()
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_DocumentLookup::SetUp_Test(ITestCase& /*testCase*/)
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_DocumentLookup::TearDown_Group()
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_DocumentLookup::TearDown_Test(ITestCase& /*testCase*/)
{
}
//---------------------------------------------------------------------------

// /////// Begin tests after this line ///////////////////////

//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_DocumentLookup, FindKey_DuplicatesPickTheWinner)
{
    // Arrange
    // [a] is a duplicate of [A], so k is in it three times
    std::string const text = "[A]\nk=1\nk=2\n[B]\nk=3\n[a]\nk=4\n";
    TASWIniOptions lastWins;
    lastWins.Reading.DuplicateKeys = ASWIni::EDuplicateKeys::LastWins;

    // Act
    TASWIniDocument const first = Parsed(text, TASWIniOptions(), __func__, __LINE__);
    TASWIniDocument const last = Parsed(text, lastWins, __func__, __LINE__);

    // Assert
    CheckFoundValue(first.FindKey("A", "k"), "1", __func__, __LINE__, "FirstWins");
    CheckTrue(first.FindKey("A", "k") == &first.GetSections()[1].GetLines()[1], __func__, __LINE__,
        "the result should point to the document's line");
    CheckFoundValue(last.FindKey("A", "k"), "4", __func__, __LINE__, "LastWins, across the duplicate sections");
    CheckFoundValue(first.FindKey("B", "k"), "3", __func__, __LINE__, "another section, with FirstWins");
    CheckFoundValue(last.FindKey("B", "k"), "3", __func__, __LINE__, "another section, with LastWins");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_DocumentLookup, FindKey_IgnoresLinesThatArentKeys)
{
    // Arrange
    std::string const text = "[S]\n; k = 1\nk\nx = k=5\n[k]\nother = 2\n";

    // Act
    TASWIniDocument const document = Parsed(text, TASWIniOptions(), __func__, __LINE__);

    // Assert
    CheckTrue(document.FindKey("S", "k") == nullptr, __func__, __LINE__,
        "a comment, a Raw line, a value and a header named k shouldn't be the key k");
    CheckTrue(document.FindKey("k", "k") == nullptr, __func__, __LINE__, "a header shouldn't be a key of its section");
    CheckFoundValue(document.FindKey("k", "other"), "2", __func__, __LINE__, "a key of the section named k");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_DocumentLookup, FindKey_IsNullWhenMissing)
{
    // Arrange
    TASWIniDocument const document = Parsed("[A]\nk=1\n", TASWIniOptions(), __func__, __LINE__);

    // Act & Assert
    CheckTrue(document.FindKey("A", "missing") == nullptr, __func__, __LINE__, "a missing key");
    CheckTrue(document.FindKey("B", "k") == nullptr, __func__, __LINE__, "a missing section");
    CheckTrue(document.FindKey("", "k") == nullptr, __func__, __LINE__, "a key that isn't in the preamble");
    CheckTrue(TASWIniDocument().FindKey("", "k") == nullptr, __func__, __LINE__, "an empty document");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_DocumentLookup, FindKey_NamesFollowCaseSensitive)
{
    // Arrange
    std::string const text = "[Main]\nWidth=1\nwidth=2\n[main]\nWIDTH=3\n";
    TASWIniOptions caseSensitive;
    caseSensitive.Reading.CaseSensitive = true;

    // Act
    TASWIniDocument const ignoringCase = Parsed(text, TASWIniOptions(), __func__, __LINE__);
    TASWIniDocument const matchingCase = Parsed(text, caseSensitive, __func__, __LINE__);

    // Assert
    CheckFoundValue(ignoringCase.FindKey("MAIN", "width"), "1", __func__, __LINE__,
        "names that differ only in case should match by default");
    CheckFoundValue(matchingCase.FindKey("Main", "width"), "2", __func__, __LINE__,
        "the key written in that case, with CaseSensitive");
    CheckFoundValue(matchingCase.FindKey("main", "WIDTH"), "3", __func__, __LINE__,
        "the section written in that case, with CaseSensitive");
    CheckTrue(matchingCase.FindKey("MAIN", "Width") == nullptr, __func__, __LINE__,
        "a section name in another case shouldn't match with CaseSensitive");
    CheckTrue(matchingCase.FindKey("main", "Width") == nullptr, __func__, __LINE__,
        "a key of the other section shouldn't match with CaseSensitive");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_DocumentLookup, FindKey_PreambleIsTheEmptyName)
{
    // Arrange
    TASWIniDocument const document = Parsed("top = 0\n[A]\ntop = 1\n", TASWIniOptions(), __func__, __LINE__);

    // Act & Assert
    CheckFoundValue(document.FindKey("", "top"), "0", __func__, __LINE__, "a key before the first header");
    CheckFoundValue(document.FindKey("A", "top"), "1", __func__, __LINE__, "the same key in a section");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_DocumentLookup, FindSection_FindsTheFirstOfThatName)
{
    // Arrange
    TASWIniDocument const document = Parsed("k=0\n[A]\nk=1\n[B]\n[a]\nk=2\n", TASWIniOptions(), __func__, __LINE__);
    std::vector<ASWIni::TASWIniDocumentSection> const& sections = document.GetSections();
    TASWIniDocument const empty;

    // Act & Assert
    CheckTrue(document.FindSection("a") == &sections[1], __func__, __LINE__,
        "the first of the sections named A, in any case");
    CheckTrue(document.FindSection("B") == &sections[2], __func__, __LINE__, "the section B");
    CheckTrue(document.FindSection("") == &sections[0], __func__, __LINE__, "\"\" should be the preamble");
    CheckTrue(empty.FindSection("") == &empty.GetSections()[0], __func__, __LINE__,
        "an empty document should have its preamble");
    CheckTrue(document.FindSection("C") == nullptr, __func__, __LINE__, "a missing section");
    CheckTrue(document.FindSection(" A") == nullptr, __func__, __LINE__, "a name with a blank isn't A");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_DocumentLookup, FindSection_NamesFollowCaseSensitive)
{
    // Arrange
    TASWIniOptions caseSensitive;
    caseSensitive.Reading.CaseSensitive = true;
    TASWIniDocument const document = Parsed("[A]\n[a]\n", caseSensitive, __func__, __LINE__);

    // Act & Assert
    CheckTrue(document.FindSection("A") == &document.GetSections()[1], __func__, __LINE__, "[A] with CaseSensitive");
    CheckTrue(document.FindSection("a") == &document.GetSections()[2], __func__, __LINE__, "[a] with CaseSensitive");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_DocumentLookup, GetKeyNames_EachKeyOnceInFileOrder)
{
    // Arrange
    std::string const text = "p=1\nq=2\n[A]\nb=1\na=2\nB=3\n; c=0\nraw\n[X]\nz=0\n[a]\nc=4\nA=5\n";
    TASWIniOptions caseSensitive;
    caseSensitive.Reading.CaseSensitive = true;

    // Act
    TASWIniDocument const ignoringCase = Parsed(text, TASWIniOptions(), __func__, __LINE__);
    TASWIniDocument const matchingCase = Parsed(text, caseSensitive, __func__, __LINE__);

    // Assert
    CheckEquals(std::string("b|a|c"), JoinedNames(ignoringCase.GetKeyNames("a")), __func__, __LINE__,
        "the keys of both [A] and [a], each once, as first written");
    CheckEquals(std::string("p|q"), JoinedNames(ignoringCase.GetKeyNames("")), __func__, __LINE__,
        "the keys before the first header");
    CheckEquals(std::string(), JoinedNames(ignoringCase.GetKeyNames("Missing")), __func__, __LINE__,
        "a missing section has no keys");
    CheckEquals(std::string("b|a|B"), JoinedNames(matchingCase.GetKeyNames("A")), __func__, __LINE__,
        "the keys of [A] with CaseSensitive");
    CheckEquals(std::string("c|A"), JoinedNames(matchingCase.GetKeyNames("a")), __func__, __LINE__,
        "the keys of [a] with CaseSensitive");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_DocumentLookup, GetSectionNames_EachNameOnceInFileOrder)
{
    // Arrange
    std::string const text = "top=1\n[B]\n[A]\n[b]\n[C]\n";
    TASWIniOptions caseSensitive;
    caseSensitive.Reading.CaseSensitive = true;

    // Act
    TASWIniDocument const ignoringCase = Parsed(text, TASWIniOptions(), __func__, __LINE__);
    TASWIniDocument const matchingCase = Parsed(text, caseSensitive, __func__, __LINE__);
    TASWIniDocument const preambleOnly = Parsed("k=1\n", TASWIniOptions(), __func__, __LINE__);

    // Assert
    CheckEquals(std::string("B|A|C"), JoinedNames(ignoringCase.GetSectionNames()), __func__, __LINE__,
        "each name once, as first written, without the preamble");
    CheckEquals(std::string("B|A|b|C"), JoinedNames(matchingCase.GetSectionNames()), __func__, __LINE__,
        "names that differ in case are different names with CaseSensitive");
    CheckEquals(std::string(), JoinedNames(preambleOnly.GetSectionNames()), __func__, __LINE__,
        "a document with no header has no section names");
    CheckEquals(std::string(), JoinedNames(TASWIniDocument().GetSectionNames()), __func__, __LINE__,
        "an empty document has no section names");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_DocumentLookup, GetSectionNames_PrefixSelectsTheNames)
{
    // Arrange
    std::string const text = "[Window.Main]\n[Other]\n[window.Tools]\n[Window]\n[Window.Main]\n";
    TASWIniOptions caseSensitive;
    caseSensitive.Reading.CaseSensitive = true;

    // Act
    TASWIniDocument const ignoringCase = Parsed(text, TASWIniOptions(), __func__, __LINE__);
    TASWIniDocument const matchingCase = Parsed(text, caseSensitive, __func__, __LINE__);

    // Assert
    CheckEquals(std::string("Window.Main|window.Tools"), JoinedNames(ignoringCase.GetSectionNames("Window.")),
        __func__, __LINE__, "the names that start with \"Window.\" in any case");
    CheckEquals(std::string("Window.Main"), JoinedNames(matchingCase.GetSectionNames("Window.")), __func__,
        __LINE__, "the names that start with \"Window.\" with CaseSensitive");
    CheckEquals(std::string("Window.Main|window.Tools|Window"), JoinedNames(ignoringCase.GetSectionNames("WINDOW")),
        __func__, __LINE__, "a prefix that is a whole name");
    CheckEquals(std::string("Window.Main|Other|window.Tools|Window"), JoinedNames(ignoringCase.GetSectionNames("")),
        __func__, __LINE__, "an empty prefix selects every name");
    CheckEquals(std::string(), JoinedNames(ignoringCase.GetSectionNames("Window.Main.Extra")), __func__, __LINE__,
        "a prefix longer than every name");
}
//---------------------------------------------------------------------------


/////////////////////////////////////////////////////////////////////////////
// TTest_ASWIni_DocumentSetValue
/////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------
TTest_ASWIni_DocumentSetValue::TTest_ASWIni_DocumentSetValue()
    : inherited("ASWIni_DocumentSetValue_Tests")
{
}
//---------------------------------------------------------------------------
TTest_ASWIni_DocumentSetValue::~TTest_ASWIni_DocumentSetValue()
{
}
//---------------------------------------------------------------------------
std::string TTest_ASWIni_DocumentSetValue::AddAndWrite(std::string_view text, TASWIniOptions const& options,
    std::string_view name, std::string const& method, int lineNumber)
{
    TASWIniResult result;
    std::vector<TASWIniMessage> messages;
    TASWIniDocument document = ParseText(text, options, result, messages);
    AssertTrue(result.IsOk(), method, lineNumber, "the text should parse: " + result.Message);
    std::string const before = document.ToText();

    result = document.AddSection(name);
    AssertTrue(result.IsOk(), method, lineNumber, "AddSection() should succeed: " + result.Message);
    std::string const written = document.ToText();
    CheckEquals(written != before, document.IsModified(), method, lineNumber,
        "the document should be modified exactly when its text changed");

    TASWIniDocument const reread = ParseText(written, options, result, messages);
    AssertTrue(result.IsOk(), method, lineNumber, "the written text should parse: " + result.Message);
    CheckTrue(reread.FindSection(name) != nullptr, method, lineNumber, "the section should read back");

    return written;
}
//---------------------------------------------------------------------------
void TTest_ASWIni_DocumentSetValue::CheckRejected(std::string_view text, TASWIniOptions const& options,
    std::string_view section, std::string_view key, std::string_view value, std::string const& method,
    int lineNumber, std::string const& msg)
{
    TASWIniResult result;
    std::vector<TASWIniMessage> messages;
    TASWIniDocument document = ParseText(text, options, result, messages);
    AssertTrue(result.IsOk(), method, lineNumber, msg + ": the text should parse: " + result.Message);
    std::string const before = document.ToText();

    result = document.SetValue(section, key, value);

    CheckEquals(EIniCode::InvalidArgument, result.Code, method, lineNumber, msg + ": the result's code");
    CheckFalse(result.Message.empty(), method, lineNumber, msg + ": the result should describe the problem");
    CheckEquals(before, document.ToText(), method, lineNumber, msg + ": the text shouldn't change");
    CheckFalse(document.IsModified(), method, lineNumber, msg + ": the document shouldn't be modified");
}
//---------------------------------------------------------------------------
std::string TTest_ASWIni_DocumentSetValue::SetAndWrite(std::string_view text, TASWIniOptions const& options,
    std::string_view section, std::string_view key, std::string_view value, std::string const& method,
    int lineNumber)
{
    TASWIniResult result;
    std::vector<TASWIniMessage> messages;
    TASWIniDocument document = ParseText(text, options, result, messages);
    AssertTrue(result.IsOk(), method, lineNumber, "the text should parse: " + result.Message);
    std::string const before = document.ToText();

    result = document.SetValue(section, key, value);
    AssertTrue(result.IsOk(), method, lineNumber, "SetValue() should succeed: " + result.Message);
    std::string const written = document.ToText();
    CheckEquals(written != before, document.IsModified(), method, lineNumber,
        "the document should be modified exactly when its text changed");

    // What was set reads back
    TASWIniDocument const reread = ParseText(written, options, result, messages);
    AssertTrue(result.IsOk(), method, lineNumber, "the written text should parse: " + result.Message);
    TASWIniDocumentLine const* const line = reread.FindKey(section, key);
    AssertTrue(line != nullptr, method, lineNumber, "the key should read back");
    CheckEquals(std::string(value), std::string(line->GetValue()), method, lineNumber, "the value should read back");

    return written;
}
//---------------------------------------------------------------------------
void TTest_ASWIni_DocumentSetValue::SetUp_Group()
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_DocumentSetValue::SetUp_Test(ITestCase& /*testCase*/)
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_DocumentSetValue::TearDown_Group()
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_DocumentSetValue::TearDown_Test(ITestCase& /*testCase*/)
{
}
//---------------------------------------------------------------------------

// /////// Begin tests after this line ///////////////////////

//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_DocumentSetValue, AddSection_AddsAnEmptySectionAtTheEnd)
{
    // Arrange
    TASWIniOptions const options;
    TASWIniOptions noBlankLine;
    noBlankLine.Writing.BlankLineBeforeNewSection = false;
    TASWIniOptions lf;
    lf.Writing.LineEnding = ASWIni::ELineEnding::LF;

    // Act & Assert
    CheckEquals(std::string("[A]\nk=1\n\n[B]\n"), AddAndWrite("[A]\nk=1\n", options, "B", __func__, __LINE__),
        __func__, __LINE__, "after a blank line");
    CheckEquals(std::string("[A]\nk=1\n[B]\n"), AddAndWrite("[A]\nk=1\n", noBlankLine, "B", __func__, __LINE__),
        __func__, __LINE__, "without a blank line when BlankLineBeforeNewSection is off");
    CheckEquals(std::string("[A]\n\n[B]\n"), AddAndWrite("[A]\n\n", options, "B", __func__, __LINE__), __func__,
        __LINE__, "after the blank line the text ends with");
    CheckEquals(std::string("[B]\n"), AddAndWrite("", lf, "B", __func__, __LINE__), __func__, __LINE__,
        "first in an empty document, with no blank line");
    CheckEquals(std::string("[A]\n\n[B]\n"), AddAndWrite("[A]", lf, "B", __func__, __LINE__), __func__, __LINE__,
        "a last line without a line break gets one");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_DocumentSetValue, AddSection_DoesNothingForAnExistingSection)
{
    // Arrange
    TASWIniOptions const options;
    std::string const text = "[A]\nk=1\n[B]\n";

    // Act & Assert
    CheckEquals(text, AddAndWrite(text, options, "a", __func__, __LINE__), __func__, __LINE__,
        "a section of that name in another case");
    CheckEquals(text, AddAndWrite(text, options, "B", __func__, __LINE__), __func__, __LINE__,
        "the last section");
    CheckEquals(text, AddAndWrite(text, options, "", __func__, __LINE__), __func__, __LINE__, "the preamble");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_DocumentSetValue, AddSection_RejectsNamesThatWouldntReadBack)
{
    // Arrange
    std::vector<std::string> const names = { " B", "B ", "\tB", "a]b", "]", "\t", "a\nb", "a\rb" };

    for (std::string const& name : names)
    {
        TASWIniDocument document;
        std::vector<TASWIniMessage> messages;
        AssertTrue(document.Parse("[A]\n", TASWIniOptions(), messages).IsOk(), __func__, __LINE__,
            "the text should parse");

        // Act
        TASWIniResult const result = document.AddSection(name);

        // Assert
        std::string const msg = "the name \"" + name + "\"";
        CheckEquals(EIniCode::InvalidArgument, result.Code, __func__, __LINE__, msg + " should be rejected");
        CheckFalse(result.Message.empty(), __func__, __LINE__, msg + ": the result should describe the problem");
        CheckEquals(std::string("[A]\n"), document.ToText(), __func__, __LINE__, msg + ": the text shouldn't change");
        CheckFalse(document.IsModified(), __func__, __LINE__, msg + ": the document shouldn't be modified");
    }
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_DocumentSetValue, Modified_IsSetByAChange)
{
    // Arrange
    TASWIniDocument document;
    std::vector<TASWIniMessage> messages;
    AssertTrue(document.Parse("[A]\nk=1\n", TASWIniOptions(), messages).IsOk(), __func__, __LINE__,
        "the text should parse");

    // Act & Assert
    AssertTrue(document.SetValue("A", "k", "2").IsOk(), __func__, __LINE__, "a value should be set");
    CheckTrue(document.IsModified(), __func__, __LINE__, "a changed value should set the flag");
    document.ClearModified();
    CheckFalse(document.IsModified(), __func__, __LINE__, "ClearModified() should clear the flag");
    AssertTrue(document.SetValue("A", "k", "2").IsOk(), __func__, __LINE__, "the same value should be set");
    CheckFalse(document.IsModified(), __func__, __LINE__, "the same value shouldn't set the flag");
    AssertTrue(document.SetValue("A", "new", "1").IsOk(), __func__, __LINE__, "a new key should be set");
    CheckTrue(document.IsModified(), __func__, __LINE__, "a new key should set the flag");
    AssertTrue(document.Parse("[A]\nk=1\n", TASWIniOptions(), messages).IsOk(), __func__, __LINE__,
        "the text should parse again");
    CheckFalse(document.IsModified(), __func__, __LINE__, "parsing should clear the flag");
    AssertTrue(document.AddSection("B").IsOk(), __func__, __LINE__, "a section should be added");
    CheckTrue(document.IsModified(), __func__, __LINE__, "a new section should set the flag");
    AssertTrue(document.Reset(TASWIniOptions()).IsOk(), __func__, __LINE__, "the document should reset");
    CheckFalse(document.IsModified(), __func__, __LINE__, "a reset should clear the flag");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_DocumentSetValue, NewKey_GoesAfterTheHeaderOfASectionWithoutKeys)
{
    // Arrange
    TASWIniOptions const options;

    // Act & Assert
    CheckEquals(std::string("[A]\nk = v\n; about\n\n[B]\n"), SetAndWrite("[A]\n; about\n\n[B]\n", options, "A", "k",
        "v", __func__, __LINE__), __func__, __LINE__, "before the comments and blank lines of the section");
    CheckEquals(std::string("  [A]\nk = v\n"), SetAndWrite("  [A]\n", options, "A", "k", "v", __func__, __LINE__),
        __func__, __LINE__, "without the indentation of the header");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_DocumentSetValue, NewKey_GoesAfterTheLastKey)
{
    // Arrange
    TASWIniOptions const options;

    // Act & Assert
    CheckEquals(std::string("[A]\na = 1\n  b = 2\n  c = 3\n; old = 3\n\n[B]\n"), SetAndWrite(
        "[A]\na = 1\n  b = 2\n; old = 3\n\n[B]\n", options, "A", "c", "3", __func__, __LINE__), __func__, __LINE__,
        "after the last key, with its indentation, before a comment and a blank line");
    CheckEquals(std::string("[A]\n\tk=1\n\tl = 2\n"), SetAndWrite("[A]\n\tk=1\n", options, "A", "l", "2", __func__,
        __LINE__), __func__, __LINE__, "a tab as indentation");
    CheckEquals(std::string("[A]\nk=1\nl = 2\nraw\n"), SetAndWrite("[A]\nk=1\nraw\n", options, "A", "l", "2",
        __func__, __LINE__), __func__, __LINE__, "a Raw line isn't a key");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_DocumentSetValue, NewKey_GoesInTheFirstSectionOfThatName)
{
    // Act
    std::string const written = SetAndWrite("[A]\na=1\n[B]\n[a]\nb=2\n", TASWIniOptions(), "a", "c", "3", __func__,
        __LINE__);

    // Assert
    CheckEquals(std::string("[A]\na=1\nc = 3\n[B]\n[a]\nb=2\n"), written, __func__, __LINE__,
        "in the first section named A, in any case");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_DocumentSetValue, NewKey_GoesUnderTheFileCommentInThePreamble)
{
    // Arrange
    TASWIniOptions const options;
    TASWIniOptions lf;
    lf.Writing.LineEnding = ASWIni::ELineEnding::LF;

    // Act & Assert
    CheckEquals(std::string("x=1\ny = 2\n\n[A]\n"), SetAndWrite("x=1\n\n[A]\n", options, "", "y", "2", __func__,
        __LINE__), __func__, __LINE__, "after the last key of the preamble");
    CheckEquals(std::string("; file\n\nk = v\n; about A\n[A]\n"), SetAndWrite("; file\n\n; about A\n[A]\n", options,
        "", "k", "v", __func__, __LINE__), __func__, __LINE__, "after the file comment and the blank line under it");
    CheckEquals(std::string("; file\n\n\nk = v\n"), SetAndWrite("; file\n\n\n", options, "", "k", "v", __func__,
        __LINE__), __func__, __LINE__, "after every blank line under the file comment");
    CheckEquals(std::string("k = v\n; about A\n[A]\n"), SetAndWrite("; about A\n[A]\n", options, "", "k", "v",
        __func__, __LINE__), __func__, __LINE__, "before a comment that leads into the first header");
    CheckEquals(std::string("; file\n\nk = v\n"), SetAndWrite("; file\n", options, "", "k", "v", __func__, __LINE__),
        __func__, __LINE__, "a blank line between the file comment and the key when there is none");
    CheckEquals(std::string("; file\n\nk = v\n"), SetAndWrite("; file", lf, "", "k", "v", __func__, __LINE__),
        __func__, __LINE__, "a file comment without a line break");
    CheckEquals(std::string("k = v\n"), SetAndWrite("", lf, "", "k", "v", __func__, __LINE__), __func__, __LINE__,
        "an empty document");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_DocumentSetValue, NewKey_IsQuotedWhenItNeedsIt)
{
    // Arrange
    TASWIniOptions quoted;
    quoted.Reading.QuotedValues = true;
    TASWIniOptions quotedWithComments = quoted;
    quotedWithComments.Reading.InlineComments = true;

    // Act & Assert
    CheckEquals(std::string("[A]\nk = \" x \"\n"), SetAndWrite("[A]\n", quoted, "A", "k", " x ", __func__, __LINE__),
        __func__, __LINE__, "blanks at either end");
    CheckEquals(std::string("[A]\nk = \"a ; b\"\n"), SetAndWrite("[A]\n", quotedWithComments, "A", "k", "a ; b",
        __func__, __LINE__), __func__, __LINE__, "a comment prefix after a blank, with InlineComments");
    CheckEquals(std::string("[A]\nk = a\n"), SetAndWrite("[A]\n", quoted, "A", "k", "a", __func__, __LINE__),
        __func__, __LINE__, "a value that doesn't need quotes");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_DocumentSetValue, NewKey_LineBreaks)
{
    // Arrange
    TASWIniOptions const options;
    TASWIniOptions crlf;
    crlf.Writing.LineEnding = ASWIni::ELineEnding::CRLF;

    // Act & Assert
    CheckEquals(std::string("[A]\r\nk=1\r\nl = 2\r\n"), SetAndWrite("[A]\r\nk=1", options, "A", "l", "2", __func__,
        __LINE__), __func__, __LINE__, "the document's line break, also for a last line without one");
    CheckEquals(std::string("[A]\rk=1\rl = 2\r"), SetAndWrite("[A]\rk=1\r", options, "A", "l", "2", __func__,
        __LINE__), __func__, __LINE__, "CR, the document's line break");
    CheckEquals(std::string("[A]\nk=1\nl = 2\n[B]\nx=1"), SetAndWrite("[A]\nk=1\n[B]\nx=1", options, "A", "l", "2",
        __func__, __LINE__), __func__, __LINE__, "a last line elsewhere keeps having no line break");
    CheckEquals(std::string("[A]\r\nk=1\r\nl = 2\r\n"), SetAndWrite("[A]\nk=1\n", crlf, "A", "l", "2", __func__,
        __LINE__), __func__, __LINE__, "the converted line break");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_DocumentSetValue, NewKey_RejectsNamesThatWouldntReadBack)
{
    // Arrange
    TASWIniOptions const options;
    std::string const text = "[A]\nk=1\n";
    std::vector<std::string> const keys = { "a=b", "a:b", ";x", "#x", "[x", " k2", "k2 ", "", "a\nb", "a\rb" };
    std::vector<std::string> const sections = { " B", "B ", "a]b", "]", "B\n" };

    // Act & Assert
    for (std::string const& key : keys)
        CheckRejected(text, options, "A", key, "v", __func__, __LINE__, "the key \"" + key + "\"");

    for (std::string const& section : sections)
        CheckRejected(text, options, section, "k", "v", __func__, __LINE__, "the new section \"" + section + "\"");

    CheckRejected(text, options, "B", ";x", "v", __func__, __LINE__,
        "a new section with a key that wouldn't read back shouldn't be added");
    CheckRejected(text, options, "A", "new", " x", __func__, __LINE__, "a value that needs quotes");
    CheckRejected(text, options, "A", "new", "a\nb", __func__, __LINE__, "a value with a line break");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_DocumentSetValue, NewKey_UsesTheWritingSeparator)
{
    // Arrange
    TASWIniOptions const spaced;
    TASWIniOptions plain;
    plain.Writing.Separator = "=";
    TASWIniOptions colon;
    colon.Writing.Separator = ": ";
    TASWIniOptions tabbed;
    tabbed.Writing.Separator = "\t=\t";

    // Act & Assert
    CheckEquals(std::string("[A]\nk = v\n"), SetAndWrite("[A]\n", spaced, "A", "k", "v", __func__, __LINE__),
        __func__, __LINE__, "the default \" = \"");
    CheckEquals(std::string("[A]\nk=v\n"), SetAndWrite("[A]\n", plain, "A", "k", "v", __func__, __LINE__), __func__,
        __LINE__, "\"=\"");
    CheckEquals(std::string("[A]\nk: v\n"), SetAndWrite("[A]\n", colon, "A", "k", "v", __func__, __LINE__), __func__,
        __LINE__, "\": \"");
    CheckEquals(std::string("[A]\nk\t=\tv\n"), SetAndWrite("[A]\n", tabbed, "A", "k", "v", __func__, __LINE__),
        __func__, __LINE__, "\"\\t=\\t\"");
    CheckEquals(std::string("[A]\nk =\n"), SetAndWrite("[A]\n", spaced, "A", "k", "", __func__, __LINE__), __func__,
        __LINE__, "an empty value without the blank after the separator");
    CheckEquals(std::string("[A]\nk:\n"), SetAndWrite("[A]\n", colon, "A", "k", "", __func__, __LINE__), __func__,
        __LINE__, "an empty value with \": \"");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_DocumentSetValue, NewSection_GoesAtTheEnd)
{
    // Arrange
    TASWIniOptions const options;
    TASWIniOptions noBlankLine;
    noBlankLine.Writing.BlankLineBeforeNewSection = false;
    TASWIniOptions lf;
    lf.Writing.LineEnding = ASWIni::ELineEnding::LF;

    // Act & Assert
    CheckEquals(std::string("[A]\nk=1\n\n[B]\nx = 1\n"), SetAndWrite("[A]\nk=1\n", options, "B", "x", "1", __func__,
        __LINE__), __func__, __LINE__, "after a blank line, with the key right after the header");
    CheckEquals(std::string("[A]\nk=1\n[B]\nx = 1\n"), SetAndWrite("[A]\nk=1\n", noBlankLine, "B", "x", "1",
        __func__, __LINE__), __func__, __LINE__, "without a blank line when BlankLineBeforeNewSection is off");
    CheckEquals(std::string("; c\n\n[B]\nx = 1\n"), SetAndWrite("; c\n", options, "B", "x", "1", __func__,
        __LINE__), __func__, __LINE__, "after a preamble");
    CheckEquals(std::string("[B]\nx = 1\n"), SetAndWrite("", lf, "B", "x", "1", __func__, __LINE__), __func__,
        __LINE__, "first in an empty document");
    CheckEquals(std::string("[A]\nk=1\n\n[B]\nx = 1\n"), SetAndWrite("[A]\nk=1", lf, "B", "x", "1", __func__,
        __LINE__), __func__, __LINE__, "after a last line without a line break");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_DocumentSetValue, Value_ChangesTheWinningDuplicate)
{
    // Arrange
    std::string const text = "[A]\nk=1\nk=2\n[a]\nk=3\n";
    TASWIniOptions lastWins;
    lastWins.Reading.DuplicateKeys = ASWIni::EDuplicateKeys::LastWins;

    // Act & Assert
    CheckEquals(std::string("[A]\nk=x\nk=2\n[a]\nk=3\n"), SetAndWrite(text, TASWIniOptions(), "A", "K", "x", __func__,
        __LINE__), __func__, __LINE__, "the first line with FirstWins");
    CheckEquals(std::string("[A]\nk=1\nk=2\n[a]\nk=x\n"), SetAndWrite(text, lastWins, "A", "K", "x", __func__,
        __LINE__), __func__, __LINE__, "the last line, in the duplicate section, with LastWins");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_DocumentSetValue, Value_EmptyGetsTheBlanksBeforeTheSeparator)
{
    // Arrange
    TASWIniOptions const options;

    // Act & Assert
    CheckEquals(std::string("a = v\n"), SetAndWrite("a =\n", options, "", "a", "v", __func__, __LINE__), __func__,
        __LINE__, "a blank before the separator");
    CheckEquals(std::string("b=v\n"), SetAndWrite("b=\n", options, "", "b", "v", __func__, __LINE__), __func__,
        __LINE__, "no blank before the separator");
    CheckEquals(std::string("c\t=\tv\n"), SetAndWrite("c\t=\n", options, "", "c", "v", __func__, __LINE__), __func__,
        __LINE__, "a tab before the separator");
    CheckEquals(std::string("d  :  v\n"), SetAndWrite("d  :\n", options, "", "d", "v", __func__, __LINE__), __func__,
        __LINE__, "two blanks before a ':'");
    CheckEquals(std::string("e = v\n"), SetAndWrite("e = \n", options, "", "e", "v", __func__, __LINE__), __func__,
        __LINE__, "a blank after the separator is kept");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_DocumentSetValue, Value_KeepsTheInlineComment)
{
    // Arrange
    TASWIniOptions withComments;
    withComments.Reading.InlineComments = true;

    // Act & Assert
    CheckEquals(std::string("k = x ; note\n"), SetAndWrite("k = v ; note\n", withComments, "", "k", "x", __func__,
        __LINE__), __func__, __LINE__, "a comment after the value");
    CheckEquals(std::string("k=x\t# note\n"), SetAndWrite("k=v\t# note\n", withComments, "", "k", "x", __func__,
        __LINE__), __func__, __LINE__, "a comment after a tab");
    CheckEquals(std::string("k = x ; note\n"), SetAndWrite("k = ; note\n", withComments, "", "k", "x", __func__,
        __LINE__), __func__, __LINE__, "a comment after an empty value gets a blank before it");
    CheckEquals(std::string("k = x\n"), SetAndWrite("k = v ; note\n", TASWIniOptions(), "", "k", "x", __func__,
        __LINE__), __func__, __LINE__, "without InlineComments, the prefix and the note are part of the value");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_DocumentSetValue, Value_KeepsTheLineBreak)
{
    // Arrange
    TASWIniOptions const options;

    // Act & Assert
    CheckEquals(std::string("a=1\r\nb=x\n"), SetAndWrite("a=1\r\nb=2\n", options, "", "b", "x", __func__, __LINE__),
        __func__, __LINE__, "the line's own line break");
    CheckEquals(std::string("a=x\rb=2\r"), SetAndWrite("a=1\rb=2\r", options, "", "a", "x", __func__, __LINE__),
        __func__, __LINE__, "CR");
    CheckEquals(std::string("k=2"), SetAndWrite("k=1", options, "", "k", "2", __func__, __LINE__), __func__,
        __LINE__, "a last line without a line break keeps having none");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_DocumentSetValue, Value_QuotedStaysQuoted)
{
    // Arrange
    TASWIniOptions quoted;
    quoted.Reading.QuotedValues = true;
    TASWIniOptions quotedWithComments = quoted;
    quotedWithComments.Reading.InlineComments = true;

    // Act & Assert
    CheckEquals(std::string("k = \"new\"\n"), SetAndWrite("k = \"old\"\n", quoted, "", "k", "new", __func__,
        __LINE__), __func__, __LINE__, "a quoted value");
    CheckEquals(std::string("k = \"x\"\n"), SetAndWrite("k = \"\"\n", quoted, "", "k", "x", __func__, __LINE__),
        __func__, __LINE__, "an empty quoted value");
    CheckEquals(std::string("k = \"b\" ; c\n"), SetAndWrite("k = \"a\" ; c\n", quotedWithComments, "", "k", "b",
        __func__, __LINE__), __func__, __LINE__, "a quoted value with a comment");
    CheckEquals(std::string("k = a\"b\n"), SetAndWrite("k = \"old\"\n", quoted, "", "k", "a\"b", __func__,
        __LINE__), __func__, __LINE__, "a value with a '\"' loses the quotes, which it doesn't need");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_DocumentSetValue, Value_QuotedWhenItNeedsIt)
{
    // Arrange
    TASWIniOptions quoted;
    quoted.Reading.QuotedValues = true;
    TASWIniOptions quotedWithComments = quoted;
    quotedWithComments.Reading.InlineComments = true;

    // Act & Assert
    CheckEquals(std::string("k = \"  spaced  \"\n"), SetAndWrite("k = v\n", quoted, "", "k", "  spaced  ", __func__,
        __LINE__), __func__, __LINE__, "blanks at either end");
    CheckEquals(std::string("k = \"open\n"), SetAndWrite("k = v\n", quoted, "", "k", "\"open", __func__, __LINE__),
        __func__, __LINE__, "a leading '\"' with no closing one reads back without quotes");
    CheckEquals(std::string("k = \"a ; b\"\n"), SetAndWrite("k = v\n", quotedWithComments, "", "k", "a ; b",
        __func__, __LINE__), __func__, __LINE__, "a comment prefix after a blank");
    CheckEquals(std::string("k = \";x\"\n"), SetAndWrite("k = v\n", quotedWithComments, "", "k", ";x", __func__,
        __LINE__), __func__, __LINE__, "a leading comment prefix, after the separator's blank");
    CheckEquals(std::string("k = a;b\n"), SetAndWrite("k = v\n", quotedWithComments, "", "k", "a;b", __func__,
        __LINE__), __func__, __LINE__, "a comment prefix with no blank before it doesn't need quotes");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_DocumentSetValue, Value_RejectedWhenItWouldntReadBack)
{
    // Arrange
    std::string const text = "k = v\n";
    TASWIniOptions const options;
    TASWIniOptions withComments;
    withComments.Reading.InlineComments = true;
    TASWIniOptions quoted;
    quoted.Reading.QuotedValues = true;

    // Act & Assert
    CheckRejected(text, options, "", "k", " lead", __func__, __LINE__, "a leading blank without QuotedValues");
    CheckRejected(text, options, "", "k", "trail\t", __func__, __LINE__, "a trailing tab without QuotedValues");
    CheckRejected(text, options, "", "k", "a\nb", __func__, __LINE__, "an LF");
    CheckRejected(text, options, "", "k", "a\rb", __func__, __LINE__, "a CR");
    CheckRejected(text, withComments, "", "k", "a ; b", __func__, __LINE__,
        "a comment prefix after a blank, with InlineComments and without QuotedValues");
    CheckRejected(text, quoted, "", "k", "\"x\"", __func__, __LINE__, "a value in quotes, with QuotedValues");
    CheckRejected(text, quoted, "", "k", " a\"b ", __func__, __LINE__, "a value that needs quotes and holds one");
    CheckRejected(text, quoted, "", "k", "a\r\nb", __func__, __LINE__, "a CRLF, with QuotedValues");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_DocumentSetValue, Value_ReplacesOnlyTheValue)
{
    // Arrange
    TASWIniOptions const options;

    // Act & Assert
    CheckEquals(std::string("  Width \t=\t 800 \t\n"), SetAndWrite("  Width \t=\t 640 \t\n", options, "", "width",
        "800", __func__, __LINE__), __func__, __LINE__, "the indentation, key, spacing and trailing blanks stay");
    CheckEquals(std::string("a: 2\n"), SetAndWrite("a: 1\n", options, "", "a", "2", __func__, __LINE__), __func__,
        __LINE__, "a ':' separator stays");
    CheckEquals(std::string("k=a=b\n"), SetAndWrite("k=v\n", options, "", "k", "a=b", __func__, __LINE__), __func__,
        __LINE__, "a value with a separator");
    CheckEquals(std::string("[S]\nurl = http://b:8080\n"), SetAndWrite("[S]\nurl = http://a\n", options, "S", "url",
        "http://b:8080", __func__, __LINE__), __func__, __LINE__, "a value with ':'");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_DocumentSetValue, Value_SameValueChangesNothing)
{
    // Arrange
    TASWIniOptions quoted;
    quoted.Reading.QuotedValues = true;
    TASWIniOptions withComments;
    withComments.Reading.InlineComments = true;

    // Act & Assert
    // SetAndWrite() also checks that the document isn't modified when its text doesn't change
    CheckEquals(std::string("k = v\n"), SetAndWrite("k = v\n", TASWIniOptions(), "", "K", "v", __func__, __LINE__),
        __func__, __LINE__, "the same value");
    CheckEquals(std::string("k = \"v\"\n"), SetAndWrite("k = \"v\"\n", quoted, "", "k", "v", __func__, __LINE__),
        __func__, __LINE__, "the same quoted value");
    CheckEquals(std::string("k = v ; note\n"), SetAndWrite("k = v ; note\n", withComments, "", "k", "v", __func__,
        __LINE__), __func__, __LINE__, "the same value with a comment");
}
//---------------------------------------------------------------------------

} // namespace ASWUnitTests

//---------------------------------------------------------------------------
ASW_REGISTER_TEST_GROUP_IN_SUITE(ASWUnitTests::TTest_ASWIni_DocumentFuzz, "Document")
ASW_REGISTER_TEST_GROUP_IN_SUITE(ASWUnitTests::TTest_ASWIni_DocumentLookup, "Document")
ASW_REGISTER_TEST_GROUP_IN_SUITE(ASWUnitTests::TTest_ASWIni_DocumentMessages, "Document")
ASW_REGISTER_TEST_GROUP_IN_SUITE(ASWUnitTests::TTest_ASWIni_DocumentModel, "Document")
ASW_REGISTER_TEST_GROUP_IN_SUITE(ASWUnitTests::TTest_ASWIni_DocumentParse, "Document")
ASW_REGISTER_TEST_GROUP_IN_SUITE(ASWUnitTests::TTest_ASWIni_DocumentRoundTrip, "Document")
ASW_REGISTER_TEST_GROUP_IN_SUITE(ASWUnitTests::TTest_ASWIni_DocumentSetValue, "Document")
ASW_REGISTER_TEST_GROUP_IN_SUITE(ASWUnitTests::TTest_ASWIni_DocumentToText, "Document")
