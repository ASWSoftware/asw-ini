/* **************************************************************************
ASWIni_Document.cpp
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
#include "ASWIni_Document.h"
//---------------------------------------------------------------------------
#include <initializer_list>
#include <iterator>
#include <map>
#include <string>
#include <utility>
//---------------------------------------------------------------------------
#include "ASWIni_Utils.h"
//---------------------------------------------------------------------------

namespace ASWIni
{

namespace
{

// The UTF-8 byte order mark
constexpr std::string_view Utf8Bom("\xEF\xBB\xBF", 3);

/////////////////////////////////////////////////////////////////////////////
// TNameLess
//
// Orders section and key names for finding duplicates: byte by byte, or with the ASCII letters' case ignored
/////////////////////////////////////////////////////////////////////////////
struct TNameLess
{
    bool CaseSensitive = false;

    bool operator()(std::string_view left, std::string_view right) const noexcept;
};

//---------------------------------------------------------------------------

std::size_t FindInlineComment(std::string_view text, std::size_t from, std::vector<std::string> const& prefixes,
    std::size_t& prefixSize) noexcept;
std::size_t MatchCommentPrefix(std::string_view text, std::size_t pos,
    std::vector<std::string> const& prefixes) noexcept;
ELineBreak MostCommonLineBreak(std::vector<Detail::TLineSpan> const& lines) noexcept;
std::string Quoted(std::string_view name);
std::size_t SkipBlanks(std::string_view text, std::size_t pos) noexcept;
std::size_t TrimBlanksBefore(std::string_view text, std::size_t begin, std::size_t end) noexcept;
TASWIniMessage Warning(EIniCode code, std::string const& text, std::size_t line, std::string_view section,
    std::string_view key);
ELineBreak WrittenLineBreak(ELineBreak lineBreak, ELineEnding lineEnding) noexcept;

//---------------------------------------------------------------------------
/*
    FindInlineComment

    The position of the first comment prefix in 'text' at or after 'from' that follows a space or a tab, with the
    prefix's size in 'prefixSize'; std::string_view::npos when there is none.
*/
std::size_t FindInlineComment(std::string_view text, std::size_t from, std::vector<std::string> const& prefixes,
    std::size_t& prefixSize) noexcept
{
    for (std::size_t i = from; i < text.size(); ++i)
    {
        if (i == 0 || !Detail::IsBlank(text[i - 1]))
            continue;

        prefixSize = MatchCommentPrefix(text, i, prefixes);
        if (prefixSize > 0)
            return i;
    }

    return std::string_view::npos;
}
//---------------------------------------------------------------------------
/*
    MatchCommentPrefix

    The size of the longest of 'prefixes' that 'text' has at 'pos'; 0 when it has none.
*/
std::size_t MatchCommentPrefix(std::string_view text, std::size_t pos,
    std::vector<std::string> const& prefixes) noexcept
{
    std::string_view const rest = text.substr(pos);
    std::size_t longest = 0;
    for (std::string const& prefix : prefixes)
    {
        if (prefix.size() > longest && rest.substr(0, prefix.size()) == prefix)
            longest = prefix.size();
    }

    return longest;
}
//---------------------------------------------------------------------------
/*
    MostCommonLineBreak

    The line break most of 'lines' end with (the one found first in a tie); ELineBreak::None when none of them has one.
*/
ELineBreak MostCommonLineBreak(std::vector<Detail::TLineSpan> const& lines) noexcept
{
    // Indexed by the ELineBreak values: None, LF, CR, CRLF
    std::size_t counts[4] = {};
    std::size_t firstLines[4] = {};

    for (std::size_t i = 0; i < lines.size(); ++i)
    {
        std::size_t const index = static_cast<std::size_t>(lines[i].Break);
        if (counts[index] == 0)
            firstLines[index] = i;
        ++counts[index];
    }

    ELineBreak mostCommon = ELineBreak::None;
    std::size_t mostCommonIndex = 0;
    for (ELineBreak const lineBreak : { ELineBreak::LF, ELineBreak::CR, ELineBreak::CRLF })
    {
        std::size_t const index = static_cast<std::size_t>(lineBreak);
        if (counts[index] == 0)
            continue;

        if (mostCommonIndex == 0 || counts[index] > counts[mostCommonIndex] ||
            (counts[index] == counts[mostCommonIndex] && firstLines[index] < firstLines[mostCommonIndex]))
        {
            mostCommon = lineBreak;
            mostCommonIndex = index;
        }
    }

    return mostCommon;
}
//---------------------------------------------------------------------------
/*
    Quoted

    'name' in double quotes, for a message.
*/
std::string Quoted(std::string_view name)
{
    std::string text = "\"";
    text += name;
    text += "\"";

    return text;
}
//---------------------------------------------------------------------------
/*
    SkipBlanks

    The position of the first character of 'text' at or after 'pos' that isn't a space or a tab (the text's size when
    there is none).
*/
std::size_t SkipBlanks(std::string_view text, std::size_t pos) noexcept
{
    while (pos < text.size() && Detail::IsBlank(text[pos]))
        ++pos;

    return pos;
}
//---------------------------------------------------------------------------
/*
    TrimBlanksBefore

    'end' moved back over the spaces and tabs before it, but not before 'begin': the end of the text from 'begin' to
    'end' without its trailing blanks.
*/
std::size_t TrimBlanksBefore(std::string_view text, std::size_t begin, std::size_t end) noexcept
{
    while (end > begin && Detail::IsBlank(text[end - 1]))
        --end;

    return end;
}
//---------------------------------------------------------------------------
/*
    Warning

    A warning message about line 'line' of section 'section' (and key 'key', when it isn't empty).
*/
TASWIniMessage Warning(EIniCode code, std::string const& text, std::size_t line, std::string_view section,
    std::string_view key)
{
    TASWIniMessage message;
    message.Severity = ESeverity::Warning;
    message.Code = code;
    message.Text = text;
    message.Line = line;
    message.Section = std::string(section);
    message.Key = std::string(key);

    return message;
}
//---------------------------------------------------------------------------
/*
    WrittenLineBreak

    The line break a line that ended with 'lineBreak' is written with: its own with ELineEnding::Auto (or a value
    that isn't an ELineEnding), else the one 'lineEnding' names. A line with no line break keeps having none.
*/
ELineBreak WrittenLineBreak(ELineBreak lineBreak, ELineEnding lineEnding) noexcept
{
    if (lineBreak == ELineBreak::None)
        return ELineBreak::None;

    switch (lineEnding)
    {
        case ELineEnding::Auto:
            return lineBreak;

        case ELineEnding::Native:
            return Detail::NativeLineBreak();

        case ELineEnding::LF:
            return ELineBreak::LF;

        case ELineEnding::CRLF:
            return ELineBreak::CRLF;
    }

    return lineBreak;
}
//---------------------------------------------------------------------------


/////////////////////////////////////////////////////////////////////////////
// TNameLess
/////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------
bool TNameLess::operator()(std::string_view left, std::string_view right) const noexcept
{
    if (CaseSensitive)
        return left < right;

    return Detail::CompareNoCase(left, right) < 0;
}
//---------------------------------------------------------------------------

} // namespace

//---------------------------------------------------------------------------
char const* ToString(ELineKind kind) noexcept
{
    switch (kind)
    {
        case ELineKind::Blank:
            return "Blank";

        case ELineKind::Comment:
            return "Comment";

        case ELineKind::Section:
            return "Section";

        case ELineKind::KeyValue:
            return "KeyValue";

        case ELineKind::Raw:
            return "Raw";
    }

    return "Unknown";
}
//---------------------------------------------------------------------------


/////////////////////////////////////////////////////////////////////////////
// TASWIniDocumentLine
/////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------
TASWIniDocumentLine::TASWIniDocumentLine(std::string_view text, ELineBreak lineBreak,
    TASWIniReadingOptions const& options, TProblem& problem)
    : m_Text(text),
      m_LineBreak(lineBreak)
{
    problem = TProblem();

    std::size_t const begin = SkipBlanks(m_Text, 0);
    if (begin == m_Text.size())
        return;

    std::size_t const prefixSize = MatchCommentPrefix(m_Text, begin, options.CommentPrefixes);
    if (prefixSize > 0)
    {
        m_Kind = ELineKind::Comment;
        ParseComment(begin + prefixSize);
        return;
    }

    if (m_Text[begin] == '[')
    {
        ParseSection(begin, options, problem);
        return;
    }

    ParseKeyValue(begin, options, problem);
}
//---------------------------------------------------------------------------
std::string_view TASWIniDocumentLine::GetComment() const noexcept
{
    if (!m_HasComment)
        return std::string_view();

    return GetPart(m_Comment);
}
//---------------------------------------------------------------------------
std::string_view TASWIniDocumentLine::GetKey() const noexcept
{
    if (m_Kind != ELineKind::KeyValue)
        return std::string_view();

    return GetPart(m_Name);
}
//---------------------------------------------------------------------------
ELineKind TASWIniDocumentLine::GetKind() const noexcept
{
    return m_Kind;
}
//---------------------------------------------------------------------------
ELineBreak TASWIniDocumentLine::GetLineBreak() const noexcept
{
    return m_LineBreak;
}
//---------------------------------------------------------------------------
std::string_view TASWIniDocumentLine::GetPart(TPart part) const noexcept
{
    return std::string_view(m_Text).substr(part.Begin, part.End - part.Begin);
}
//---------------------------------------------------------------------------
std::string_view TASWIniDocumentLine::GetSectionName() const noexcept
{
    if (m_Kind != ELineKind::Section)
        return std::string_view();

    return GetPart(m_Name);
}
//---------------------------------------------------------------------------
char TASWIniDocumentLine::GetSeparator() const noexcept
{
    if (m_Kind != ELineKind::KeyValue)
        return '\0';

    return m_Text[m_Separator];
}
//---------------------------------------------------------------------------
std::string const& TASWIniDocumentLine::GetText() const noexcept
{
    return m_Text;
}
//---------------------------------------------------------------------------
std::string_view TASWIniDocumentLine::GetValue() const noexcept
{
    if (m_Kind != ELineKind::KeyValue)
        return std::string_view();

    return GetPart(m_Value);
}
//---------------------------------------------------------------------------
bool TASWIniDocumentLine::HasComment() const noexcept
{
    return m_HasComment;
}
//---------------------------------------------------------------------------
bool TASWIniDocumentLine::IsQuoted() const noexcept
{
    return m_Kind == ELineKind::KeyValue && m_Quoted;
}
//---------------------------------------------------------------------------
void TASWIniDocumentLine::ParseComment(std::size_t textBegin)
{
    std::size_t const begin = SkipBlanks(m_Text, textBegin);

    m_HasComment = true;
    m_Comment.Begin = begin;
    m_Comment.End = TrimBlanksBefore(m_Text, begin, m_Text.size());
}
//---------------------------------------------------------------------------
void TASWIniDocumentLine::ParseKeyValue(std::size_t begin, TASWIniReadingOptions const& options, TProblem& problem)
{
    std::size_t const separator = m_Text.find_first_of(options.Separators, begin);
    if (separator == std::string::npos)
    {
        m_Kind = ELineKind::Raw;
        problem.Code = EIniCode::MalformedLine;
        problem.Text = "The line is neither a section header, a key with a value, a comment nor blank";
        return;
    }

    std::size_t const keyEnd = TrimBlanksBefore(m_Text, begin, separator);
    if (keyEnd == begin)
    {
        m_Kind = ELineKind::Raw;
        problem.Code = EIniCode::MalformedLine;
        problem.Text = "The line has a value but no key";
        return;
    }

    m_Kind = ELineKind::KeyValue;
    m_Name.Begin = begin;
    m_Name.End = keyEnd;
    m_Separator = separator;

    std::size_t const valueBegin = SkipBlanks(m_Text, separator + 1);
    if (options.QuotedValues && ParseQuotedValue(valueBegin, options))
        return;

    std::size_t valueEnd = m_Text.size();
    if (options.InlineComments)
    {
        std::size_t prefixSize = 0;
        std::size_t const comment = FindInlineComment(m_Text, valueBegin, options.CommentPrefixes, prefixSize);
        if (comment != std::string_view::npos)
        {
            valueEnd = comment;
            ParseComment(comment + prefixSize);
        }
    }

    m_Value.Begin = valueBegin;
    m_Value.End = TrimBlanksBefore(m_Text, valueBegin, valueEnd);
}
//---------------------------------------------------------------------------
bool TASWIniDocumentLine::ParseQuotedValue(std::size_t valueBegin, TASWIniReadingOptions const& options)
{
    if (valueBegin >= m_Text.size() || m_Text[valueBegin] != '"')
        return false;

    std::size_t const closingQuote = m_Text.find('"', valueBegin + 1);
    if (closingQuote == std::string::npos)
        return false;

    // After the closing quote, only blanks, or (with InlineComments) blanks and a comment
    std::size_t const after = SkipBlanks(m_Text, closingQuote + 1);
    if (after < m_Text.size())
    {
        if (!options.InlineComments || after == closingQuote + 1)
            return false;

        std::size_t const prefixSize = MatchCommentPrefix(m_Text, after, options.CommentPrefixes);
        if (prefixSize == 0)
            return false;

        ParseComment(after + prefixSize);
    }

    m_Value.Begin = valueBegin + 1;
    m_Value.End = closingQuote;
    m_Quoted = true;

    return true;
}
//---------------------------------------------------------------------------
void TASWIniDocumentLine::ParseSection(std::size_t begin, TASWIniReadingOptions const& options, TProblem& problem)
{
    std::size_t const closingBracket = m_Text.find(']', begin + 1);
    if (closingBracket == std::string::npos)
    {
        m_Kind = ELineKind::Raw;
        problem.Code = EIniCode::UnclosedSection;
        problem.Text = "The section header has no closing ']'";
        return;
    }

    std::size_t const nameBegin = SkipBlanks(m_Text, begin + 1);
    std::size_t const nameEnd = TrimBlanksBefore(m_Text, nameBegin, closingBracket);
    if (nameBegin == nameEnd)
    {
        m_Kind = ELineKind::Raw;
        problem.Code = EIniCode::MalformedLine;
        problem.Text = "The section header has no name";
        return;
    }

    m_Kind = ELineKind::Section;
    m_Name.Begin = nameBegin;
    m_Name.End = nameEnd;

    // After the ']', only blanks, or a comment
    std::size_t const after = SkipBlanks(m_Text, closingBracket + 1);
    if (after == m_Text.size())
        return;

    std::size_t const prefixSize = MatchCommentPrefix(m_Text, after, options.CommentPrefixes);
    if (prefixSize > 0)
    {
        ParseComment(after + prefixSize);
        return;
    }

    problem.Code = EIniCode::MalformedLine;
    problem.Text = "The section header is followed by text that isn't a comment";
}
//---------------------------------------------------------------------------


/////////////////////////////////////////////////////////////////////////////
// TASWIniDocumentSection
/////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------
std::vector<TASWIniDocumentLine> const& TASWIniDocumentSection::GetLines() const noexcept
{
    return m_Lines;
}
//---------------------------------------------------------------------------
std::string_view TASWIniDocumentSection::GetName() const noexcept
{
    if (!HasHeader())
        return std::string_view();

    return m_Lines.front().GetSectionName();
}
//---------------------------------------------------------------------------
bool TASWIniDocumentSection::HasHeader() const noexcept
{
    return !m_Lines.empty() && m_Lines.front().GetKind() == ELineKind::Section;
}
//---------------------------------------------------------------------------


/////////////////////////////////////////////////////////////////////////////
// TASWIniDocument
/////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------
TASWIniDocument::TASWIniDocument()
    : m_Sections(1),
      m_LineBreak(Detail::NativeLineBreak())
{
}
//---------------------------------------------------------------------------
void TASWIniDocument::ClearModified() noexcept
{
    m_Modified = false;
}
//---------------------------------------------------------------------------
ELineBreak TASWIniDocument::GetLineBreak() const noexcept
{
    return m_LineBreak;
}
//---------------------------------------------------------------------------
std::size_t TASWIniDocument::GetLineCount() const noexcept
{
    std::size_t count = 0;
    for (TASWIniDocumentSection const& section : m_Sections)
        count += section.GetLines().size();

    return count;
}
//---------------------------------------------------------------------------
TASWIniOptions const& TASWIniDocument::GetOptions() const noexcept
{
    return m_Options;
}
//---------------------------------------------------------------------------
std::vector<TASWIniDocumentSection> const& TASWIniDocument::GetSections() const noexcept
{
    return m_Sections;
}
//---------------------------------------------------------------------------
bool TASWIniDocument::HasBom() const noexcept
{
    return m_HasBom;
}
//---------------------------------------------------------------------------
bool TASWIniDocument::IsModified() const noexcept
{
    return m_Modified;
}
//---------------------------------------------------------------------------
TASWIniResult TASWIniDocument::Parse(std::string_view text, TASWIniOptions const& options,
    std::vector<TASWIniMessage>& messages)
{
    TASWIniResult result = options.Validate();
    if (!result)
        return result;

    TASWIniReadingOptions const& reading = options.Reading;

    Detail::EBom const bom = Detail::DetectBom(text);
    if (bom != Detail::EBom::None && bom != Detail::EBom::Utf8)
    {
        result.Code = EIniCode::UnsupportedEncoding;
        result.Message = "The text starts with a UTF-16 or UTF-32 byte order mark; only UTF-8 text can be read.";
        return result;
    }

    // Parsed into a new document, which replaces this one only when the whole text is parsed
    TASWIniDocument document;
    document.m_Options = options;
    document.m_HasBom = bom == Detail::EBom::Utf8;

    std::vector<Detail::TLineSpan> const lines = Detail::SplitLines(text.substr(Detail::BomSize(bom)));
    std::vector<TASWIniMessage> warnings;

    // The line each section and each key of the current section is first on. The names are views of 'text'.
    TNameLess const nameLess{ reading.CaseSensitive };
    std::map<std::string_view, std::size_t, TNameLess> sectionLines(nameLess);
    std::map<std::string_view, std::size_t, TNameLess> keyLines(nameLess);
    std::string_view sectionName;
    bool invalidUtf8Found = false;

    for (std::size_t i = 0; i < lines.size(); ++i)
    {
        Detail::TLineSpan const& span = lines[i];
        std::size_t const lineNumber = i + 1;

        TASWIniDocumentLine::TProblem problem;
        TASWIniDocumentLine line(span.Text, span.Break, reading, problem);

        if (!invalidUtf8Found && !Detail::IsValidUtf8(span.Text))
        {
            // A UTF-8 sequence can't hold a line break, so checking each line finds what checking the text would
            invalidUtf8Found = true;
            warnings.push_back(Warning(EIniCode::InvalidUtf8,
                "The line isn't valid UTF-8 (only the first such line is reported); its bytes are kept as they are.",
                lineNumber, sectionName, std::string_view()));
        }

        if (line.m_Kind == ELineKind::Section)
        {
            sectionName = span.Text.substr(line.m_Name.Begin, line.m_Name.End - line.m_Name.Begin);
            keyLines.clear();
            document.m_Sections.emplace_back();

            auto const inserted = sectionLines.emplace(sectionName, lineNumber);
            if (!inserted.second)
            {
                warnings.push_back(Warning(EIniCode::DuplicateSection, "The section " + Quoted(sectionName) +
                    " appears more than once (first on line " + std::to_string(inserted.first->second) +
                    "); each one is kept.", lineNumber, sectionName, std::string_view()));
            }
        }

        if (problem.Code != EIniCode::Ok)
        {
            if (reading.StrictParsing)
            {
                result.Code = EIniCode::ParseFailed;
                result.Message = std::string(problem.Text) + " (line " + std::to_string(lineNumber) +
                    "), and StrictParsing is set.";
                result.Line = lineNumber;
                return result;
            }

            std::string warningText = problem.Text;
            if (line.m_Kind == ELineKind::Raw)
                warningText += "; the line is kept as it is.";
            else
                warningText += "; the text is kept, and the section is used.";
            warnings.push_back(Warning(problem.Code, warningText, lineNumber, sectionName, std::string_view()));
        }

        if (line.m_Kind == ELineKind::KeyValue)
        {
            std::string_view const key = span.Text.substr(line.m_Name.Begin, line.m_Name.End - line.m_Name.Begin);

            auto const inserted = keyLines.emplace(key, lineNumber);
            if (!inserted.second)
            {
                char const* const winner = reading.DuplicateKeys == EDuplicateKeys::LastWins ? "last" : "first";
                warnings.push_back(Warning(EIniCode::DuplicateKey, "The key " + Quoted(key) +
                    " appears more than once in its section (first on line " +
                    std::to_string(inserted.first->second) + "); the " + winner + " one's value is used.",
                    lineNumber, sectionName, key));
            }
        }

        document.m_Sections.back().m_Lines.push_back(std::move(line));
    }

    ELineBreak const lineBreak = MostCommonLineBreak(lines);
    if (lineBreak != ELineBreak::None)
        document.m_LineBreak = lineBreak;

    // Only the reserve() can throw; the moves after it can't
    messages.reserve(messages.size() + warnings.size());
    *this = std::move(document);
    messages.insert(messages.end(), std::make_move_iterator(warnings.begin()),
        std::make_move_iterator(warnings.end()));

    return result;
}
//---------------------------------------------------------------------------
TASWIniResult TASWIniDocument::Reset(TASWIniOptions const& options)
{
    TASWIniResult result = options.Validate();
    if (!result)
        return result;

    // Made before this document changes, since 'options' may be its own
    TASWIniDocument document;
    document.m_Options = options;
    *this = std::move(document);

    return result;
}
//---------------------------------------------------------------------------
std::string TASWIniDocument::ToText() const
{
    TASWIniWritingOptions const& options = m_Options.Writing;

    bool writeBom = m_HasBom;
    if (options.Bom == EBomMode::Always)
        writeBom = true;
    else if (options.Bom == EBomMode::Never)
        writeBom = false;

    // Room for the mark and every line with a two-character line break, so the text is allocated once
    std::size_t size = Utf8Bom.size();
    for (TASWIniDocumentSection const& section : m_Sections)
    {
        for (TASWIniDocumentLine const& line : section.m_Lines)
            size += line.m_Text.size() + 2;
    }

    std::string text;
    text.reserve(size);

    if (writeBom)
        text += Utf8Bom;

    for (TASWIniDocumentSection const& section : m_Sections)
    {
        for (TASWIniDocumentLine const& line : section.m_Lines)
        {
            text += line.m_Text;
            text += Detail::LineBreakText(WrittenLineBreak(line.m_LineBreak, options.LineEnding));
        }
    }

    return text;
}
//---------------------------------------------------------------------------

} // namespace ASWIni
