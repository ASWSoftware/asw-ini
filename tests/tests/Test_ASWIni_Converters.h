/* **************************************************************************
Test_ASWIni_Converters.h
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
#ifndef Test_ASWIni_ConvertersH
#define Test_ASWIni_ConvertersH
//---------------------------------------------------------------------------
#include <string>
//---------------------------------------------------------------------------
#include "ASWUnitTests_TestBase.h"
//---------------------------------------------------------------------------
#include "ASWIni_Types.h"
//---------------------------------------------------------------------------

namespace ASWUnitTests
{

/////////////////////////////////////////////////////////////////////////////
// TTest_ASWIni_BoolConverter
/////////////////////////////////////////////////////////////////////////////
class TTest_ASWIni_BoolConverter : public TAutoTestGroup<TTest_ASWIni_BoolConverter>
{
private:
    typedef TAutoTestGroup<TTest_ASWIni_BoolConverter> inherited;

private:
    // Checks that 'text' reads as 'expected'
    void CheckReads(std::string const& text, bool expected, std::string const& method, int line);

    // Checks that 'text' fails to read with InvalidValue and a message that quotes it, leaving the value as it was
    void CheckRejected(std::string const& text, std::string const& method, int line);

public: // Test methods, each defined with ASW_TEST
    void Test_FromText_IgnoresBlanksAroundTheText();
    void Test_FromText_IgnoresCase();
    void Test_FromText_ReadsEveryForm();
    void Test_FromText_RejectsOtherTexts();
    void Test_RoundTrip_EveryFormat();
    void Test_ToText_UnknownFormatWritesTrueFalse();
    void Test_ToText_WritesTheFormatsForm();

public:
    TTest_ASWIni_BoolConverter();
    ~TTest_ASWIni_BoolConverter() override;

    void SetUp_Group() override;
    void SetUp_Test(ITestCase& testCase) override;
    void TearDown_Group() override;
    void TearDown_Test(ITestCase& testCase) override;
};


/////////////////////////////////////////////////////////////////////////////
// TTest_ASWIni_IntegerConverter
/////////////////////////////////////////////////////////////////////////////
class TTest_ASWIni_IntegerConverter : public TAutoTestGroup<TTest_ASWIni_IntegerConverter>
{
private:
    typedef TAutoTestGroup<TTest_ASWIni_IntegerConverter> inherited;

private:
    // Checks T's limits: its smallest and largest values (in decimal and hex) read as themselves, and one less and one
    // more are ValueOutOfRange with a message that names the range
    template <typename T>
    void CheckLimits(std::string const& typeName, std::string const& method, int line);

    // Checks that 'text' reads as 'expected' of type T
    template <typename T>
    void CheckReads(std::string const& text, T expected, std::string const& method, int line);

    // Checks that 'text' fails to read as T with 'code' and a message that quotes it, leaving the value as it was
    template <typename T>
    void CheckRejected(std::string const& text, ASWIni::EIniCode code, std::string const& method, int line);

    // Checks that T's smallest and largest values, 0, 1 and (for a signed T) -1 read back as themselves after
    // ToText()
    template <typename T>
    void CheckRoundTrip(std::string const& typeName, std::string const& method, int line);

public: // Test methods, each defined with ASW_TEST
    void Test_FromText_HexIsAValueNotABitPattern();
    void Test_FromText_HugeNumbersAreOutOfRange();
    void Test_FromText_IgnoresBlanksAroundTheText();
    void Test_FromText_ReadsDecimal();
    void Test_FromText_ReadsEachTypesLimits();
    void Test_FromText_ReadsHex();
    void Test_FromText_RejectsOtherTexts();
    void Test_FromText_UnsignedRejectsNegatives();
    void Test_Messages_NameTheProblem();
    void Test_RoundTrip_EveryType();
    void Test_ToText_WritesDecimal();

public:
    TTest_ASWIni_IntegerConverter();
    ~TTest_ASWIni_IntegerConverter() override;

    void SetUp_Group() override;
    void SetUp_Test(ITestCase& testCase) override;
    void TearDown_Group() override;
    void TearDown_Test(ITestCase& testCase) override;
};


/////////////////////////////////////////////////////////////////////////////
// TTest_ASWIni_TextConverter
/////////////////////////////////////////////////////////////////////////////
class TTest_ASWIni_TextConverter : public TAutoTestGroup<TTest_ASWIni_TextConverter>
{
private:
    typedef TAutoTestGroup<TTest_ASWIni_TextConverter> inherited;

public: // Test methods, each defined with ASW_TEST
    void Test_String_FromTextKeepsTheTextExactly();
    void Test_String_ToTextWritesTheValueExactly();

public:
    TTest_ASWIni_TextConverter();
    ~TTest_ASWIni_TextConverter() override;

    void SetUp_Group() override;
    void SetUp_Test(ITestCase& testCase) override;
    void TearDown_Group() override;
    void TearDown_Test(ITestCase& testCase) override;
};


/////////////////////////////////////////////////////////////////////////////
// TTest_ASWIni_Converters
/////////////////////////////////////////////////////////////////////////////
class TTest_ASWIni_Converters : public TAutoTestGroup<TTest_ASWIni_Converters>
{
private:
    typedef TAutoTestGroup<TTest_ASWIni_Converters> inherited;

public: // Test methods, each defined with ASW_TEST
    void Test_MessageText_CutsLongTextsBetweenSequences();
    void Test_MessageText_QuotesTheText();
    void Test_MessageText_ReplacesControlCharacters();
    void Test_ValueFailure_MakesTheMessage();

public:
    TTest_ASWIni_Converters();
    ~TTest_ASWIni_Converters() override;

    void SetUp_Group() override;
    void SetUp_Test(ITestCase& testCase) override;
    void TearDown_Group() override;
    void TearDown_Test(ITestCase& testCase) override;
};

} // namespace ASWUnitTests

//---------------------------------------------------------------------------
#endif // #ifndef Test_ASWIni_ConvertersH
