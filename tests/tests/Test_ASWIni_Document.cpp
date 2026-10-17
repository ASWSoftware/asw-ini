/* **************************************************************************
Test_ASWIni_Document.cpp
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
#include "Test_ASWIni_Document.h"
//---------------------------------------------------------------------------
#include "ASWUnitTests_Registry.h"
//---------------------------------------------------------------------------
#include <string>
//---------------------------------------------------------------------------
#include "ASWIni_Document.h"
#include "ASWIni_Types.h"
//---------------------------------------------------------------------------

namespace ASWUnitTests
{

/////////////////////////////////////////////////////////////////////////////
// TTest_ASWIni_DocumentModel
/////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------
TTest_ASWIni_DocumentModel::TTest_ASWIni_DocumentModel()
    : inherited("ASWIni_DocumentModel_Tests")
{
}
//---------------------------------------------------------------------------
TTest_ASWIni_DocumentModel::~TTest_ASWIni_DocumentModel()
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_DocumentModel::SetUp_Group()
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_DocumentModel::SetUp_Test(ITestCase& /*testCase*/)
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_DocumentModel::TearDown_Group()
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_DocumentModel::TearDown_Test(ITestCase& /*testCase*/)
{
}
//---------------------------------------------------------------------------

// /////// Begin tests after this line ///////////////////////

//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_DocumentModel, Document_DefaultIsEmpty)
{
    // Act
    ASWIni::TASWIniDocument const document;

    // Assert
    AssertEquals(1, static_cast<int>(document.GetSections().size()), __func__, __LINE__,
        "a new document should have one section, its preamble");
    CheckFalse(document.GetSections().front().HasHeader(), __func__, __LINE__, "the preamble should have no header");
    CheckEquals(0, static_cast<int>(document.GetLineCount()), __func__, __LINE__, "a new document should have no lines");
    CheckFalse(document.HasBom(), __func__, __LINE__, "a new document should have no byte order mark");
#if defined(_WIN32)
    CheckEquals(ASWIni::ELineBreak::CRLF, document.GetLineBreak(), __func__, __LINE__,
        "new lines should get the platform's line break, CRLF on Windows");
#else
    CheckEquals(ASWIni::ELineBreak::LF, document.GetLineBreak(), __func__, __LINE__,
        "new lines should get the platform's line break, LF on other platforms");
#endif
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_DocumentModel, Line_DefaultIsBlank)
{
    // Act
    ASWIni::TASWIniDocumentLine const line;

    // Assert
    CheckEquals(ASWIni::ELineKind::Blank, line.GetKind(), __func__, __LINE__, "a new line should be blank");
    CheckEquals(std::string(), line.GetText(), __func__, __LINE__, "a new line should have no text");
    CheckEquals(ASWIni::ELineBreak::None, line.GetLineBreak(), __func__, __LINE__,
        "a new line should have no line break");
    CheckEquals(std::string(), std::string(line.GetKey()), __func__, __LINE__, "a blank line should have no key");
    CheckEquals(std::string(), std::string(line.GetValue()), __func__, __LINE__, "a blank line should have no value");
    CheckEquals(std::string(), std::string(line.GetSectionName()), __func__, __LINE__,
        "a blank line should have no section name");
    CheckEquals(0, static_cast<int>(line.GetSeparator()), __func__, __LINE__,
        "a blank line's separator should be '\\0'");
    CheckFalse(line.HasComment(), __func__, __LINE__, "a blank line should have no comment");
    CheckEquals(std::string(), std::string(line.GetComment()), __func__, __LINE__,
        "a blank line's comment should be empty");
    CheckFalse(line.IsQuoted(), __func__, __LINE__, "a blank line should have no quoted value");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_DocumentModel, Section_DefaultIsEmptyPreamble)
{
    // Act
    ASWIni::TASWIniDocumentSection const section;

    // Assert
    CheckFalse(section.HasHeader(), __func__, __LINE__, "a new section should have no header");
    CheckEquals(std::string(), std::string(section.GetName()), __func__, __LINE__,
        "a section with no header should have the name \"\"");
    CheckEquals(0, static_cast<int>(section.GetLines().size()), __func__, __LINE__,
        "a new section should have no lines");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_DocumentModel, ToString_LineKindNames)
{
    CheckEquals(std::string("Blank"), std::string(ASWIni::ToString(ASWIni::ELineKind::Blank)), __func__, __LINE__,
        "Blank's name");
    CheckEquals(std::string("Comment"), std::string(ASWIni::ToString(ASWIni::ELineKind::Comment)), __func__, __LINE__,
        "Comment's name");
    CheckEquals(std::string("Section"), std::string(ASWIni::ToString(ASWIni::ELineKind::Section)), __func__, __LINE__,
        "Section's name");
    CheckEquals(std::string("KeyValue"), std::string(ASWIni::ToString(ASWIni::ELineKind::KeyValue)), __func__,
        __LINE__, "KeyValue's name");
    CheckEquals(std::string("Raw"), std::string(ASWIni::ToString(ASWIni::ELineKind::Raw)), __func__, __LINE__,
        "Raw's name");
    CheckEquals(std::string("Unknown"), std::string(ASWIni::ToString(static_cast<ASWIni::ELineKind>(99))), __func__,
        __LINE__, "a value that isn't an ELineKind should be \"Unknown\"");
}
//---------------------------------------------------------------------------

} // namespace ASWUnitTests

//---------------------------------------------------------------------------
ASW_REGISTER_TEST_GROUP_IN_SUITE(ASWUnitTests::TTest_ASWIni_DocumentModel, "Document")
