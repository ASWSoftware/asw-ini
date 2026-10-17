/* **************************************************************************
Test_ASWIni_Converters.cpp
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
#include "Test_ASWIni_Converters.h"
//---------------------------------------------------------------------------
#include "ASWUnitTests_Registry.h"
//---------------------------------------------------------------------------
#include <string>
#include <string_view>
//---------------------------------------------------------------------------
#include "ASWIni_Converters.h"
//---------------------------------------------------------------------------

namespace ASWUnitTests
{

using ASWIni::EBoolFormat;
using ASWIni::EIniCode;
using ASWIni::TASWIniConverter;
using ASWIni::TASWIniResult;
using ASWIni::TASWIniValueOptions;

/////////////////////////////////////////////////////////////////////////////
// TTest_ASWIni_BoolConverter
/////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------
TTest_ASWIni_BoolConverter::TTest_ASWIni_BoolConverter()
    : inherited("ASWIni_BoolConverter_Tests")
{
}
//---------------------------------------------------------------------------
TTest_ASWIni_BoolConverter::~TTest_ASWIni_BoolConverter()
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_BoolConverter::CheckReads(std::string const& text, bool expected, std::string const& method,
    int line)
{
    // The value starts as the opposite, so a read that does nothing fails
    bool value = !expected;
    TASWIniResult const result = TASWIniConverter<bool>::FromText(text, value, TASWIniValueOptions());

    CheckTrue(result.IsOk(), method, line, "\"" + text + "\" should read: " + result.Message);
    CheckEquals(expected, value, method, line, "\"" + text + "\" should read as " + (expected ? "true" : "false"));
}
//---------------------------------------------------------------------------
void TTest_ASWIni_BoolConverter::CheckRejected(std::string const& text, std::string const& method, int line)
{
    for (bool const before : { false, true })
    {
        bool value = before;
        TASWIniResult const result = TASWIniConverter<bool>::FromText(text, value, TASWIniValueOptions());

        CheckEquals(EIniCode::InvalidValue, result.Code, method, line, "\"" + text + "\" should be InvalidValue");
        CheckContains(result.Message, ASWIni::Detail::MessageText(text), method, line,
            "the message should quote \"" + text + "\"");
        CheckEquals(before, value, method, line, "\"" + text + "\" should leave the value as it was");
    }
}
//---------------------------------------------------------------------------
void TTest_ASWIni_BoolConverter::SetUp_Group()
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_BoolConverter::SetUp_Test(ITestCase& /*testCase*/)
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_BoolConverter::TearDown_Group()
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_BoolConverter::TearDown_Test(ITestCase& /*testCase*/)
{
}
//---------------------------------------------------------------------------

// /////// Begin tests after this line ///////////////////////

//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_BoolConverter, FromText_IgnoresBlanksAroundTheText)
{
    // Act & Assert
    CheckReads(" true", true, __func__, __LINE__);
    CheckReads("no\t", false, __func__, __LINE__);
    CheckReads("\t 1 \t", true, __func__, __LINE__);
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_BoolConverter, FromText_IgnoresCase)
{
    // Act & Assert
    CheckReads("TRUE", true, __func__, __LINE__);
    CheckReads("Yes", true, __func__, __LINE__);
    CheckReads("oN", true, __func__, __LINE__);
    CheckReads("fALSE", false, __func__, __LINE__);
    CheckReads("NO", false, __func__, __LINE__);
    CheckReads("Off", false, __func__, __LINE__);
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_BoolConverter, FromText_ReadsEveryForm)
{
    // Act & Assert
    CheckReads("true", true, __func__, __LINE__);
    CheckReads("yes", true, __func__, __LINE__);
    CheckReads("on", true, __func__, __LINE__);
    CheckReads("1", true, __func__, __LINE__);
    CheckReads("false", false, __func__, __LINE__);
    CheckReads("no", false, __func__, __LINE__);
    CheckReads("off", false, __func__, __LINE__);
    CheckReads("0", false, __func__, __LINE__);
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_BoolConverter, FromText_RejectsOtherTexts)
{
    // Arrange
    // Only the eight forms, whole: no abbreviations, other numbers or words, or forms in quotes
    std::string const texts[] = {
        "", " ", "t", "f", "y", "n", "2", "-1", "01", "1.0", "tru", "truee", "true false", "o n", "enabled",
        "\"true\"", "\xC3\xA9", std::string("on\0", 3),
    };

    // Act & Assert
    for (std::string const& text : texts)
        CheckRejected(text, __func__, __LINE__);

    // The message says what a bool can be
    bool value = false;
    TASWIniResult const result = TASWIniConverter<bool>::FromText("maybe", value, TASWIniValueOptions());
    CheckEquals(std::string("The value \"maybe\" isn't a bool: it must be true, false, yes, no, on, off, 1 or 0."),
        result.Message, __func__, __LINE__, "the message should name the text and the forms");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_BoolConverter, RoundTrip_EveryFormat)
{
    // Arrange
    TASWIniValueOptions options;
    EBoolFormat const formats[] = {
        EBoolFormat::TrueFalse, EBoolFormat::YesNo, EBoolFormat::OnOff, EBoolFormat::OneZero,
    };

    // Act & Assert
    for (EBoolFormat const format : formats)
    {
        options.BoolFormat = format;
        for (bool const value : { false, true })
        {
            std::string text;
            bool readBack = !value;
            TASWIniResult const written = TASWIniConverter<bool>::ToText(value, text, options);
            TASWIniResult const read = TASWIniConverter<bool>::FromText(text, readBack, options);

            std::string const what = std::string(ASWIni::ToString(format)) + " " + (value ? "true" : "false");
            CheckTrue(written.IsOk() && read.IsOk(), __func__, __LINE__, what + " should be written and read");
            CheckEquals(value, readBack, __func__, __LINE__, what + " should read back as itself");
        }
    }
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_BoolConverter, ToText_UnknownFormatWritesTrueFalse)
{
    // Arrange
    // Options that weren't validated can hold a number that isn't an EBoolFormat
    TASWIniValueOptions options;
    options.BoolFormat = static_cast<EBoolFormat>(99);
    std::string yes;
    std::string no;

    // Act
    TASWIniResult const yesResult = TASWIniConverter<bool>::ToText(true, yes, options);
    TASWIniResult const noResult = TASWIniConverter<bool>::ToText(false, no, options);

    // Assert
    CheckTrue(yesResult.IsOk() && noResult.IsOk(), __func__, __LINE__, "writing a bool should never fail");
    CheckEquals(std::string("true"), yes, __func__, __LINE__, "true with an unknown format");
    CheckEquals(std::string("false"), no, __func__, __LINE__, "false with an unknown format");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_BoolConverter, ToText_WritesTheFormatsForm)
{
    // Arrange
    struct TCase
    {
        EBoolFormat Format;
        char const* True;
        char const* False;
    };

    TCase const cases[] = {
        { EBoolFormat::TrueFalse, "true", "false" }, { EBoolFormat::YesNo, "yes", "no" },
        { EBoolFormat::OnOff, "on", "off" }, { EBoolFormat::OneZero, "1", "0" },
    };

    // Act & Assert
    for (TCase const& c : cases)
    {
        TASWIniValueOptions options;
        options.BoolFormat = c.Format;
        std::string const format = ASWIni::ToString(c.Format);

        // The text is replaced, not appended to
        std::string text = "old";
        TASWIniResult result = TASWIniConverter<bool>::ToText(true, text, options);
        CheckTrue(result.IsOk(), __func__, __LINE__, format + ": writing true should succeed");
        CheckEquals(std::string(c.True), text, __func__, __LINE__, format + ": true");

        text = "old";
        result = TASWIniConverter<bool>::ToText(false, text, options);
        CheckTrue(result.IsOk(), __func__, __LINE__, format + ": writing false should succeed");
        CheckEquals(std::string(c.False), text, __func__, __LINE__, format + ": false");
    }
}
//---------------------------------------------------------------------------


/////////////////////////////////////////////////////////////////////////////
// TTest_ASWIni_TextConverter
/////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------
TTest_ASWIni_TextConverter::TTest_ASWIni_TextConverter()
    : inherited("ASWIni_TextConverter_Tests")
{
}
//---------------------------------------------------------------------------
TTest_ASWIni_TextConverter::~TTest_ASWIni_TextConverter()
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_TextConverter::SetUp_Group()
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_TextConverter::SetUp_Test(ITestCase& /*testCase*/)
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_TextConverter::TearDown_Group()
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_TextConverter::TearDown_Test(ITestCase& /*testCase*/)
{
}
//---------------------------------------------------------------------------

// /////// Begin tests after this line ///////////////////////

//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_TextConverter, String_FromTextKeepsTheTextExactly)
{
    // Arrange
    // Blanks, quotes, comment prefixes, NUL and UTF-8 all stay
    std::string const texts[] = {
        std::string(), " a ", "\t", "\"q\"", "x=y ; z", std::string("a\0b", 3), "\xC3\xA9t\xC3\xA9",
    };

    // Act & Assert
    // The previous value is replaced
    for (std::string const& text : texts)
    {
        std::string value = "previous";
        TASWIniResult const result = TASWIniConverter<std::string>::FromText(text, value, TASWIniValueOptions());

        CheckTrue(result.IsOk(), __func__, __LINE__, "reading a string should never fail");
        CheckEquals(text, value, __func__, __LINE__, "the value should be the text exactly");
    }
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_TextConverter, String_ToTextWritesTheValueExactly)
{
    // Act & Assert
    for (std::string const& value : { std::string(), std::string(" a "), std::string("\"q\""), std::string("a\0b", 3) })
    {
        std::string text = "previous";
        TASWIniResult const result = TASWIniConverter<std::string>::ToText(value, text, TASWIniValueOptions());

        CheckTrue(result.IsOk(), __func__, __LINE__, "writing a string should never fail");
        CheckEquals(value, text, __func__, __LINE__, "the text should be the value exactly");
    }
}
//---------------------------------------------------------------------------


/////////////////////////////////////////////////////////////////////////////
// TTest_ASWIni_Converters
/////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------
TTest_ASWIni_Converters::TTest_ASWIni_Converters()
    : inherited("ASWIni_Converters_Tests")
{
}
//---------------------------------------------------------------------------
TTest_ASWIni_Converters::~TTest_ASWIni_Converters()
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_Converters::SetUp_Group()
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_Converters::SetUp_Test(ITestCase& /*testCase*/)
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_Converters::TearDown_Group()
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_Converters::TearDown_Test(ITestCase& /*testCase*/)
{
}
//---------------------------------------------------------------------------

// /////// Begin tests after this line ///////////////////////

//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_Converters, MessageText_CutsLongTextsBetweenSequences)
{
    // Arrange
    std::size_t const max = ASWIni::Detail::MaxMessageTextSize;
    std::string const longest(max, 'a');
    std::string const tooLong(max + 1, 'a');
    // An e with an acute accent (2 bytes) that would be split by a cut after MaxMessageTextSize bytes
    std::string const splitSequence = std::string(max - 1, 'a') + "\xC3\xA9" "b";

    // Act & Assert
    CheckEquals("\"" + longest + "\"", ASWIni::Detail::MessageText(longest), __func__, __LINE__,
        "a text of MaxMessageTextSize bytes should be whole");
    CheckEquals("\"" + longest + "\"...", ASWIni::Detail::MessageText(tooLong), __func__, __LINE__,
        "a longer text should be cut to MaxMessageTextSize bytes");
    CheckEquals("\"" + std::string(max - 1, 'a') + "\"...", ASWIni::Detail::MessageText(splitSequence), __func__,
        __LINE__, "a cut should go before a UTF-8 sequence it would split");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_Converters, MessageText_QuotesTheText)
{
    // Act & Assert
    CheckEquals(std::string("\"abc\""), ASWIni::Detail::MessageText("abc"), __func__, __LINE__, "a text");
    CheckEquals(std::string("\"\""), ASWIni::Detail::MessageText(""), __func__, __LINE__, "an empty text");
    CheckEquals(std::string("\" \xC3\xA9 \""), ASWIni::Detail::MessageText(" \xC3\xA9 "), __func__, __LINE__,
        "blanks and UTF-8 should stay");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_Converters, MessageText_ReplacesControlCharacters)
{
    // Arrange
    std::string const text = std::string("a\nb\tc\x7F" "d\r", 8) + std::string(1, '\0') + "\x1F";

    // Act
    std::string const message = ASWIni::Detail::MessageText(text);

    // Assert
    CheckEquals(std::string("\"a?b?c?d???\""), message, __func__, __LINE__,
        "each control character should be '?', so a message stays one line of text");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_Converters, ValueFailure_MakesTheMessage)
{
    // Act
    TASWIniResult const result = ASWIni::Detail::ValueFailure(EIniCode::ValueOutOfRange, "300",
        "is out of range: it must be 0 to 255.");

    // Assert
    CheckEquals(EIniCode::ValueOutOfRange, result.Code, __func__, __LINE__, "the code");
    CheckEquals(std::string("The value \"300\" is out of range: it must be 0 to 255."), result.Message, __func__,
        __LINE__, "the message should quote the text, then name the problem");
}
//---------------------------------------------------------------------------

} // namespace ASWUnitTests

//---------------------------------------------------------------------------
ASW_REGISTER_TEST_GROUP_IN_SUITE(ASWUnitTests::TTest_ASWIni_BoolConverter, "Converters")
ASW_REGISTER_TEST_GROUP_IN_SUITE(ASWUnitTests::TTest_ASWIni_Converters, "Converters")
ASW_REGISTER_TEST_GROUP_IN_SUITE(ASWUnitTests::TTest_ASWIni_TextConverter, "Converters")
