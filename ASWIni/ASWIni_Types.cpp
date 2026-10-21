/* **************************************************************************
ASWIni_Types.cpp
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
#include "ASWIni_Types.h"
//---------------------------------------------------------------------------

namespace ASWIni
{

//---------------------------------------------------------------------------
char const* ToString(EIniCode code) noexcept
{
    switch (code)
    {
        case EIniCode::Ok:
            return "Ok";

        case EIniCode::InvalidArgument:
            return "InvalidArgument";

        case EIniCode::InvalidOptions:
            return "InvalidOptions";

        case EIniCode::NoFilePath:
            return "NoFilePath";

        case EIniCode::FileNotFound:
            return "FileNotFound";

        case EIniCode::AccessDenied:
            return "AccessDenied";

        case EIniCode::FileTooLarge:
            return "FileTooLarge";

        case EIniCode::OpenFailed:
            return "OpenFailed";

        case EIniCode::ReadFailed:
            return "ReadFailed";

        case EIniCode::UnsupportedEncoding:
            return "UnsupportedEncoding";

        case EIniCode::ParseFailed:
            return "ParseFailed";

        case EIniCode::CreateFolderFailed:
            return "CreateFolderFailed";

        case EIniCode::WriteFailed:
            return "WriteFailed";

        case EIniCode::ReplaceFailed:
            return "ReplaceFailed";

        case EIniCode::UnexpectedException:
            return "UnexpectedException";

        case EIniCode::MalformedLine:
            return "MalformedLine";

        case EIniCode::UnclosedSection:
            return "UnclosedSection";

        case EIniCode::DuplicateKey:
            return "DuplicateKey";

        case EIniCode::DuplicateSection:
            return "DuplicateSection";

        case EIniCode::InvalidUtf8:
            return "InvalidUtf8";

        case EIniCode::BackupFailed:
            return "BackupFailed";

        case EIniCode::TempFileCleanupFailed:
            return "TempFileCleanupFailed";

        case EIniCode::NotFound:
            return "NotFound";

        case EIniCode::AlreadyExists:
            return "AlreadyExists";

        case EIniCode::InvalidValue:
            return "InvalidValue";

        case EIniCode::ValueOutOfRange:
            return "ValueOutOfRange";

        case EIniCode::KeyAdded:
            return "KeyAdded";

        case EIniCode::KeyRenamed:
            return "KeyRenamed";
    }

    return "Unknown";
}
//---------------------------------------------------------------------------
char const* ToString(ELineBreak lineBreak) noexcept
{
    switch (lineBreak)
    {
        case ELineBreak::None:
            return "None";

        case ELineBreak::LF:
            return "LF";

        case ELineBreak::CR:
            return "CR";

        case ELineBreak::CRLF:
            return "CRLF";
    }

    return "Unknown";
}
//---------------------------------------------------------------------------
char const* ToString(ESeverity severity) noexcept
{
    switch (severity)
    {
        case ESeverity::Info:
            return "Info";

        case ESeverity::Warning:
            return "Warning";

        case ESeverity::Error:
            return "Error";
    }

    return "Unknown";
}
//---------------------------------------------------------------------------
std::string ToString(TASWIniMessage const& message)
{
    std::string text = ToString(message.Severity);
    text += " ";
    text += ToString(message.Code);
    text += ": ";
    text += message.Text;

    // The parts that are set, e.g. (file "app.ini", line 12)
    std::string details;
    auto const addDetail = [&details](std::string const& detail)
        {
            details += details.empty() ? " (" : ", ";
            details += detail;
        };

    if (!message.FilePath.empty())
        addDetail("file \"" + message.FilePath + "\"");

    if (message.Line > 0)
        addDetail("line " + std::to_string(message.Line));

    if (!message.Section.empty())
        addDetail("section \"" + message.Section + "\"");

    if (!message.Key.empty())
        addDetail("key \"" + message.Key + "\"");

    if (message.SystemError != 0)
        addDetail("system error " + std::to_string(message.SystemError));

    if (!details.empty())
        text += details + ")";

    return text;
}
//---------------------------------------------------------------------------

} // namespace ASWIni
