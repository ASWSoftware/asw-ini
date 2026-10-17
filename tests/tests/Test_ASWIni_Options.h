/* **************************************************************************
Test_ASWIni_Options.h
Author: Anthony S. West - ASW Software

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
#ifndef Test_ASWIni_OptionsH
#define Test_ASWIni_OptionsH
//---------------------------------------------------------------------------
#include <string>
//---------------------------------------------------------------------------
#include "ASWUnitTests_TestBase.h"
//---------------------------------------------------------------------------
#include "ASWIni_Options.h"
//---------------------------------------------------------------------------

namespace ASWUnitTests
{

/////////////////////////////////////////////////////////////////////////////
// TTest_ASWIni_ReadingOptions
/////////////////////////////////////////////////////////////////////////////
class TTest_ASWIni_ReadingOptions : public TAutoTestGroup<TTest_ASWIni_ReadingOptions>
{
private:
    typedef TAutoTestGroup<TTest_ASWIni_ReadingOptions> inherited;

private:
    // Checks that 'options' fail Validate() with InvalidOptions and a message that contains 'expectedText'
    void CheckInvalid(ASWIni::TASWIniReadingOptions const& options, std::string const& expectedText,
        std::string const& method, int line, std::string const& msg);

public: // Test methods, each defined with ASW_TEST
    void Test_Defaults_AreValid();
    void Test_Defaults_MatchDocumentedValues();
    void Test_ToString_DuplicateKeysNames();
    void Test_Validate_RejectsBadCommentPrefixes();
    void Test_Validate_RejectsBadSeparators();
    void Test_Validate_RejectsCommentPrefixStartingWithSeparator();
    void Test_Validate_RejectsEmptyLists();
    void Test_Validate_RejectsUnknownDuplicateKeys();
    void Test_Validate_RejectsZeroMaxFileSize();

public:
    TTest_ASWIni_ReadingOptions();
    ~TTest_ASWIni_ReadingOptions() override;

    void SetUp_Group() override;
    void SetUp_Test(ITestCase& testCase) override;
    void TearDown_Group() override;
    void TearDown_Test(ITestCase& testCase) override;
};


/////////////////////////////////////////////////////////////////////////////
// TTest_ASWIni_WritingOptions
/////////////////////////////////////////////////////////////////////////////
class TTest_ASWIni_WritingOptions : public TAutoTestGroup<TTest_ASWIni_WritingOptions>
{
private:
    typedef TAutoTestGroup<TTest_ASWIni_WritingOptions> inherited;

private:
    // Checks that 'options' fail Validate() with InvalidOptions and a message that contains 'expectedText'
    void CheckInvalid(ASWIni::TASWIniWritingOptions const& options, std::string const& expectedText,
        std::string const& method, int line, std::string const& msg);

public: // Test methods, each defined with ASW_TEST
    void Test_Defaults_AreValid();
    void Test_Defaults_MatchDocumentedValues();
    void Test_ToString_EnumNames();
    void Test_Validate_AcceptsSeparatorVariants();
    void Test_Validate_RejectsBadCommentPrefix();
    void Test_Validate_RejectsBadSeparatorShape();
    void Test_Validate_RejectsUnknownEnumValues();

public:
    TTest_ASWIni_WritingOptions();
    ~TTest_ASWIni_WritingOptions() override;

    void SetUp_Group() override;
    void SetUp_Test(ITestCase& testCase) override;
    void TearDown_Group() override;
    void TearDown_Test(ITestCase& testCase) override;
};


/////////////////////////////////////////////////////////////////////////////
// TTest_ASWIni_SavingOptions
/////////////////////////////////////////////////////////////////////////////
class TTest_ASWIni_SavingOptions : public TAutoTestGroup<TTest_ASWIni_SavingOptions>
{
private:
    typedef TAutoTestGroup<TTest_ASWIni_SavingOptions> inherited;

private:
    // Checks that 'options' fail Validate() with InvalidOptions and a message that contains 'expectedText'
    void CheckInvalid(ASWIni::TASWIniSavingOptions const& options, std::string const& expectedText,
        std::string const& method, int line, std::string const& msg);

public: // Test methods, each defined with ASW_TEST
    void Test_Defaults_AreValid();
    void Test_Defaults_MatchDocumentedValues();
    void Test_Validate_ChecksBackupSuffixOnlyWithBackup();

public:
    TTest_ASWIni_SavingOptions();
    ~TTest_ASWIni_SavingOptions() override;

    void SetUp_Group() override;
    void SetUp_Test(ITestCase& testCase) override;
    void TearDown_Group() override;
    void TearDown_Test(ITestCase& testCase) override;
};


/////////////////////////////////////////////////////////////////////////////
// TTest_ASWIni_Options
/////////////////////////////////////////////////////////////////////////////
class TTest_ASWIni_Options : public TAutoTestGroup<TTest_ASWIni_Options>
{
private:
    typedef TAutoTestGroup<TTest_ASWIni_Options> inherited;

private:
    // Checks that 'options' fail Validate() with InvalidOptions and a message that contains 'expectedText'
    void CheckInvalid(ASWIni::TASWIniOptions const& options, std::string const& expectedText, std::string const& method,
        int line, std::string const& msg);

public: // Test methods, each defined with ASW_TEST
    void Test_Defaults_AreValid();
    void Test_Validate_AcceptsMatchingCustomDialect();
    void Test_Validate_RejectsCommentPrefixNotInReadingPrefixes();
    void Test_Validate_RejectsSeparatorNotInReadingSeparators();
    void Test_Validate_ReportsEachGroupsProblems();

public:
    TTest_ASWIni_Options();
    ~TTest_ASWIni_Options() override;

    void SetUp_Group() override;
    void SetUp_Test(ITestCase& testCase) override;
    void TearDown_Group() override;
    void TearDown_Test(ITestCase& testCase) override;
};

} // namespace ASWUnitTests

//---------------------------------------------------------------------------
#endif // #ifndef Test_ASWIni_OptionsH
