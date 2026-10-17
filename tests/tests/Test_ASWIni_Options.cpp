/* **************************************************************************
Test_ASWIni_Options.cpp
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
#include "Test_ASWIni_Options.h"
//---------------------------------------------------------------------------
#include "ASWUnitTests_Registry.h"
//---------------------------------------------------------------------------
#include <cstdint>
#include <string>
#include <vector>
//---------------------------------------------------------------------------

namespace ASWUnitTests
{

/////////////////////////////////////////////////////////////////////////////
// TTest_ASWIni_Options
/////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------
TTest_ASWIni_Options::TTest_ASWIni_Options()
    : inherited("ASWIni_Options_Tests")
{
}
//---------------------------------------------------------------------------
TTest_ASWIni_Options::~TTest_ASWIni_Options()
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_Options::CheckInvalid(ASWIni::TASWIniOptions const& options, std::string const& expectedText,
    std::string const& method, int line, std::string const& msg)
{
    ASWIni::TASWIniResult const result = options.Validate();

    CheckEquals(ASWIni::EIniCode::InvalidOptions, result.Code, method, line,
        msg + ": the code should be InvalidOptions");
    CheckContains(result.Message, expectedText, method, line, msg + ": the message should name the problem");
}
//---------------------------------------------------------------------------
void TTest_ASWIni_Options::SetUp_Group()
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_Options::SetUp_Test(ITestCase& /*testCase*/)
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_Options::TearDown_Group()
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_Options::TearDown_Test(ITestCase& /*testCase*/)
{
}
//---------------------------------------------------------------------------

// /////// Begin tests after this line ///////////////////////

//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_Options, Defaults_AreValid)
{
    // Arrange
    ASWIni::TASWIniOptions const options;

    // Act
    ASWIni::TASWIniResult const result = options.Validate();

    // Assert
    CheckTrue(result.IsOk(), __func__, __LINE__, "the default options should be valid: " + result.Message);
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_Options, Defaults_MatchDocumentedValues)
{
    // Act
    ASWIni::TASWIniOptions const options;

    // Assert: the defaults the README and the 1.0 decisions document
    CheckEquals(std::string("=:"), options.Separators, __func__, __LINE__, "Separators");
    AssertEquals(2, static_cast<int>(options.CommentPrefixes.size()), __func__, __LINE__, "two comment prefixes");
    CheckEquals(std::string(";"), options.CommentPrefixes[0], __func__, __LINE__, "the first comment prefix");
    CheckEquals(std::string("#"), options.CommentPrefixes[1], __func__, __LINE__, "the second comment prefix");
    CheckFalse(options.InlineComments, __func__, __LINE__, "InlineComments should be off");
    CheckFalse(options.QuotedValues, __func__, __LINE__, "QuotedValues should be off");
    CheckFalse(options.CaseSensitive, __func__, __LINE__, "names should be case-insensitive");
    CheckEquals(ASWIni::EDuplicateKeys::FirstWins, options.DuplicateKeys, __func__, __LINE__,
        "the first duplicate should win");
    CheckFalse(options.StrictParsing, __func__, __LINE__, "StrictParsing should be off");
    CheckEquals(static_cast<uint64_t>(16 * 1024 * 1024), static_cast<uint64_t>(options.MaxFileSize), __func__,
        __LINE__, "MaxFileSize should be 16 MiB");
    CheckEquals(std::string(" = "), options.Separator, __func__, __LINE__, "Separator");
    CheckEquals(std::string("; "), options.CommentPrefix, __func__, __LINE__, "CommentPrefix");
    CheckEquals(ASWIni::ELineEnding::Auto, options.LineEnding, __func__, __LINE__, "LineEnding");
    CheckEquals(ASWIni::EBomMode::Auto, options.Bom, __func__, __LINE__, "Bom");
    CheckTrue(options.BlankLineBeforeNewSection, __func__, __LINE__, "BlankLineBeforeNewSection should be on");
    CheckTrue(options.AtomicSave, __func__, __LINE__, "AtomicSave should be on");
    CheckFalse(options.Backup, __func__, __LINE__, "Backup should be off");
    CheckEquals(std::string(".bak"), options.BackupSuffix, __func__, __LINE__, "BackupSuffix");
    CheckTrue(options.CreateFolders, __func__, __LINE__, "CreateFolders should be on");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_Options, ToString_EnumNames)
{
    CheckEquals(std::string("Auto"), std::string(ASWIni::ToString(ASWIni::ELineEnding::Auto)), __func__, __LINE__,
        "ELineEnding::Auto");
    CheckEquals(std::string("Native"), std::string(ASWIni::ToString(ASWIni::ELineEnding::Native)), __func__, __LINE__,
        "ELineEnding::Native");
    CheckEquals(std::string("LF"), std::string(ASWIni::ToString(ASWIni::ELineEnding::LF)), __func__, __LINE__,
        "ELineEnding::LF");
    CheckEquals(std::string("CRLF"), std::string(ASWIni::ToString(ASWIni::ELineEnding::CRLF)), __func__, __LINE__,
        "ELineEnding::CRLF");
    CheckEquals(std::string("Auto"), std::string(ASWIni::ToString(ASWIni::EBomMode::Auto)), __func__, __LINE__,
        "EBomMode::Auto");
    CheckEquals(std::string("Always"), std::string(ASWIni::ToString(ASWIni::EBomMode::Always)), __func__, __LINE__,
        "EBomMode::Always");
    CheckEquals(std::string("Never"), std::string(ASWIni::ToString(ASWIni::EBomMode::Never)), __func__, __LINE__,
        "EBomMode::Never");
    CheckEquals(std::string("FirstWins"), std::string(ASWIni::ToString(ASWIni::EDuplicateKeys::FirstWins)), __func__,
        __LINE__, "EDuplicateKeys::FirstWins");
    CheckEquals(std::string("LastWins"), std::string(ASWIni::ToString(ASWIni::EDuplicateKeys::LastWins)), __func__,
        __LINE__, "EDuplicateKeys::LastWins");
    CheckEquals(std::string("Unknown"), std::string(ASWIni::ToString(static_cast<ASWIni::ELineEnding>(99))), __func__,
        __LINE__, "an unknown value");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_Options, Validate_AcceptsSeparatorVariants)
{
    // Arrange: one separator character, with or without spaces or tabs around it
    std::vector<std::string> const separators = { "=", " = ", "= ", ":", ": ", "\t=\t" };

    for (std::string const& separator : separators)
    {
        ASWIni::TASWIniOptions options;
        options.Separator = separator;

        // Act
        ASWIni::TASWIniResult const result = options.Validate();

        // Assert
        CheckTrue(result.IsOk(), __func__, __LINE__, "Separator \"" + separator + "\" should be valid: " +
            result.Message);
    }
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_Options, Validate_ChecksBackupSuffixOnlyWithBackup)
{
    // Arrange
    ASWIni::TASWIniOptions options;
    options.BackupSuffix = "";

    // Act & Assert
    CheckTrue(options.Validate().IsOk(), __func__, __LINE__, "an empty suffix should be valid without Backup");

    options.Backup = true;
    CheckInvalid(options, "BackupSuffix", __func__, __LINE__, "an empty suffix with Backup");

    options.BackupSuffix = "/../x";
    CheckInvalid(options, "BackupSuffix", __func__, __LINE__, "a suffix with a path separator");

    options.BackupSuffix = ".old";
    CheckTrue(options.Validate().IsOk(), __func__, __LINE__, "\".old\" should be a valid suffix");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_Options, Validate_RejectsBadCommentPrefixes)
{
    // Arrange & Act & Assert
    ASWIni::TASWIniOptions options;
    options.CommentPrefixes = { ";", "" };
    CheckInvalid(options, "empty prefix", __func__, __LINE__, "an empty prefix");

    options.CommentPrefixes = { ";", " #" };
    CheckInvalid(options, "\" #\"", __func__, __LINE__, "a prefix that starts with a space");

    options.CommentPrefixes = { ";", "[x" };
    CheckInvalid(options, "\"[x\"", __func__, __LINE__, "a prefix that starts with '['");

    options.CommentPrefixes = { ";", "#\n" };
    CheckInvalid(options, "line break", __func__, __LINE__, "a prefix with a line break");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_Options, Validate_RejectsBadSeparators)
{
    // Arrange: characters that would be taken for something else on a line
    std::vector<std::string> const separators = { "= ", "=\t", "=\n", "=[", "=]", "=\"", std::string("=\0", 2) };

    for (std::string const& badSeparators : separators)
    {
        ASWIni::TASWIniOptions options;
        options.Separators = badSeparators;

        // Act & Assert
        CheckInvalid(options, "Separators can't hold", __func__, __LINE__, "a bad separator character");
    }
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_Options, Validate_RejectsCommentPrefixStartingWithSeparator)
{
    // Arrange: ';' as a separator and as the comment prefix would make "a;b" ambiguous
    ASWIni::TASWIniOptions options;
    options.Separators = "=;";

    // Act & Assert
    CheckInvalid(options, "also one of the Separators", __func__, __LINE__, "';' as both");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_Options, Validate_RejectsEmptyLists)
{
    // Arrange & Act & Assert
    ASWIni::TASWIniOptions options;
    options.Separators.clear();
    CheckInvalid(options, "Separators is empty", __func__, __LINE__, "no separators");

    options = ASWIni::TASWIniOptions();
    options.CommentPrefixes.clear();
    CheckInvalid(options, "CommentPrefixes is empty", __func__, __LINE__, "no comment prefixes");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_Options, Validate_RejectsSeparatorNotInSeparators)
{
    // Arrange
    // A file written with any of these couldn't be read back with the default Separators
    std::vector<std::string> const separators = { "", " ", "-", "==", " = = ", "=x" };

    for (std::string const& separator : separators)
    {
        ASWIni::TASWIniOptions options;
        options.Separator = separator;

        // Act & Assert
        CheckInvalid(options, "Separator \"" + separator + "\"", __func__, __LINE__,
            "Separator \"" + separator + "\"");
    }
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_Options, Validate_RejectsUnknownCommentPrefix)
{
    // Arrange
    ASWIni::TASWIniOptions options;
    options.CommentPrefix = "// ";

    // Act & Assert
    CheckInvalid(options, "CommentPrefix \"// \"", __func__, __LINE__, "\"//\" isn't a default prefix");

    options.CommentPrefixes.push_back("//");
    CheckTrue(options.Validate().IsOk(), __func__, __LINE__, "\"// \" should be valid once \"//\" is a prefix");

    options.CommentPrefix = "#";
    CheckTrue(options.Validate().IsOk(), __func__, __LINE__, "\"#\" should be valid");

    options.CommentPrefix = "; x\n";
    CheckInvalid(options, "line break", __func__, __LINE__, "a prefix with a line break");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_Options, Validate_RejectsUnknownEnumValues)
{
    // Arrange & Act & Assert
    ASWIni::TASWIniOptions options;
    options.DuplicateKeys = static_cast<ASWIni::EDuplicateKeys>(7);
    CheckInvalid(options, "DuplicateKeys", __func__, __LINE__, "an unknown EDuplicateKeys");

    options = ASWIni::TASWIniOptions();
    options.LineEnding = static_cast<ASWIni::ELineEnding>(7);
    CheckInvalid(options, "LineEnding", __func__, __LINE__, "an unknown ELineEnding");

    options = ASWIni::TASWIniOptions();
    options.Bom = static_cast<ASWIni::EBomMode>(7);
    CheckInvalid(options, "Bom", __func__, __LINE__, "an unknown EBomMode");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_Options, Validate_RejectsZeroMaxFileSize)
{
    // Arrange
    ASWIni::TASWIniOptions options;
    options.MaxFileSize = 0;

    // Act & Assert
    CheckInvalid(options, "MaxFileSize", __func__, __LINE__, "a zero MaxFileSize");
}
//---------------------------------------------------------------------------

} // namespace ASWUnitTests

//---------------------------------------------------------------------------
ASW_REGISTER_TEST_GROUP(ASWUnitTests::TTest_ASWIni_Options)
