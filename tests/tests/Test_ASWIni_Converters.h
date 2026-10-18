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
#include <initializer_list>
#include <string>
#include <vector>
//---------------------------------------------------------------------------
#include "ASWUnitTests_TestBase.h"
//---------------------------------------------------------------------------
#include "ASWIni_Options.h"
#include "ASWIni_Platform.h"
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
// TTest_ASWIni_EnumConverter
/////////////////////////////////////////////////////////////////////////////
class TTest_ASWIni_EnumConverter : public TAutoTestGroup<TTest_ASWIni_EnumConverter>
{
private:
    typedef TAutoTestGroup<TTest_ASWIni_EnumConverter> inherited;

private:
    // Checks that 'text' reads as 'expected' of the enum T
    template <typename T>
    void CheckReads(std::string const& text, T expected, std::string const& method, int line);

    // Checks that 'text' fails to read as the enum T with 'code' and a message that quotes it, leaving the value as it
    // was
    template <typename T>
    void CheckRejected(std::string const& text, ASWIni::EIniCode code, std::string const& method, int line);

    // Checks that each of 'values' of the enum T is written as a text that reads back as itself
    template <typename T>
    void CheckRoundTrip(std::initializer_list<T> values, std::string const& method, int line);

public: // Test methods, each defined with ASW_TEST
    void Test_FromText_IgnoresCaseAndBlanks();
    void Test_FromText_ReadsEveryName();
    void Test_FromText_ReadsTheNumberOfAListedValue();
    void Test_FromText_RejectsOtherTexts();
    void Test_Messages_NameTheProblem();
    void Test_RoundTrip_EveryListedValue();
    void Test_ToText_UnnamedValueIsInvalid();
    void Test_ToText_WritesTheFirstName();
    void Test_WithoutNames_ConvertsAsTheUnderlyingType();
    void Test_WithoutNames_RejectsWhatTheUnderlyingTypeRejects();

public:
    TTest_ASWIni_EnumConverter();
    ~TTest_ASWIni_EnumConverter() override;

    void SetUp_Group() override;
    void SetUp_Test(ITestCase& testCase) override;
    void TearDown_Group() override;
    void TearDown_Test(ITestCase& testCase) override;
};


/////////////////////////////////////////////////////////////////////////////
// TTest_ASWIni_FloatConverter
//
// Most tests check every float conversion (see ASWIni::Detail::EFloatConversion), not only the one the converters use
/////////////////////////////////////////////////////////////////////////////
class TTest_ASWIni_FloatConverter : public TAutoTestGroup<TTest_ASWIni_FloatConverter>
{
private:
    typedef TAutoTestGroup<TTest_ASWIni_FloatConverter> inherited;

private:
    // Checks that 'text' reads as 'expected' (the same bits, or a NaN) with every conversion
    template <typename T>
    void CheckReads(std::string const& text, T expected, std::string const& method, int line);

    // Checks that 'text' fails to read with 'code' and a message that quotes it, leaving the value as it was, with
    // every conversion
    template <typename T>
    void CheckRejected(std::string const& text, ASWIni::EIniCode code, std::string const& method, int line);

    // Checks that random values of type T (with the bits TBits) are written as texts that read back as the same bits
    // with every conversion, and that every conversion writes the text the Standard one does (or, with strtold(),
    // sometimes a longer one that the Standard one reads back as the same bits)
    template <typename T, typename TBits>
    void CheckRandomValues(std::string const& typeName, std::string const& method, int line);

    // Checks that 'value' is written as 'expected' with every conversion
    template <typename T>
    void CheckWrites(T value, std::string const& expected, std::string const& method, int line);

public: // Test methods, each defined with ASW_TEST
    void Test_Converters_UseTheDefaultConversion();
    void Test_FromText_IgnoresBlanksAroundTheText();
    void Test_FromText_ReadsDecimalForms();
    void Test_FromText_ReadsInfinityAndNan();
    void Test_FromText_ReadsTheLimits();
    void Test_FromText_RejectsOtherTexts();
    void Test_FromText_TooLargeIsOutOfRange();
    void Test_FromText_TooSmallReadsAsZero();
    void Test_Locale_DecimalCommaChangesNothing();
    void Test_Messages_NameTheProblem();
    void Test_RandomValues_RoundTripWithEveryConversion();
    void Test_ToText_WritesInfinityAndNan();
    void Test_ToText_WritesTheShortestText();

public:
    TTest_ASWIni_FloatConverter();
    ~TTest_ASWIni_FloatConverter() override;

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
// TTest_ASWIni_ListConverter
/////////////////////////////////////////////////////////////////////////////
class TTest_ASWIni_ListConverter : public TAutoTestGroup<TTest_ASWIni_ListConverter>
{
private:
    typedef TAutoTestGroup<TTest_ASWIni_ListConverter> inherited;

private:
    ASWIni::TASWIniValueOptions m_Options; // What the checks convert with; the defaults at the start of each test

private:
    // Checks that 'text' reads as the list 'expected'
    template <typename T>
    void CheckReads(std::string const& text, std::vector<T> const& expected, std::string const& method, int line);

    // Checks that 'text' fails to read as a list of T with 'code' and a message that quotes it, leaving the list as it
    // was
    template <typename T>
    void CheckRejected(std::string const& text, ASWIni::EIniCode code, std::string const& method, int line);

    // Checks that the list 'value' is written as a text that reads back as itself
    template <typename T>
    void CheckRoundTrip(std::vector<T> const& value, std::string const& method, int line);

    // Checks that the list 'value' is written as 'expected'
    template <typename T>
    void CheckWrites(std::vector<T> const& value, std::string const& expected, std::string const& method, int line);

public: // Test methods, each defined with ASW_TEST
    void Test_Delimiter_IsTheOptionsOne();
    void Test_Document_ListsReadBackWithQuotedValuesOnAndOff();
    void Test_FromText_EmptyTextIsAnEmptyList();
    void Test_FromText_FailingItemFailsTheList();
    void Test_FromText_QuoteInsideAnItemIsOrdinary();
    void Test_FromText_ReadsQuotedItems();
    void Test_FromText_RejectsBadQuoting();
    void Test_FromText_SplitsAtTheDelimiter();
    void Test_ItemTypes_BoolEnumAndNumbers();
    void Test_Nested_ListsOfLists();
    void Test_RoundTrip_EveryCombinationOfTrickyItems();
    void Test_ToText_FailingItemFailsTheList();
    void Test_ToText_JoinsWithTheDelimiterAndASpace();
    void Test_ToText_QuotesOnlyWhenNeeded();

public:
    TTest_ASWIni_ListConverter();
    ~TTest_ASWIni_ListConverter() override;

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
    void Test_Path_FromTextIsUtf8OnEveryPlatform();
    void Test_Path_FromTextKeepsTheTextExactly();
    void Test_Path_FromTextRejectsInvalidUtf8();
    void Test_Path_RoundTripsInAList();
    void Test_Path_ToTextRejectsWhatWouldNotReadBack();
    void Test_Path_ToTextWritesUtf8();
    void Test_String_FromTextKeepsTheTextExactly();
    void Test_String_ToTextWritesTheValueExactly();
#if ASWINI_HAS_U8STRING
    void Test_U8String_FromTextKeepsTheBytesExactly();
    void Test_U8String_RoundTripsInAList();
    void Test_U8String_ToTextWritesTheBytesExactly();
#endif

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
