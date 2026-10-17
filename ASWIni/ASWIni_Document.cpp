/* **************************************************************************
ASWIni_Document.cpp
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
#include "ASWIni_Document.h"
//---------------------------------------------------------------------------
#include "ASWIni_Utils.h"
//---------------------------------------------------------------------------

namespace ASWIni
{

//---------------------------------------------------------------------------
char const* ToString(ELineKind kind) noexcept
{
    switch (kind)
    {
        case ELineKind::Blank:
            return "Blank";

        case ELineKind::Comment:
            return "Comment";

        case ELineKind::Section:
            return "Section";

        case ELineKind::KeyValue:
            return "KeyValue";

        case ELineKind::Raw:
            return "Raw";
    }

    return "Unknown";
}
//---------------------------------------------------------------------------


/////////////////////////////////////////////////////////////////////////////
// TASWIniDocumentLine
/////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------
std::string_view TASWIniDocumentLine::GetComment() const noexcept
{
    if (!m_HasComment)
        return std::string_view();

    return GetPart(m_Comment);
}
//---------------------------------------------------------------------------
std::string_view TASWIniDocumentLine::GetKey() const noexcept
{
    if (m_Kind != ELineKind::KeyValue)
        return std::string_view();

    return GetPart(m_Name);
}
//---------------------------------------------------------------------------
ELineKind TASWIniDocumentLine::GetKind() const noexcept
{
    return m_Kind;
}
//---------------------------------------------------------------------------
ELineBreak TASWIniDocumentLine::GetLineBreak() const noexcept
{
    return m_LineBreak;
}
//---------------------------------------------------------------------------
std::string_view TASWIniDocumentLine::GetPart(TPart part) const noexcept
{
    return std::string_view(m_Text).substr(part.Begin, part.End - part.Begin);
}
//---------------------------------------------------------------------------
std::string_view TASWIniDocumentLine::GetSectionName() const noexcept
{
    if (m_Kind != ELineKind::Section)
        return std::string_view();

    return GetPart(m_Name);
}
//---------------------------------------------------------------------------
char TASWIniDocumentLine::GetSeparator() const noexcept
{
    if (m_Kind != ELineKind::KeyValue)
        return '\0';

    return m_Text[m_Separator];
}
//---------------------------------------------------------------------------
std::string const& TASWIniDocumentLine::GetText() const noexcept
{
    return m_Text;
}
//---------------------------------------------------------------------------
std::string_view TASWIniDocumentLine::GetValue() const noexcept
{
    if (m_Kind != ELineKind::KeyValue)
        return std::string_view();

    return GetPart(m_Value);
}
//---------------------------------------------------------------------------
bool TASWIniDocumentLine::HasComment() const noexcept
{
    return m_HasComment;
}
//---------------------------------------------------------------------------
bool TASWIniDocumentLine::IsQuoted() const noexcept
{
    return m_Kind == ELineKind::KeyValue && m_Quoted;
}
//---------------------------------------------------------------------------


/////////////////////////////////////////////////////////////////////////////
// TASWIniDocumentSection
/////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------
std::vector<TASWIniDocumentLine> const& TASWIniDocumentSection::GetLines() const noexcept
{
    return m_Lines;
}
//---------------------------------------------------------------------------
std::string_view TASWIniDocumentSection::GetName() const noexcept
{
    if (!HasHeader())
        return std::string_view();

    return m_Lines.front().GetSectionName();
}
//---------------------------------------------------------------------------
bool TASWIniDocumentSection::HasHeader() const noexcept
{
    return !m_Lines.empty() && m_Lines.front().GetKind() == ELineKind::Section;
}
//---------------------------------------------------------------------------


/////////////////////////////////////////////////////////////////////////////
// TASWIniDocument
/////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------
TASWIniDocument::TASWIniDocument()
    : m_Sections(1),
      m_LineBreak(Detail::NativeLineBreak())
{
}
//---------------------------------------------------------------------------
ELineBreak TASWIniDocument::GetLineBreak() const noexcept
{
    return m_LineBreak;
}
//---------------------------------------------------------------------------
std::size_t TASWIniDocument::GetLineCount() const noexcept
{
    std::size_t count = 0;
    for (TASWIniDocumentSection const& section : m_Sections)
        count += section.GetLines().size();

    return count;
}
//---------------------------------------------------------------------------
std::vector<TASWIniDocumentSection> const& TASWIniDocument::GetSections() const noexcept
{
    return m_Sections;
}
//---------------------------------------------------------------------------
bool TASWIniDocument::HasBom() const noexcept
{
    return m_HasBom;
}
//---------------------------------------------------------------------------

} // namespace ASWIni
