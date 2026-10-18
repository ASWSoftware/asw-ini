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
#include <set>
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

std::size_t CommentBlockBegin(std::vector<TASWIniDocumentLine> const& lines, std::size_t end) noexcept;
std::string DoubledQuotes(std::string_view text);
TASWIniResult Failure(EIniCode code, std::string const& message);
std::size_t FindClosingQuote(std::string_view text, std::size_t from) noexcept;
std::size_t FindInlineComment(std::string_view text, std::size_t from, std::vector<std::string> const& prefixes,
    std::size_t& prefixSize) noexcept;
bool HasLineBreak(std::string_view text) noexcept;
bool HasNamePrefix(std::string_view name, std::string_view prefix, bool caseSensitive) noexcept;
TASWIniResult InvalidKeyName(std::string_view key);
TASWIniResult InvalidSectionName(std::string_view name);
TASWIniResult InvalidValue(std::string_view section, std::string_view key, std::string_view value);
std::string JoinedComments(std::vector<TASWIniDocumentLine> const& lines, std::size_t begin, std::size_t end);
std::size_t MatchCommentPrefix(std::string_view text, std::size_t pos,
    std::vector<std::string> const& prefixes) noexcept;
ELineBreak MostCommonLineBreak(std::vector<Detail::TLineSpan> const& lines) noexcept;
std::string Quoted(std::string_view name);
template <typename T>
void RemoveMarked(std::vector<T>& items, std::vector<bool> const& marks);
bool SameName(std::string_view left, std::string_view right, bool caseSensitive) noexcept;
std::size_t SkipBlanks(std::string_view text, std::size_t pos) noexcept;
std::size_t TrimBlanksBefore(std::string_view text, std::size_t begin, std::size_t end) noexcept;
std::string UndoubledQuotes(std::string_view text);
TASWIniMessage Warning(EIniCode code, std::string const& text, std::size_t line, std::string_view section,
    std::string_view key);
ELineBreak WrittenLineBreak(ELineBreak lineBreak, ELineEnding lineEnding) noexcept;

//---------------------------------------------------------------------------
/*
    CommentBlockBegin

    The index of the first of the comment lines directly before line 'end' of 'lines' (or before their end, when
    'end' is their size), with no other line between them; 'end' when the line before it isn't a comment.
*/
std::size_t CommentBlockBegin(std::vector<TASWIniDocumentLine> const& lines, std::size_t end) noexcept
{
    std::size_t begin = end;
    while (begin > 0 && lines[begin - 1].GetKind() == ELineKind::Comment)
        --begin;

    return begin;
}
//---------------------------------------------------------------------------
/*
    DoubledQuotes

    'text' with each '"' in it written as "", as it goes between the double quotes of a quoted value.
*/
std::string DoubledQuotes(std::string_view text)
{
    std::string doubled;
    doubled.reserve(text.size());
    for (char const c : text)
    {
        if (c == '"')
            doubled += '"';
        doubled += c;
    }

    return doubled;
}
//---------------------------------------------------------------------------
/*
    Failure

    A failed result with 'code' and 'message'.
*/
TASWIniResult Failure(EIniCode code, std::string const& message)
{
    TASWIniResult result;
    result.Code = code;
    result.Message = message;

    return result;
}
//---------------------------------------------------------------------------
/*
    FindClosingQuote

    The position of the '"' that closes a quoted value whose text starts at 'from' in 'text': the first '"' that isn't
    one of a pair ("" stands for one '"'); std::string_view::npos when there is none.
*/
std::size_t FindClosingQuote(std::string_view text, std::size_t from) noexcept
{
    std::size_t quote = text.find('"', from);
    while (quote != std::string_view::npos && quote + 1 < text.size() && text[quote + 1] == '"')
        quote = text.find('"', quote + 2);

    return quote;
}
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
    HasLineBreak

    Whether 'text' holds a CR or an LF, so it can't be (part of) a single line.
*/
bool HasLineBreak(std::string_view text) noexcept
{
    return text.find_first_of("\r\n") != std::string_view::npos;
}
//---------------------------------------------------------------------------
/*
    HasNamePrefix

    Whether section or key name 'name' starts with 'prefix', byte by byte or with the ASCII letters' case ignored.
*/
bool HasNamePrefix(std::string_view name, std::string_view prefix, bool caseSensitive) noexcept
{
    if (name.size() < prefix.size())
        return false;

    return SameName(name.substr(0, prefix.size()), prefix, caseSensitive);
}
//---------------------------------------------------------------------------
/*
    InvalidKeyName

    The InvalidArgument result of key name 'key', which can't be written in a line that reads back as it is.
*/
TASWIniResult InvalidKeyName(std::string_view key)
{
    return Failure(EIniCode::InvalidArgument, "The key name " + Quoted(key) + " can't be written as a line that "
        "reads back as it is: a key can't be empty, start or end with a space or tab, start with a comment prefix or "
        "'[', or hold a separator or a line break.");
}
//---------------------------------------------------------------------------
/*
    InvalidSectionName

    The InvalidArgument result of section name 'name', which can't be written as a header that reads back as it is.
*/
TASWIniResult InvalidSectionName(std::string_view name)
{
    return Failure(EIniCode::InvalidArgument, "The section name " + Quoted(name) + " can't be written as a header "
        "that reads back as it is: a section name can't be empty, start or end with a space or tab, or hold ']' or a "
        "line break.");
}
//---------------------------------------------------------------------------
/*
    InvalidValue

    The InvalidArgument result of value 'value' of key 'key' of section 'section', which can't be written so that it
    reads back as it is.
*/
TASWIniResult InvalidValue(std::string_view section, std::string_view key, std::string_view value)
{
    std::string const what = "The value of the key " + Quoted(key) + " of the section " + Quoted(section);
    if (HasLineBreak(value))
        return Failure(EIniCode::InvalidArgument, what + " holds a line break; a value must be one line.");

    return Failure(EIniCode::InvalidArgument, what + " can't be written so that it reads back as it is: blanks at "
        "either end, a leading '\"' and, with InlineComments, a comment prefix after a blank need QuotedValues.");
}
//---------------------------------------------------------------------------
/*
    JoinedComments

    The comments of lines 'begin' up to 'end' of 'lines' (see TASWIniDocumentLine::GetComment()), joined with "\n".
*/
std::string JoinedComments(std::vector<TASWIniDocumentLine> const& lines, std::size_t begin, std::size_t end)
{
    std::string text;
    for (std::size_t i = begin; i < end; ++i)
    {
        if (i > begin)
            text += '\n';
        text += lines[i].GetComment();
    }

    return text;
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
    RemoveMarked

    Removes the items of 'items' whose mark in 'marks' is set, keeping the others in order. Moves the items, so it
    can't throw for items whose moves can't (such as lines and sections).
*/
template <typename T>
void RemoveMarked(std::vector<T>& items, std::vector<bool> const& marks)
{
    std::size_t kept = 0;
    for (std::size_t i = 0; i < items.size(); ++i)
    {
        if (marks[i])
            continue;

        if (kept != i)
            items[kept] = std::move(items[i]);
        ++kept;
    }

    items.erase(items.begin() + static_cast<std::ptrdiff_t>(kept), items.end());
}
//---------------------------------------------------------------------------
/*
    SameName

    Whether section or key names 'left' and 'right' are the same, byte by byte or with the ASCII letters' case ignored.
*/
bool SameName(std::string_view left, std::string_view right, bool caseSensitive) noexcept
{
    if (caseSensitive)
        return left == right;

    return Detail::EqualsNoCase(left, right);
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
    UndoubledQuotes

    'text', the text between the double quotes of a quoted value, with each "" in it as one '"'.
*/
std::string UndoubledQuotes(std::string_view text)
{
    std::string undoubled;
    undoubled.reserve(text.size());
    for (std::size_t i = 0; i < text.size(); ++i)
    {
        undoubled += text[i];
        if (text[i] == '"')
            ++i;
    }

    return undoubled;
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

    if (!m_UnquotedValue.empty())
        return m_UnquotedValue;

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

    std::size_t const closingQuote = FindClosingQuote(m_Text, valueBegin + 1);
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

    // Doubled quotes are made single once, so GetValue() can return a view
    std::string_view const value = GetPart(m_Value);
    if (value.find('"') != std::string_view::npos)
        m_UnquotedValue = UndoubledQuotes(value);

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
TASWIniResult TASWIniDocument::AddSection(std::string_view name)
{
    if (FindSectionIndex(name) < m_Sections.size())
        return TASWIniResult();

    TASWIniDocumentSection section;
    TASWIniResult const result = MakeSection(name, section);
    if (!result)
        return result;

    AppendSection(section);

    return result;
}
//---------------------------------------------------------------------------
void TASWIniDocument::AppendSection(TASWIniDocumentSection& section)
{
    // The document's last line, when it has one, is in its last section: only the preamble can have no lines
    bool const blankLineFirst = m_Options.Writing.BlankLineBeforeNewSection && !m_Sections.back().m_Lines.empty() &&
        m_Sections.back().m_Lines.back().m_Kind != ELineKind::Blank;
    TASWIniDocumentLine blankLine = MakeLine(std::string_view(), NewLineBreak());

    // Room is made first, so nothing after it can throw
    m_Sections.reserve(m_Sections.size() + 1);
    std::vector<TASWIniDocumentLine>& lastLines = m_Sections.back().m_Lines;
    if (blankLineFirst)
        lastLines.reserve(lastLines.size() + 1);

    EndLastLine();
    if (blankLineFirst)
        lastLines.push_back(std::move(blankLine));
    m_Sections.push_back(std::move(section));
    FixMergingLineBreaks();
    m_Modified = true;
}
//---------------------------------------------------------------------------
void TASWIniDocument::ClearModified() noexcept
{
    m_Modified = false;
}
//---------------------------------------------------------------------------
void TASWIniDocument::EndLastLine() noexcept
{
    // Only the last line can have no line break, and it is in the last section (only the preamble can have no lines)
    std::vector<TASWIniDocumentLine>& lines = m_Sections.back().m_Lines;
    if (!lines.empty() && lines.back().m_LineBreak == ELineBreak::None)
        lines.back().m_LineBreak = NewLineBreak();
}
//---------------------------------------------------------------------------
std::size_t TASWIniDocument::FileCommentSize() const noexcept
{
    std::vector<TASWIniDocumentLine> const& lines = m_Sections.front().m_Lines;

    std::size_t size = 0;
    while (size < lines.size() && lines[size].m_Kind == ELineKind::Comment)
        ++size;

    if (size < lines.size())
        return lines[size].m_Kind == ELineKind::Blank ? size : 0;

    // The comment lines are the whole preamble: they are the file comment only when no header follows them
    return m_Sections.size() == 1 ? size : 0;
}
//---------------------------------------------------------------------------
TASWIniDocumentLine const* TASWIniDocument::FindKey(std::string_view section, std::string_view key) const
{
    std::size_t sectionIndex = 0;
    std::size_t lineIndex = 0;
    if (!FindKeyLine(section, key, sectionIndex, lineIndex))
        return nullptr;

    return &m_Sections[sectionIndex].m_Lines[lineIndex];
}
//---------------------------------------------------------------------------
void TASWIniDocument::FixMergingLineBreaks() noexcept
{
    TASWIniDocumentLine* previous = nullptr;
    for (TASWIniDocumentSection& section : m_Sections)
    {
        for (TASWIniDocumentLine& line : section.m_Lines)
        {
            if (previous != nullptr && previous->m_LineBreak == ELineBreak::CR && line.m_Text.empty() &&
                line.m_LineBreak == ELineBreak::LF)
            {
                previous->m_LineBreak = ELineBreak::CRLF;
            }

            previous = &line;
        }
    }
}
//---------------------------------------------------------------------------
bool TASWIniDocument::FindKeyLine(std::string_view section, std::string_view key, std::size_t& sectionIndex,
    std::size_t& lineIndex) const noexcept
{
    bool const caseSensitive = m_Options.Reading.CaseSensitive;
    bool const lastWins = m_Options.Reading.DuplicateKeys == EDuplicateKeys::LastWins;

    bool found = false;
    for (std::size_t s = 0; s < m_Sections.size(); ++s)
    {
        if (!SameName(m_Sections[s].GetName(), section, caseSensitive))
            continue;

        std::vector<TASWIniDocumentLine> const& lines = m_Sections[s].m_Lines;
        for (std::size_t i = 0; i < lines.size(); ++i)
        {
            if (lines[i].m_Kind != ELineKind::KeyValue || !SameName(lines[i].GetKey(), key, caseSensitive))
                continue;

            sectionIndex = s;
            lineIndex = i;
            if (!lastWins)
                return true;

            found = true;
        }
    }

    return found;
}
//---------------------------------------------------------------------------
TASWIniDocumentSection const* TASWIniDocument::FindSection(std::string_view name) const
{
    std::size_t const index = FindSectionIndex(name);
    if (index == m_Sections.size())
        return nullptr;

    return &m_Sections[index];
}
//---------------------------------------------------------------------------
std::size_t TASWIniDocument::FindSectionIndex(std::string_view name) const noexcept
{
    for (std::size_t index = 0; index < m_Sections.size(); ++index)
    {
        if (SameName(m_Sections[index].GetName(), name, m_Options.Reading.CaseSensitive))
            return index;
    }

    return m_Sections.size();
}
//---------------------------------------------------------------------------
std::string TASWIniDocument::GetFileComment() const
{
    return JoinedComments(m_Sections.front().m_Lines, 0, FileCommentSize());
}
//---------------------------------------------------------------------------
std::string TASWIniDocument::GetKeyComment(std::string_view section, std::string_view key) const
{
    std::size_t sectionIndex = 0;
    std::size_t lineIndex = 0;
    if (!FindKeyLine(section, key, sectionIndex, lineIndex))
        return std::string();

    std::vector<TASWIniDocumentLine> const& lines = m_Sections[sectionIndex].m_Lines;

    return JoinedComments(lines, CommentBlockBegin(lines, lineIndex), lineIndex);
}
//---------------------------------------------------------------------------
std::vector<std::string> TASWIniDocument::GetKeyNames(std::string_view section) const
{
    bool const caseSensitive = m_Options.Reading.CaseSensitive;

    std::vector<std::string> names;
    std::set<std::string_view, TNameLess> found(TNameLess{ caseSensitive });
    for (TASWIniDocumentSection const& documentSection : m_Sections)
    {
        if (!SameName(documentSection.GetName(), section, caseSensitive))
            continue;

        for (TASWIniDocumentLine const& line : documentSection.m_Lines)
        {
            if (line.m_Kind == ELineKind::KeyValue && found.insert(line.GetKey()).second)
                names.emplace_back(line.GetKey());
        }
    }

    return names;
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
std::string TASWIniDocument::GetSectionComment(std::string_view section) const
{
    // The comment of a section is at the end of the section before it
    std::size_t const sectionIndex = FindSectionIndex(section);
    if (sectionIndex == 0 || sectionIndex == m_Sections.size())
        return std::string();

    std::vector<TASWIniDocumentLine> const& lines = m_Sections[sectionIndex - 1].m_Lines;

    return JoinedComments(lines, CommentBlockBegin(lines, lines.size()), lines.size());
}
//---------------------------------------------------------------------------
std::vector<std::string> TASWIniDocument::GetSectionNames(std::string_view prefix) const
{
    bool const caseSensitive = m_Options.Reading.CaseSensitive;

    std::vector<std::string> names;
    std::set<std::string_view, TNameLess> found(TNameLess{ caseSensitive });
    for (TASWIniDocumentSection const& section : m_Sections)
    {
        std::string_view const name = section.GetName();
        if (section.HasHeader() && HasNamePrefix(name, prefix, caseSensitive) && found.insert(name).second)
            names.emplace_back(name);
    }

    return names;
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
TASWIniResult TASWIniDocument::MakeCommentLines(std::string_view text, std::string_view indentation,
    std::vector<TASWIniDocumentLine>& lines) const
{
    std::string const& prefix = m_Options.Writing.CommentPrefix;
    std::string_view const emptyLinePrefix = Detail::TrimRight(prefix);

    lines.clear();
    for (Detail::TLineSpan const& span : Detail::SplitLines(text))
    {
        std::string_view const comment = Detail::Trim(span.Text);
        std::string lineText(indentation);
        lineText += comment.empty() ? emptyLinePrefix : std::string_view(prefix);
        lineText += comment;

        TASWIniDocumentLine line = MakeLine(lineText, NewLineBreak());
        if (line.m_Kind != ELineKind::Comment || line.GetComment() != comment)
        {
            return Failure(EIniCode::InvalidArgument, "The comment line " + Quoted(comment) + " can't be written "
                "after the comment prefix " + Quoted(prefix) + " so that it reads back as it is.");
        }

        lines.push_back(std::move(line));
    }

    return TASWIniResult();
}
//---------------------------------------------------------------------------
TASWIniResult TASWIniDocument::MakeKeyLine(std::string_view section, std::string_view key, std::string_view value,
    std::string_view indentation, TASWIniDocumentLine& line) const
{
    // The key with an empty value first: without the blanks after the separator, so "key =" rather than "key = "
    std::string const before = std::string(indentation) + std::string(key) + m_Options.Writing.Separator;
    TASWIniDocumentLine keyLine = MakeLine(std::string_view(before).substr(0,
        TrimBlanksBefore(before, indentation.size() + key.size(), before.size())), NewLineBreak());
    if (HasLineBreak(key) || keyLine.m_Kind != ELineKind::KeyValue || keyLine.GetKey() != key ||
        !keyLine.GetValue().empty())
    {
        return InvalidKeyName(key);
    }

    if (!value.empty() && !TryValueLine(before, value, std::string_view(), false, keyLine))
        return InvalidValue(section, key, value);

    line = std::move(keyLine);

    return TASWIniResult();
}
//---------------------------------------------------------------------------
TASWIniDocumentLine TASWIniDocument::MakeLine(std::string_view text, ELineBreak lineBreak) const
{
    // A line that can't be parsed is made all the same: the callers check its parts
    TASWIniDocumentLine::TProblem problem;

    return TASWIniDocumentLine(text, lineBreak, m_Options.Reading, problem);
}
//---------------------------------------------------------------------------
TASWIniResult TASWIniDocument::MakeSection(std::string_view name, TASWIniDocumentSection& section) const
{
    TASWIniDocumentLine header = MakeLine("[" + std::string(name) + "]", NewLineBreak());
    if (HasLineBreak(name) || header.m_Kind != ELineKind::Section || header.GetSectionName() != name)
        return InvalidSectionName(name);

    section.m_Lines.clear();
    section.m_Lines.push_back(std::move(header));

    return TASWIniResult();
}
//---------------------------------------------------------------------------
TASWIniResult TASWIniDocument::MakeValueLine(TASWIniDocumentLine const& line, std::string_view section,
    std::string_view value, TASWIniDocumentLine& changed) const
{
    // The value is replaced with its quotes, when it has them
    std::string_view const text = line.m_Text;
    std::size_t begin = line.m_Value.Begin;
    std::size_t end = line.m_Value.End;
    if (line.m_Quoted)
    {
        --begin;
        ++end;
    }

    std::string before(text.substr(0, begin));
    std::string after(text.substr(end));
    if (begin == end)
    {
        // An empty value right after the separator gets the blanks written before the separator ("key =" ->
        // "key = value"), and a comment right after an empty value gets a blank before it ("key = ; note" ->
        // "key = value ; note")
        if (begin == line.m_Separator + 1)
            before += text.substr(line.m_Name.End, line.m_Separator - line.m_Name.End);
        if (!after.empty())
            after.insert(0, 1, ' ');
    }

    // A quoted value stays quoted when it can; an unquoted one is quoted only when it has to be
    TASWIniDocumentLine result = line;
    if (!TryValueLine(before, value, after, line.m_Quoted, result))
        return InvalidValue(section, line.GetKey(), value);

    changed = std::move(result);

    return TASWIniResult();
}
//---------------------------------------------------------------------------
std::size_t TASWIniDocument::NewKeyIndex(std::size_t sectionIndex, std::string_view& indentation,
    bool& blankLineFirst) const
{
    std::vector<TASWIniDocumentLine> const& lines = m_Sections[sectionIndex].m_Lines;
    indentation = std::string_view();
    blankLineFirst = false;

    // After the last key, with its indentation
    for (std::size_t i = lines.size(); i > 0; --i)
    {
        TASWIniDocumentLine const& line = lines[i - 1];
        if (line.m_Kind == ELineKind::KeyValue)
        {
            indentation = std::string_view(line.m_Text).substr(0, line.m_Name.Begin);
            return i;
        }
    }

    // Right after the header
    if (sectionIndex > 0)
        return 1;

    // In a preamble with no keys: after the file comment and the blank lines under it, or after a new blank line
    // when there are none, so the file comment isn't read back as the key's comment
    std::size_t index = FileCommentSize();
    if (index == 0)
        return 0;

    if (index == lines.size())
    {
        blankLineFirst = true;
        return index;
    }

    while (index < lines.size() && lines[index].m_Kind == ELineKind::Blank)
        ++index;

    return index;
}
//---------------------------------------------------------------------------
ELineBreak TASWIniDocument::NewLineBreak() const noexcept
{
    return WrittenLineBreak(m_LineBreak, m_Options.Writing.LineEnding);
}
//---------------------------------------------------------------------------
TASWIniResult TASWIniDocument::Parse(std::string_view text, TASWIniOptions const& options,
    std::vector<TASWIniMessage>& messages)
{
    TASWIniResult result = options.Validate();
    if (!result)
        return result;

    Detail::EBom const bom = Detail::DetectBom(text);
    if (bom != Detail::EBom::None && bom != Detail::EBom::Utf8)
    {
        result.Code = EIniCode::UnsupportedEncoding;
        result.Message = "The text starts with a UTF-16 or UTF-32 byte order mark; only UTF-8 text can be read.";
        return result;
    }

    return ParseLines(text.substr(Detail::BomSize(bom)), bom == Detail::EBom::Utf8, options, messages);
}
//---------------------------------------------------------------------------
TASWIniResult TASWIniDocument::ParseLines(std::string_view text, bool hasBom, TASWIniOptions const& options,
    std::vector<TASWIniMessage>& messages)
{
    TASWIniResult result;
    TASWIniReadingOptions const& reading = options.Reading;

    // Parsed into a new document, which replaces this one only when the whole text is parsed
    TASWIniDocument document;
    document.m_Options = options;
    document.m_HasBom = hasBom;

    std::vector<Detail::TLineSpan> const lines = Detail::SplitLines(text);
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
bool TASWIniDocument::RemoveKey(std::string_view section, std::string_view key)
{
    bool const caseSensitive = m_Options.Reading.CaseSensitive;

    // The lines are marked first, since only that can throw: each line of the key and its comment
    std::vector<std::vector<bool> > marks(m_Sections.size());
    bool found = false;
    for (std::size_t sectionIndex = 0; sectionIndex < m_Sections.size(); ++sectionIndex)
    {
        std::vector<TASWIniDocumentLine> const& lines = m_Sections[sectionIndex].m_Lines;
        if (!SameName(m_Sections[sectionIndex].GetName(), section, caseSensitive))
            continue;

        for (std::size_t i = 0; i < lines.size(); ++i)
        {
            if (lines[i].m_Kind != ELineKind::KeyValue || !SameName(lines[i].GetKey(), key, caseSensitive))
                continue;

            if (marks[sectionIndex].empty())
                marks[sectionIndex].resize(lines.size());

            for (std::size_t j = CommentBlockBegin(lines, i); j <= i; ++j)
                marks[sectionIndex][j] = true;

            found = true;
        }
    }

    if (!found)
        return false;

    for (std::size_t sectionIndex = 0; sectionIndex < m_Sections.size(); ++sectionIndex)
    {
        if (!marks[sectionIndex].empty())
            RemoveMarked(m_Sections[sectionIndex].m_Lines, marks[sectionIndex]);
    }

    FixMergingLineBreaks();
    m_Modified = true;

    return true;
}
//---------------------------------------------------------------------------
bool TASWIniDocument::RemoveSection(std::string_view name)
{
    // The preamble (section 0) is never removed: no header has the name ""
    std::size_t const count = m_Sections.size();
    std::vector<bool> removed(count);
    bool found = false;
    for (std::size_t sectionIndex = 1; sectionIndex < count; ++sectionIndex)
    {
        if (SameName(m_Sections[sectionIndex].GetName(), name, m_Options.Reading.CaseSensitive))
        {
            removed[sectionIndex] = true;
            found = true;
        }
    }

    if (!found)
        return false;

    // What can throw comes first. A removed section's comment is at the end of the section before it (unless that one
    // is removed too). The comment of a kept section after it is at its own end, and moves to the last kept section
    // before it, which gets room for it here.
    std::vector<std::vector<bool> > lineMarks(count);
    std::vector<std::size_t> movedLines(count);
    std::size_t lastKept = 0;
    for (std::size_t sectionIndex = 1; sectionIndex < count; ++sectionIndex)
    {
        if (!removed[sectionIndex])
        {
            lastKept = sectionIndex;
            continue;
        }

        std::vector<TASWIniDocumentLine> const& before = m_Sections[sectionIndex - 1].m_Lines;
        std::size_t const commentBegin = CommentBlockBegin(before, before.size());
        if (!removed[sectionIndex - 1] && commentBegin < before.size())
        {
            lineMarks[sectionIndex - 1].resize(before.size());
            for (std::size_t i = commentBegin; i < before.size(); ++i)
                lineMarks[sectionIndex - 1][i] = true;
        }

        std::vector<TASWIniDocumentLine> const& lines = m_Sections[sectionIndex].m_Lines;
        if (sectionIndex + 1 < count && !removed[sectionIndex + 1])
            movedLines[lastKept] += lines.size() - CommentBlockBegin(lines, lines.size());
    }

    for (std::size_t sectionIndex = 0; sectionIndex < count; ++sectionIndex)
    {
        std::vector<TASWIniDocumentLine>& lines = m_Sections[sectionIndex].m_Lines;
        if (movedLines[sectionIndex] > 0)
            lines.reserve(lines.size() + movedLines[sectionIndex]);
    }

    // Then the moves, which can't throw
    lastKept = 0;
    for (std::size_t sectionIndex = 1; sectionIndex < count; ++sectionIndex)
    {
        if (!removed[sectionIndex])
        {
            lastKept = sectionIndex;
            continue;
        }

        if (!lineMarks[sectionIndex - 1].empty())
            RemoveMarked(m_Sections[sectionIndex - 1].m_Lines, lineMarks[sectionIndex - 1]);

        std::vector<TASWIniDocumentLine>& lines = m_Sections[sectionIndex].m_Lines;
        if (sectionIndex + 1 < count && !removed[sectionIndex + 1])
        {
            std::vector<TASWIniDocumentLine>& target = m_Sections[lastKept].m_Lines;
            for (std::size_t i = CommentBlockBegin(lines, lines.size()); i < lines.size(); ++i)
                target.push_back(std::move(lines[i]));
        }
    }

    RemoveMarked(m_Sections, removed);
    FixMergingLineBreaks();
    m_Modified = true;

    return true;
}
//---------------------------------------------------------------------------
TASWIniResult TASWIniDocument::RenameKey(std::string_view section, std::string_view key, std::string_view newKey)
{
    bool const caseSensitive = m_Options.Reading.CaseSensitive;

    std::vector<TASWIniDocumentLine*> lines;
    bool newKeyFound = false;
    for (TASWIniDocumentSection& documentSection : m_Sections)
    {
        if (!SameName(documentSection.GetName(), section, caseSensitive))
            continue;

        for (TASWIniDocumentLine& line : documentSection.m_Lines)
        {
            if (line.m_Kind != ELineKind::KeyValue)
                continue;

            if (SameName(line.GetKey(), key, caseSensitive))
                lines.push_back(&line);
            else if (SameName(line.GetKey(), newKey, caseSensitive))
                newKeyFound = true;
        }
    }

    if (lines.empty())
    {
        return Failure(EIniCode::NotFound, "The key " + Quoted(key) + " of the section " + Quoted(section) +
            " can't be renamed: it doesn't exist.");
    }

    // The renamed lines are made first, so a name that doesn't read back in one of them changes nothing
    std::vector<TASWIniDocumentLine> renamed;
    renamed.reserve(lines.size());
    for (TASWIniDocumentLine const* const line : lines)
    {
        std::string_view const text = line->m_Text;
        std::string const newText = std::string(text.substr(0, line->m_Name.Begin)) + std::string(newKey) +
            std::string(text.substr(line->m_Name.End));
        TASWIniDocumentLine changed = MakeLine(newText, line->m_LineBreak);
        if (HasLineBreak(newKey) || changed.m_Kind != ELineKind::KeyValue || changed.GetKey() != newKey ||
            changed.GetValue() != line->GetValue() || changed.IsQuoted() != line->IsQuoted() ||
            changed.HasComment() != line->HasComment() || changed.GetComment() != line->GetComment())
        {
            return InvalidKeyName(newKey);
        }

        renamed.push_back(std::move(changed));
    }

    if (newKeyFound)
    {
        return Failure(EIniCode::AlreadyExists, "The key " + Quoted(key) + " of the section " + Quoted(section) +
            " can't be renamed to " + Quoted(newKey) + ": the section has a key of that name.");
    }

    for (std::size_t i = 0; i < lines.size(); ++i)
    {
        if (renamed[i].m_Text == lines[i]->m_Text)
            continue;

        *lines[i] = std::move(renamed[i]);
        m_Modified = true;
    }

    return TASWIniResult();
}
//---------------------------------------------------------------------------
TASWIniResult TASWIniDocument::RenameSection(std::string_view name, std::string_view newName)
{
    if (name.empty())
    {
        return Failure(EIniCode::InvalidArgument,
            "The section \"\" (the lines before the first header) has no header, so it can't be renamed.");
    }

    std::vector<TASWIniDocumentLine*> headers;
    for (std::size_t sectionIndex = 1; sectionIndex < m_Sections.size(); ++sectionIndex)
    {
        if (SameName(m_Sections[sectionIndex].GetName(), name, m_Options.Reading.CaseSensitive))
            headers.push_back(&m_Sections[sectionIndex].m_Lines.front());
    }

    if (headers.empty())
        return Failure(EIniCode::NotFound, "The section " + Quoted(name) + " can't be renamed: it doesn't exist.");

    // The renamed headers are made first, so a name that doesn't read back in one of them changes nothing
    std::vector<TASWIniDocumentLine> renamed;
    renamed.reserve(headers.size());
    for (TASWIniDocumentLine const* const header : headers)
    {
        std::string_view const text = header->m_Text;
        std::string const newText = std::string(text.substr(0, header->m_Name.Begin)) + std::string(newName) +
            std::string(text.substr(header->m_Name.End));
        TASWIniDocumentLine changed = MakeLine(newText, header->m_LineBreak);
        if (HasLineBreak(newName) || changed.m_Kind != ELineKind::Section || changed.GetSectionName() != newName ||
            changed.HasComment() != header->HasComment() || changed.GetComment() != header->GetComment())
        {
            return InvalidSectionName(newName);
        }

        renamed.push_back(std::move(changed));
    }

    if (!SameName(name, newName, m_Options.Reading.CaseSensitive) && FindSectionIndex(newName) < m_Sections.size())
    {
        return Failure(EIniCode::AlreadyExists, "The section " + Quoted(name) + " can't be renamed to " +
            Quoted(newName) + ": a section of that name exists.");
    }

    for (std::size_t i = 0; i < headers.size(); ++i)
    {
        if (renamed[i].m_Text == headers[i]->m_Text)
            continue;

        *headers[i] = std::move(renamed[i]);
        m_Modified = true;
    }

    return TASWIniResult();
}
//---------------------------------------------------------------------------
TASWIniResult TASWIniDocument::ReplaceComment(std::size_t sectionIndex, std::size_t begin, std::size_t end,
    std::string_view text, std::string_view indentation, bool fileComment)
{
    std::vector<TASWIniDocumentLine> lines;
    TASWIniResult const result = MakeCommentLines(text, indentation, lines);
    if (!result)
        return result;

    // A comment that reads the same stays as it is, even when its lines are written another way
    std::vector<TASWIniDocumentLine> const& current = m_Sections[sectionIndex].m_Lines;
    bool same = lines.size() == end - begin;
    for (std::size_t i = 0; same && i < lines.size(); ++i)
        same = lines[i].GetComment() == current[begin + i].GetComment();

    if (same)
        return result;

    // A file comment is followed by a blank line, which goes with it, unless the comment lines after it would then be
    // read as the file comment
    bool const blankLineFollows = end < current.size() && current[end].m_Kind == ELineKind::Blank;
    if (fileComment && lines.empty() && blankLineFollows)
    {
        std::size_t next = end + 1;
        while (next < current.size() && current[next].m_Kind == ELineKind::Comment)
            ++next;

        bool const nextIsFileComment = next > end + 1 &&
            (next < current.size() ? current[next].m_Kind == ELineKind::Blank : m_Sections.size() == 1);
        if (!nextIsFileComment)
            ++end;
    }
    else if (fileComment && begin == end && !blankLineFollows)
    {
        lines.push_back(MakeLine(std::string_view(), NewLineBreak()));
    }

    ReplaceLines(sectionIndex, begin, end, lines);

    return result;
}
//---------------------------------------------------------------------------
void TASWIniDocument::ReplaceLines(std::size_t sectionIndex, std::size_t begin, std::size_t end,
    std::vector<TASWIniDocumentLine>& lines)
{
    std::vector<TASWIniDocumentLine>& target = m_Sections[sectionIndex].m_Lines;

    // Room is made first, so nothing after it can throw
    target.reserve(target.size() - (end - begin) + lines.size());

    if (sectionIndex + 1 == m_Sections.size() && begin == target.size())
        EndLastLine();

    std::ptrdiff_t const first = static_cast<std::ptrdiff_t>(begin);
    target.erase(target.begin() + first, target.begin() + static_cast<std::ptrdiff_t>(end));
    target.insert(target.begin() + first, std::make_move_iterator(lines.begin()), std::make_move_iterator(lines.end()));
    FixMergingLineBreaks();
    m_Modified = true;
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
TASWIniResult TASWIniDocument::SetFileComment(std::string_view text)
{
    return ReplaceComment(0, 0, FileCommentSize(), text, std::string_view(), true);
}
//---------------------------------------------------------------------------
TASWIniResult TASWIniDocument::SetKeyComment(std::string_view section, std::string_view key, std::string_view text)
{
    std::size_t sectionIndex = 0;
    std::size_t lineIndex = 0;
    if (!FindKeyLine(section, key, sectionIndex, lineIndex))
    {
        return Failure(EIniCode::NotFound, "The comment of the key " + Quoted(key) + " of the section " +
            Quoted(section) + " can't be set: the key doesn't exist.");
    }

    // Directly above the key, with its indentation
    std::vector<TASWIniDocumentLine> const& lines = m_Sections[sectionIndex].m_Lines;
    TASWIniDocumentLine const& line = lines[lineIndex];

    return ReplaceComment(sectionIndex, CommentBlockBegin(lines, lineIndex), lineIndex, text,
        std::string_view(line.m_Text).substr(0, line.m_Name.Begin), false);
}
//---------------------------------------------------------------------------
TASWIniResult TASWIniDocument::SetOptions(TASWIniOptions const& options, std::vector<TASWIniMessage>& messages)
{
    TASWIniResult result = options.Validate();
    if (!result)
        return result;

    // Each line with its own line break, and the byte order mark passed on as a flag, so a first line that starts with
    // the mark's bytes keeps them
    ELineBreak const lineBreak = m_LineBreak;
    bool const modified = m_Modified;
    result = ParseLines(WriteText(false, ELineEnding::Auto), m_HasBom, options, messages);
    if (!result)
        return result;

    // The text is the same, so what was found in it before still holds
    m_LineBreak = lineBreak;
    m_Modified = modified;

    return result;
}
//---------------------------------------------------------------------------
TASWIniResult TASWIniDocument::SetSectionComment(std::string_view section, std::string_view text)
{
    if (section.empty())
    {
        return Failure(EIniCode::InvalidArgument, "The section \"\" (the lines before the first header) has no "
            "header to put a comment above; SetFileComment() sets the comment at the start of the file.");
    }

    std::size_t const sectionIndex = FindSectionIndex(section);
    if (sectionIndex == m_Sections.size())
    {
        return Failure(EIniCode::NotFound, "The comment of the section " + Quoted(section) + " can't be set: the "
            "section doesn't exist.");
    }

    // At the end of the section before it, with the header's indentation
    std::vector<TASWIniDocumentLine> const& lines = m_Sections[sectionIndex - 1].m_Lines;
    std::string const& header = m_Sections[sectionIndex].m_Lines.front().m_Text;

    return ReplaceComment(sectionIndex - 1, CommentBlockBegin(lines, lines.size()), lines.size(), text,
        std::string_view(header).substr(0, SkipBlanks(header, 0)), false);
}
//---------------------------------------------------------------------------
TASWIniResult TASWIniDocument::SetValue(std::string_view section, std::string_view key, std::string_view value)
{
    std::size_t keySectionIndex = 0;
    std::size_t keyLineIndex = 0;
    if (FindKeyLine(section, key, keySectionIndex, keyLineIndex))
    {
        TASWIniDocumentLine& found = m_Sections[keySectionIndex].m_Lines[keyLineIndex];
        if (found.GetValue() == value)
            return TASWIniResult();

        TASWIniDocumentLine changed;
        TASWIniResult const result = MakeValueLine(found, section, value, changed);
        if (!result)
            return result;

        found = std::move(changed);
        m_Modified = true;

        return result;
    }

    std::size_t const sectionIndex = FindSectionIndex(section);
    if (sectionIndex == m_Sections.size())
    {
        // A new section at the end, with the key right after its header
        TASWIniDocumentSection newSection;
        TASWIniResult result = MakeSection(section, newSection);
        if (!result)
            return result;

        TASWIniDocumentLine line;
        result = MakeKeyLine(section, key, value, std::string_view(), line);
        if (!result)
            return result;

        newSection.m_Lines.push_back(std::move(line));
        AppendSection(newSection);

        return result;
    }

    std::string_view indentation;
    bool blankLineFirst = false;
    std::size_t const lineIndex = NewKeyIndex(sectionIndex, indentation, blankLineFirst);

    std::vector<TASWIniDocumentLine> lines;
    if (blankLineFirst)
        lines.push_back(MakeLine(std::string_view(), NewLineBreak()));
    lines.emplace_back();

    TASWIniResult const result = MakeKeyLine(section, key, value, indentation, lines.back());
    if (!result)
        return result;

    ReplaceLines(sectionIndex, lineIndex, lineIndex, lines);

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

    return WriteText(writeBom, options.LineEnding);
}
//---------------------------------------------------------------------------
bool TASWIniDocument::TryValueLine(std::string_view before, std::string_view value, std::string_view after,
    bool quotedFirst, TASWIniDocumentLine& line) const
{
    if (HasLineBreak(value))
        return false;

    for (bool const quoted : { quotedFirst, !quotedFirst })
    {
        if (quoted && !m_Options.Reading.QuotedValues)
            continue;

        std::string text(before);
        if (quoted)
            text += '"' + DoubledQuotes(value) + '"';
        else
            text += value;
        text += after;

        TASWIniDocumentLine changed = MakeLine(text, line.m_LineBreak);
        if (changed.m_Kind == ELineKind::KeyValue && changed.GetKey() == line.GetKey() &&
            changed.GetValue() == value && changed.HasComment() == line.HasComment() &&
            changed.GetComment() == line.GetComment())
        {
            line = std::move(changed);
            return true;
        }
    }

    return false;
}
//---------------------------------------------------------------------------
std::string TASWIniDocument::WriteText(bool bom, ELineEnding lineEnding) const
{
    // Room for the mark and every line with a two-character line break, so the text is allocated once
    std::size_t size = Utf8Bom.size();
    for (TASWIniDocumentSection const& section : m_Sections)
    {
        for (TASWIniDocumentLine const& line : section.m_Lines)
            size += line.m_Text.size() + 2;
    }

    std::string text;
    text.reserve(size);

    if (bom)
        text += Utf8Bom;

    for (TASWIniDocumentSection const& section : m_Sections)
    {
        for (TASWIniDocumentLine const& line : section.m_Lines)
        {
            text += line.m_Text;
            text += Detail::LineBreakText(WrittenLineBreak(line.m_LineBreak, lineEnding));
        }
    }

    return text;
}
//---------------------------------------------------------------------------

} // namespace ASWIni
