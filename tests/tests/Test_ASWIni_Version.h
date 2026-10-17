/* **************************************************************************
Test_ASWIni_Version.h
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
#ifndef Test_ASWIni_VersionH
#define Test_ASWIni_VersionH
//---------------------------------------------------------------------------
#include "ASWUnitTests_TestBase.h"
//---------------------------------------------------------------------------

namespace ASWUnitTests
{

/////////////////////////////////////////////////////////////////////////////
// TTest_ASWIni_Version
/////////////////////////////////////////////////////////////////////////////
class TTest_ASWIni_Version : public TAutoTestGroup<TTest_ASWIni_Version>
{
private:
    typedef TAutoTestGroup<TTest_ASWIni_Version> inherited;

public: // Test methods, each defined with ASW_TEST
    void Test_Constants_MatchMacros();
    void Test_PreRelease_IsValidSemVer();
    void Test_VersionString_MatchesParts();

public:
    TTest_ASWIni_Version();
    ~TTest_ASWIni_Version() override;

    void SetUp_Group() override;
    void SetUp_Test(ITestCase& testCase) override;
    void TearDown_Group() override;
    void TearDown_Test(ITestCase& testCase) override;
};

} // namespace ASWUnitTests

//---------------------------------------------------------------------------
#endif // #ifndef Test_ASWIni_VersionH
