/* **************************************************************************
Test_ASWIni_Document.h
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
#ifndef Test_ASWIni_DocumentH
#define Test_ASWIni_DocumentH
//---------------------------------------------------------------------------
#include <cstddef>
#include <string>
#include <string_view>
//---------------------------------------------------------------------------
#include "ASWUnitTests_TestBase.h"
//---------------------------------------------------------------------------
#include "ASWIni_Document.h"
#include "ASWIni_Options.h"
#include "ASWIni_Types.h"
//---------------------------------------------------------------------------

namespace ASWUnitTests
{

/////////////////////////////////////////////////////////////////////////////
// TTest_ASWIni_DocumentModel
/////////////////////////////////////////////////////////////////////////////
class TTest_ASWIni_DocumentModel : public TAutoTestGroup<TTest_ASWIni_DocumentModel>
{
private:
    typedef TAutoTestGroup<TTest_ASWIni_DocumentModel> inherited;

public: // Test methods, each defined with ASW_TEST
    void Test_Document_CopyIsIndependent();
    void Test_Document_DefaultIsEmpty();
    void Test_Line_DefaultIsBlank();
    void Test_Section_DefaultIsEmptyPreamble();
    void Test_ToString_LineKindNames();

public:
    TTest_ASWIni_DocumentModel();
    ~TTest_ASWIni_DocumentModel() override;

    void SetUp_Group() override;
    void SetUp_Test(ITestCase& testCase) override;
    void TearDown_Group() override;
    void TearDown_Test(ITestCase& testCase) override;
};


/////////////////////////////////////////////////////////////////////////////
// TTest_ASWIni_DocumentParse
/////////////////////////////////////////////////////////////////////////////
class TTest_ASWIni_DocumentParse : public TAutoTestGroup<TTest_ASWIni_DocumentParse>
{
private:
    typedef TAutoTestGroup<TTest_ASWIni_DocumentParse> inherited;

private:
    // Checks that 'line' is a KeyValue line with 'key' and 'value'
    void CheckKeyValue(ASWIni::TASWIniDocumentLine const& line, std::string const& key, std::string const& value,
        std::string const& method, int lineNumber, std::string const& msg);

    // 'text' parsed with 'options', checking that it parses with no messages
    ASWIni::TASWIniDocument ParseValid(std::string_view text, ASWIni::TASWIniReadingOptions const& options,
        std::string const& method, int lineNumber);

public: // Test methods, each defined with ASW_TEST
    void Test_Bom_IsDetectedAndNotPartOfALine();
    void Test_CommentPrefixes_AreConfigurable();
    void Test_Comments_KeepTheirText();
    void Test_EmptyText_HasNoLines();
    void Test_InlineComments_AreOffByDefault();
    void Test_InlineComments_NeedABlankBeforeThePrefix();
    void Test_KeyValue_FirstSeparatorEndsTheKey();
    void Test_KeyValue_KeepsNulAndLongValues();
    void Test_KeyValue_KeyAndValueAreTrimmed();
    void Test_LineBreak_IsTheMostCommonOne();
    void Test_LineBreaks_AreKeptForEachLine();
    void Test_Parse_ReplacesPreviousContents();
    void Test_QuotedValues_AreOffByDefault();
    void Test_QuotedValues_LoseTheirQuotes();
    void Test_Sections_DuplicatesAreSeparate();
    void Test_Sections_HoldTheLinesAfterTheirHeader();
    void Test_Sections_KeepATrailingComment();
    void Test_Sections_NamesAreTrimmedAndFlat();

public:
    TTest_ASWIni_DocumentParse();
    ~TTest_ASWIni_DocumentParse() override;

    void SetUp_Group() override;
    void SetUp_Test(ITestCase& testCase) override;
    void TearDown_Group() override;
    void TearDown_Test(ITestCase& testCase) override;
};


/////////////////////////////////////////////////////////////////////////////
// TTest_ASWIni_DocumentMessages
/////////////////////////////////////////////////////////////////////////////
class TTest_ASWIni_DocumentMessages : public TAutoTestGroup<TTest_ASWIni_DocumentMessages>
{
private:
    typedef TAutoTestGroup<TTest_ASWIni_DocumentMessages> inherited;

private:
    // Checks that parsing 'text' with 'options' fails with 'code', leaving a parsed document and its messages as they
    // were
    void CheckParseFails(std::string_view text, ASWIni::TASWIniReadingOptions const& options, ASWIni::EIniCode code,
        std::string const& method, int lineNumber, std::string const& msg);

    // Checks that 'message' is a warning with 'code' about line 'line' of section 'section'
    void CheckWarning(ASWIni::TASWIniMessage const& message, ASWIni::EIniCode code, std::size_t line,
        std::string const& section, std::string const& method, int lineNumber, std::string const& msg);

public: // Test methods, each defined with ASW_TEST
    void Test_DuplicateKey_IsReportedWithTheFirstLine();
    void Test_DuplicateKey_MatchesCaseSensitiveAndNamesTheWinner();
    void Test_DuplicateSection_IsReportedWithTheFirstLine();
    void Test_EmptyKey_IsMalformed();
    void Test_EmptySectionName_IsMalformed();
    void Test_InvalidOptions_Fail();
    void Test_InvalidUtf8_IsReportedOnce();
    void Test_NoSeparator_IsMalformed();
    void Test_StrictParsing_FailsAtTheFirstMalformedLine();
    void Test_StrictParsing_KeepsOtherWarnings();
    void Test_TextAfterSectionHeader_IsMalformedButUsed();
    void Test_UnclosedSection_IsRawAndKeysStayInThePreviousSection();
    void Test_UnsupportedEncoding_Fails();
    void Test_ValidText_HasNoMessages();
    void Test_Warnings_AreAppended();

public:
    TTest_ASWIni_DocumentMessages();
    ~TTest_ASWIni_DocumentMessages() override;

    void SetUp_Group() override;
    void SetUp_Test(ITestCase& testCase) override;
    void TearDown_Group() override;
    void TearDown_Test(ITestCase& testCase) override;
};

} // namespace ASWUnitTests

//---------------------------------------------------------------------------
#endif // #ifndef Test_ASWIni_DocumentH
