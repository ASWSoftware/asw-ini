/* **************************************************************************
Test_ASWIni_Settings.h
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
#ifndef Test_ASWIni_SettingsH
#define Test_ASWIni_SettingsH
//---------------------------------------------------------------------------
#include "ASWUnitTests_TestBase.h"
//---------------------------------------------------------------------------

namespace ASWUnitTests
{

/////////////////////////////////////////////////////////////////////////////
// TTest_ASWIni_SettingsAccess
//
// TASWIniSection and TASWIniSetting<T>: Get(), Set() and Reset() through the owner's document, the range, the
// validator, the value options, the comment of a key Set() adds, and what is reported
/////////////////////////////////////////////////////////////////////////////
class TTest_ASWIni_SettingsAccess : public TAutoTestGroup<TTest_ASWIni_SettingsAccess>
{
private:
    typedef TAutoTestGroup<TTest_ASWIni_SettingsAccess> inherited;

public: // Test methods, each defined with ASW_TEST
    void Test_Get_InvalidValueGivesTheDefault();
    void Test_Get_ReadsTheDocument();
    void Test_New_GivesTheDefaults();
    void Test_OutOfMemory_SetChangesNothing();
    void Test_Range_ChecksGetAndSet();
    void Test_Reset_SetsTheDefault();
    void Test_Set_AddsTheKeyWithItsComment();
    void Test_Set_CommentThatCantBeWrittenChangesNothing();
    void Test_Set_FailuresAreReported();
    void Test_Set_WritesTheDocument();
    void Test_Types_WithoutOperatorLessWork();
    void Test_Validator_ChecksGetAndSet();
    void Test_ValueOptions_AreTheSettingsOwnOrTheOwners();

public:
    TTest_ASWIni_SettingsAccess();
    ~TTest_ASWIni_SettingsAccess() override;

    void SetUp_Group() override;
    void SetUp_Test(ITestCase& testCase) override;
    void TearDown_Group() override;
    void TearDown_Test(ITestCase& testCase) override;
};


/////////////////////////////////////////////////////////////////////////////
// TTest_ASWIni_SettingsLoad
//
// What Load() and LoadText() do with the settings and sections bound to the object: missing keys added, invalid
// values replaced, comments as the CommentPolicy says, what is reported, and the bindings' lifetimes
/////////////////////////////////////////////////////////////////////////////
class TTest_ASWIni_SettingsLoad : public TAutoTestGroup<TTest_ASWIni_SettingsLoad>
{
private:
    typedef TAutoTestGroup<TTest_ASWIni_SettingsLoad> inherited;

public: // Test methods, each defined with ASW_TEST
    void Test_Bindings_FollowTheirLifetimes();
    void Test_Comments_CodeWinsReplacesTheFilesComments();
    void Test_Comments_FileWinsKeepsTheFilesComments();
    void Test_Load_AddsMissingKeysInDeclarationOrder();
    void Test_Load_FailureAppliesNothing();
    void Test_Load_InvalidDefaultIsAnError();
    void Test_Load_LeavesAValidDocumentAsItIs();
    void Test_Load_MissingFileGetsEverySetting();
    void Test_Load_OptionalKeyIsNotAdded();
    void Test_Load_ReplacesAnInvalidValueWithTheDefault();
    void Test_Load_WriteFailuresAreErrors();
    void Test_SetOptions_DoesntApplyTheSettings();

public:
    TTest_ASWIni_SettingsLoad();
    ~TTest_ASWIni_SettingsLoad() override;

    void SetUp_Group() override;
    void SetUp_Test(ITestCase& testCase) override;
    void TearDown_Group() override;
    void TearDown_Test(ITestCase& testCase) override;
};

} // namespace ASWUnitTests

//---------------------------------------------------------------------------
#endif // #ifndef Test_ASWIni_SettingsH
