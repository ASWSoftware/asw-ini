/* **************************************************************************
ASWIni_Settings.cpp
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
#include "ASWIni_Settings.h"
//---------------------------------------------------------------------------
#include <exception>
#include <string>
#include <string_view>
#include <vector>
//---------------------------------------------------------------------------
#include "ASWIni_Document.h"
#include "ASWIni_Interface.h"
#include "ASWIni_Options.h"
//---------------------------------------------------------------------------

namespace ASWIni
{

namespace
{

TASWIniMessage SettingMessage(ESeverity severity, TASWIniResult const& result, std::string const& section,
    std::string const& key);
bool WritesCodeComment(TASWIniDocument const& document, std::string const& fileComment);

//---------------------------------------------------------------------------
/*
    SettingMessage

    A message with 'severity', and the code and text of 'result', about key 'key' of section 'section' (or about the
    section when 'key' is empty).
*/
TASWIniMessage SettingMessage(ESeverity severity, TASWIniResult const& result, std::string const& section,
    std::string const& key)
{
    TASWIniMessage message;
    message.Severity = severity;
    message.Code = result.Code;
    message.Text = result.Message;
    message.Section = section;
    message.Key = key;

    return message;
}
//---------------------------------------------------------------------------
/*
    WritesCodeComment

    Whether a comment the code declares replaces 'fileComment', the comment in 'document', as its options'
    Writing.CommentPolicy says: always with CodeWins, and only when there is none with FileWins.
*/
bool WritesCodeComment(TASWIniDocument const& document, std::string const& fileComment)
{
    return document.GetOptions().Writing.CommentPolicy != ECommentPolicy::FileWins || fileComment.empty();
}
//---------------------------------------------------------------------------

} // namespace

//---------------------------------------------------------------------------

namespace Detail
{

//---------------------------------------------------------------------------
TASWIniResult OutOfSettingRange(std::string_view text, std::string const* minText, std::string const* maxText)
{
    std::string problem = "is out of the setting's range";
    if (minText != nullptr && maxText != nullptr)
        problem += ": it must be " + *minText + " to " + *maxText;
    else if (minText != nullptr)
        problem += ": it must be at least " + *minText;
    else if (maxText != nullptr)
        problem += ": it must be at most " + *maxText;

    problem += ".";

    return ValueFailure(EIniCode::ValueOutOfRange, text, problem);
}
//---------------------------------------------------------------------------
TASWIniResult RefusedValue(std::string_view text, std::string_view problem)
{
    std::string sentence = "isn't valid: ";
    sentence += problem;
    if (sentence.back() != '.')
        sentence += ".";

    return ValueFailure(EIniCode::InvalidValue, text, sentence);
}
//---------------------------------------------------------------------------


/////////////////////////////////////////////////////////////////////////////
// TSettingBase
/////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------
TSettingBase::TSettingBase(TASWIniSection const& section, std::string_view key, std::string_view comment)
    : m_Owner(&section.GetOwner()),
      m_SectionName(section.GetName()),
      m_Key(key),
      m_Comment(comment)
{
}
//---------------------------------------------------------------------------
void TSettingBase::AddInvalidDefault(TASWIniResult const& result, std::vector<TASWIniMessage>& messages) const
{
    TASWIniResult const failure = KeyFailure(m_SectionName, m_Key, "can't be given its default value", result.Code,
        result.Message);
    messages.push_back(SettingMessage(ESeverity::Error, failure, m_SectionName, m_Key));
}
//---------------------------------------------------------------------------
void TSettingBase::ApplyComment(TASWIniDocument& document, std::vector<TASWIniMessage>& messages) const
{
    if (m_Comment.empty() || !WritesCodeComment(document, document.GetKeyComment(m_SectionName, m_Key)))
        return;

    TASWIniResult const result = document.SetKeyComment(m_SectionName, m_Key, m_Comment);
    if (!result.IsOk())
        messages.push_back(SettingMessage(ESeverity::Error, result, m_SectionName, m_Key));
}
//---------------------------------------------------------------------------
void TSettingBase::ApplyDefault(TASWIniDocument& document, std::string const& defaultText,
    TASWIniResult const* problem, std::vector<TASWIniMessage>& messages) const
{
    if (problem != nullptr)
        messages.push_back(DefaultUsedMessage(*problem, &defaultText));

    TASWIniResult const result = document.SetValue(m_SectionName, m_Key, defaultText);
    if (!result.IsOk())
    {
        messages.push_back(SettingMessage(ESeverity::Error, result, m_SectionName, m_Key));
        return;
    }

    if (problem == nullptr)
    {
        TASWIniResult const added = KeyFailure(m_SectionName, m_Key, "is missing", EIniCode::KeyAdded,
            "it is added with its default value " + MessageText(defaultText) + ".");
        messages.push_back(SettingMessage(ESeverity::Info, added, m_SectionName, m_Key));
    }

    ApplyComment(document, messages);
}
//---------------------------------------------------------------------------
bool TSettingBase::ApplyOldKeys(TASWIniDocument& document, std::vector<std::string> const& oldKeys,
    std::vector<TASWIniMessage>& messages) const
{
    for (std::string const& oldKey : oldKeys)
    {
        if (document.FindKey(m_SectionName, oldKey) == nullptr)
            continue;

        TASWIniResult const result = document.RenameKey(m_SectionName, oldKey, m_Key);
        if (!result.IsOk())
        {
            messages.push_back(SettingMessage(ESeverity::Error, result, m_SectionName, m_Key));
            return false;
        }

        TASWIniResult const renamed = KeyFailure(m_SectionName, m_Key, "is missing", EIniCode::KeyRenamed,
            "its earlier name \"" + oldKey + "\" is renamed to it.");
        messages.push_back(SettingMessage(ESeverity::Info, renamed, m_SectionName, m_Key));
        break;
    }

    return true;
}
//---------------------------------------------------------------------------
TASWIniResult TSettingBase::CaughtSetFailure() const noexcept
{
    TASWIniResult result;
    result.Code = EIniCode::UnexpectedException;

    try
    {
        try
        {
            // Not a throw expression, which bcc32c would warn about in a noexcept function
            std::rethrow_exception(std::current_exception());
        }
        catch (std::exception const& e)
        {
            result = KeyFailure(m_SectionName, m_Key, "can't be set", EIniCode::UnexpectedException, e.what());
        }
        catch (...)
        {
            result = KeyFailure(m_SectionName, m_Key, "can't be set", EIniCode::UnexpectedException,
                TASWBaseIni::UnknownExceptionText);
        }
    }
    catch (...)
    {
        // The message couldn't be made (std::bad_alloc); the code says what happened
    }

    return result;
}
//---------------------------------------------------------------------------
TASWIniMessage TSettingBase::DefaultUsedMessage(TASWIniResult const& result, std::string const* defaultText) const
{
    TASWIniMessage message = SettingMessage(ESeverity::Warning, result, m_SectionName, m_Key);
    if (defaultText != nullptr)
        message.Text += " The default value " + MessageText(*defaultText) + " is used.";
    else
        message.Text += " The default value is used.";

    return message;
}
//---------------------------------------------------------------------------
std::string const& TSettingBase::GetComment() const noexcept
{
    return m_Comment;
}
//---------------------------------------------------------------------------
std::string const& TSettingBase::GetKey() const noexcept
{
    return m_Key;
}
//---------------------------------------------------------------------------
TASWBaseIni& TSettingBase::GetOwner() const noexcept
{
    return *m_Owner;
}
//---------------------------------------------------------------------------
std::string const& TSettingBase::GetSectionName() const noexcept
{
    return m_SectionName;
}
//---------------------------------------------------------------------------
bool TSettingBase::IsPresent() const
{
    return m_Owner->HasKey(m_SectionName, m_Key);
}
//---------------------------------------------------------------------------
void TSettingBase::Register() const
{
    m_Owner->Register(*this);
}
//---------------------------------------------------------------------------
void TSettingBase::ReportDefaultUsed(TASWIniResult const& result, std::string const* defaultText) const noexcept
{
    try
    {
        m_Owner->Report(DefaultUsedMessage(result, defaultText));
    }
    catch (...)
    {
        // The message couldn't be made (std::bad_alloc); Report() itself doesn't throw
    }
}
//---------------------------------------------------------------------------
void TSettingBase::ReportFailure(TASWIniResult const& result) const noexcept
{
    m_Owner->ReportFailure(result);
}
//---------------------------------------------------------------------------
TASWIniResult TSettingBase::SetFailure(TASWIniResult const& result) const
{
    return KeyFailure(m_SectionName, m_Key, "can't be set", result.Code, result.Message);
}
//---------------------------------------------------------------------------
TASWIniResult TSettingBase::SetText(std::string_view text)
{
    return m_Owner->SetSettingText(m_SectionName, m_Key, text, m_Comment);
}
//---------------------------------------------------------------------------
bool TSettingBase::TryGetText(std::string& text) const
{
    return m_Owner->TryGetText(m_SectionName, m_Key, text);
}
//---------------------------------------------------------------------------
void TSettingBase::Unregister() const
{
    m_Owner->Unregister(*this);
}
//---------------------------------------------------------------------------

} // namespace Detail

//---------------------------------------------------------------------------


/////////////////////////////////////////////////////////////////////////////
// TASWIniSection
/////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------
TASWIniSection::TASWIniSection(TASWBaseIni& owner, std::string_view name, std::string_view comment)
    : m_Owner(&owner),
      m_Name(name),
      m_Comment(comment)
{
    m_Owner->Register(*this);
}
//---------------------------------------------------------------------------
TASWIniSection::~TASWIniSection()
{
    m_Owner->Unregister(*this);
}
//---------------------------------------------------------------------------
void TASWIniSection::Apply(TASWIniDocument& document, std::vector<TASWIniMessage>& messages) const
{
    if (m_Comment.empty())
        return;

    // The section "" has no header: its comment is the file comment
    TASWIniResult result;
    if (m_Name.empty())
    {
        if (!WritesCodeComment(document, document.GetFileComment()))
            return;

        result = document.SetFileComment(m_Comment);
    }
    else
    {
        // A section isn't added for its comment
        if (document.FindSection(m_Name) == nullptr)
            return;

        if (!WritesCodeComment(document, document.GetSectionComment(m_Name)))
            return;

        result = document.SetSectionComment(m_Name, m_Comment);
    }

    if (!result.IsOk())
        messages.push_back(SettingMessage(ESeverity::Error, result, m_Name, std::string()));
}
//---------------------------------------------------------------------------
std::string const& TASWIniSection::GetComment() const noexcept
{
    return m_Comment;
}
//---------------------------------------------------------------------------
std::string const& TASWIniSection::GetName() const noexcept
{
    return m_Name;
}
//---------------------------------------------------------------------------
TASWBaseIni& TASWIniSection::GetOwner() const noexcept
{
    return *m_Owner;
}
//---------------------------------------------------------------------------

} // namespace ASWIni
