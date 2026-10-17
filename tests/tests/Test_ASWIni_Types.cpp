/* **************************************************************************
Test_ASWIni_Types.cpp
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
#include "Test_ASWIni_Types.h"
//---------------------------------------------------------------------------
#include "ASWUnitTests_Registry.h"
//---------------------------------------------------------------------------
#include <set>
#include <string>
#include <vector>
//---------------------------------------------------------------------------
#include "ASWIni_Types.h"
//---------------------------------------------------------------------------

namespace ASWUnitTests
{

namespace
{

// Every EIniCode, in declaration order
std::vector<ASWIni::EIniCode> AllIniCodes()
{
    using ASWIni::EIniCode;

    std::vector<EIniCode> codes;
    codes.push_back(EIniCode::Ok);
    codes.push_back(EIniCode::InvalidArgument);
    codes.push_back(EIniCode::InvalidOptions);
    codes.push_back(EIniCode::NoFilePath);
    codes.push_back(EIniCode::FileNotFound);
    codes.push_back(EIniCode::AccessDenied);
    codes.push_back(EIniCode::FileTooLarge);
    codes.push_back(EIniCode::OpenFailed);
    codes.push_back(EIniCode::ReadFailed);
    codes.push_back(EIniCode::UnsupportedEncoding);
    codes.push_back(EIniCode::ParseFailed);
    codes.push_back(EIniCode::CreateFolderFailed);
    codes.push_back(EIniCode::WriteFailed);
    codes.push_back(EIniCode::ReplaceFailed);
    codes.push_back(EIniCode::UnexpectedException);
    codes.push_back(EIniCode::MalformedLine);
    codes.push_back(EIniCode::UnclosedSection);
    codes.push_back(EIniCode::DuplicateKey);
    codes.push_back(EIniCode::DuplicateSection);
    codes.push_back(EIniCode::InvalidUtf8);
    codes.push_back(EIniCode::BackupFailed);
    codes.push_back(EIniCode::TempFileCleanupFailed);
    codes.push_back(EIniCode::NotFound);
    codes.push_back(EIniCode::AlreadyExists);
    codes.push_back(EIniCode::InvalidValue);
    codes.push_back(EIniCode::ValueOutOfRange);
    return codes;
}

} // namespace

//---------------------------------------------------------------------------

/////////////////////////////////////////////////////////////////////////////
// TTest_ASWIni_Types
/////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------
TTest_ASWIni_Types::TTest_ASWIni_Types()
    : inherited("ASWIni_Types_Tests")
{
}
//---------------------------------------------------------------------------
TTest_ASWIni_Types::~TTest_ASWIni_Types()
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_Types::SetUp_Group()
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_Types::SetUp_Test(ITestCase& /*testCase*/)
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_Types::TearDown_Group()
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_Types::TearDown_Test(ITestCase& /*testCase*/)
{
}
//---------------------------------------------------------------------------

// /////// Begin tests after this line ///////////////////////

//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_Types, IniCode_ValuesNeverChange)
{
    // Arrange
    // Applications may store or compare the values, so each code keeps its number and new codes are added
    // at the end, numbered in declaration order from 0
    std::vector<ASWIni::EIniCode> const codes = AllIniCodes();

    // Act & Assert
    for (std::size_t i = 0; i < codes.size(); ++i)
    {
        CheckEquals(static_cast<int>(i), static_cast<int>(codes[i]), __func__, __LINE__,
            std::string("EIniCode::") + ASWIni::ToString(codes[i]) + " should keep its value");
    }
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_Types, Message_DefaultsToEmptyInfo)
{
    // Act
    ASWIni::TASWIniMessage const message;

    // Assert
    CheckEquals(ASWIni::ESeverity::Info, message.Severity, __func__, __LINE__, "the severity should be Info");
    CheckEquals(ASWIni::EIniCode::Ok, message.Code, __func__, __LINE__, "the code should be Ok");
    CheckEmpty(message.Text, __func__, __LINE__, "the text should be empty");
    CheckEmpty(message.FilePath, __func__, __LINE__, "the file path should be empty");
    CheckEquals(0, static_cast<int>(message.Line), __func__, __LINE__, "the line should be 0");
    CheckEmpty(message.Section, __func__, __LINE__, "the section should be empty");
    CheckEmpty(message.Key, __func__, __LINE__, "the key should be empty");
    CheckEquals(0, message.SystemError, __func__, __LINE__, "the system error should be 0");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_Types, Result_DefaultsToOk)
{
    // Act
    ASWIni::TASWIniResult const result;

    // Assert
    CheckEquals(ASWIni::EIniCode::Ok, result.Code, __func__, __LINE__, "the code should be Ok");
    CheckTrue(result.IsOk(), __func__, __LINE__, "IsOk() should be true");
    CheckTrue(static_cast<bool>(result), __func__, __LINE__, "the result should convert to true");
    CheckEmpty(result.Message, __func__, __LINE__, "the message should be empty");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_Types, Result_FailsForAnyOtherCode)
{
    for (ASWIni::EIniCode const code : AllIniCodes())
    {
        if (code == ASWIni::EIniCode::Ok)
            continue;

        // Arrange
        ASWIni::TASWIniResult result;
        result.Code = code;

        // Act & Assert
        std::string const name = ASWIni::ToString(code);
        CheckFalse(result.IsOk(), __func__, __LINE__, "IsOk() should be false for " + name);
        CheckFalse(static_cast<bool>(result), __func__, __LINE__, "the result should convert to false for " + name);
    }
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_Types, ToString_IniCodeNamesAreUnique)
{
    // Arrange
    std::set<std::string> names;

    for (ASWIni::EIniCode const code : AllIniCodes())
    {
        // Act
        std::string const name = ASWIni::ToString(code);

        // Assert
        CheckNotEquals(std::string("Unknown"), name, __func__, __LINE__,
            "code " + std::to_string(static_cast<int>(code)) + " should have a name");
        CheckTrue(names.insert(name).second, __func__, __LINE__, "the name \"" + name + "\" should be unique");
    }

    CheckEquals(std::string("FileNotFound"), std::string(ASWIni::ToString(ASWIni::EIniCode::FileNotFound)), __func__,
        __LINE__, "the name should be the enumerator's");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_Types, ToString_LineBreakNames)
{
    CheckEquals(std::string("None"), std::string(ASWIni::ToString(ASWIni::ELineBreak::None)), __func__, __LINE__,
        "None's name");
    CheckEquals(std::string("LF"), std::string(ASWIni::ToString(ASWIni::ELineBreak::LF)), __func__, __LINE__,
        "LF's name");
    CheckEquals(std::string("CR"), std::string(ASWIni::ToString(ASWIni::ELineBreak::CR)), __func__, __LINE__,
        "CR's name");
    CheckEquals(std::string("CRLF"), std::string(ASWIni::ToString(ASWIni::ELineBreak::CRLF)), __func__, __LINE__,
        "CRLF's name");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_Types, ToString_MessageWithAllParts)
{
    // Arrange
    ASWIni::TASWIniMessage message;
    message.Severity = ASWIni::ESeverity::Warning;
    message.Code = ASWIni::EIniCode::DuplicateKey;
    message.Text = "The key appears again; the first one is used.";
    message.FilePath = "app.ini";
    message.Line = 12;
    message.Section = "General";
    message.Key = "LogLevel";
    message.SystemError = 5;

    // Act
    std::string const text = ASWIni::ToString(message);

    // Assert
    CheckEquals(std::string("Warning DuplicateKey: The key appears again; the first one is used. (file \"app.ini\", "
        "line 12, section \"General\", key \"LogLevel\", system error 5)"), text, __func__, __LINE__,
        "every part should be written, in order");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_Types, ToString_MessageWithTextOnly)
{
    // Arrange
    ASWIni::TASWIniMessage message;
    message.Severity = ASWIni::ESeverity::Error;
    message.Code = ASWIni::EIniCode::InvalidOptions;
    message.Text = "Separators is empty.";

    // Act
    std::string const text = ASWIni::ToString(message);

    // Assert
    CheckEquals(std::string("Error InvalidOptions: Separators is empty."), text, __func__, __LINE__,
        "the parts that aren't set should be left out, with no parentheses");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_Types, ToString_SeverityNames)
{
    CheckEquals(std::string("Info"), std::string(ASWIni::ToString(ASWIni::ESeverity::Info)), __func__, __LINE__,
        "Info's name");
    CheckEquals(std::string("Warning"), std::string(ASWIni::ToString(ASWIni::ESeverity::Warning)), __func__,
        __LINE__, "Warning's name");
    CheckEquals(std::string("Error"), std::string(ASWIni::ToString(ASWIni::ESeverity::Error)), __func__, __LINE__,
        "Error's name");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_Types, ToString_UnknownValues)
{
    // Arrange
    // Values that aren't enumerators, e.g. read from a newer version or cast from a corrupt number
    ASWIni::EIniCode const code = static_cast<ASWIni::EIniCode>(9999);
    ASWIni::ELineBreak const lineBreak = static_cast<ASWIni::ELineBreak>(4);
    ASWIni::ESeverity const severity = static_cast<ASWIni::ESeverity>(-1);

    // Act & Assert
    CheckEquals(std::string("Unknown"), std::string(ASWIni::ToString(code)), __func__, __LINE__,
        "an unknown code should be \"Unknown\"");
    CheckEquals(std::string("Unknown"), std::string(ASWIni::ToString(lineBreak)), __func__, __LINE__,
        "an unknown line break should be \"Unknown\"");
    CheckEquals(std::string("Unknown"), std::string(ASWIni::ToString(severity)), __func__, __LINE__,
        "an unknown severity should be \"Unknown\"");
}
//---------------------------------------------------------------------------

} // namespace ASWUnitTests

//---------------------------------------------------------------------------
ASW_REGISTER_TEST_GROUP(ASWUnitTests::TTest_ASWIni_Types)
