/* **************************************************************************
Test_ASWIni_Interface.h
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
#ifndef Test_ASWIni_InterfaceH
#define Test_ASWIni_InterfaceH
//---------------------------------------------------------------------------
#include "ASWUnitTests_TestBase.h"
//---------------------------------------------------------------------------

namespace ASWUnitTests
{

/////////////////////////////////////////////////////////////////////////////
// TTest_ASWIni_Interface
//
// The typed GetValue(), TryGetValue() and SetValue() of IASWIni, over a fake IASWIni that keeps its values in a map
/////////////////////////////////////////////////////////////////////////////
class TTest_ASWIni_Interface : public TAutoTestGroup<TTest_ASWIni_Interface>
{
private:
    typedef TAutoTestGroup<TTest_ASWIni_Interface> inherited;

public: // Test methods, each defined with ASW_TEST
    void Test_GetValue_ConverterExceptionIsThrown();
    void Test_GetValue_ConvertsTheValue();
    void Test_GetValue_InvalidValueGivesTheDefaultAndAWarning();
    void Test_GetValue_MissingKeyGivesTheDefault();
    void Test_GetValue_OutOfRangeValueIsAWarning();
    void Test_GetValue_TextDefaultGivesTheText();
    void Test_GetValue_TypeWithoutDefaultConstructor();
    void Test_GetValue_UsesTheGivenValueOptions();
    void Test_GetValue_UsesTheValueOptions();
    void Test_SetValue_ConversionFailureIsReported();
    void Test_SetValue_ConverterExceptionIsReported();
    void Test_SetValue_ConvertsTheValue();
    void Test_SetValue_SetTextFailureIsReturned();
    void Test_SetValue_TextIsSetAsItIs();
    void Test_SetValue_UsesTheGivenValueOptions();
    void Test_SetValue_UsesTheValueOptions();
    void Test_TryGetValue_ConverterExceptionIsReturned();
    void Test_TryGetValue_ConvertsTheValue();
    void Test_TryGetValue_InvalidValueKeepsTheValue();
    void Test_TryGetValue_MissingKeyIsNotFound();
    void Test_TryGetValue_UsesTheGivenValueOptions();
    void Test_TryGetValue_UsesTheValueOptions();

public:
    TTest_ASWIni_Interface();
    ~TTest_ASWIni_Interface() override;

    void SetUp_Group() override;
    void SetUp_Test(ITestCase& testCase) override;
    void TearDown_Group() override;
    void TearDown_Test(ITestCase& testCase) override;
};

} // namespace ASWUnitTests

//---------------------------------------------------------------------------
#endif // #ifndef Test_ASWIni_InterfaceH
