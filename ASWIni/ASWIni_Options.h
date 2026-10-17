/* **************************************************************************
ASWIni_Options.h
Author: Anthony S. West - ASW Software

An INI file reader/writer library.

TASWIniOptions: how INI text is read (separators, comments, duplicates),
written (separator, comment prefix, line ending, byte order mark) and saved
(atomically, with a backup, creating the folder), with Validate() to check
that the options describe a file that can be read back.

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
    Auto = 0, // The loaded file's (its most common one); the platform's for a new file
    Native = 1, // The platform's: CRLF on Windows, LF elsewhere
    LF = 2, // "\n"
    CRLF = 3, // "\r\n"
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
// TASWIniOptions
//
// How INI text is read, written and saved. The defaults read the common INI dialects (Windows, Python's configparser,
// most Linux tools) and write "key = value" lines and "; comment" lines. Options only affect what is read and what is
// newly written: a line read from a file is written back as it was, unless its value changes, and then it keeps its
// own separator and spacing.
/////////////////////////////////////////////////////////////////////////////
struct TASWIniOptions
{
    // The default MaxFileSize: 16 MiB, far more than a settings file needs
    static constexpr std::size_t DefaultMaxFileSize = 16 * 1024 * 1024;

    // ----- Reading -----

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

    // ----- Writing -----

    // Written between the key and the value of a new line. Must be one of Separators, with optional spaces or tabs
    // around it (e.g. "=", " = " or ": ").
    std::string Separator = " = ";

    // Written before the text of a new comment line, so a comment "Valid range: 0-4" is "; Valid range: 0-4". Must
    // start with one of CommentPrefixes.
    std::string CommentPrefix = "; ";

    // The line ending lines are written with
    ELineEnding LineEnding = ELineEnding::Auto;

    // Whether a saved file starts with a UTF-8 byte order mark
    EBomMode Bom = EBomMode::Auto;

    // Whether a new section is written after a blank line (unless it is the first thing in the file)
    bool BlankLineBeforeNewSection = true;

    // ----- Saving -----

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

    // Ok if these options can be used, else EIniCode::InvalidOptions with the first problem found, e.g. a Separator
    // that isn't one of the Separators (a file written with it couldn't be read back).
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
