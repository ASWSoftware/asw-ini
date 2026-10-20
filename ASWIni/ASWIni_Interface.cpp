/* **************************************************************************
ASWIni_Interface.cpp
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
#include "ASWIni_Interface.h"
//---------------------------------------------------------------------------
#include <string>
#include <string_view>
//---------------------------------------------------------------------------

namespace ASWIni
{

namespace
{

std::string Quoted(std::string_view name);

//---------------------------------------------------------------------------
/*
    Quoted

    'name' in double quotes, as a message names a section or a key.
*/
std::string Quoted(std::string_view name)
{
    std::string text = "\"";
    text += name;
    text += "\"";

    return text;
}
//---------------------------------------------------------------------------

} // namespace

//---------------------------------------------------------------------------

namespace Detail
{

//---------------------------------------------------------------------------
TASWIniResult KeyFailure(std::string_view section, std::string_view key, char const* action, EIniCode code,
    std::string_view problem)
{
    TASWIniResult result;
    result.Code = code;
    result.Message = "The key " + Quoted(key) + " of the section " + Quoted(section) + " " + action + ": ";
    result.Message += problem;

    return result;
}
//---------------------------------------------------------------------------

} // namespace Detail

//---------------------------------------------------------------------------


/////////////////////////////////////////////////////////////////////////////
// IASWIni
/////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------
std::string IASWIni::GetValue(std::string_view section, std::string_view key, char const* defaultValue) const
{
    return GetValue(section, key, defaultValue != nullptr ? std::string_view(defaultValue) : std::string_view());
}
//---------------------------------------------------------------------------
std::string IASWIni::GetValue(std::string_view section, std::string_view key, std::string_view defaultValue) const
{
    std::string text;
    if (!TryGetText(section, key, text))
        return std::string(defaultValue);

    return text;
}
//---------------------------------------------------------------------------
TASWIniResult IASWIni::KeyNotFound(std::string_view section, std::string_view key)
{
    TASWIniResult result;
    result.Code = EIniCode::NotFound;
    result.Message = "The key " + Quoted(key) + " of the section " + Quoted(section) + " doesn't exist.";

    return result;
}
//---------------------------------------------------------------------------
void IASWIni::ReportFailure(TASWIniResult const& result) const noexcept
{
    try
    {
        TASWIniMessage message;
        message.Severity = ESeverity::Error;
        message.Code = result.Code;
        message.Text = result.Message;
        message.FilePath = result.FilePath;
        message.Line = result.Line;
        message.SystemError = result.SystemError;
        Report(message);
    }
    catch (...)
    {
        // The message couldn't be made (std::bad_alloc); Report() itself doesn't throw
    }
}
//---------------------------------------------------------------------------
void IASWIni::ReportValueWarning(std::string_view section, std::string_view key,
    TASWIniResult const& result) const noexcept
{
    try
    {
        TASWIniMessage message;
        message.Severity = ESeverity::Warning;
        message.Code = result.Code;
        message.Text = result.Message;
        message.Section = section;
        message.Key = key;
        Report(message);
    }
    catch (...)
    {
        // The message couldn't be made (std::bad_alloc); Report() itself doesn't throw
    }
}
//---------------------------------------------------------------------------
TASWIniResult IASWIni::SetValue(std::string_view section, std::string_view key, char const* value)
{
    return SetText(section, key, value != nullptr ? std::string_view(value) : std::string_view());
}
//---------------------------------------------------------------------------
TASWIniResult IASWIni::SetValue(std::string_view section, std::string_view key, std::string_view value)
{
    return SetText(section, key, value);
}
//---------------------------------------------------------------------------

} // namespace ASWIni
