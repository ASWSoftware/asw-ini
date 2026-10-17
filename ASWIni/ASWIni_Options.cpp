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
char const* ToString(EBoolFormat boolFormat) noexcept
{
    switch (boolFormat)
    {
        case EBoolFormat::TrueFalse:
            return "TrueFalse";

        case EBoolFormat::YesNo:
            return "YesNo";

        case EBoolFormat::OnOff:
            return "OnOff";

        case EBoolFormat::OneZero:
            return "OneZero";
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
// TASWIniReadingOptions
/////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------
TASWIniResult TASWIniReadingOptions::Validate() const
{
    // Separators: characters that can't also start or end something else on a line
    if (Separators.empty())
        return Invalid("Reading.Separators is empty; it needs at least one character, e.g. \"=\".");

    for (char const c : Separators)
    {
        if (Detail::IsBlank(c) || c == '\r' || c == '\n' || c == '\0' || c == '[' || c == ']' || c == '"')
        {
            return Invalid("Reading.Separators can't hold spaces, tabs, line breaks, NUL, '[', ']' or '\"'; it holds " +
                (c == '\0' ? std::string("NUL") : Quoted(c)) + ".");
        }
    }

    // CommentPrefixes: each must be recognizable at the start of a line, and not start with a separator (a key line
    // would then be ambiguous)
    if (CommentPrefixes.empty())
        return Invalid("Reading.CommentPrefixes is empty; it needs at least one prefix, e.g. \";\".");

    for (std::string const& prefix : CommentPrefixes)
    {
        if (prefix.empty())
            return Invalid("Reading.CommentPrefixes holds an empty prefix.");

        if (HasLineBreakOrNul(prefix))
        {
            return Invalid("The comment prefix \"" + prefix +
                "\" in Reading.CommentPrefixes holds a line break or NUL.");
        }

        if (Detail::IsBlank(prefix[0]) || prefix[0] == '[')
        {
            return Invalid("The comment prefix \"" + prefix +
                "\" in Reading.CommentPrefixes starts with a space, tab or '['.");
        }

        if (Separators.find(prefix[0]) != std::string::npos)
        {
            return Invalid("The comment prefix \"" + prefix + "\" in Reading.CommentPrefixes starts with " +
                Quoted(prefix[0]) + ", which is also one of the Reading.Separators.");
        }
    }

    if (MaxFileSize == 0)
        return Invalid("Reading.MaxFileSize is 0; no file could be loaded.");

    // The enumeration, in case it was cast from a number
    if (std::string_view(ToString(DuplicateKeys)) == "Unknown")
        return Invalid("Reading.DuplicateKeys isn't one of the EDuplicateKeys values.");

    return TASWIniResult();
}
//---------------------------------------------------------------------------


/////////////////////////////////////////////////////////////////////////////
// TASWIniWritingOptions
/////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------
TASWIniResult TASWIniWritingOptions::Validate() const
{
    // Separator: one character, with only spaces or tabs around it
    if (HasLineBreakOrNul(Separator) || Detail::Trim(Separator).size() != 1)
    {
        return Invalid("Writing.Separator \"" + Separator +
            "\" must be one character, with only spaces or tabs around it.");
    }

    if (CommentPrefix.empty())
        return Invalid("Writing.CommentPrefix is empty; it needs a comment prefix, e.g. \"; \".");

    if (HasLineBreakOrNul(CommentPrefix))
        return Invalid("Writing.CommentPrefix holds a line break or NUL.");

    // The enumerations, in case one was cast from a number
    if (std::string_view(ToString(LineEnding)) == "Unknown")
        return Invalid("Writing.LineEnding isn't one of the ELineEnding values.");

    if (std::string_view(ToString(Bom)) == "Unknown")
        return Invalid("Writing.Bom isn't one of the EBomMode values.");

    return TASWIniResult();
}
//---------------------------------------------------------------------------


/////////////////////////////////////////////////////////////////////////////
// TASWIniSavingOptions
/////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------
TASWIniResult TASWIniSavingOptions::Validate() const
{
    if (Backup)
    {
        if (BackupSuffix.empty())
            return Invalid("Saving.BackupSuffix is empty, so the backup would replace the file itself.");

        if (HasLineBreakOrNul(BackupSuffix) || BackupSuffix.find_first_of("/\\:") != std::string::npos)
        {
            return Invalid("Saving.BackupSuffix \"" + BackupSuffix +
                "\" can't hold '/', '\\', ':', line breaks or NUL.");
        }
    }

    return TASWIniResult();
}
//---------------------------------------------------------------------------


/////////////////////////////////////////////////////////////////////////////
// TASWIniValueOptions
/////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------
TASWIniResult TASWIniValueOptions::Validate() const
{
    // The enumeration, in case it was cast from a number
    if (std::string_view(ToString(BoolFormat)) == "Unknown")
        return Invalid("Values.BoolFormat isn't one of the EBoolFormat values.");

    // ListDelimiter: not a blank (items are trimmed), not a line break, and not the quote that items are written in
    char const c = ListDelimiter;
    if (Detail::IsBlank(c) || c == '\r' || c == '\n' || c == '\0' || c == '"')
    {
        return Invalid("Values.ListDelimiter can't be a space, tab, line break, NUL or '\"'; it is " +
            (c == '\0' ? std::string("NUL") : Quoted(c)) + ".");
    }

    return TASWIniResult();
}
//---------------------------------------------------------------------------


/////////////////////////////////////////////////////////////////////////////
// TASWIniOptions
/////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------
TASWIniResult TASWIniOptions::Validate() const
{
    TASWIniResult result = Reading.Validate();
    if (!result.IsOk())
        return result;

    result = Writing.Validate();
    if (!result.IsOk())
        return result;

    result = Saving.Validate();
    if (!result.IsOk())
        return result;

    result = Values.Validate();
    if (!result.IsOk())
        return result;

    // A written key line must read back as a key: its separator must be one the reader looks for
    char const separator = Detail::Trim(Writing.Separator)[0];
    if (Reading.Separators.find(separator) == std::string::npos)
    {
        return Invalid("Writing.Separator \"" + Writing.Separator + "\" must be one of the Reading.Separators (\"" +
            Reading.Separators + "\").");
    }

    // A written comment line must read back as a comment
    bool commentPrefixKnown = false;
    for (std::string const& prefix : Reading.CommentPrefixes)
    {
        if (Writing.CommentPrefix.compare(0, prefix.size(), prefix) == 0)
        {
            commentPrefixKnown = true;
            break;
        }
    }

    if (!commentPrefixKnown)
    {
        return Invalid("Writing.CommentPrefix \"" + Writing.CommentPrefix +
            "\" must start with one of the Reading.CommentPrefixes.");
    }

    // A list with an empty item has the delimiter after a blank ("a, , b"), which starts an inline comment when the
    // delimiter starts a comment prefix
    if (Reading.InlineComments)
    {
        for (std::string const& prefix : Reading.CommentPrefixes)
        {
            if (prefix[0] == Values.ListDelimiter)
            {
                return Invalid("Values.ListDelimiter " + Quoted(Values.ListDelimiter) +
                    " starts the comment prefix \"" + prefix +
                    "\" of Reading.CommentPrefixes, so with Reading.InlineComments a list could hold a comment.");
            }
        }
    }

    return TASWIniResult();
}
//---------------------------------------------------------------------------

} // namespace ASWIni
