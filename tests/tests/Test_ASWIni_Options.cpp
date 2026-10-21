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
// TTest_ASWIni_ReadingOptions
/////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------
TTest_ASWIni_ReadingOptions::TTest_ASWIni_ReadingOptions()
    : inherited("ASWIni_ReadingOptions_Tests")
{
}
//---------------------------------------------------------------------------
TTest_ASWIni_ReadingOptions::~TTest_ASWIni_ReadingOptions()
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_ReadingOptions::CheckInvalid(ASWIni::TASWIniReadingOptions const& options,
    std::string const& expectedText, std::string const& method, int line, std::string const& msg)
{
    ASWIni::TASWIniResult const result = options.Validate();

    CheckEquals(ASWIni::EIniCode::InvalidOptions, result.Code, method, line,
        msg + ": the code should be InvalidOptions");
    CheckContains(result.Message, expectedText, method, line, msg + ": the message should name the problem");
}
//---------------------------------------------------------------------------
void TTest_ASWIni_ReadingOptions::SetUp_Group()
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_ReadingOptions::SetUp_Test(ITestCase& /*testCase*/)
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_ReadingOptions::TearDown_Group()
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_ReadingOptions::TearDown_Test(ITestCase& /*testCase*/)
{
}
//---------------------------------------------------------------------------

// /////// Begin tests after this line ///////////////////////

//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_ReadingOptions, Defaults_AreValid)
{
    // Arrange
    ASWIni::TASWIniReadingOptions const options;

    // Act
    ASWIni::TASWIniResult const result = options.Validate();

    // Assert
    CheckTrue(result.IsOk(), __func__, __LINE__, "the default reading options should be valid: " + result.Message);
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_ReadingOptions, Defaults_MatchDocumentedValues)
{
    // Act
    ASWIni::TASWIniReadingOptions const options;

    // Assert
    // The defaults the README and the 1.0 decisions document
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
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_ReadingOptions, ToString_DuplicateKeysNames)
{
    CheckEquals(std::string("FirstWins"), std::string(ASWIni::ToString(ASWIni::EDuplicateKeys::FirstWins)), __func__,
        __LINE__, "EDuplicateKeys::FirstWins");
    CheckEquals(std::string("LastWins"), std::string(ASWIni::ToString(ASWIni::EDuplicateKeys::LastWins)), __func__,
        __LINE__, "EDuplicateKeys::LastWins");
    CheckEquals(std::string("Unknown"), std::string(ASWIni::ToString(static_cast<ASWIni::EDuplicateKeys>(99))),
        __func__, __LINE__, "an unknown value");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_ReadingOptions, Validate_RejectsBadCommentPrefixes)
{
    // Arrange & Act & Assert
    ASWIni::TASWIniReadingOptions options;
    options.CommentPrefixes = { ";", "" };
    CheckInvalid(options, "Reading.CommentPrefixes holds an empty prefix", __func__, __LINE__, "an empty prefix");

    options.CommentPrefixes = { ";", " #" };
    CheckInvalid(options, "\" #\"", __func__, __LINE__, "a prefix that starts with a space");

    options.CommentPrefixes = { ";", "[x" };
    CheckInvalid(options, "\"[x\"", __func__, __LINE__, "a prefix that starts with '['");

    options.CommentPrefixes = { ";", "#\n" };
    CheckInvalid(options, "line break", __func__, __LINE__, "a prefix with a line break");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_ReadingOptions, Validate_RejectsBadSeparators)
{
    // Arrange
    // Characters that would be taken for something else on a line
    std::vector<std::string> const separators = { "= ", "=\t", "=\n", "=[", "=]", "=\"", std::string("=\0", 2) };

    for (std::string const& badSeparators : separators)
    {
        ASWIni::TASWIniReadingOptions options;
        options.Separators = badSeparators;

        // Act & Assert
        CheckInvalid(options, "Reading.Separators can't hold", __func__, __LINE__, "a bad separator character");
    }
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_ReadingOptions, Validate_RejectsCommentPrefixStartingWithSeparator)
{
    // Arrange
    // ';' as a separator and as the comment prefix would make "a;b" ambiguous
    ASWIni::TASWIniReadingOptions options;
    options.Separators = "=;";

    // Act & Assert
    CheckInvalid(options, "also one of the Reading.Separators", __func__, __LINE__, "';' as both");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_ReadingOptions, Validate_RejectsEmptyLists)
{
    // Arrange & Act & Assert
    ASWIni::TASWIniReadingOptions options;
    options.Separators.clear();
    CheckInvalid(options, "Reading.Separators is empty", __func__, __LINE__, "no separators");

    options = ASWIni::TASWIniReadingOptions();
    options.CommentPrefixes.clear();
    CheckInvalid(options, "Reading.CommentPrefixes is empty", __func__, __LINE__, "no comment prefixes");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_ReadingOptions, Validate_RejectsUnknownDuplicateKeys)
{
    // Arrange
    ASWIni::TASWIniReadingOptions options;
    options.DuplicateKeys = static_cast<ASWIni::EDuplicateKeys>(7);

    // Act & Assert
    CheckInvalid(options, "Reading.DuplicateKeys", __func__, __LINE__, "an unknown EDuplicateKeys");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_ReadingOptions, Validate_RejectsZeroMaxFileSize)
{
    // Arrange
    ASWIni::TASWIniReadingOptions options;
    options.MaxFileSize = 0;

    // Act & Assert
    CheckInvalid(options, "Reading.MaxFileSize", __func__, __LINE__, "a zero MaxFileSize");
}
//---------------------------------------------------------------------------


/////////////////////////////////////////////////////////////////////////////
// TTest_ASWIni_WritingOptions
/////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------
TTest_ASWIni_WritingOptions::TTest_ASWIni_WritingOptions()
    : inherited("ASWIni_WritingOptions_Tests")
{
}
//---------------------------------------------------------------------------
TTest_ASWIni_WritingOptions::~TTest_ASWIni_WritingOptions()
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_WritingOptions::CheckInvalid(ASWIni::TASWIniWritingOptions const& options,
    std::string const& expectedText, std::string const& method, int line, std::string const& msg)
{
    ASWIni::TASWIniResult const result = options.Validate();

    CheckEquals(ASWIni::EIniCode::InvalidOptions, result.Code, method, line,
        msg + ": the code should be InvalidOptions");
    CheckContains(result.Message, expectedText, method, line, msg + ": the message should name the problem");
}
//---------------------------------------------------------------------------
void TTest_ASWIni_WritingOptions::SetUp_Group()
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_WritingOptions::SetUp_Test(ITestCase& /*testCase*/)
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_WritingOptions::TearDown_Group()
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_WritingOptions::TearDown_Test(ITestCase& /*testCase*/)
{
}
//---------------------------------------------------------------------------

// /////// Begin tests after this line ///////////////////////

//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_WritingOptions, Defaults_AreValid)
{
    // Arrange
    ASWIni::TASWIniWritingOptions const options;

    // Act
    ASWIni::TASWIniResult const result = options.Validate();

    // Assert
    CheckTrue(result.IsOk(), __func__, __LINE__, "the default writing options should be valid: " + result.Message);
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_WritingOptions, Defaults_MatchDocumentedValues)
{
    // Act
    ASWIni::TASWIniWritingOptions const options;

    // Assert
    // The defaults the README and the 1.0 decisions document
    CheckEquals(std::string(" = "), options.Separator, __func__, __LINE__, "Separator");
    CheckEquals(std::string("; "), options.CommentPrefix, __func__, __LINE__, "CommentPrefix");
    CheckEquals(ASWIni::ELineEnding::Auto, options.LineEnding, __func__, __LINE__, "LineEnding");
    CheckEquals(ASWIni::EBomMode::Auto, options.Bom, __func__, __LINE__, "Bom");
    CheckTrue(options.BlankLineBeforeNewSection, __func__, __LINE__, "BlankLineBeforeNewSection should be on");
    CheckEquals(ASWIni::ECommentPolicy::CodeWins, options.CommentPolicy, __func__, __LINE__, "CommentPolicy");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_WritingOptions, ToString_EnumNames)
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
    CheckEquals(std::string("Unknown"), std::string(ASWIni::ToString(static_cast<ASWIni::ELineEnding>(99))), __func__,
        __LINE__, "an unknown ELineEnding");
    CheckEquals(std::string("CodeWins"), std::string(ASWIni::ToString(ASWIni::ECommentPolicy::CodeWins)), __func__,
        __LINE__, "ECommentPolicy::CodeWins");
    CheckEquals(std::string("FileWins"), std::string(ASWIni::ToString(ASWIni::ECommentPolicy::FileWins)), __func__,
        __LINE__, "ECommentPolicy::FileWins");
    CheckEquals(std::string("Unknown"), std::string(ASWIni::ToString(static_cast<ASWIni::EBomMode>(99))), __func__,
        __LINE__, "an unknown EBomMode");
    CheckEquals(std::string("Unknown"), std::string(ASWIni::ToString(static_cast<ASWIni::ECommentPolicy>(99))),
        __func__, __LINE__, "an unknown ECommentPolicy");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_WritingOptions, Validate_AcceptsSeparatorVariants)
{
    // Arrange
    // One separator character, with or without spaces or tabs around it
    std::vector<std::string> const separators = { "=", " = ", "= ", ":", ": ", "\t=\t" };

    for (std::string const& separator : separators)
    {
        ASWIni::TASWIniWritingOptions options;
        options.Separator = separator;

        // Act
        ASWIni::TASWIniResult const result = options.Validate();

        // Assert
        CheckTrue(result.IsOk(), __func__, __LINE__, "Separator \"" + separator + "\" should be valid: " +
            result.Message);
    }
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_WritingOptions, Validate_RejectsBadCommentPrefix)
{
    // Arrange & Act & Assert
    ASWIni::TASWIniWritingOptions options;
    options.CommentPrefix = "";
    CheckInvalid(options, "Writing.CommentPrefix is empty", __func__, __LINE__, "an empty prefix");

    options.CommentPrefix = "; x\n";
    CheckInvalid(options, "line break", __func__, __LINE__, "a prefix with a line break");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_WritingOptions, Validate_RejectsBadSeparatorShape)
{
    // Arrange
    // Not one character with only spaces or tabs around it
    std::vector<std::string> const separators = { "", " ", "==", " = = ", "=x", "=\n" };

    for (std::string const& separator : separators)
    {
        ASWIni::TASWIniWritingOptions options;
        options.Separator = separator;

        // Act & Assert
        CheckInvalid(options, "Writing.Separator \"" + separator + "\"", __func__, __LINE__,
            "Separator \"" + separator + "\"");
    }
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_WritingOptions, Validate_RejectsUnknownEnumValues)
{
    // Arrange & Act & Assert
    ASWIni::TASWIniWritingOptions options;
    options.LineEnding = static_cast<ASWIni::ELineEnding>(7);
    CheckInvalid(options, "Writing.LineEnding", __func__, __LINE__, "an unknown ELineEnding");

    options = ASWIni::TASWIniWritingOptions();
    options.Bom = static_cast<ASWIni::EBomMode>(7);
    CheckInvalid(options, "Writing.Bom", __func__, __LINE__, "an unknown EBomMode");

    options = ASWIni::TASWIniWritingOptions();
    options.CommentPolicy = static_cast<ASWIni::ECommentPolicy>(7);
    CheckInvalid(options, "Writing.CommentPolicy", __func__, __LINE__, "an unknown ECommentPolicy");
}
//---------------------------------------------------------------------------


/////////////////////////////////////////////////////////////////////////////
// TTest_ASWIni_SavingOptions
/////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------
TTest_ASWIni_SavingOptions::TTest_ASWIni_SavingOptions()
    : inherited("ASWIni_SavingOptions_Tests")
{
}
//---------------------------------------------------------------------------
TTest_ASWIni_SavingOptions::~TTest_ASWIni_SavingOptions()
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_SavingOptions::CheckInvalid(ASWIni::TASWIniSavingOptions const& options,
    std::string const& expectedText, std::string const& method, int line, std::string const& msg)
{
    ASWIni::TASWIniResult const result = options.Validate();

    CheckEquals(ASWIni::EIniCode::InvalidOptions, result.Code, method, line,
        msg + ": the code should be InvalidOptions");
    CheckContains(result.Message, expectedText, method, line, msg + ": the message should name the problem");
}
//---------------------------------------------------------------------------
void TTest_ASWIni_SavingOptions::SetUp_Group()
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_SavingOptions::SetUp_Test(ITestCase& /*testCase*/)
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_SavingOptions::TearDown_Group()
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_SavingOptions::TearDown_Test(ITestCase& /*testCase*/)
{
}
//---------------------------------------------------------------------------

// /////// Begin tests after this line ///////////////////////

//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_SavingOptions, Defaults_AreValid)
{
    // Arrange
    ASWIni::TASWIniSavingOptions const options;

    // Act
    ASWIni::TASWIniResult const result = options.Validate();

    // Assert
    CheckTrue(result.IsOk(), __func__, __LINE__, "the default saving options should be valid: " + result.Message);
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_SavingOptions, Defaults_MatchDocumentedValues)
{
    // Act
    ASWIni::TASWIniSavingOptions const options;

    // Assert
    // The defaults the README and the 1.0 decisions document
    CheckTrue(options.AtomicSave, __func__, __LINE__, "AtomicSave should be on");
    CheckFalse(options.Backup, __func__, __LINE__, "Backup should be off");
    CheckEquals(std::string(".bak"), options.BackupSuffix, __func__, __LINE__, "BackupSuffix");
    CheckTrue(options.CreateFolders, __func__, __LINE__, "CreateFolders should be on");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_SavingOptions, Validate_ChecksBackupSuffixOnlyWithBackup)
{
    // Arrange
    ASWIni::TASWIniSavingOptions options;
    options.BackupSuffix = "";

    // Act & Assert
    CheckTrue(options.Validate().IsOk(), __func__, __LINE__, "an empty suffix should be valid without Backup");

    options.Backup = true;
    CheckInvalid(options, "Saving.BackupSuffix is empty", __func__, __LINE__, "an empty suffix with Backup");

    options.BackupSuffix = "/../x";
    CheckInvalid(options, "Saving.BackupSuffix \"/../x\"", __func__, __LINE__, "a suffix with a path separator");

    options.BackupSuffix = ".old";
    CheckTrue(options.Validate().IsOk(), __func__, __LINE__, "\".old\" should be a valid suffix");
}
//---------------------------------------------------------------------------


/////////////////////////////////////////////////////////////////////////////
// TTest_ASWIni_ValueOptions
/////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------
TTest_ASWIni_ValueOptions::TTest_ASWIni_ValueOptions()
    : inherited("ASWIni_ValueOptions_Tests")
{
}
//---------------------------------------------------------------------------
TTest_ASWIni_ValueOptions::~TTest_ASWIni_ValueOptions()
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_ValueOptions::CheckInvalid(ASWIni::TASWIniValueOptions const& options,
    std::string const& expectedText, std::string const& method, int line, std::string const& msg)
{
    ASWIni::TASWIniResult const result = options.Validate();

    CheckEquals(ASWIni::EIniCode::InvalidOptions, result.Code, method, line,
        msg + ": the code should be InvalidOptions");
    CheckContains(result.Message, expectedText, method, line, msg + ": the message should name the problem");
}
//---------------------------------------------------------------------------
void TTest_ASWIni_ValueOptions::SetUp_Group()
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_ValueOptions::SetUp_Test(ITestCase& /*testCase*/)
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_ValueOptions::TearDown_Group()
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_ValueOptions::TearDown_Test(ITestCase& /*testCase*/)
{
}
//---------------------------------------------------------------------------

// /////// Begin tests after this line ///////////////////////

//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_ValueOptions, Defaults_AreValid)
{
    // Arrange
    ASWIni::TASWIniValueOptions const options;

    // Act
    ASWIni::TASWIniResult const result = options.Validate();

    // Assert
    CheckTrue(result.IsOk(), __func__, __LINE__, "the default value options should be valid: " + result.Message);
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_ValueOptions, Defaults_MatchDocumentedValues)
{
    // Act
    ASWIni::TASWIniValueOptions const options;

    // Assert
    // The defaults the 1.0 decisions document: booleans written as true/false, lists as "a, b, c"
    CheckEquals(ASWIni::EBoolFormat::TrueFalse, options.BoolFormat, __func__, __LINE__, "BoolFormat");
    CheckEquals(',', options.ListDelimiter, __func__, __LINE__, "ListDelimiter");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_ValueOptions, ToString_BoolFormatNames)
{
    CheckEquals(std::string("TrueFalse"), std::string(ASWIni::ToString(ASWIni::EBoolFormat::TrueFalse)), __func__,
        __LINE__, "EBoolFormat::TrueFalse");
    CheckEquals(std::string("YesNo"), std::string(ASWIni::ToString(ASWIni::EBoolFormat::YesNo)), __func__, __LINE__,
        "EBoolFormat::YesNo");
    CheckEquals(std::string("OnOff"), std::string(ASWIni::ToString(ASWIni::EBoolFormat::OnOff)), __func__, __LINE__,
        "EBoolFormat::OnOff");
    CheckEquals(std::string("OneZero"), std::string(ASWIni::ToString(ASWIni::EBoolFormat::OneZero)), __func__,
        __LINE__, "EBoolFormat::OneZero");
    CheckEquals(std::string("Unknown"), std::string(ASWIni::ToString(static_cast<ASWIni::EBoolFormat>(99))), __func__,
        __LINE__, "an unknown EBoolFormat");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_ValueOptions, Validate_AcceptsOtherDelimiters)
{
    // Arrange
    ASWIni::TASWIniValueOptions options;

    // Act & Assert
    for (char const delimiter : { ';', '|', '/', '=', ':', '#' })
    {
        options.ListDelimiter = delimiter;
        ASWIni::TASWIniResult const result = options.Validate();
        CheckTrue(result.IsOk(), __func__, __LINE__, std::string("'") + delimiter + "' should be a valid delimiter: " +
            result.Message);
    }
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_ValueOptions, Validate_RejectsBadDelimiters)
{
    // Arrange
    ASWIni::TASWIniValueOptions options;

    // Act & Assert
    // Blanks are trimmed from items, a line break would end the value, and '"' quotes an item
    options.ListDelimiter = ' ';
    CheckInvalid(options, "Values.ListDelimiter can't be a space", __func__, __LINE__, "a space");

    options.ListDelimiter = '\t';
    CheckInvalid(options, "Values.ListDelimiter can't be a space", __func__, __LINE__, "a tab");

    options.ListDelimiter = '\r';
    CheckInvalid(options, "Values.ListDelimiter can't be a space", __func__, __LINE__, "a CR");

    options.ListDelimiter = '\n';
    CheckInvalid(options, "Values.ListDelimiter can't be a space", __func__, __LINE__, "an LF");

    options.ListDelimiter = '\0';
    CheckInvalid(options, "it is NUL", __func__, __LINE__, "NUL");

    options.ListDelimiter = '"';
    CheckInvalid(options, "it is '\"'", __func__, __LINE__, "a double quote");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_ValueOptions, Validate_RejectsUnknownBoolFormat)
{
    // Arrange
    ASWIni::TASWIniValueOptions options;
    options.BoolFormat = static_cast<ASWIni::EBoolFormat>(99);

    // Act & Assert
    CheckInvalid(options, "Values.BoolFormat isn't one of the EBoolFormat values", __func__, __LINE__,
        "an EBoolFormat cast from a number");

    ASWIni::EBoolFormat const formats[] = {
        ASWIni::EBoolFormat::TrueFalse, ASWIni::EBoolFormat::YesNo, ASWIni::EBoolFormat::OnOff,
        ASWIni::EBoolFormat::OneZero,
    };
    for (ASWIni::EBoolFormat const format : formats)
    {
        options.BoolFormat = format;
        CheckTrue(options.Validate().IsOk(), __func__, __LINE__, std::string(ASWIni::ToString(format)) +
            " should be valid");
    }
}
//---------------------------------------------------------------------------


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
ASW_TEST(TTest_ASWIni_Options, Validate_AcceptsMatchingCustomDialect)
{
    // Arrange
    // A dialect of "key: value" lines and "# comment" lines only
    ASWIni::TASWIniOptions options;
    options.Reading.Separators = ":";
    options.Reading.CommentPrefixes = { "#" };
    options.Writing.Separator = ": ";
    options.Writing.CommentPrefix = "# ";

    // Act
    ASWIni::TASWIniResult const result = options.Validate();

    // Assert
    CheckTrue(result.IsOk(), __func__, __LINE__, "matching reading and writing options should be valid: " +
        result.Message);
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_Options, Validate_RejectsCommentPrefixNotInReadingPrefixes)
{
    // Arrange
    ASWIni::TASWIniOptions options;
    options.Writing.CommentPrefix = "// ";

    // Act & Assert
    CheckInvalid(options, "Writing.CommentPrefix \"// \" must start with one of the Reading.CommentPrefixes", __func__,
        __LINE__, "\"//\" isn't a default prefix");

    options.Reading.CommentPrefixes.push_back("//");
    CheckTrue(options.Validate().IsOk(), __func__, __LINE__, "\"// \" should be valid once \"//\" is a prefix");

    options.Writing.CommentPrefix = "#";
    CheckTrue(options.Validate().IsOk(), __func__, __LINE__, "\"#\" should be valid");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_Options, Validate_RejectsListDelimiterStartingCommentPrefix)
{
    // Arrange
    // With InlineComments, an empty item ("a; ; b") would put the delimiter after a blank, where it starts a comment
    ASWIni::TASWIniOptions options;
    options.Values.ListDelimiter = ';';

    // Act & Assert
    CheckTrue(options.Validate().IsOk(), __func__, __LINE__, "';' should be valid without InlineComments");

    options.Reading.InlineComments = true;
    CheckInvalid(options, "Values.ListDelimiter ';' starts the comment prefix \";\"", __func__, __LINE__,
        "';' with InlineComments");

    options.Values.ListDelimiter = '/';
    CheckTrue(options.Validate().IsOk(), __func__, __LINE__, "'/' should be valid when \"//\" isn't a prefix");

    options.Reading.CommentPrefixes.push_back("//");
    CheckInvalid(options, "Values.ListDelimiter '/' starts the comment prefix \"//\"", __func__, __LINE__,
        "'/' with InlineComments and the prefix \"//\"");

    options.Values.ListDelimiter = ',';
    CheckTrue(options.Validate().IsOk(), __func__, __LINE__, "the default ',' should be valid with InlineComments");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_Options, Validate_RejectsSeparatorNotInReadingSeparators)
{
    // Arrange
    // A file written with these couldn't be read back
    ASWIni::TASWIniOptions options;
    options.Writing.Separator = " - ";

    // Act & Assert
    CheckInvalid(options, "Writing.Separator \" - \" must be one of the Reading.Separators", __func__, __LINE__,
        "'-' isn't a default separator");

    options = ASWIni::TASWIniOptions();
    options.Reading.Separators = "=";
    options.Writing.Separator = ": ";
    CheckInvalid(options, "Writing.Separator \": \"", __func__, __LINE__, "':' after it is removed from Separators");

    options.Writing.Separator = "=";
    CheckTrue(options.Validate().IsOk(), __func__, __LINE__, "\"=\" should be valid with Separators \"=\"");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_Options, Validate_ReportsEachGroupsProblems)
{
    // Arrange & Act & Assert
    // A problem within one group comes back from the whole options' Validate(), naming the group
    ASWIni::TASWIniOptions options;
    options.Reading.Separators.clear();
    CheckInvalid(options, "Reading.Separators is empty", __func__, __LINE__, "a reading problem");

    options = ASWIni::TASWIniOptions();
    options.Writing.CommentPrefix.clear();
    CheckInvalid(options, "Writing.CommentPrefix is empty", __func__, __LINE__, "a writing problem");

    options = ASWIni::TASWIniOptions();
    options.Saving.Backup = true;
    options.Saving.BackupSuffix.clear();
    CheckInvalid(options, "Saving.BackupSuffix is empty", __func__, __LINE__, "a saving problem");

    options = ASWIni::TASWIniOptions();
    options.Values.ListDelimiter = ' ';
    CheckInvalid(options, "Values.ListDelimiter can't be a space", __func__, __LINE__, "a values problem");
}
//---------------------------------------------------------------------------

} // namespace ASWUnitTests

//---------------------------------------------------------------------------
ASW_REGISTER_TEST_GROUP_IN_SUITE(ASWUnitTests::TTest_ASWIni_Options, "Options")
ASW_REGISTER_TEST_GROUP_IN_SUITE(ASWUnitTests::TTest_ASWIni_ReadingOptions, "Options")
ASW_REGISTER_TEST_GROUP_IN_SUITE(ASWUnitTests::TTest_ASWIni_SavingOptions, "Options")
ASW_REGISTER_TEST_GROUP_IN_SUITE(ASWUnitTests::TTest_ASWIni_ValueOptions, "Options")
ASW_REGISTER_TEST_GROUP_IN_SUITE(ASWUnitTests::TTest_ASWIni_WritingOptions, "Options")
