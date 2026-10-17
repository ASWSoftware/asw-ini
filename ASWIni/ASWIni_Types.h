/* **************************************************************************
ASWIni_Types.h
Author: Anthony S. West - ASW Software

An INI file reader/writer library.

The types every part of the library shares: EIniCode (what happened),
ELineBreak, ESeverity, TASWIniMessage (a warning or error reported to the
application's log callback) and TASWIniResult (the outcome of an operation
such as a load or a save).

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

#ifndef ASWIni_TypesH
#define ASWIni_TypesH
//---------------------------------------------------------------------------
#include <cstddef>
#include <string>
//---------------------------------------------------------------------------

namespace ASWIni
{

// What happened, in a TASWIniResult or a TASWIniMessage. The values never change (an application may store or
// compare them), and new codes are added at the end. Whether a code is an error or a warning depends on where it is
// reported: a TASWIniResult only fails for an error, and a TASWIniMessage has its own Severity (e.g. MalformedLine is a
// warning, or an error with TASWIniOptions::StrictParsing).
enum class EIniCode : int
{
    Ok = 0,

    // Errors: the operation failed
    InvalidArgument = 1, // An argument is invalid, e.g. an empty file path
    InvalidOptions = 2, // TASWIniOptions::Validate() rejected the options
    NoFilePath = 3, // There is no file path to save to or reload from
    FileNotFound = 4, // The file, or a folder in its path, doesn't exist
    AccessDenied = 5, // The operating system denied access to the file or its folder
    FileTooLarge = 6, // The file is larger than TASWIniOptions::MaxFileSize
    OpenFailed = 7, // The file couldn't be opened (for another reason than the codes above)
    ReadFailed = 8, // Reading the file failed
    UnsupportedEncoding = 9, // The file starts with a UTF-16 or UTF-32 byte order mark
    ParseFailed = 10, // A line couldn't be parsed and TASWIniOptions::StrictParsing is set
    CreateFolderFailed = 11, // The file's folder doesn't exist and couldn't be created
    WriteFailed = 12, // Writing the file (or its temporary file) failed
    ReplaceFailed = 13, // The temporary file couldn't replace the file (an atomic save)
    UnexpectedException = 14, // An unexpected exception was caught, e.g. std::bad_alloc

    // Warnings: the operation went on
    MalformedLine = 15, // A line is neither a section, a key, a comment nor blank; it is kept as it is
    UnclosedSection = 16, // A section header has no closing ']'
    DuplicateKey = 17, // A key appears more than once in a section
    DuplicateSection = 18, // A section appears more than once
    InvalidUtf8 = 19, // The text isn't valid UTF-8; it is kept byte for byte
    BackupFailed = 20, // The backup copy couldn't be made; the file was saved anyway
    TempFileCleanupFailed = 21, // A temporary file of a failed save couldn't be deleted

    // Errors added after the warnings: an edit of a TASWIniDocument failed
    NotFound = 22, // The section or key to rename, or to set the comment of, doesn't exist
    AlreadyExists = 23, // A section or key can't be renamed to the name of another one that exists

    // Errors added for values: a value couldn't be converted to or from its text
    InvalidValue = 24, // The text isn't a value of the type, or the value can't be written as text
    ValueOutOfRange = 25, // The number is outside the range of its type (or of its setting)
};

// The line break that ends a line of an INI text
enum class ELineBreak : int
{
    None = 0, // The last line of a text that doesn't end with a line break
    LF = 1, // "\n" (Linux, and most files)
    CR = 2, // "\r" (classic Mac OS)
    CRLF = 3, // "\r\n" (Windows)
};

// How serious a TASWIniMessage is
enum class ESeverity : int
{
    Info = 0,
    Warning = 1,
    Error = 2,
};


/////////////////////////////////////////////////////////////////////////////
// TASWIniMessage
//
// A warning, error or note about a file, reported to the application's log callback. The text is a complete sentence;
// the other fields let an application filter or format messages its own way (see ToString()).
/////////////////////////////////////////////////////////////////////////////
struct TASWIniMessage
{
    ESeverity Severity = ESeverity::Info;
    EIniCode Code = EIniCode::Ok;
    std::string Text;
    std::string FilePath; // UTF-8; empty when the text wasn't loaded from or saved to a file
    std::size_t Line = 0; // 1-based; 0 when the message isn't about one line
    std::string Section; // Empty when the message isn't about a section, or is about the keys before the first one
    std::string Key; // Empty when the message isn't about a key
    int SystemError = 0; // The operating system's error code (GetLastError() or errno); 0 when there is none
};


/////////////////////////////////////////////////////////////////////////////
// TASWIniResult
//
// The outcome of an operation: Ok, or the error that made it fail, with a description. Warnings don't make an
// operation fail; they are reported as TASWIniMessage objects instead.
/////////////////////////////////////////////////////////////////////////////
struct [[nodiscard]] TASWIniResult
{
    EIniCode Code = EIniCode::Ok;
    std::string Message; // A complete sentence describing the error; empty when Code is Ok
    std::string FilePath; // UTF-8; empty when the operation wasn't about a file
    std::size_t Line = 0; // 1-based; 0 when the error isn't about one line
    int SystemError = 0; // The operating system's error code (GetLastError() or errno); 0 when there is none

    bool IsOk() const noexcept
    {
        return Code == EIniCode::Ok;
    }

    explicit operator bool() const noexcept
    {
        return IsOk();
    }
};

// The code's name, e.g. "FileNotFound"; "Unknown" for a value that isn't an EIniCode
char const* ToString(EIniCode code) noexcept;

// The line break's name: "None", "LF", "CR" or "CRLF"; "Unknown" for a value that isn't an ELineBreak
char const* ToString(ELineBreak lineBreak) noexcept;

// The severity's name: "Info", "Warning" or "Error"; "Unknown" for a value that isn't an ESeverity
char const* ToString(ESeverity severity) noexcept;

// The message as one line of text: the severity, the code and the text, followed by the parts that are set, e.g.
// Warning MalformedLine: The line has no separator; it is kept as it is. (file "app.ini", line 12, section "General")
std::string ToString(TASWIniMessage const& message);

} // namespace ASWIni

//---------------------------------------------------------------------------
#endif // #ifndef ASWIni_TypesH
