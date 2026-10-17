/* **************************************************************************
ASWIni_Document.h
Author: Anthony S. West - ASW Software

An INI file reader/writer library.

TASWIniDocument, the lossless model of an INI text: its sections in file
order (TASWIniDocumentSection), each with its lines (TASWIniDocumentLine),
each line with its original text, its line break and the parts the parser
found in it, so a text that is read and written back without changes is the
same byte for byte. Parse() reads a text into it and keeps the options it was
read with, which ToText() writes it back with. Not thread-safe; copyable.

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
// malformed lines, duplicate keys and sections, and each line's own line break. It keeps its options: the lines'
// parts depend on the reading options, so they only change when the document is parsed again or reset. Not
// thread-safe (TASWBaseIni wraps one with a lock); copyable.
/////////////////////////////////////////////////////////////////////////////
class TASWIniDocument
{
private:
    std::vector<TASWIniDocumentSection> m_Sections; // Never empty: the preamble first
    TASWIniOptions m_Options; // Valid (TASWIniOptions::Validate() accepts them)
    ELineBreak m_LineBreak;
    bool m_HasBom = false;
    bool m_Modified = false;

private:
    // Adds 'section' at the end, after a blank line when Writing.BlankLineBeforeNewSection says so, and sets the
    // modified flag. Throws only std::bad_alloc, and then nothing changes.
    void AppendSection(TASWIniDocumentSection& section);

    // Gives the document's last line a line break (NewLineBreak()) when it has none
    void EndLastLine() noexcept;

    // The number of lines of the file comment: the comment lines at the start of the file, when a blank line or the
    // end of the file follows them (comment lines that lead straight into the first header are its comment); 0 when
    // there is none
    std::size_t FileCommentSize() const noexcept;

    // The index of the first section named 'name' (0 for ""), or m_Sections.size() when there is none
    std::size_t FindSectionIndex(std::string_view name) const noexcept;

    // Inserts 'lines' before line 'lineIndex' of section 'sectionIndex' (giving the line before them a line break
    // when they go at the end of the document), and sets the modified flag. Throws only std::bad_alloc, and then
    // nothing changes.
    void InsertLines(std::size_t sectionIndex, std::size_t lineIndex, std::vector<TASWIniDocumentLine>& lines);

    // In 'line', a new line of key 'key' with value 'value', written after 'indentation' with Writing.Separator (and
    // in double quotes when it needs them, with QuotedValues). Fails with InvalidArgument when the key or the value
    // wouldn't read back as it is.
    TASWIniResult MakeKeyLine(std::string_view section, std::string_view key, std::string_view value,
        std::string_view indentation, TASWIniDocumentLine& line) const;

    // 'text' parsed as a line that ends with 'lineBreak', with the reading options
    TASWIniDocumentLine MakeLine(std::string_view text, ELineBreak lineBreak) const;

    // In 'section', a new section with only its header, named 'name'. Fails with InvalidArgument when the header
    // wouldn't read back as 'name'.
    TASWIniResult MakeSection(std::string_view name, TASWIniDocumentSection& section) const;

    // In 'changed', key line 'line' of section 'section' with its value changed to 'value' and everything else kept.
    // Fails with InvalidArgument when the value wouldn't read back as it is.
    TASWIniResult MakeValueLine(TASWIniDocumentLine const& line, std::string_view section, std::string_view value,
        TASWIniDocumentLine& changed) const;

    // Where a new key of section 'sectionIndex' goes (see SetValue()): the index of the line it goes before, the
    // indentation it copies (a view of a line's text), and whether a blank line goes before it
    std::size_t NewKeyIndex(std::size_t sectionIndex, std::string_view& indentation, bool& blankLineFirst) const;

    // The line break of new lines: GetLineBreak(), or the one Writing.LineEnding converts every line to
    ELineBreak NewLineBreak() const noexcept;

    // Whether 'before', 'value' and 'after', as a line, read back as key line 'line' with the value 'value' (the same
    // key and comment); if they do, that line replaces 'line'. 'value' is tried in double quotes when 'quotedFirst',
    // else without them, and then, with QuotedValues, the other way. A value with a line break never reads back.
    bool TryValueLine(std::string_view before, std::string_view value, std::string_view after, bool quotedFirst,
        TASWIniDocumentLine& line) const;

public:
    // An empty document with the default options: an empty preamble, no byte order mark, and the platform's line
    // break
    TASWIniDocument();

    // Adds an empty section named 'name' at the end of the document, after a blank line when
    // Writing.BlankLineBeforeNewSection is set and the document has lines and doesn't end with a blank one. Does
    // nothing when a section of that name exists ("" is the preamble, which always does). Fails with InvalidArgument,
    // and then nothing changes, when the header wouldn't read back as 'name': a name can't be empty, start or end
    // with a space or tab, or hold ']' or a line break. Throws only std::bad_alloc, and then nothing changes either.
    TASWIniResult AddSection(std::string_view name);

    // Clears the modified flag (see IsModified()), e.g. after the document was saved
    void ClearModified() noexcept;

    // The line of key 'key' of section 'section' ("" for the keys before the first header), or nullptr when there is
    // none. Names compare as Reading.CaseSensitive says. Sections of the same name count as one: the key is looked for
    // in each of them, in file order, and when it is there more than once, Reading.DuplicateKeys picks the first or
    // the last line. The pointer is valid until the document changes.
    TASWIniDocumentLine const* FindKey(std::string_view section, std::string_view key) const;

    // The first section named 'name' (compared as Reading.CaseSensitive says), or nullptr when there is none; "" is
    // the preamble, which every document has. The pointer is valid until the document changes.
    TASWIniDocumentSection const* FindSection(std::string_view name) const;

    // The keys of section 'section' ("" for the keys before the first header), each once, in file order, as the first
    // of its lines writes it. Sections of the same name count as one. Empty when the section doesn't exist or has no
    // keys.
    std::vector<std::string> GetKeyNames(std::string_view section) const;

    // The line break new lines get with ELineEnding::Auto: the most common one of the text that was read (the one
    // found first in a tie), or the platform's (CRLF on Windows, LF elsewhere) when there was none
    ELineBreak GetLineBreak() const noexcept;

    // The number of lines in all the sections, headers included
    std::size_t GetLineCount() const noexcept;

    // The options the document was parsed or reset with (the defaults for a new document)
    TASWIniOptions const& GetOptions() const noexcept;

    // The names of the sections that have a header (so not the preamble), each once, in file order, as the first of
    // its headers writes it. With 'prefix', only the names that start with it (compared as Reading.CaseSensitive
    // says), e.g. "Window." for [Window.Main] and [Window.Tools]; a dot is an ordinary character in a name.
    std::vector<std::string> GetSectionNames(std::string_view prefix = std::string_view()) const;

    // The sections in file order: the preamble (with no header and the name "") first, then one for each header
    std::vector<TASWIniDocumentSection> const& GetSections() const noexcept;

    // Whether the text that was read started with a UTF-8 byte order mark (it isn't part of any line)
    bool HasBom() const noexcept;

    // Whether an edit changed the document's text since it was created, parsed or reset, or since ClearModified().
    // An edit that leaves the text as it was doesn't set it.
    bool IsModified() const noexcept;

    // Reads 'text' (UTF-8, with or without a byte order mark; lines end with LF, CRLF or CR) into this document,
    // replacing what it held, as 'options' say (their Reading group), and keeps 'options' for ToText() and the edits.
    // Every line is kept as it is. The warnings (MalformedLine and UnclosedSection for a line that can't be parsed,
    // DuplicateSection, DuplicateKey, and one InvalidUtf8 for the first line that isn't valid UTF-8) are added to
    // 'messages', each with its line number (1-based) and section. Clears the modified flag. Fails with
    // InvalidOptions (see TASWIniOptions::Validate()), UnsupportedEncoding (a UTF-16 or UTF-32 byte order mark) or,
    // with StrictParsing, ParseFailed for the first line that can't be parsed (its number in Line); then neither the
    // document nor 'messages' changes. Throws only std::bad_alloc, and then nothing changes either.
    TASWIniResult Parse(std::string_view text, TASWIniOptions const& options, std::vector<TASWIniMessage>& messages);

    // Removes every line of key 'key' from every section named 'section' ("" for the keys before the first header),
    // each with its comment (the comment lines directly above it), so no duplicate of the key is left. Returns whether
    // anything was removed (and then sets the modified flag). Throws only std::bad_alloc, and then nothing changes.
    bool RemoveKey(std::string_view section, std::string_view key);

    // Removes every section named 'name', each with all its lines and its comment (the comment lines directly above
    // its header). The comment of the section after it (the comment lines at its end) is kept, unless that section is
    // removed too. The preamble ("") is never removed. Returns whether anything was removed (and then sets the
    // modified flag). Throws only std::bad_alloc, and then nothing changes.
    bool RemoveSection(std::string_view name);

    // Renames key 'key' of section 'section' ("" for the keys before the first header) to 'newKey' in every line of
    // it, in every section of that name, changing only the name: the indentation, spacing, value and comments stay.
    // 'newKey' may differ from 'key' only in case. Fails with NotFound when there is no such key, InvalidArgument when
    // a line with 'newKey' wouldn't read back as it is (see SetValue()), or AlreadyExists when the section has
    // another key named 'newKey'; then nothing changes. Throws only std::bad_alloc, and then nothing changes either.
    TASWIniResult RenameKey(std::string_view section, std::string_view key, std::string_view newKey);

    // Renames section 'name' to 'newName' in every header of that name, changing only the name: the spacing and a
    // comment after the header stay. 'newName' may differ from 'name' only in case. Fails with InvalidArgument when
    // either name is "" (the preamble has no header) or the header wouldn't read back as 'newName' (see
    // AddSection()), NotFound when there is no such section, or AlreadyExists when another section is named
    // 'newName'; then nothing changes. Throws only std::bad_alloc, and then nothing changes either.
    TASWIniResult RenameSection(std::string_view name, std::string_view newName);

    // Empties this document, as a new one is (see TASWIniDocument()), and gives it 'options', which may be its own
    // (Reset(GetOptions()) only empties it). Clears the modified flag. Fails with InvalidOptions (see
    // TASWIniOptions::Validate()), and then nothing changes. Throws only std::bad_alloc, and then nothing changes
    // either.
    TASWIniResult Reset(TASWIniOptions const& options);

    // Sets the value of key 'key' of section 'section' ("" for the keys before the first header). Of a key that
    // exists, only the value of the line FindKey() finds changes: its indentation, key, separator, spacing and inline
    // comment stay, a quoted value stays quoted, and an empty value gets the blanks written before the separator.
    // A missing key is added after the last key of the first section of that name, with that key's indentation (or
    // right after the header, or in a preamble with no keys after the file comment and the blank lines under it), as
    // indentation, key, Writing.Separator and value; a missing section is added first, as AddSection() does. With
    // QuotedValues, a value is written in double quotes when it needs them to read back (blanks at either end, a
    // leading '"', or with InlineComments a comment prefix after a blank). Setting the value a key has changes
    // nothing. Fails with InvalidArgument, and then nothing changes, when a name or the value wouldn't read back as
    // it is: e.g. a line break, a key that is empty, holds a separator or starts with a comment prefix or '[', blanks
    // around a name, or a value that needs quotes without QuotedValues (or holds a '"' that would end them). Throws
    // only std::bad_alloc, and then nothing changes either.
    TASWIniResult SetValue(std::string_view section, std::string_view key, std::string_view value);

    // The document as text, as its writing options say: a UTF-8 byte order mark (with EBomMode::Auto, when the text
    // that was read had one), then every line followed by its line break. With ELineEnding::Auto each line keeps its
    // own line break; LF, CRLF and Native write every line's with that one instead. A last line that has no line break
    // never gets one. With the default options, a parsed text is written back byte for byte. Throws only
    // std::bad_alloc.
    std::string ToText() const;
};

//---------------------------------------------------------------------------

// The line kind's name, e.g. "KeyValue"; "Unknown" for a value that isn't an ELineKind
char const* ToString(ELineKind kind) noexcept;

} // namespace ASWIni

//---------------------------------------------------------------------------
#endif // #ifndef ASWIni_DocumentH
