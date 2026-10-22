/* **************************************************************************
Test_ASWIni_File.h
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
#ifndef Test_ASWIni_FileH
#define Test_ASWIni_FileH
//---------------------------------------------------------------------------
#include "ASWUnitTests_TestBase.h"
//---------------------------------------------------------------------------

namespace ASWUnitTests
{

/////////////////////////////////////////////////////////////////////////////
// TTest_ASWIni_File
/////////////////////////////////////////////////////////////////////////////
class TTest_ASWIni_File : public TAutoTestGroup<TTest_ASWIni_File>
{
private:
    typedef TAutoTestGroup<TTest_ASWIni_File> inherited;

public: // Test methods, each defined with ASW_TEST
    void Test_PathToUtf8_RoundTripsAscii();
    void Test_ReadWholeFile_EmptyFile();
    void Test_ReadWholeFile_EmptyPath();
    void Test_ReadWholeFile_Folder();
    void Test_ReadWholeFile_LargerThanMaxSize();
    void Test_ReadWholeFile_MissingFile();
    void Test_ReadWholeFile_MissingFolder();
    void Test_ReadWholeFile_ReadsExactBytes();
    void Test_Utf8ToPath_ConvertsFourByteSequence();
    void Test_Utf8ToPath_ConvertsNonAscii();
    void Test_WriteWholeFile_Backup_CopiesPreviousFile();
    void Test_WriteWholeFile_Backup_SkippedForNewFile();
    void Test_WriteWholeFile_CreatesFile();
    void Test_WriteWholeFile_CreatesMissingFolders();
    void Test_WriteWholeFile_EmptyPath();
    void Test_WriteWholeFile_FailedReplace_KeepsTargetAndRemovesTemp();
    void Test_WriteWholeFile_InPlace_OverwritesAndTruncates();
#if defined(_WIN32)
    void Test_WriteWholeFile_KeepsHiddenAttribute();
#endif
#if !defined(_WIN32)
    void Test_WriteWholeFile_KeepsPermissions();
    void Test_WriteWholeFile_KeepsSymbolicLink();
#endif
    void Test_WriteWholeFile_NonAsciiPath();
#if defined(_WIN32)
    void Test_WriteWholeFile_ReadOnlyFile_Fails();
#endif
    void Test_WriteWholeFile_ReplacesFile();
    void Test_WriteWholeFile_WithoutCreateFolders_FailsForMissingFolder();

public:
    TTest_ASWIni_File();
    ~TTest_ASWIni_File() override;

    void SetUp_Group() override;
    void SetUp_Test(ITestCase& testCase) override;
    void TearDown_Group() override;
    void TearDown_Test(ITestCase& testCase) override;
};

} // namespace ASWUnitTests

//---------------------------------------------------------------------------
#endif // #ifndef Test_ASWIni_FileH
