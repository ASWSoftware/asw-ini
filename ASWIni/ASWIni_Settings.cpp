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
//---------------------------------------------------------------------------
#include "ASWIni_Interface.h"
//---------------------------------------------------------------------------

namespace ASWIni
{

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
void TSettingBase::ReportDefaultUsed(TASWIniResult const& result, std::string const* defaultText) const noexcept
{
    try
    {
        TASWIniMessage message;
        message.Severity = ESeverity::Warning;
        message.Code = result.Code;
        message.Text = result.Message;
        if (defaultText != nullptr)
            message.Text += " The default value " + MessageText(*defaultText) + " is used.";
        else
            message.Text += " The default value is used.";

        message.Section = m_SectionName;
        message.Key = m_Key;
        m_Owner->Report(message);
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
