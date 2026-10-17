/* **************************************************************************
ASWIni_Document.h
Author: Anthony S. West - ASW Software

An INI file reader/writer library.

TASWIniDocument, the lossless model of an INI text: its sections in file
order (TASWIniDocumentSection), each with its lines (TASWIniDocumentLine),
each line with its original text, its line break and the parts the parser
found in it, so a text that is read and written back without changes is the
same byte for byte. Parse() reads a text into it. Not thread-safe; copyable.

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

#ifndef ASWIni_DocumentH
#define ASWIni_DocumentH
//---------------------------------------------------------------------------
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>
//---------------------------------------------------------------------------
#include "ASWIni_Options.h"
#include "ASWIni_Types.h"
//---------------------------------------------------------------------------

namespace ASWIni
{

// What a line of an INI text is
enum class ELineKind : int
{
    Blank = 0, // Empty, or only spaces and tabs
    Comment = 1, // A comment: one of the reading options' CommentPrefixes, after any spaces and tabs
    Section = 2, // A section header, e.g. "[General]"
    KeyValue = 3, // A key and its value, e.g. "Width = 640"
    Raw = 4, // A line that couldn't be parsed (reported as a warning); it is kept as it is
};


/////////////////////////////////////////////////////////////////////////////
// TASWIniDocumentLine
//
// One line of an INI text: its text (without its line break), its line break, its kind and the parts the parser found
// in it. The parts are kept as positions in the text, so a line is written back byte for byte, and a line whose value
// changes keeps its indentation, separator, spacing and comment. The std::string_view results point into the line's
// text: they are valid until the line is changed or destroyed.
/////////////////////////////////////////////////////////////////////////////
class TASWIniDocumentLine
{
private:
    friend class TASWIniDocument;

    // A part of the line's text: the characters from Begin up to (not including) End
    struct TPart
    {
        std::size_t Begin = 0;
        std::size_t End = 0;
    };

    // What is wrong with a line that was parsed: MalformedLine or UnclosedSection, and what the problem is (e.g. "The
    // line has an empty key"); Ok when nothing is
    struct TProblem
    {
        EIniCode Code = EIniCode::Ok;
        char const* Text = "";
    };

    std::string m_Text;
    ELineBreak m_LineBreak = ELineBreak::None;
    ELineKind m_Kind = ELineKind::Blank;

    // The parts, each set only for the kinds that have it
    TPart m_Name; // Section, KeyValue: the section's name or the key, without the spaces and tabs around it
    std::size_t m_Separator = 0; // KeyValue: the separator's position
    TPart m_Value; // KeyValue: the value, without the spaces and tabs around it, and without its quotes when quoted
    bool m_Quoted = false; // KeyValue: whether the value is in double quotes (with QuotedValues)
    bool m_HasComment = false; // Comment, Section, KeyValue: whether the line has a comment
    TPart m_Comment; // The comment's text, after its prefix, without the spaces and tabs around it

private:
    // Parses 'text', a line of an INI text that ended with 'lineBreak', as 'options' say. A line that can't be parsed
    // is kept as a Raw line, and 'problem' says why; so does a section header followed by other text than a comment,
    // which is still a Section line.
    TASWIniDocumentLine(std::string_view text, ELineBreak lineBreak, TASWIniReadingOptions const& options,
        TProblem& problem);

    std::string_view GetPart(TPart part) const noexcept;

    // Sets the comment, whose text (after its prefix) starts at 'textBegin'
    void ParseComment(std::size_t textBegin);

    // Parses the line as a key and its value, whose key starts at 'begin' (after the indentation)
    void ParseKeyValue(std::size_t begin, TASWIniReadingOptions const& options, TProblem& problem);

    // Whether the value, which starts at 'valueBegin', is a quoted value (with QuotedValues); if it is, sets it
    bool ParseQuotedValue(std::size_t valueBegin, TASWIniReadingOptions const& options);

    // Parses the line as a section header, whose '[' is at 'begin'
    void ParseSection(std::size_t begin, TASWIniReadingOptions const& options, TProblem& problem);

public:
    // A blank line with no line break
    TASWIniDocumentLine() = default;

    // The comment's text, after its prefix and without the spaces and tabs around it: the whole comment of a Comment
    // line, or the comment after a section header ("[General] ; note") or, with InlineComments, after a value. Empty
    // when the line has no comment (see HasComment()).
    std::string_view GetComment() const noexcept;

    // The key, without the spaces and tabs around it; empty unless the line is a KeyValue
    std::string_view GetKey() const noexcept;

    ELineKind GetKind() const noexcept;

    // The line break that ended the line in the text (None for a last line that has none)
    ELineBreak GetLineBreak() const noexcept;

    // The name between '[' and ']', without the spaces and tabs around it; empty unless the line is a Section
    std::string_view GetSectionName() const noexcept;

    // The character that separates the key from the value, as written ('=' or ':' with the default options); '\0'
    // unless the line is a KeyValue
    char GetSeparator() const noexcept;

    // The line's text as it was read, without its line break
    std::string const& GetText() const noexcept;

    // The value, without the spaces and tabs around it, without its inline comment (with InlineComments) and without
    // its double quotes when it is quoted (with QuotedValues); empty unless the line is a KeyValue
    std::string_view GetValue() const noexcept;

    // Whether the line has a comment: it is a Comment line, or a Section or KeyValue line followed by a comment
    bool HasComment() const noexcept;

    // Whether the value was written in double quotes (only with QuotedValues; the quotes aren't part of GetValue())
    bool IsQuoted() const noexcept;
};


/////////////////////////////////////////////////////////////////////////////
// TASWIniDocumentSection
//
// A section of an INI text: its header line ("[General]") and the lines after it, up to the next header, in file
// order. The first section of a document is its preamble: the lines before the first header (often none), with no
// header and the name "". Duplicate sections are separate TASWIniDocumentSection objects.
/////////////////////////////////////////////////////////////////////////////
class TASWIniDocumentSection
{
private:
    friend class TASWIniDocument;

    std::vector<TASWIniDocumentLine> m_Lines; // The header first, when the section has one

public:
    // A section with no header and no lines: an empty preamble
    TASWIniDocumentSection() = default;

    // The section's lines in file order: its header first (when it has one), then every line up to the next header
    std::vector<TASWIniDocumentLine> const& GetLines() const noexcept;

    // The section's name as its header writes it, without the spaces and tabs around it; "" for the preamble
    std::string_view GetName() const noexcept;

    // Whether the section starts with a header line; false only for the preamble
    bool HasHeader() const noexcept;
};


/////////////////////////////////////////////////////////////////////////////
// TASWIniDocument
//
// An INI text as sections of lines, in file order, keeping every line as it was read: comments, blank lines,
// malformed lines, duplicate keys and sections, and each line's own line break. Not thread-safe (TASWBaseIni wraps one
// with a lock); copyable.
/////////////////////////////////////////////////////////////////////////////
class TASWIniDocument
{
private:
    std::vector<TASWIniDocumentSection> m_Sections; // Never empty: the preamble first
    ELineBreak m_LineBreak;
    bool m_HasBom = false;

public:
    // An empty document: an empty preamble, no byte order mark, and the platform's line break
    TASWIniDocument();

    // The line break new lines get with ELineEnding::Auto: the most common one of the text that was read (the one
    // found first in a tie), or the platform's (CRLF on Windows, LF elsewhere) when there was none
    ELineBreak GetLineBreak() const noexcept;

    // The number of lines in all the sections, headers included
    std::size_t GetLineCount() const noexcept;

    // The sections in file order: the preamble (with no header and the name "") first, then one for each header
    std::vector<TASWIniDocumentSection> const& GetSections() const noexcept;

    // Whether the text that was read started with a UTF-8 byte order mark (it isn't part of any line)
    bool HasBom() const noexcept;

    // Reads 'text' (UTF-8, with or without a byte order mark; lines end with LF, CRLF or CR) into this document,
    // replacing what it held, as 'options' say. Every line is kept as it is. The warnings (MalformedLine and
    // UnclosedSection for a line that can't be parsed, DuplicateSection, DuplicateKey, and one InvalidUtf8 for the
    // first line that isn't valid UTF-8) are added to 'messages', each with its line number (1-based) and section.
    // Fails with InvalidOptions (see TASWIniReadingOptions::Validate()), UnsupportedEncoding (a UTF-16 or UTF-32 byte
    // order mark) or, with StrictParsing, ParseFailed for the first line that can't be parsed (its number in Line);
    // then neither the document nor 'messages' changes. Throws only std::bad_alloc, and then nothing changes either.
    TASWIniResult Parse(std::string_view text, TASWIniReadingOptions const& options,
        std::vector<TASWIniMessage>& messages);
};

//---------------------------------------------------------------------------

// The line kind's name, e.g. "KeyValue"; "Unknown" for a value that isn't an ELineKind
char const* ToString(ELineKind kind) noexcept;

} // namespace ASWIni

//---------------------------------------------------------------------------
#endif // #ifndef ASWIni_DocumentH
