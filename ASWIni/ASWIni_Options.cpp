/* **************************************************************************
ASWIni_Options.cpp
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
#include "ASWIni_Options.h"
//---------------------------------------------------------------------------
#include <string>
#include <string_view>
#include <utility>
//---------------------------------------------------------------------------
#include "ASWIni_Utils.h"
//---------------------------------------------------------------------------

namespace ASWIni
{

namespace
{

bool HasLineBreakOrNul(std::string_view text) noexcept;
TASWIniResult Invalid(std::string message);
std::string Quoted(char c);

//---------------------------------------------------------------------------
/*
    HasLineBreakOrNul

    Whether 'text' has a line break or a NUL, which no option text may have
*/
bool HasLineBreakOrNul(std::string_view text) noexcept
{
    return text.find_first_of(std::string_view("\r\n\0", 3)) != std::string_view::npos;
}
//---------------------------------------------------------------------------
/*
    Invalid

    An InvalidOptions result with 'message'
*/
TASWIniResult Invalid(std::string message)
{
    TASWIniResult result;
    result.Code = EIniCode::InvalidOptions;
    result.Message = std::move(message);
    return result;
}
//---------------------------------------------------------------------------
/*
    Quoted

    'c' as text for a message, e.g. "'='"
*/
std::string Quoted(char c)
{
    return std::string("'") + c + "'";
}
//---------------------------------------------------------------------------

} // namespace

//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
char const* ToString(EBomMode bom) noexcept
{
    switch (bom)
    {
        case EBomMode::Auto:
            return "Auto";

        case EBomMode::Always:
            return "Always";

        case EBomMode::Never:
            return "Never";
    }

    return "Unknown";
}
//---------------------------------------------------------------------------
char const* ToString(EDuplicateKeys duplicateKeys) noexcept
{
    switch (duplicateKeys)
    {
        case EDuplicateKeys::FirstWins:
            return "FirstWins";

        case EDuplicateKeys::LastWins:
            return "LastWins";
    }

    return "Unknown";
}
//---------------------------------------------------------------------------
char const* ToString(ELineEnding lineEnding) noexcept
{
    switch (lineEnding)
    {
        case ELineEnding::Auto:
            return "Auto";

        case ELineEnding::Native:
            return "Native";

        case ELineEnding::LF:
            return "LF";

        case ELineEnding::CRLF:
            return "CRLF";
    }

    return "Unknown";
}
//---------------------------------------------------------------------------


/////////////////////////////////////////////////////////////////////////////
// TASWIniOptions
/////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------
TASWIniResult TASWIniOptions::Validate() const
{
    // Separators: characters that can't also start or end something else on a line
    if (Separators.empty())
        return Invalid("Separators is empty; it needs at least one character, e.g. \"=\".");

    for (char const c : Separators)
    {
        if (Detail::IsBlank(c) || c == '\r' || c == '\n' || c == '\0' || c == '[' || c == ']' || c == '"')
        {
            return Invalid("Separators can't hold spaces, tabs, line breaks, NUL, '[', ']' or '\"'; it holds " +
                (c == '\0' ? std::string("NUL") : Quoted(c)) + ".");
        }
    }

    // CommentPrefixes: each must be recognizable at the start of a line, and not start with a separator (a key line
    // would then be ambiguous)
    if (CommentPrefixes.empty())
        return Invalid("CommentPrefixes is empty; it needs at least one prefix, e.g. \";\".");

    for (std::string const& prefix : CommentPrefixes)
    {
        if (prefix.empty())
            return Invalid("CommentPrefixes holds an empty prefix.");

        if (HasLineBreakOrNul(prefix))
            return Invalid("The comment prefix \"" + prefix + "\" holds a line break or NUL.");

        if (Detail::IsBlank(prefix[0]) || prefix[0] == '[')
            return Invalid("The comment prefix \"" + prefix + "\" starts with a space, tab or '['.");

        if (Separators.find(prefix[0]) != std::string::npos)
        {
            return Invalid("The comment prefix \"" + prefix + "\" starts with " + Quoted(prefix[0]) +
                ", which is also one of the Separators.");
        }
    }

    // Separator: one of Separators, with only spaces or tabs around it, so the written line reads back as a key
    std::string_view const separator = Detail::Trim(Separator);
    if (separator.size() != 1 || Separators.find(separator[0]) == std::string::npos)
    {
        return Invalid("Separator \"" + Separator + "\" must be one of the Separators (\"" + Separators +
            "\"), with only spaces or tabs around it.");
    }

    // CommentPrefix: starts with one of CommentPrefixes, so the written line reads back as a comment
    if (HasLineBreakOrNul(CommentPrefix))
        return Invalid("CommentPrefix holds a line break or NUL.");

    bool commentPrefixKnown = false;
    for (std::string const& prefix : CommentPrefixes)
    {
        if (CommentPrefix.compare(0, prefix.size(), prefix) == 0)
        {
            commentPrefixKnown = true;
            break;
        }
    }
    if (!commentPrefixKnown)
        return Invalid("CommentPrefix \"" + CommentPrefix + "\" must start with one of the CommentPrefixes.");

    if (MaxFileSize == 0)
        return Invalid("MaxFileSize is 0; no file could be loaded.");

    if (Backup)
    {
        if (BackupSuffix.empty())
            return Invalid("BackupSuffix is empty, so the backup would replace the file itself.");

        if (HasLineBreakOrNul(BackupSuffix) || BackupSuffix.find_first_of("/\\:") != std::string::npos)
            return Invalid("BackupSuffix \"" + BackupSuffix + "\" can't hold '/', '\\', ':', line breaks or NUL.");
    }

    // The enumerations, in case one was cast from a number
    if (std::string_view(ToString(DuplicateKeys)) == "Unknown")
        return Invalid("DuplicateKeys isn't one of the EDuplicateKeys values.");

    if (std::string_view(ToString(LineEnding)) == "Unknown")
        return Invalid("LineEnding isn't one of the ELineEnding values.");

    if (std::string_view(ToString(Bom)) == "Unknown")
        return Invalid("Bom isn't one of the EBomMode values.");

    return TASWIniResult();
}
//---------------------------------------------------------------------------

} // namespace ASWIni
