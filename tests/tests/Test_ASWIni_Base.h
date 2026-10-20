/* **************************************************************************
Test_ASWIni_Base.h
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
#ifndef Test_ASWIni_BaseH
#define Test_ASWIni_BaseH
//---------------------------------------------------------------------------
#include "ASWUnitTests_TestBase.h"
//---------------------------------------------------------------------------

namespace ASWUnitTests
{

/////////////////////////////////////////////////////////////////////////////
// TTest_ASWIni_BaseAccess
//
// TASWBaseIni's lookups, edits, options, LoadText() and ToText(), and what they do when memory runs out
/////////////////////////////////////////////////////////////////////////////
class TTest_ASWIni_BaseAccess : public TAutoTestGroup<TTest_ASWIni_BaseAccess>
{
private:
    typedef TAutoTestGroup<TTest_ASWIni_BaseAccess> inherited;

public: // Test methods, each defined with ASW_TEST
    void Test_Edits_FailureChangesNothing();
    void Test_Edits_MatchTheDocument();
    void Test_GetDocument_IsACopy();
    void Test_LoadText_FailureChangesNothing();
    void Test_LoadText_ReplacesTheDocument();
    void Test_LoadText_UsesTheOptions();
    void Test_Locks_WorkLikeStandardMutexes();
    void Test_Lookups_MatchTheDocument();
    void Test_New_IsEmptyWithTheDefaultOptions();
    void Test_OutOfMemory_EditChangesNothing();
    void Test_OutOfMemory_LoadTextChangesNothing();
    void Test_OutOfMemory_SetOptionsChangesNothing();
    void Test_Remove_ReturnsWhetherAnythingWasRemoved();
    void Test_SetOptions_InvalidOptionsChangeNothing();
    void Test_SetOptions_ReadsTheTextAgain();
    void Test_TypedValues_UseTheValueOptions();

public:
    TTest_ASWIni_BaseAccess();
    ~TTest_ASWIni_BaseAccess() override;

    void SetUp_Group() override;
    void SetUp_Test(ITestCase& testCase) override;
    void TearDown_Group() override;
    void TearDown_Test(ITestCase& testCase) override;
};


/////////////////////////////////////////////////////////////////////////////
// TTest_ASWIni_BaseCallback
//
// How TASWBaseIni reports to its log callback: what, in which order, and with no lock held
/////////////////////////////////////////////////////////////////////////////
class TTest_ASWIni_BaseCallback : public TAutoTestGroup<TTest_ASWIni_BaseCallback>
{
private:
    typedef TAutoTestGroup<TTest_ASWIni_BaseCallback> inherited;

public: // Test methods, each defined with ASW_TEST
    void Test_Callback_ExceptionIsDropped();
    void Test_Callback_MayCallTheObject();
    void Test_Callback_MayReplaceItself();
    void Test_FailedEdit_IsReportedAsAnError();
    void Test_GetValue_WarningIsReported();
    void Test_LoadText_WarningsAreReportedInOrder();
    void Test_Lookups_ReportNothing();
    void Test_OutOfMemory_IsReportedAsAnError();
    void Test_SetLogCallback_EmptyTurnsReportingOff();
    void Test_SetOptions_WarningsAreReported();

public:
    TTest_ASWIni_BaseCallback();
    ~TTest_ASWIni_BaseCallback() override;

    void SetUp_Group() override;
    void SetUp_Test(ITestCase& testCase) override;
    void TearDown_Group() override;
    void TearDown_Test(ITestCase& testCase) override;
};


/////////////////////////////////////////////////////////////////////////////
// TTest_ASWIni_BaseFile
//
// TASWBaseIni's Load(), Save() and SaveAs(): with files, their failures and what they report, the modified flag, and
// the ReadFile() and WriteFile() seams
/////////////////////////////////////////////////////////////////////////////
class TTest_ASWIni_BaseFile : public TAutoTestGroup<TTest_ASWIni_BaseFile>
{
private:
    typedef TAutoTestGroup<TTest_ASWIni_BaseFile> inherited;

public: // Test methods, each defined with ASW_TEST
    void Test_EmptyPath_IsInvalidArgument();
    void Test_FileOperations_DontOverlap();
    void Test_Load_FileErrorsChangeNothing();
    void Test_Load_MissingFileEmptiesTheDocument();
    void Test_Load_MissingFolderIsCreatedBySave();
    void Test_Load_ReadsTheFile();
    void Test_Load_ReadsWithoutTheLock();
    void Test_OutOfMemory_LoadChangesNothing();
    void Test_OutOfMemory_SaveChangesNothing();
    void Test_SaveAs_SetsTheFilePath();
    void Test_Save_EditDuringSaveKeepsTheModifiedFlag();
    void Test_Save_NoFilePathFails();
#if defined(_WIN32)
    void Test_Save_ReadOnlyFileChangesNothing();
#endif
    void Test_Save_RoundTripsByteForByte();
    void Test_Save_WarningsAreReported();
    void Test_Save_WritesAndClearsTheModifiedFlag();
    void Test_Seams_ExceptionIsUnexpectedException();
    void Test_Seams_FailuresChangeNothing();
    void Test_Seams_FileNotFoundIsAMissingFile();
    void Test_Seams_GetTheirArguments();

public:
    TTest_ASWIni_BaseFile();
    ~TTest_ASWIni_BaseFile() override;

    void SetUp_Group() override;
    void SetUp_Test(ITestCase& testCase) override;
    void TearDown_Group() override;
    void TearDown_Test(ITestCase& testCase) override;
};

} // namespace ASWUnitTests

//---------------------------------------------------------------------------
#endif // #ifndef Test_ASWIni_BaseH
