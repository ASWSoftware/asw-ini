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
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>
//---------------------------------------------------------------------------
#include "ASWIni_Converters.h"
//---------------------------------------------------------------------------

// Only the integral types that aren't bool or wide or Unicode characters are converted as integers. Plain char is one
// (bcc32c's std::int8_t is char).
static_assert(ASWIni::Detail::IsConvertibleInteger<char>, "char should be an integer");
static_assert(ASWIni::Detail::IsConvertibleInteger<signed char>, "signed char should be an integer");
static_assert(ASWIni::Detail::IsConvertibleInteger<unsigned char>, "unsigned char should be an integer");
static_assert(ASWIni::Detail::IsConvertibleInteger<std::int8_t>, "std::int8_t should be an integer");
static_assert(ASWIni::Detail::IsConvertibleInteger<std::uint64_t>, "std::uint64_t should be an integer");
static_assert(!ASWIni::Detail::IsConvertibleInteger<bool>, "bool has its own converter");
static_assert(!ASWIni::Detail::IsConvertibleInteger<wchar_t>, "wchar_t is a character");
static_assert(!ASWIni::Detail::IsConvertibleInteger<char16_t>, "char16_t is a character");
static_assert(!ASWIni::Detail::IsConvertibleInteger<char32_t>, "char32_t is a character");
#if defined(__cpp_char8_t)
static_assert(!ASWIni::Detail::IsConvertibleInteger<char8_t>, "char8_t is a character");
#endif
static_assert(!ASWIni::Detail::IsConvertibleInteger<double>, "double isn't an integer");

namespace ASWUnitTests
{

using ASWIni::EBoolFormat;
using ASWIni::EIniCode;
using ASWIni::TASWIniConverter;
using ASWIni::TASWIniResult;
using ASWIni::TASWIniValueOptions;

namespace
{

std::string AddOne(std::string digits);
std::string HexText(unsigned long long value);
template <typename T>
std::string NumberText(T value);
template <typename T>
std::string Written(T value);

//---------------------------------------------------------------------------
/*
    AddOne

    'digits', a decimal number without a sign, plus one, e.g. "199" -> "200".
*/
std::string AddOne(std::string digits)
{
    std::size_t i = digits.size();
    while (i > 0 && digits[i - 1] == '9')
    {
        digits[i - 1] = '0';
        --i;
    }

    if (i == 0)
        digits.insert(0, 1, '1');
    else
        ++digits[i - 1];

    return digits;
}
//---------------------------------------------------------------------------
/*
    HexText

    'value' as "0x" and upper case hex digits, e.g. "0xFF".
*/
std::string HexText(unsigned long long value)
{
    std::string digits;
    while (digits.empty() || value > 0)
    {
        digits.insert(0, 1, "0123456789ABCDEF"[value % 16]);
        value /= 16;
    }

    return "0x" + digits;
}
//---------------------------------------------------------------------------
/*
    NumberText

    'value' as decimal digits (a signed char or unsigned char as a number, not a character).
*/
template <typename T>
std::string NumberText(T value)
{
    return std::to_string(+value);
}
//---------------------------------------------------------------------------
/*
    Written

    The text TASWIniConverter<T>::ToText() writes for 'value' (replacing "old"), or "failed: " and the message.
*/
template <typename T>
std::string Written(T value)
{
    std::string text = "old";
    TASWIniResult const result = TASWIniConverter<T>::ToText(value, text, TASWIniValueOptions());

    return result.IsOk() ? text : "failed: " + result.Message;
}
//---------------------------------------------------------------------------

} // namespace

//---------------------------------------------------------------------------

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
// TTest_ASWIni_IntegerConverter
/////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------
TTest_ASWIni_IntegerConverter::TTest_ASWIni_IntegerConverter()
    : inherited("ASWIni_IntegerConverter_Tests")
{
}
//---------------------------------------------------------------------------
TTest_ASWIni_IntegerConverter::~TTest_ASWIni_IntegerConverter()
{
}
//---------------------------------------------------------------------------
template <typename T>
void TTest_ASWIni_IntegerConverter::CheckLimits(std::string const& typeName, std::string const& method, int line)
{
    T const min = std::numeric_limits<T>::min();
    T const max = std::numeric_limits<T>::max();
    std::string const minText = NumberText(min);
    std::string const maxText = NumberText(max);

    CheckReads<T>(minText, min, method, line);
    CheckReads<T>(maxText, max, method, line);
    CheckReads<T>(HexText(static_cast<unsigned long long>(max)), max, method, line);

    // One less than the smallest value, and one more than the largest
    std::string const below = std::is_signed_v<T> ? "-" + AddOne(minText.substr(1)) : std::string("-1");
    std::string const above = AddOne(maxText);
    CheckRejected<T>(below, EIniCode::ValueOutOfRange, method, line);
    CheckRejected<T>(above, EIniCode::ValueOutOfRange, method, line);

    T value = 0;
    TASWIniResult const result = TASWIniConverter<T>::FromText(above, value, TASWIniValueOptions());
    CheckContains(result.Message, "it must be " + minText + " to " + maxText + ".", method, line,
        typeName + ": the message should name the range");
}
//---------------------------------------------------------------------------
template <typename T>
void TTest_ASWIni_IntegerConverter::CheckReads(std::string const& text, T expected, std::string const& method,
    int line)
{
    // The value starts as something else, so a read that does nothing fails
    T value = static_cast<T>(expected == 0 ? 1 : 0);
    TASWIniResult const result = TASWIniConverter<T>::FromText(text, value, TASWIniValueOptions());

    CheckTrue(result.IsOk(), method, line, "\"" + text + "\" should read: " + result.Message);
    CheckTrue(value == expected, method, line, "\"" + text + "\" should read as " + NumberText(expected) + ", not " +
        NumberText(value));
}
//---------------------------------------------------------------------------
template <typename T>
void TTest_ASWIni_IntegerConverter::CheckRejected(std::string const& text, EIniCode code, std::string const& method,
    int line)
{
    for (T const before : { static_cast<T>(0), static_cast<T>(1) })
    {
        T value = before;
        TASWIniResult const result = TASWIniConverter<T>::FromText(text, value, TASWIniValueOptions());

        CheckEquals(code, result.Code, method, line, "\"" + text + "\" should be " + ASWIni::ToString(code));
        CheckContains(result.Message, ASWIni::Detail::MessageText(text), method, line,
            "the message should quote \"" + text + "\"");
        CheckTrue(value == before, method, line, "\"" + text + "\" should leave the value as it was");
    }
}
//---------------------------------------------------------------------------
template <typename T>
void TTest_ASWIni_IntegerConverter::CheckRoundTrip(std::string const& typeName, std::string const& method, int line)
{
    std::vector<T> values = { std::numeric_limits<T>::min(), std::numeric_limits<T>::max(), 0, 1 };
    if constexpr (std::is_signed_v<T>)
        values.push_back(static_cast<T>(-1));

    for (T const value : values)
    {
        std::string text;
        T readBack = static_cast<T>(value == 0 ? 1 : 0);
        TASWIniResult const written = TASWIniConverter<T>::ToText(value, text, TASWIniValueOptions());
        TASWIniResult const read = TASWIniConverter<T>::FromText(text, readBack, TASWIniValueOptions());

        std::string const what = typeName + " " + NumberText(value);
        CheckTrue(written.IsOk() && read.IsOk(), method, line, what + " should be written and read");
        CheckTrue(readBack == value, method, line, what + " should read back as itself, not " + NumberText(readBack));
    }
}
//---------------------------------------------------------------------------
void TTest_ASWIni_IntegerConverter::SetUp_Group()
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_IntegerConverter::SetUp_Test(ITestCase& /*testCase*/)
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_IntegerConverter::TearDown_Group()
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_IntegerConverter::TearDown_Test(ITestCase& /*testCase*/)
{
}
//---------------------------------------------------------------------------

// /////// Begin tests after this line ///////////////////////

//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_IntegerConverter, FromText_HexIsAValueNotABitPattern)
{
    // Act & Assert
    CheckRejected<std::int32_t>("0xFFFFFFFF", EIniCode::ValueOutOfRange, __func__, __LINE__);
    CheckReads<std::uint32_t>("0xFFFFFFFF", 4294967295u, __func__, __LINE__);
    CheckRejected<std::int8_t>("0x80", EIniCode::ValueOutOfRange, __func__, __LINE__);
    CheckReads<std::int8_t>("-0x80", -128, __func__, __LINE__);
    CheckReads<std::int8_t>("0x7F", 127, __func__, __LINE__);
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_IntegerConverter, FromText_HugeNumbersAreOutOfRange)
{
    // Act & Assert
    // Numbers too large even for unsigned long long
    CheckRejected<int>("99999999999999999999999", EIniCode::ValueOutOfRange, __func__, __LINE__);
    CheckRejected<unsigned long long>("99999999999999999999999", EIniCode::ValueOutOfRange, __func__, __LINE__);
    CheckRejected<unsigned long long>("0x1FFFFFFFFFFFFFFFF", EIniCode::ValueOutOfRange, __func__, __LINE__);
    CheckRejected<long long>("-99999999999999999999999", EIniCode::ValueOutOfRange, __func__, __LINE__);
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_IntegerConverter, FromText_IgnoresBlanksAroundTheText)
{
    // Act & Assert
    CheckReads<int>(" 5", 5, __func__, __LINE__);
    CheckReads<int>("5\t", 5, __func__, __LINE__);
    CheckReads<int>("\t -0x1F  ", -31, __func__, __LINE__);
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_IntegerConverter, FromText_ReadsDecimal)
{
    // Act & Assert
    CheckReads<int>("0", 0, __func__, __LINE__);
    CheckReads<int>("42", 42, __func__, __LINE__);
    CheckReads<int>("-42", -42, __func__, __LINE__);
    CheckReads<int>("+42", 42, __func__, __LINE__);
    CheckReads<int>("-0", 0, __func__, __LINE__);
    CheckReads<int>("+0", 0, __func__, __LINE__);
    CheckReads<int>("010", 10, __func__, __LINE__);
    CheckReads<int>("0000000000000000000000000042", 42, __func__, __LINE__);
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_IntegerConverter, FromText_ReadsEachTypesLimits)
{
    // Act & Assert
    CheckLimits<char>("char", __func__, __LINE__);
    CheckLimits<signed char>("signed char", __func__, __LINE__);
    CheckLimits<short>("short", __func__, __LINE__);
    CheckLimits<int>("int", __func__, __LINE__);
    CheckLimits<long>("long", __func__, __LINE__);
    CheckLimits<long long>("long long", __func__, __LINE__);
    CheckLimits<unsigned char>("unsigned char", __func__, __LINE__);
    CheckLimits<unsigned short>("unsigned short", __func__, __LINE__);
    CheckLimits<unsigned int>("unsigned int", __func__, __LINE__);
    CheckLimits<unsigned long>("unsigned long", __func__, __LINE__);
    CheckLimits<unsigned long long>("unsigned long long", __func__, __LINE__);
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_IntegerConverter, FromText_ReadsHex)
{
    // Act & Assert
    CheckReads<int>("0x1F", 31, __func__, __LINE__);
    CheckReads<int>("0X1f", 31, __func__, __LINE__);
    CheckReads<int>("0xabcdef", 11259375, __func__, __LINE__);
    CheckReads<int>("-0x10", -16, __func__, __LINE__);
    CheckReads<int>("+0xff", 255, __func__, __LINE__);
    CheckReads<int>("0x0", 0, __func__, __LINE__);
    CheckReads<int>("0x00ff", 255, __func__, __LINE__);
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_IntegerConverter, FromText_RejectsOtherTexts)
{
    // Arrange
    // Signs without digits, a second sign, other bases and number forms, separators, and digits that aren't ASCII
    std::string const texts[] = {
        "", " ", "+", "-", "0x", "0X", "x10", "1.5", "1.", ".5", "1e3", "12abc", "1 2", "--1", "+-1", "-+1", "0x-1",
        "0x+1", "0x 1", "- 1", "0b101", "0xg", "1_000", "1,000", "\"1\"", "inf", "nan", "\xD9\xA1",
        std::string("1\0", 2),
    };

    // Act & Assert
    for (std::string const& text : texts)
        CheckRejected<int>(text, EIniCode::InvalidValue, __func__, __LINE__);

    CheckRejected<unsigned int>("-x", EIniCode::InvalidValue, __func__, __LINE__);
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_IntegerConverter, FromText_UnsignedRejectsNegatives)
{
    // Act & Assert
    CheckRejected<unsigned int>("-1", EIniCode::ValueOutOfRange, __func__, __LINE__);
    CheckRejected<std::uint8_t>("-0x1", EIniCode::ValueOutOfRange, __func__, __LINE__);
    CheckReads<unsigned int>("-0", 0u, __func__, __LINE__);
    CheckReads<unsigned long long>("-0x0", 0u, __func__, __LINE__);
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_IntegerConverter, Messages_NameTheProblem)
{
    // Arrange
    std::uint8_t byte = 0;
    std::int8_t signedByte = 0;
    int number = 0;

    // Act
    TASWIniResult const above = TASWIniConverter<std::uint8_t>::FromText("300", byte, TASWIniValueOptions());
    TASWIniResult const below = TASWIniConverter<std::int8_t>::FromText("-129", signedByte, TASWIniValueOptions());
    TASWIniResult const invalid = TASWIniConverter<int>::FromText("abc", number, TASWIniValueOptions());

    // Assert
    CheckEquals(std::string("The value \"300\" is out of range: it must be 0 to 255."), above.Message, __func__,
        __LINE__, "a number above the range");
    CheckEquals(std::string("The value \"-129\" is out of range: it must be -128 to 127."), below.Message, __func__,
        __LINE__, "a number below the range");
    CheckEquals(std::string("The value \"abc\" isn't a whole number: it must be decimal digits, or 0x and hex digits, "
        "after an optional sign."), invalid.Message, __func__, __LINE__, "a text that isn't a number");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_IntegerConverter, RoundTrip_EveryType)
{
    // Act & Assert
    CheckRoundTrip<char>("char", __func__, __LINE__);
    CheckRoundTrip<signed char>("signed char", __func__, __LINE__);
    CheckRoundTrip<short>("short", __func__, __LINE__);
    CheckRoundTrip<int>("int", __func__, __LINE__);
    CheckRoundTrip<long>("long", __func__, __LINE__);
    CheckRoundTrip<long long>("long long", __func__, __LINE__);
    CheckRoundTrip<unsigned char>("unsigned char", __func__, __LINE__);
    CheckRoundTrip<unsigned short>("unsigned short", __func__, __LINE__);
    CheckRoundTrip<unsigned int>("unsigned int", __func__, __LINE__);
    CheckRoundTrip<unsigned long>("unsigned long", __func__, __LINE__);
    CheckRoundTrip<unsigned long long>("unsigned long long", __func__, __LINE__);
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_IntegerConverter, ToText_WritesDecimal)
{
    // Act & Assert
    // Written() starts from "old", so these also check that the text is replaced
    CheckEquals(std::string("0"), Written(0), __func__, __LINE__, "0");
    CheckEquals(std::string("-42"), Written(-42), __func__, __LINE__, "a negative int");
    CheckEquals(std::string("-128"), Written(static_cast<std::int8_t>(-128)), __func__, __LINE__,
        "std::int8_t's smallest value, as a number");
    CheckEquals(std::string("255"), Written(static_cast<std::uint8_t>(255)), __func__, __LINE__,
        "std::uint8_t's largest value, as a number");
    CheckEquals(std::string("-9223372036854775808"), Written(std::numeric_limits<std::int64_t>::min()), __func__,
        __LINE__, "std::int64_t's smallest value");
    CheckEquals(std::string("18446744073709551615"), Written(std::numeric_limits<std::uint64_t>::max()), __func__,
        __LINE__, "std::uint64_t's largest value");
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
ASW_REGISTER_TEST_GROUP_IN_SUITE(ASWUnitTests::TTest_ASWIni_IntegerConverter, "Converters")
ASW_REGISTER_TEST_GROUP_IN_SUITE(ASWUnitTests::TTest_ASWIni_TextConverter, "Converters")
