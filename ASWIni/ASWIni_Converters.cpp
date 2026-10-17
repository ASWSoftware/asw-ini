/* **************************************************************************
ASWIni_Converters.cpp
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
#include "ASWIni_Converters.h"
//---------------------------------------------------------------------------
#include <string>
#include <string_view>
//---------------------------------------------------------------------------
#include "ASWIni_Utils.h"
//---------------------------------------------------------------------------

namespace ASWIni
{

namespace
{

// The texts a bool is read from, with the ASCII letters' case ignored
constexpr std::string_view TrueTexts[] = { "true", "yes", "on", "1" };
constexpr std::string_view FalseTexts[] = { "false", "no", "off", "0" };

} // namespace

//---------------------------------------------------------------------------

namespace Detail
{

//---------------------------------------------------------------------------
std::string MessageText(std::string_view text)
{
    // A long text is cut before a byte that starts a UTF-8 sequence (or is ASCII), so no sequence is split
    std::size_t size = text.size();
    if (size > MaxMessageTextSize)
    {
        size = MaxMessageTextSize;
        while (size > 0 && (static_cast<unsigned char>(text[size]) & 0xC0) == 0x80)
            --size;
    }

    std::string quoted = "\"";
    for (char const c : text.substr(0, size))
    {
        unsigned char const byte = static_cast<unsigned char>(c);
        quoted += (byte < 0x20 || byte == 0x7F) ? '?' : c;
    }

    quoted += '"';
    if (size < text.size())
        quoted += "...";

    return quoted;
}
//---------------------------------------------------------------------------
TASWIniResult ValueFailure(EIniCode code, std::string_view text, std::string_view problem)
{
    TASWIniResult result;
    result.Code = code;
    result.Message = "The value " + MessageText(text) + " ";
    result.Message += problem;

    return result;
}
//---------------------------------------------------------------------------

} // namespace Detail

//---------------------------------------------------------------------------

/////////////////////////////////////////////////////////////////////////////
// TASWIniConverter<bool>
/////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------
TASWIniResult TASWIniConverter<bool>::FromText(std::string_view text, bool& value,
    TASWIniValueOptions const& /*options*/)
{
    std::string_view const trimmed = Detail::Trim(text);
    for (std::string_view const trueText : TrueTexts)
    {
        if (Detail::EqualsNoCase(trimmed, trueText))
        {
            value = true;
            return TASWIniResult();
        }
    }

    for (std::string_view const falseText : FalseTexts)
    {
        if (Detail::EqualsNoCase(trimmed, falseText))
        {
            value = false;
            return TASWIniResult();
        }
    }

    return Detail::ValueFailure(EIniCode::InvalidValue, text,
        "isn't a bool: it must be true, false, yes, no, on, off, 1 or 0.");
}
//---------------------------------------------------------------------------
TASWIniResult TASWIniConverter<bool>::ToText(bool value, std::string& text, TASWIniValueOptions const& options)
{
    // A BoolFormat that isn't an EBoolFormat writes the default form
    char const* written = value ? "true" : "false";
    switch (options.BoolFormat)
    {
        case EBoolFormat::TrueFalse:
            break;

        case EBoolFormat::YesNo:
            written = value ? "yes" : "no";
            break;

        case EBoolFormat::OnOff:
            written = value ? "on" : "off";
            break;

        case EBoolFormat::OneZero:
            written = value ? "1" : "0";
            break;
    }

    text = written;

    return TASWIniResult();
}
//---------------------------------------------------------------------------


/////////////////////////////////////////////////////////////////////////////
// TASWIniConverter<std::string>
/////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------
TASWIniResult TASWIniConverter<std::string>::FromText(std::string_view text, std::string& value,
    TASWIniValueOptions const& /*options*/)
{
    value.assign(text.data(), text.size());

    return TASWIniResult();
}
//---------------------------------------------------------------------------
TASWIniResult TASWIniConverter<std::string>::ToText(std::string const& value, std::string& text,
    TASWIniValueOptions const& /*options*/)
{
    text = value;

    return TASWIniResult();
}
//---------------------------------------------------------------------------

} // namespace ASWIni
