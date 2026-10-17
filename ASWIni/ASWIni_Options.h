/* **************************************************************************
ASWIni_Options.h
Author: Anthony S. West - ASW Software

An INI file reader/writer library.

TASWIniOptions and its three groups: how INI text is read
(TASWIniReadingOptions: separators, comments, duplicates), written
(TASWIniWritingOptions: separator, comment prefix, line ending, byte order
mark) and saved (TASWIniSavingOptions: atomically, with a backup, creating
the folder), each with Validate() to check that the options describe a file
that can be read back.

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

#ifndef ASWIni_OptionsH
#define ASWIni_OptionsH
//---------------------------------------------------------------------------
#include <cstddef>
#include <string>
#include <vector>
//---------------------------------------------------------------------------
#include "ASWIni_Types.h"
//---------------------------------------------------------------------------

namespace ASWIni
{

// The line ending lines are written with
enum class ELineEnding : int
{
    Auto = 0, // Each line keeps its own; new lines get the file's most common one (the platform's for a new file)
    Native = 1, // Every line gets the platform's: CRLF on Windows, LF elsewhere
    LF = 2, // Every line gets "\n"
    CRLF = 3, // Every line gets "\r\n"
};

// Whether a saved file starts with a UTF-8 byte order mark
enum class EBomMode : int
{
    Auto = 0, // As the loaded file did; none for a new file
    Always = 1,
    Never = 2,
};

// Which value a key that appears more than once in a section has. Every line is kept either way, and the duplicate is
// reported as a warning (EIniCode::DuplicateKey).
enum class EDuplicateKeys : int
{
    FirstWins = 0, // The first line's (as the Windows profile API reads them)
    LastWins = 1, // The last line's
};


/////////////////////////////////////////////////////////////////////////////
// TASWIniReadingOptions
//
// How INI text is read: what separates a key from its value, what starts a comment, and how names and duplicates are
// matched. The defaults read the common INI dialects (Windows, Python's configparser, most Linux tools).
// InlineComments and QuotedValues also decide how values are written, so a written value reads back the same.
/////////////////////////////////////////////////////////////////////////////
struct TASWIniReadingOptions
{
    // The default MaxFileSize: 16 MiB, far more than a settings file needs
    static constexpr std::size_t DefaultMaxFileSize = 16 * 1024 * 1024;

    // The characters that can separate a key from its value. The first of them on a line ends the key, so with the
    // default "=:", "url = http://x" is the key "url" and the value "http://x".
    std::string Separators = "=:";

    // The texts that start a comment line (after any indentation). Add "//" for files that use it.
    std::vector<std::string> CommentPrefixes{ ";", "#" };

    // Whether a comment prefix after a value starts a comment ("key = value ; note"). Off by default, since values
    // such as connection strings can hold the prefixes. When on, the prefix must follow a space or tab.
    bool InlineComments = false;

    // Whether a value in double quotes ("  spaced value  ") keeps its spaces and loses its quotes when read, and is
    // written in quotes when it needs them (leading or trailing spaces, or a comment prefix with InlineComments).
    bool QuotedValues = false;

    // Whether section and key names are case-sensitive. When not (the default), they compare with the ASCII letters'
    // case ignored.
    bool CaseSensitive = false;

    // Which value a key that appears more than once in a section has
    EDuplicateKeys DuplicateKeys = EDuplicateKeys::FirstWins;

    // Whether a malformed line (e.g. a section header without its ']') fails the load (EIniCode::ParseFailed) instead
    // of being kept as it is with a warning
    bool StrictParsing = false;

    // The largest file a load reads, in bytes; a larger one fails with EIniCode::FileTooLarge
    std::size_t MaxFileSize = DefaultMaxFileSize;

    // Ok if these options can be used, else EIniCode::InvalidOptions with the first problem found, e.g. an empty
    // Separators
    TASWIniResult Validate() const;
};


/////////////////////////////////////////////////////////////////////////////
// TASWIniWritingOptions
//
// How new lines are written: the separator and comment prefix of lines that are added, and the line ending and byte
// order mark of the whole text. A line read from a file is written back as it was, unless its value changes, and then
// it keeps its own separator and spacing.
/////////////////////////////////////////////////////////////////////////////
struct TASWIniWritingOptions
{
    // Written between the key and the value of a new line: one of the reading options' Separators, with optional spaces
    // or tabs around it (e.g. "=", " = " or ": ")
    std::string Separator = " = ";

    // Written before the text of a new comment line, so a comment "Valid range: 0-4" is "; Valid range: 0-4". Must
    // start with one of the reading options' CommentPrefixes.
    std::string CommentPrefix = "; ";

    // The line ending lines are written with
    ELineEnding LineEnding = ELineEnding::Auto;

    // Whether a saved file starts with a UTF-8 byte order mark
    EBomMode Bom = EBomMode::Auto;

    // Whether a new section is written after a blank line (unless it is the first thing in the file)
    bool BlankLineBeforeNewSection = true;

    // Ok if these options can be used on their own, else EIniCode::InvalidOptions with the first problem found, e.g. a
    // Separator of two characters. Whether Separator and CommentPrefix match the reading options is checked by
    // TASWIniOptions::Validate().
    TASWIniResult Validate() const;
};


/////////////////////////////////////////////////////////////////////////////
// TASWIniSavingOptions
//
// How a file is saved: atomically or in place, with or without a backup, and whether a missing folder is created
/////////////////////////////////////////////////////////////////////////////
struct TASWIniSavingOptions
{
    // Whether a save writes a temporary file in the same folder and then replaces the file with it, so a failure or a
    // crash while saving leaves the previous file whole. Turn it off only when the folder can't be written to (the file
    // itself is then overwritten in place).
    bool AtomicSave = true;

    // Whether a save first copies the previous file to the same name followed by BackupSuffix (e.g. "app.ini.bak"),
    // replacing an older backup. A backup that fails is reported as a warning (EIniCode::BackupFailed); the file is
    // still saved.
    bool Backup = false;

    // Added to the file name for the backup
    std::string BackupSuffix = ".bak";

    // Whether a save creates the file's folder (and the folders above it) when it doesn't exist
    bool CreateFolders = true;

    // Ok if these options can be used, else EIniCode::InvalidOptions with the first problem found, e.g. an empty
    // BackupSuffix with Backup on
    TASWIniResult Validate() const;
};


/////////////////////////////////////////////////////////////////////////////
// TASWIniOptions
//
// How INI text is read, written and saved, e.g. options.Writing.Separator = "=". The defaults read the common INI
// dialects and write "key = value" lines and "; comment" lines.
/////////////////////////////////////////////////////////////////////////////
struct TASWIniOptions
{
    TASWIniReadingOptions Reading;
    TASWIniWritingOptions Writing;
    TASWIniSavingOptions Saving;

    // Ok if these options can be used, else EIniCode::InvalidOptions with the first problem found: each group's own
    // (see their Validate()), then whether a file written with them could be read back (Writing.Separator must be one
    // of Reading.Separators, and Writing.CommentPrefix must start with one of Reading.CommentPrefixes).
    TASWIniResult Validate() const;
};

//---------------------------------------------------------------------------

// The value's name, e.g. "CRLF"; "Unknown" for a value that isn't one of the enumerators
char const* ToString(EBomMode bom) noexcept;
char const* ToString(EDuplicateKeys duplicateKeys) noexcept;
char const* ToString(ELineEnding lineEnding) noexcept;

} // namespace ASWIni

//---------------------------------------------------------------------------
#endif // #ifndef ASWIni_OptionsH
