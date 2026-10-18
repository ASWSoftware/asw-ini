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
#include <array>
#include <clocale>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <initializer_list>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>
//---------------------------------------------------------------------------
#include "ASWIni_Converters.h"
#include "ASWIni_Document.h"
#include "ASWIni_File.h"
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
using ASWIni::TASWIniEnumName;
using ASWIni::TASWIniResult;
using ASWIni::TASWIniValueOptions;

namespace
{

using ASWIni::Detail::EFloatConversion;

// The enums of the enum converter tests. Those with names declare ASWIniEnumNames() as an application does, next to
// the enum.

// Names in an array, as TASWIniEnumName shows: Green has a second name, and Unlisted none
enum class EColor
{
    Red = -1,
    Green = 2,
    Blue = 40,
    Unlisted = 7,
};

auto const& ASWIniEnumNames(EColor)
{
    static constexpr TASWIniEnumName<EColor> names[] = {
        { EColor::Red, "Red" }, { EColor::Green, "Green" }, { EColor::Blue, "Blue" }, { EColor::Green, "Lime" },
    };

    return names;
}

// No names at all
enum class EEmpty
{
    Only,
};

std::vector<TASWIniEnumName<EEmpty> > ASWIniEnumNames(EEmpty)
{
    return std::vector<TASWIniEnumName<EEmpty> >();
}

// No ASWIniEnumNames(), so converted as the underlying type: one for each kind of underlying type
enum class EFlags : unsigned int
{
    None = 0,
    Read = 1,
    Write = 2,
};

enum class ESmall : std::int8_t
{
    Low = -128,
    High = 127,
};

enum class ESwitch : bool
{
    Off = false,
    On = true,
};

enum class EWide : char16_t
{
    First = 0,
};

// Names in a std::vector, returned by value
enum class EMode
{
    Off,
    Auto,
    Manual,
};

std::vector<TASWIniEnumName<EMode> > ASWIniEnumNames(EMode)
{
    return { { EMode::Off, "Off" }, { EMode::Auto, "Auto" }, { EMode::Manual, "Manual" } };
}

// An unscoped enum, with names in an array
enum EPlain
{
    PlainOne = 1,
    PlainTwo = 2,
};

auto const& ASWIniEnumNames(EPlain)
{
    static constexpr TASWIniEnumName<EPlain> names[] = { { PlainOne, "One" }, { PlainTwo, "Two" } };

    return names;
}

// Names in a std::array, with an unsigned char underlying type: Medium's first entry has no name (nullptr), so its
// second one is written, and Huge's only entry has none, so it isn't listed
enum class ESize : unsigned char
{
    Small = 0,
    Medium = 1,
    Huge = 200,
    Large = 255,
};

std::array<TASWIniEnumName<ESize>, 5> const& ASWIniEnumNames(ESize)
{
    static std::array<TASWIniEnumName<ESize>, 5> const names = { {
        { ESize::Small, "Small" }, { ESize::Medium, nullptr }, { ESize::Medium, "Medium" }, { ESize::Huge, nullptr },
        { ESize::Large, "Large" },
    } };

    return names;
}

// A class that converts to an enum with names, but isn't an enum (never defined)
struct TColorHolder
{
    operator EColor() const;
};


/////////////////////////////////////////////////////////////////////////////
// TWindow
//
// A class with an enum whose names are a friend function, which only argument-dependent lookup finds
/////////////////////////////////////////////////////////////////////////////
class TWindow
{
public:
    enum class EState
    {
        Normal,
        Maximized,
    };

    friend auto const& ASWIniEnumNames(EState)
    {
        static constexpr TASWIniEnumName<EState> names[] = {
            { EState::Normal, "Normal" }, { EState::Maximized, "Maximized" },
        };

        return names;
    }
};

// Argument-dependent lookup finds ASWIniEnumNames() wherever an application may declare it, and an enum without one
// has no names
static_assert(ASWIni::Detail::IsNamedEnum<EColor>, "names in an array should be found");
static_assert(ASWIni::Detail::IsNamedEnum<ESize>, "names in a std::array should be found");
static_assert(ASWIni::Detail::IsNamedEnum<EMode>, "names in a std::vector should be found");
static_assert(ASWIni::Detail::IsNamedEnum<EEmpty>, "an empty std::vector of names should be found");
static_assert(ASWIni::Detail::IsNamedEnum<EPlain>, "the names of an unscoped enum should be found");
static_assert(ASWIni::Detail::IsNamedEnum<TWindow::EState>, "names in a friend function should be found");
static_assert(!ASWIni::Detail::IsNamedEnum<EFlags>, "an enum without ASWIniEnumNames() has no names");
static_assert(!ASWIni::Detail::IsNamedEnum<int>, "int isn't an enum");
static_assert(ASWIni::Detail::THasEnumNames<TColorHolder>::value && !ASWIni::Detail::IsNamedEnum<TColorHolder>,
    "a class that converts to an enum with names isn't an enum with names");

//---------------------------------------------------------------------------

std::string AddOne(std::string digits);
template <typename TBits, typename T>
TBits Bits(T value);
std::string ConversionName(EFloatConversion conversion);
template <typename T>
std::string EnumNumber(T value);
std::vector<EFloatConversion> FloatConversions();
std::string HexText(unsigned long long value);
std::vector<std::vector<std::string> > ListsOfItems(std::size_t maxSize);
std::uint64_t NextRandom(std::uint64_t& state);
template <typename T>
std::string NumberText(T value);
#if ASWINI_HAS_U8STRING
std::u8string U8(std::string_view bytes);
#endif
template <typename T>
std::string Written(T const& value, TASWIniValueOptions const& options = TASWIniValueOptions());

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
    Bits

    The bits of floating-point 'value', as a TBits of the same size.
*/
template <typename TBits, typename T>
TBits Bits(T value)
{
    static_assert(sizeof(TBits) == sizeof(T), "TBits should have T's size");
    TBits bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));

    return bits;
}
//---------------------------------------------------------------------------
/*
    ConversionName

    The name of 'conversion', for a message.
*/
std::string ConversionName(EFloatConversion conversion)
{
    switch (conversion)
    {
        case EFloatConversion::Standard:
            return "Standard";

        case EFloatConversion::CRuntime:
            return "CRuntime";

        case EFloatConversion::UniversalCrt:
            return "UniversalCrt";
    }

    return "Unknown";
}
//---------------------------------------------------------------------------
/*
    EnumNumber

    The number of the enum 'value', as decimal digits.
*/
template <typename T>
std::string EnumNumber(T value)
{
    return std::to_string(+static_cast<std::underlying_type_t<T> >(value));
}
//---------------------------------------------------------------------------
/*
    FloatConversions

    The float conversions to test: every one, except Standard where it isn't reliable (ASWINI_STD_FLOAT_CHARCONV 0).
*/
std::vector<EFloatConversion> FloatConversions()
{
    std::vector<EFloatConversion> conversions;
#if ASWINI_STD_FLOAT_CHARCONV
    conversions.push_back(EFloatConversion::Standard);
#endif
    conversions.push_back(EFloatConversion::CRuntime);
    conversions.push_back(EFloatConversion::UniversalCrt);

    return conversions;
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
    ListsOfItems

    Every list of up to 'maxSize' items, each one of the items that a list must quote, or that are near them: blanks,
    delimiters, quotes, and empty items.
*/
std::vector<std::vector<std::string> > ListsOfItems(std::size_t maxSize)
{
    std::string const items[] = {
        "", " ", "\t", "a", " a", "a ", "a b", ",", ";", "|", "\"", "\"\"", "a\"b", "\"a\"", " \"a", "a, b",
    };

    // The lists of each size, made from those one item shorter
    std::vector<std::vector<std::string> > lists(1);
    std::size_t shorter = 0;
    for (std::size_t size = 1; size <= maxSize; ++size)
    {
        std::size_t const end = lists.size();
        for (std::size_t i = shorter; i < end; ++i)
        {
            for (std::string const& item : items)
            {
                std::vector<std::string> list = lists[i];
                list.push_back(item);
                lists.push_back(std::move(list));
            }
        }

        shorter = end;
    }

    return lists;
}
//---------------------------------------------------------------------------
/*
    NextRandom

    The next number of the xorshift sequence 'state' (which must not be 0), the same on every compiler.
*/
std::uint64_t NextRandom(std::uint64_t& state)
{
    state ^= state << 13;
    state ^= state >> 7;
    state ^= state << 17;

    return state;
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
#if ASWINI_HAS_U8STRING
/*
    U8

    'bytes' as a std::u8string (the same bytes); u8"" literals aren't used for non-ASCII test data.
*/
std::u8string U8(std::string_view bytes)
{
    return std::u8string(bytes.begin(), bytes.end());
}
//---------------------------------------------------------------------------
#endif
/*
    Written

    The text TASWIniConverter<T>::ToText() writes for 'value' with 'options' (replacing "old"), or "failed: " and the
    message.
*/
template <typename T>
std::string Written(T const& value, TASWIniValueOptions const& options)
{
    std::string text = "old";
    TASWIniResult const result = TASWIniConverter<T>::ToText(value, text, options);

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
// TTest_ASWIni_EnumConverter
/////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------
TTest_ASWIni_EnumConverter::TTest_ASWIni_EnumConverter()
    : inherited("ASWIni_EnumConverter_Tests")
{
}
//---------------------------------------------------------------------------
TTest_ASWIni_EnumConverter::~TTest_ASWIni_EnumConverter()
{
}
//---------------------------------------------------------------------------
template <typename T>
void TTest_ASWIni_EnumConverter::CheckReads(std::string const& text, T expected, std::string const& method,
    int line)
{
    // The value starts as something else, so a read that does nothing fails
    typedef std::underlying_type_t<T> TNumber;
    T value = static_cast<T>(static_cast<TNumber>(expected) == 0 ? 1 : 0);
    TASWIniResult const result = TASWIniConverter<T>::FromText(text, value, TASWIniValueOptions());

    CheckTrue(result.IsOk(), method, line, "\"" + text + "\" should read: " + result.Message);
    CheckTrue(value == expected, method, line, "\"" + text + "\" should read as " + EnumNumber(expected) + ", not " +
        EnumNumber(value));
}
//---------------------------------------------------------------------------
template <typename T>
void TTest_ASWIni_EnumConverter::CheckRejected(std::string const& text, EIniCode code, std::string const& method,
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
void TTest_ASWIni_EnumConverter::CheckRoundTrip(std::initializer_list<T> values, std::string const& method, int line)
{
    for (T const value : values)
    {
        std::string text;
        TASWIniResult const written = TASWIniConverter<T>::ToText(value, text, TASWIniValueOptions());

        CheckTrue(written.IsOk(), method, line, EnumNumber(value) + " should be written: " + written.Message);
        CheckReads<T>(text, value, method, line);
    }
}
//---------------------------------------------------------------------------
void TTest_ASWIni_EnumConverter::SetUp_Group()
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_EnumConverter::SetUp_Test(ITestCase& /*testCase*/)
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_EnumConverter::TearDown_Group()
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_EnumConverter::TearDown_Test(ITestCase& /*testCase*/)
{
}
//---------------------------------------------------------------------------

// /////// Begin tests after this line ///////////////////////

//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_EnumConverter, FromText_IgnoresCaseAndBlanks)
{
    // Act & Assert
    CheckReads("red", EColor::Red, __func__, __LINE__);
    CheckReads("GREEN", EColor::Green, __func__, __LINE__);
    CheckReads("lIME", EColor::Green, __func__, __LINE__);
    CheckReads(" Blue\t", EColor::Blue, __func__, __LINE__);
    CheckReads("\t maximized ", TWindow::EState::Maximized, __func__, __LINE__);
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_EnumConverter, FromText_ReadsEveryName)
{
    // Act & Assert
    // Every kind of names: an array (with a second name), a std::array (with a nullptr name), a std::vector, an
    // unscoped enum's and a friend function
    CheckReads("Red", EColor::Red, __func__, __LINE__);
    CheckReads("Green", EColor::Green, __func__, __LINE__);
    CheckReads("Blue", EColor::Blue, __func__, __LINE__);
    CheckReads("Lime", EColor::Green, __func__, __LINE__);
    CheckReads("Small", ESize::Small, __func__, __LINE__);
    CheckReads("Medium", ESize::Medium, __func__, __LINE__);
    CheckReads("Large", ESize::Large, __func__, __LINE__);
    CheckReads("Off", EMode::Off, __func__, __LINE__);
    CheckReads("Auto", EMode::Auto, __func__, __LINE__);
    CheckReads("Manual", EMode::Manual, __func__, __LINE__);
    CheckReads("One", PlainOne, __func__, __LINE__);
    CheckReads("Two", PlainTwo, __func__, __LINE__);
    CheckReads("Normal", TWindow::EState::Normal, __func__, __LINE__);
    CheckReads("Maximized", TWindow::EState::Maximized, __func__, __LINE__);
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_EnumConverter, FromText_ReadsTheNumberOfAListedValue)
{
    // Act & Assert
    // Read as the underlying type is: with a sign, in hex, with blanks around it
    CheckReads("-1", EColor::Red, __func__, __LINE__);
    CheckReads("2", EColor::Green, __func__, __LINE__);
    CheckReads("40", EColor::Blue, __func__, __LINE__);
    CheckReads("0x28", EColor::Blue, __func__, __LINE__);
    CheckReads(" +2\t", EColor::Green, __func__, __LINE__);
    CheckReads("255", ESize::Large, __func__, __LINE__);
    CheckReads("2", PlainTwo, __func__, __LINE__);
    CheckReads("1", TWindow::EState::Maximized, __func__, __LINE__);

    // Medium's first entry has no name, but its second one lists it
    CheckReads("1", ESize::Medium, __func__, __LINE__);
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_EnumConverter, FromText_RejectsOtherTexts)
{
    // Arrange
    // Other names, names with more around them, an enumerator that isn't listed, the numbers of values that aren't
    // listed or are outside the underlying type, and other number forms
    std::string const texts[] = {
        "", " ", "Purple", "Unlisted", "Re", "Reds", "Re d", "Red,", "Red Green", "\"Red\"", "Red;", "7", "3", "0",
        "1.0", "2e0", "99999999999999999999", "-99999999999999999999", std::string("Red\0", 4), "R\xC3\xA9" "d",
    };

    // Act & Assert
    for (std::string const& text : texts)
        CheckRejected<EColor>(text, EIniCode::InvalidValue, __func__, __LINE__);

    // A number outside the underlying type is InvalidValue too, not ValueOutOfRange, and so is the number of a value
    // that is listed with no name only
    CheckRejected<ESize>("-1", EIniCode::InvalidValue, __func__, __LINE__);
    CheckRejected<ESize>("256", EIniCode::InvalidValue, __func__, __LINE__);
    CheckRejected<ESize>("2", EIniCode::InvalidValue, __func__, __LINE__);
    CheckRejected<ESize>("200", EIniCode::InvalidValue, __func__, __LINE__);

    // An enum without names reads nothing, its enumerator's number neither
    CheckRejected<EEmpty>("Only", EIniCode::InvalidValue, __func__, __LINE__);
    CheckRejected<EEmpty>("0", EIniCode::InvalidValue, __func__, __LINE__);
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_EnumConverter, Messages_NameTheProblem)
{
    // Arrange
    EColor color = EColor::Red;
    ESize size = ESize::Small;
    TWindow::EState state = TWindow::EState::Normal;
    EEmpty empty = EEmpty::Only;
    std::string text = "old";

    // Act
    TASWIniResult const colorRead = TASWIniConverter<EColor>::FromText("Purple", color, TASWIniValueOptions());
    TASWIniResult const sizeRead = TASWIniConverter<ESize>::FromText("Giant", size, TASWIniValueOptions());
    TASWIniResult const stateRead = TASWIniConverter<TWindow::EState>::FromText("Min", state, TASWIniValueOptions());
    TASWIniResult const emptyRead = TASWIniConverter<EEmpty>::FromText("Only", empty, TASWIniValueOptions());
    TASWIniResult const written = TASWIniConverter<EColor>::ToText(EColor::Unlisted, text, TASWIniValueOptions());

    // Assert
    // The names listed are those written: a second name of a value, or a nullptr one, isn't
    CheckEquals(std::string("The value \"Purple\" isn't a name of the enum: it must be Red, Green or Blue."),
        colorRead.Message, __func__, __LINE__, "the message should list the names written, without Lime");
    CheckEquals(std::string("The value \"Giant\" isn't a name of the enum: it must be Small, Medium or Large."),
        sizeRead.Message, __func__, __LINE__, "the message should list each value's name once");
    CheckEquals(std::string("The value \"Min\" isn't a name of the enum: it must be Normal or Maximized."),
        stateRead.Message, __func__, __LINE__, "the message should list two names with \"or\"");
    CheckEquals(std::string("The value \"Only\" isn't a name of the enum: it has no names."), emptyRead.Message,
        __func__, __LINE__, "the message should say that the enum has no names");
    CheckEquals(std::string("The value \"7\" has no name in the enum's ASWIniEnumNames(), so it can't be written."),
        written.Message, __func__, __LINE__, "the message should name the value's number");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_EnumConverter, RoundTrip_EveryListedValue)
{
    // Act & Assert
    CheckRoundTrip({ EColor::Red, EColor::Green, EColor::Blue }, __func__, __LINE__);
    CheckRoundTrip({ ESize::Small, ESize::Medium, ESize::Large }, __func__, __LINE__);
    CheckRoundTrip({ EMode::Off, EMode::Auto, EMode::Manual }, __func__, __LINE__);
    CheckRoundTrip({ PlainOne, PlainTwo }, __func__, __LINE__);
    CheckRoundTrip({ TWindow::EState::Normal, TWindow::EState::Maximized }, __func__, __LINE__);
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_EnumConverter, ToText_UnnamedValueIsInvalid)
{
    // Arrange
    // Enumerators that aren't listed or are listed with no name only, numbers that are no enumerator, and the
    // enumerator of an enum without names
    struct TCase
    {
        std::string Expected;
        std::string Text;
    };

    TCase const cases[] = {
        { Written(EColor::Unlisted), "\"7\"" },
        { Written(static_cast<EColor>(-1000)), "\"-1000\"" },
        { Written(static_cast<ESize>(2)), "\"2\"" },
        { Written(ESize::Huge), "\"200\"" },
        { Written(EEmpty::Only), "\"0\"" },
    };

    // Act & Assert
    for (TCase const& c : cases)
    {
        CheckContains(c.Expected, "failed: The value " + c.Text + " has no name", __func__, __LINE__,
            c.Text + " should fail with a message that quotes its number");
    }

    // The text is left as it was
    std::string text = "old";
    TASWIniResult const result = TASWIniConverter<EColor>::ToText(EColor::Unlisted, text, TASWIniValueOptions());
    CheckEquals(EIniCode::InvalidValue, result.Code, __func__, __LINE__, "an unnamed value should be InvalidValue");
    CheckEquals(std::string("old"), text, __func__, __LINE__, "an unnamed value should leave the text as it was");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_EnumConverter, ToText_WritesTheFirstName)
{
    // Act & Assert
    // Written() starts from "old", so these also check that the text is replaced
    CheckEquals(std::string("Red"), Written(EColor::Red), __func__, __LINE__, "Red");
    CheckEquals(std::string("Green"), Written(EColor::Green), __func__, __LINE__, "Green, not its second name Lime");
    CheckEquals(std::string("Blue"), Written(EColor::Blue), __func__, __LINE__, "Blue");
    CheckEquals(std::string("Medium"), Written(ESize::Medium), __func__, __LINE__,
        "Medium, whose first entry has no name");
    CheckEquals(std::string("Large"), Written(ESize::Large), __func__, __LINE__, "Large");
    CheckEquals(std::string("Manual"), Written(EMode::Manual), __func__, __LINE__, "Manual, from a std::vector");
    CheckEquals(std::string("Two"), Written(PlainTwo), __func__, __LINE__, "PlainTwo, of an unscoped enum");
    CheckEquals(std::string("Maximized"), Written(TWindow::EState::Maximized), __func__, __LINE__,
        "Maximized, from a friend function");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_EnumConverter, WithoutNames_ConvertsAsTheUnderlyingType)
{
    // Act & Assert
    // Any number of the underlying type, an enumerator or not, as a number (not as a character or a bool's word)
    CheckReads("3", static_cast<EFlags>(3), __func__, __LINE__);
    CheckReads(" 0x2 ", EFlags::Write, __func__, __LINE__);
    CheckReads("4294967295", static_cast<EFlags>(4294967295u), __func__, __LINE__);
    CheckReads("-128", ESmall::Low, __func__, __LINE__);
    CheckReads("127", ESmall::High, __func__, __LINE__);
    CheckReads("1", ESwitch::On, __func__, __LINE__);
    CheckReads("0", ESwitch::Off, __func__, __LINE__);
    CheckReads("65535", static_cast<EWide>(65535), __func__, __LINE__);

    CheckEquals(std::string("5"), Written(static_cast<EFlags>(5)), __func__, __LINE__, "flags that are no enumerator");
    CheckEquals(std::string("-128"), Written(ESmall::Low), __func__, __LINE__, "a std::int8_t enum, as a number");
    CheckEquals(std::string("1"), Written(ESwitch::On), __func__, __LINE__, "a bool enum, as a number");
    CheckEquals(std::string("65"), Written(static_cast<EWide>(65)), __func__, __LINE__, "a char16_t enum, as a number");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_EnumConverter, WithoutNames_RejectsWhatTheUnderlyingTypeRejects)
{
    // Act & Assert
    // The enumerators' names aren't known
    CheckRejected<EFlags>("Read", EIniCode::InvalidValue, __func__, __LINE__);
    CheckRejected<ESwitch>("true", EIniCode::InvalidValue, __func__, __LINE__);
    CheckRejected<EWide>("A", EIniCode::InvalidValue, __func__, __LINE__);

    // The underlying type's range
    CheckRejected<EFlags>("-1", EIniCode::ValueOutOfRange, __func__, __LINE__);
    CheckRejected<EFlags>("4294967296", EIniCode::ValueOutOfRange, __func__, __LINE__);
    CheckRejected<ESmall>("128", EIniCode::ValueOutOfRange, __func__, __LINE__);
    CheckRejected<ESwitch>("2", EIniCode::ValueOutOfRange, __func__, __LINE__);
    CheckRejected<EWide>("65536", EIniCode::ValueOutOfRange, __func__, __LINE__);

    ESwitch value = ESwitch::Off;
    TASWIniResult const result = TASWIniConverter<ESwitch>::FromText("2", value, TASWIniValueOptions());
    CheckEquals(std::string("The value \"2\" is out of range: it must be 0 to 1."), result.Message, __func__,
        __LINE__, "the message should name the underlying type's range");
}
//---------------------------------------------------------------------------


/////////////////////////////////////////////////////////////////////////////
// TTest_ASWIni_FloatConverter
/////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------
TTest_ASWIni_FloatConverter::TTest_ASWIni_FloatConverter()
    : inherited("ASWIni_FloatConverter_Tests")
{
}
//---------------------------------------------------------------------------
TTest_ASWIni_FloatConverter::~TTest_ASWIni_FloatConverter()
{
}
//---------------------------------------------------------------------------
template <typename T>
void TTest_ASWIni_FloatConverter::CheckReads(std::string const& text, T expected, std::string const& method,
    int line)
{
    using TBits = std::conditional_t<sizeof(T) == 8, std::uint64_t, std::uint32_t>;
    for (EFloatConversion const conversion : FloatConversions())
    {
        std::string const what = "\"" + text + "\" (" + ConversionName(conversion) + ")";
        T value = static_cast<T>(-12345);
        TASWIniResult const result = ASWIni::Detail::ReadFloatingPoint(text, value, conversion);

        CheckTrue(result.IsOk(), method, line, what + " should read: " + result.Message);
        if (std::isnan(expected))
        {
            CheckTrue(std::isnan(value), method, line, what + " should read as a NaN");
        }
        else
        {
            CheckTrue(Bits<TBits>(value) == Bits<TBits>(expected), method, line, what + " should read as " +
                ASWIni::Detail::FloatingPointText(expected, conversion) + ", not " +
                ASWIni::Detail::FloatingPointText(value, conversion));
        }
    }
}
//---------------------------------------------------------------------------
template <typename T, typename TBits>
void TTest_ASWIni_FloatConverter::CheckRandomValues(std::string const& typeName, std::string const& method, int line)
{
    constexpr int Count = 20000;
    std::vector<EFloatConversion> const conversions = FloatConversions();
    std::vector<int> failures(conversions.size(), 0);
    std::vector<int> otherTexts(conversions.size(), 0);
    std::vector<std::string> firstFailures(conversions.size());
    std::vector<std::string> firstOtherTexts(conversions.size());

    // One value in ten is subnormal, and one in ten has the largest exponent
    TBits const infinity = Bits<TBits>(std::numeric_limits<T>::infinity());
    TBits const signAndMantissa = static_cast<TBits>(~infinity);
    TBits const largestExponent = static_cast<TBits>(infinity -
        (static_cast<TBits>(1) << (std::numeric_limits<T>::digits - 1)));
    std::uint64_t state = 20261018;
    for (int i = 0; i < Count; ++i)
    {
        TBits bits = static_cast<TBits>(NextRandom(state));
        if (i % 10 == 0)
            bits &= signAndMantissa;
        else if (i % 10 == 1)
            bits = static_cast<TBits>((bits & signAndMantissa) | largestExponent);

        T value = 0;
        std::memcpy(&value, &bits, sizeof(value));
        if (!std::isfinite(value))
            continue;

        std::string standardText;
        for (std::size_t c = 0; c < conversions.size(); ++c)
        {
            std::string const text = ASWIni::Detail::FloatingPointText(value, conversions[c]);
            T readBack = 0;
            TASWIniResult const result = ASWIni::Detail::ReadFloatingPoint(text, readBack, conversions[c]);
            if ((!result.IsOk() || Bits<TBits>(readBack) != bits) && failures[c]++ == 0)
                firstFailures[c] = text;

            // The first conversion is Standard where it is reliable. Another may only write a longer text, when
            // strtold() can't prove a shorter one (see CRuntimeRead()), and an exact reader must read it back.
            if (conversions[0] != EFloatConversion::Standard)
                continue;

            if (c == 0)
            {
                standardText = text;
                continue;
            }

            // On Windows, the Universal C Runtime is exact: UniversalCrt writes what Standard does, and reads it
            bool exactConversion = false;
#if defined(_WIN32)
            exactConversion = conversions[c] == EFloatConversion::UniversalCrt;
            if (exactConversion)
            {
                T standardBack = 0;
                bool const read = ASWIni::Detail::ReadFloatingPoint(standardText, standardBack, conversions[c]).IsOk();
                if ((!read || Bits<TBits>(standardBack) != bits) && failures[c]++ == 0)
                    firstFailures[c] = standardText + " (written by Standard)";
            }
#endif

            if (text == standardText)
                continue;

            T exactBack = 0;
            bool const exact = ASWIni::Detail::ReadFloatingPoint(text, exactBack, EFloatConversion::Standard).IsOk() &&
                Bits<TBits>(exactBack) == bits;
            if ((exactConversion || !exact || text.size() <= standardText.size()) && otherTexts[c]++ == 0)
                firstOtherTexts[c] = text + " instead of " + standardText;
        }
    }

    for (std::size_t c = 0; c < conversions.size(); ++c)
    {
        std::string const what = typeName + " (" + ConversionName(conversions[c]) + ")";
        CheckEquals(0, failures[c], method, line, what + ": every value should read back as itself; the first that "
            "doesn't is written as " + firstFailures[c]);
        CheckEquals(0, otherTexts[c], method, line, what + ": every value should be written as the Standard "
            "conversion writes it, or longer and reading back as itself there; the first that isn't is " +
            firstOtherTexts[c]);
    }
}
//---------------------------------------------------------------------------
template <typename T>
void TTest_ASWIni_FloatConverter::CheckRejected(std::string const& text, EIniCode code, std::string const& method,
    int line)
{
    for (EFloatConversion const conversion : FloatConversions())
    {
        std::string const what = "\"" + text + "\" (" + ConversionName(conversion) + ")";
        for (T const before : { static_cast<T>(1.5), static_cast<T>(-2) })
        {
            T value = before;
            TASWIniResult const result = ASWIni::Detail::ReadFloatingPoint(text, value, conversion);

            CheckEquals(code, result.Code, method, line, what + " should be " + ASWIni::ToString(code));
            CheckContains(result.Message, ASWIni::Detail::MessageText(text), method, line,
                "the message should quote " + what);
            CheckTrue(value == before, method, line, what + " should leave the value as it was");
        }
    }
}
//---------------------------------------------------------------------------
template <typename T>
void TTest_ASWIni_FloatConverter::CheckWrites(T value, std::string const& expected, std::string const& method,
    int line)
{
    for (EFloatConversion const conversion : FloatConversions())
    {
        CheckEquals(expected, ASWIni::Detail::FloatingPointText(value, conversion), method, line,
            "the text of " + expected + " (" + ConversionName(conversion) + ")");
    }
}
//---------------------------------------------------------------------------
void TTest_ASWIni_FloatConverter::SetUp_Group()
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_FloatConverter::SetUp_Test(ITestCase& /*testCase*/)
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_FloatConverter::TearDown_Group()
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_FloatConverter::TearDown_Test(ITestCase& /*testCase*/)
{
}
//---------------------------------------------------------------------------

// /////// Begin tests after this line ///////////////////////

//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_FloatConverter, Converters_UseTheDefaultConversion)
{
    // Arrange
    double number = 1.5;
    float single = 1.5f;
    std::string numberText;
    std::string singleText;

    // Act
    TASWIniResult const numberRead = TASWIniConverter<double>::FromText("0.1", number, TASWIniValueOptions());
    TASWIniResult const singleRead = TASWIniConverter<float>::FromText("0.1", single, TASWIniValueOptions());
    TASWIniResult const numberWritten = TASWIniConverter<double>::ToText(0.1, numberText, TASWIniValueOptions());
    TASWIniResult const singleWritten = TASWIniConverter<float>::ToText(0.1f, singleText, TASWIniValueOptions());
    double invalidNumber = 2.5;
    TASWIniResult const invalid = TASWIniConverter<double>::FromText("abc", invalidNumber, TASWIniValueOptions());

    // Assert
    CheckTrue(numberRead.IsOk() && number == 0.1, __func__, __LINE__, "\"0.1\" should read as the double 0.1");
    CheckTrue(singleRead.IsOk() && single == 0.1f, __func__, __LINE__, "\"0.1\" should read as the float 0.1");
    CheckTrue(numberWritten.IsOk() && singleWritten.IsOk(), __func__, __LINE__, "writing should succeed");
    CheckEquals(std::string("0.1"), numberText, __func__, __LINE__, "the double 0.1");
    CheckEquals(std::string("0.1"), singleText, __func__, __LINE__, "the float 0.1");
    CheckEquals(EIniCode::InvalidValue, invalid.Code, __func__, __LINE__, "\"abc\" should be InvalidValue");
    CheckTrue(invalidNumber == 2.5, __func__, __LINE__, "a failed read should leave the value as it was");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_FloatConverter, FromText_IgnoresBlanksAroundTheText)
{
    // Act & Assert
    CheckReads<double>(" 1.5", 1.5, __func__, __LINE__);
    CheckReads<double>("1.5\t", 1.5, __func__, __LINE__);
    CheckReads<double>("\t -2e3  ", -2000.0, __func__, __LINE__);
    CheckReads<double>(" inf ", std::numeric_limits<double>::infinity(), __func__, __LINE__);
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_FloatConverter, FromText_ReadsDecimalForms)
{
    // Act & Assert
    CheckReads<double>("1.5", 1.5, __func__, __LINE__);
    CheckReads<double>(".5", 0.5, __func__, __LINE__);
    CheckReads<double>("5.", 5.0, __func__, __LINE__);
    CheckReads<double>("1e5", 1e5, __func__, __LINE__);
    CheckReads<double>("1E-3", 1e-3, __func__, __LINE__);
    CheckReads<double>("2.5e+2", 250.0, __func__, __LINE__);
    CheckReads<double>("-2.5", -2.5, __func__, __LINE__);
    CheckReads<double>("+2.5", 2.5, __func__, __LINE__);
    CheckReads<double>("007.25", 7.25, __func__, __LINE__);
    CheckReads<double>("0", 0.0, __func__, __LINE__);
    CheckReads<double>("-0", -0.0, __func__, __LINE__);
    CheckReads<double>("0.1", 0.1, __func__, __LINE__);
    CheckReads<double>("123456789012345680", 123456789012345680.0, __func__, __LINE__);
    CheckReads<float>("0.1", 0.1f, __func__, __LINE__);
    CheckReads<float>("16777216", 16777216.0f, __func__, __LINE__);
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_FloatConverter, FromText_ReadsInfinityAndNan)
{
    // Act & Assert
    // With the ASCII letters' case ignored, and a NaN's sign dropped
    double const infinity = std::numeric_limits<double>::infinity();
    double const nan = std::numeric_limits<double>::quiet_NaN();
    CheckReads<double>("inf", infinity, __func__, __LINE__);
    CheckReads<double>("INF", infinity, __func__, __LINE__);
    CheckReads<double>("Infinity", infinity, __func__, __LINE__);
    CheckReads<double>("+inf", infinity, __func__, __LINE__);
    CheckReads<double>("-inf", -infinity, __func__, __LINE__);
    CheckReads<double>("nan", nan, __func__, __LINE__);
    CheckReads<double>("NaN", nan, __func__, __LINE__);
    CheckReads<double>("-nan", nan, __func__, __LINE__);
    CheckReads<float>("-Infinity", -std::numeric_limits<float>::infinity(), __func__, __LINE__);
    CheckReads<float>("nan", std::numeric_limits<float>::quiet_NaN(), __func__, __LINE__);
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_FloatConverter, FromText_ReadsTheLimits)
{
    // Act & Assert
    // The largest and smallest values, normal and subnormal (bcc32c's own strtod() reads subnormals as 0)
    CheckReads<double>("1.7976931348623157e+308", std::numeric_limits<double>::max(), __func__, __LINE__);
    CheckReads<double>("-1.7976931348623157e308", std::numeric_limits<double>::lowest(), __func__, __LINE__);
    CheckReads<double>("1.7976931348623158e308", std::numeric_limits<double>::max(), __func__, __LINE__);
    CheckReads<double>("2.2250738585072014e-308", std::numeric_limits<double>::min(), __func__, __LINE__);
    CheckReads<double>("4.9406564584124654e-324", std::numeric_limits<double>::denorm_min(), __func__, __LINE__);
    CheckReads<double>("5e-324", std::numeric_limits<double>::denorm_min(), __func__, __LINE__);
    CheckReads<float>("3.4028235e+38", std::numeric_limits<float>::max(), __func__, __LINE__);
    CheckReads<float>("1.17549435e-38", std::numeric_limits<float>::min(), __func__, __LINE__);
    CheckReads<float>("1e-45", std::numeric_limits<float>::denorm_min(), __func__, __LINE__);
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_FloatConverter, FromText_RejectsOtherTexts)
{
    // Arrange
    // No sign alone, second sign, decimal comma, hex, digit separator, blank inside, other suffix or spelling
    std::string const texts[] = {
        "", " ", "+", "-", ".", "-.", "e5", ".e5", "1e", "1e+", "1.5.2", "1,5", "0x1p3", "0x10", "--1", "+-1", "1 5",
        "1.5e3x", "1e5.5", "1d5", "1f", "1_000", "infx", "in f", "nann", "nan(1)", "+-inf", "\xD9\xA1",
        std::string("1\0", 2),
    };

    // Act & Assert
    for (std::string const& text : texts)
        CheckRejected<double>(text, EIniCode::InvalidValue, __func__, __LINE__);

    CheckRejected<float>("1,5", EIniCode::InvalidValue, __func__, __LINE__);
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_FloatConverter, FromText_TooLargeIsOutOfRange)
{
    // Act & Assert
    CheckRejected<double>("1e400", EIniCode::ValueOutOfRange, __func__, __LINE__);
    CheckRejected<double>("-1e400", EIniCode::ValueOutOfRange, __func__, __LINE__);
    CheckRejected<double>("1.8e308", EIniCode::ValueOutOfRange, __func__, __LINE__);
    CheckRejected<double>("1e99999999999999999999", EIniCode::ValueOutOfRange, __func__, __LINE__);
    CheckRejected<float>("3.5e38", EIniCode::ValueOutOfRange, __func__, __LINE__);
    CheckRejected<float>("-1e39", EIniCode::ValueOutOfRange, __func__, __LINE__);
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_FloatConverter, FromText_TooSmallReadsAsZero)
{
    // Act & Assert
    // Below half the smallest subnormal, a number rounds to 0 (with its sign)
    CheckReads<double>("1e-400", 0.0, __func__, __LINE__);
    CheckReads<double>("-1e-400", -0.0, __func__, __LINE__);
    CheckReads<double>("2e-324", 0.0, __func__, __LINE__);
    CheckReads<double>("1e-99999999999999999999", 0.0, __func__, __LINE__);
    CheckReads<double>("0.000000000000000000001e-400", 0.0, __func__, __LINE__);
    CheckReads<float>("1e-46", 0.0f, __func__, __LINE__);
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_FloatConverter, Locale_DecimalCommaChangesNothing)
{
    // Arrange
    // A C locale with a ',' decimal mark, if one is installed (Linux images often have none; then "C" is checked)
    std::string const previous = std::setlocale(LC_ALL, nullptr);
    std::string used = "C";
    for (char const* name : { "de_DE.UTF-8", "de_DE", "German_Germany.1252", "de-DE" })
    {
        if (std::setlocale(LC_ALL, name) != nullptr && std::localeconv()->decimal_point == std::string(","))
        {
            used = name;
            break;
        }
    }

    if (used == "C")
        std::setlocale(LC_ALL, "C");

    // Act & Assert
    std::string const where = " in the locale " + used;
    CheckReads<double>("1.5", 1.5, __func__, __LINE__);
    CheckReads<float>("0.25", 0.25f, __func__, __LINE__);
    CheckRejected<double>("1,5", EIniCode::InvalidValue, __func__, __LINE__);
    CheckWrites<double>(1.5, "1.5", __func__, __LINE__);
    CheckWrites<double>(1234567.25, "1234567.25", __func__, __LINE__);
    CheckWrites<float>(0.1f, "0.1", __func__, __LINE__);

    int integer = 0;
    std::string integerText;
    TASWIniResult const integerRead = TASWIniConverter<int>::FromText("1234567", integer, TASWIniValueOptions());
    TASWIniResult const integerWritten = TASWIniConverter<int>::ToText(1234567, integerText, TASWIniValueOptions());
    CheckTrue(integerRead.IsOk() && integer == 1234567, __func__, __LINE__, "an integer should read" + where);
    CheckTrue(integerWritten.IsOk() && integerText == "1234567", __func__, __LINE__, "an integer should be written "
        "without separators" + where);

    std::setlocale(LC_ALL, previous.c_str());
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_FloatConverter, Messages_NameTheProblem)
{
    // Arrange
    double number = 0;
    float single = 0;

    // Act
    TASWIniResult const invalid = TASWIniConverter<double>::FromText("1,5", number, TASWIniValueOptions());
    TASWIniResult const tooLarge = TASWIniConverter<double>::FromText("1e400", number, TASWIniValueOptions());
    TASWIniResult const floatTooLarge = TASWIniConverter<float>::FromText("1e39", single, TASWIniValueOptions());

    // Assert
    CheckEquals(std::string("The value \"1,5\" isn't a number: it must be digits with an optional '.' and exponent, "
        "after an optional sign (e.g. 1.5, -2e-3), or inf or nan."), invalid.Message, __func__, __LINE__,
        "a text that isn't a number");
    CheckEquals(std::string("The value \"1e400\" is out of range: it must be -1.7976931348623157e+308 to "
        "1.7976931348623157e+308."), tooLarge.Message, __func__, __LINE__, "a number too large for a double");
    CheckEquals(std::string("The value \"1e39\" is out of range: it must be -3.4028235e+38 to 3.4028235e+38."),
        floatTooLarge.Message, __func__, __LINE__, "a number too large for a float");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_FloatConverter, RandomValues_RoundTripWithEveryConversion)
{
    // Act & Assert
    CheckRandomValues<double, std::uint64_t>("double", __func__, __LINE__);
    CheckRandomValues<float, std::uint32_t>("float", __func__, __LINE__);
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_FloatConverter, ToText_WritesInfinityAndNan)
{
    // Act & Assert
    CheckWrites<double>(std::numeric_limits<double>::infinity(), "inf", __func__, __LINE__);
    CheckWrites<double>(-std::numeric_limits<double>::infinity(), "-inf", __func__, __LINE__);
    CheckWrites<double>(std::numeric_limits<double>::quiet_NaN(), "nan", __func__, __LINE__);
    CheckWrites<double>(-std::numeric_limits<double>::quiet_NaN(), "nan", __func__, __LINE__);
    CheckWrites<float>(-std::numeric_limits<float>::infinity(), "-inf", __func__, __LINE__);
    CheckWrites<float>(std::numeric_limits<float>::quiet_NaN(), "nan", __func__, __LINE__);
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_FloatConverter, ToText_WritesTheShortestText)
{
    // Act & Assert
    // As std::to_chars() writes them: the fewest digits that read back, in fixed or scientific notation, whichever
    // is shorter (fixed when both are as long)
    CheckWrites<double>(0.1, "0.1", __func__, __LINE__);
    CheckWrites<double>(1.0 / 3, "0.3333333333333333", __func__, __LINE__);
    CheckWrites<double>(3.0, "3", __func__, __LINE__);
    CheckWrites<double>(0.0, "0", __func__, __LINE__);
    CheckWrites<double>(-0.0, "-0", __func__, __LINE__);
    CheckWrites<double>(-2.5, "-2.5", __func__, __LINE__);
    CheckWrites<double>(100.0, "100", __func__, __LINE__);
    CheckWrites<double>(10000.0, "10000", __func__, __LINE__);
    CheckWrites<double>(100000.0, "1e+05", __func__, __LINE__);
    CheckWrites<double>(1e21, "1e+21", __func__, __LINE__);
    CheckWrites<double>(123456789012345680.0, "123456789012345680", __func__, __LINE__);
    // A whole number above 2^53 is written with all its digits, not the shortest ones padded with zeros
    CheckWrites<double>(143860433449586288.0, "143860433449586288", __func__, __LINE__);
    CheckWrites<double>(1152921504606846976.0, "1152921504606846976", __func__, __LINE__);
    CheckWrites<float>(57457352.0f, "57457352", __func__, __LINE__);
    CheckWrites<double>(0.001, "0.001", __func__, __LINE__);
    CheckWrites<double>(1e-7, "1e-07", __func__, __LINE__);
    CheckWrites<double>(1.5e-10, "1.5e-10", __func__, __LINE__);
    CheckWrites<double>(1e300, "1e+300", __func__, __LINE__);
    CheckWrites<double>(std::numeric_limits<double>::max(), "1.7976931348623157e+308", __func__, __LINE__);
    CheckWrites<double>(std::numeric_limits<double>::min(), "2.2250738585072014e-308", __func__, __LINE__);
    CheckWrites<double>(std::numeric_limits<double>::denorm_min(), "5e-324", __func__, __LINE__);
    CheckWrites<float>(0.1f, "0.1", __func__, __LINE__);
    CheckWrites<float>(1.0f / 3, "0.33333334", __func__, __LINE__);
    CheckWrites<float>(16777216.0f, "16777216", __func__, __LINE__);
    CheckWrites<float>(1e10f, "1e+10", __func__, __LINE__);
    CheckWrites<float>(std::numeric_limits<float>::max(), "3.4028235e+38", __func__, __LINE__);
    CheckWrites<float>(std::numeric_limits<float>::denorm_min(), "1e-45", __func__, __LINE__);
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
// TTest_ASWIni_ListConverter
/////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------
TTest_ASWIni_ListConverter::TTest_ASWIni_ListConverter()
    : inherited("ASWIni_ListConverter_Tests")
{
}
//---------------------------------------------------------------------------
TTest_ASWIni_ListConverter::~TTest_ASWIni_ListConverter()
{
}
//---------------------------------------------------------------------------
template <typename T>
void TTest_ASWIni_ListConverter::CheckReads(std::string const& text, std::vector<T> const& expected,
    std::string const& method, int line)
{
    // The list starts as something else, so a read that does nothing fails
    std::vector<T> value;
    if (expected.empty())
        value.push_back(T());

    TASWIniResult const result = TASWIniConverter<std::vector<T> >::FromText(text, value, m_Options);

    CheckTrue(result.IsOk(), method, line, "\"" + text + "\" should read: " + result.Message);
    CheckTrue(value == expected, method, line, "\"" + text + "\" should read as " + Written(expected, m_Options) +
        " (" + std::to_string(expected.size()) + " items), not " + Written(value, m_Options) + " (" +
        std::to_string(value.size()) + " items)");
}
//---------------------------------------------------------------------------
template <typename T>
void TTest_ASWIni_ListConverter::CheckRejected(std::string const& text, EIniCode code, std::string const& method,
    int line)
{
    for (std::vector<T> const& before : { std::vector<T>(), std::vector<T>(1, T()) })
    {
        std::vector<T> value = before;
        TASWIniResult const result = TASWIniConverter<std::vector<T> >::FromText(text, value, m_Options);

        CheckEquals(code, result.Code, method, line, "\"" + text + "\" should be " + ASWIni::ToString(code));
        CheckContains(result.Message, ASWIni::Detail::MessageText(text), method, line,
            "the message should quote \"" + text + "\"");
        CheckTrue(value == before, method, line, "\"" + text + "\" should leave the list as it was");
    }
}
//---------------------------------------------------------------------------
template <typename T>
void TTest_ASWIni_ListConverter::CheckRoundTrip(std::vector<T> const& value, std::string const& method, int line)
{
    std::string text;
    TASWIniResult const written = TASWIniConverter<std::vector<T> >::ToText(value, text, m_Options);
    CheckTrue(written.IsOk(), method, line, "the list should be written: " + written.Message);
    if (written.IsOk())
        CheckReads(text, value, method, line);
}
//---------------------------------------------------------------------------
template <typename T>
void TTest_ASWIni_ListConverter::CheckWrites(std::vector<T> const& value, std::string const& expected,
    std::string const& method, int line)
{
    CheckEquals(expected, Written(value, m_Options), method, line, "the list should be written as \"" + expected +
        "\"");
}
//---------------------------------------------------------------------------
void TTest_ASWIni_ListConverter::SetUp_Group()
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_ListConverter::SetUp_Test(ITestCase& /*testCase*/)
{
    m_Options = TASWIniValueOptions();
}
//---------------------------------------------------------------------------
void TTest_ASWIni_ListConverter::TearDown_Group()
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_ListConverter::TearDown_Test(ITestCase& /*testCase*/)
{
}
//---------------------------------------------------------------------------

// /////// Begin tests after this line ///////////////////////

//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_ListConverter, Delimiter_IsTheOptionsOne)
{
    // Act & Assert
    // The default delimiter is then an ordinary character
    m_Options.ListDelimiter = ';';
    CheckReads<std::string>("a; b,c ;d", { "a", "b,c", "d" }, __func__, __LINE__);
    CheckReads<std::string>("\"a;b\"; c", { "a;b", "c" }, __func__, __LINE__);
    CheckWrites<std::string>({ "a", "b,c", "d;e" }, "a; b,c; \"d;e\"", __func__, __LINE__);

    m_Options.ListDelimiter = '|';
    CheckReads<int>("1|2 | 3", { 1, 2, 3 }, __func__, __LINE__);
    CheckWrites<int>({ 1, 2, 3 }, "1| 2| 3", __func__, __LINE__);
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_ListConverter, Document_ListsReadBackWithQuotedValuesOnAndOff)
{
    // Arrange
    // Every list of up to two tricky items, among them the one-item lists in quotes ("\"a, b\"" and "\"\"") that the
    // document itself quotes with QuotedValues
    std::vector<std::vector<std::string> > const lists = ListsOfItems(2);

    // Act & Assert
    for (bool const quotedValues : { false, true })
    {
        ASWIni::TASWIniOptions options;
        options.Reading.QuotedValues = quotedValues;
        std::string const mode = quotedValues ? "with QuotedValues: " : "without QuotedValues: ";

        for (std::vector<std::string> const& list : lists)
        {
            std::string text;
            TASWIniResult result = TASWIniConverter<std::vector<std::string> >::ToText(list, text, options.Values);
            ASWIni::TASWIniDocument written;
            if (result.IsOk())
                result = written.Reset(options);

            if (result.IsOk())
                result = written.SetValue("Lists", "List", text);

            CheckTrue(result.IsOk(), __func__, __LINE__, mode + "\"" + text + "\" should be set: " + result.Message);
            if (!result.IsOk())
                continue;

            // The file text parsed again, as a load would
            std::vector<ASWIni::TASWIniMessage> messages;
            ASWIni::TASWIniDocument read;
            result = read.Parse(written.ToText(), options, messages);
            ASWIni::TASWIniDocumentLine const* const line = read.FindKey("Lists", "List");
            CheckTrue(result.IsOk() && line != nullptr, __func__, __LINE__, mode + "\"" + text + "\" should be parsed");
            if (!result.IsOk() || line == nullptr)
                continue;

            std::vector<std::string> readBack;
            result = TASWIniConverter<std::vector<std::string> >::FromText(line->GetValue(), readBack,
                options.Values);
            CheckTrue(result.IsOk() && readBack == list, __func__, __LINE__, mode + "\"" + text +
                "\" should read back as the list it was written from, not \"" + std::string(line->GetValue()) + "\"");
        }
    }

    // With QuotedValues, the document quotes a one-item quoted list again, doubling its quotes
    ASWIni::TASWIniOptions options;
    options.Reading.QuotedValues = true;
    ASWIni::TASWIniDocument document;
    std::string text;
    TASWIniResult const written = TASWIniConverter<std::vector<std::string> >::ToText({ "a, b" }, text, options.Values);
    TASWIniResult const reset = document.Reset(options);
    TASWIniResult const set = document.SetValue("", "List", text);
    CheckTrue(written.IsOk() && reset.IsOk() && set.IsOk(), __func__, __LINE__, "the list should be set");
    CheckContains(document.ToText(), "List = \"\"\"a, b\"\"\"", __func__, __LINE__,
        "the line should hold the list in quotes, with its quotes doubled");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_ListConverter, FromText_EmptyTextIsAnEmptyList)
{
    // Act & Assert
    CheckReads<std::string>("", {}, __func__, __LINE__);
    CheckReads<std::string>(" \t ", {}, __func__, __LINE__);
    CheckReads<int>("", {}, __func__, __LINE__);
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_ListConverter, FromText_FailingItemFailsTheList)
{
    // Act & Assert
    // The item's code is the list's: InvalidValue or ValueOutOfRange
    CheckRejected<int>("1,,2", EIniCode::InvalidValue, __func__, __LINE__);
    CheckRejected<int>("1, 2,", EIniCode::InvalidValue, __func__, __LINE__);
    CheckRejected<int>("1, x", EIniCode::InvalidValue, __func__, __LINE__);
    CheckRejected<int>("\"1\"\"\"", EIniCode::InvalidValue, __func__, __LINE__);
    CheckRejected<std::uint8_t>("1, 300", EIniCode::ValueOutOfRange, __func__, __LINE__);
    CheckRejected<bool>("true, maybe", EIniCode::InvalidValue, __func__, __LINE__);

    // The message names the item, and gives its own message
    std::vector<int> value;
    TASWIniResult const result = TASWIniConverter<std::vector<int> >::FromText("1, x, 3", value, m_Options);
    CheckEquals(std::string("The value \"1, x, 3\" can't be read: item 2: The value \"x\" isn't a whole number: it "
        "must be decimal digits, or 0x and hex digits, after an optional sign."), result.Message, __func__, __LINE__,
        "the message should name the item that failed");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_ListConverter, FromText_QuoteInsideAnItemIsOrdinary)
{
    // Act & Assert
    CheckReads<std::string>("5\" disk, 3.5\" disk", { "5\" disk", "3.5\" disk" }, __func__, __LINE__);
    CheckReads<std::string>("a\"b, c\"\"", { "a\"b", "c\"\"" }, __func__, __LINE__);
    CheckReads<std::string>("a \"b, c\" d", { "a \"b", "c\" d" }, __func__, __LINE__);
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_ListConverter, FromText_ReadsQuotedItems)
{
    // Act & Assert
    // A quoted item keeps its blanks and may hold the delimiter, and "" in it is one '"'
    CheckReads<std::string>("\"a, b\", c", { "a, b", "c" }, __func__, __LINE__);
    CheckReads<std::string>("\" a \"", { " a " }, __func__, __LINE__);
    CheckReads<std::string>("\"\"", { "" }, __func__, __LINE__);
    CheckReads<std::string>("\"\", \"\"", { "", "" }, __func__, __LINE__);
    CheckReads<std::string>("\"say \"\"hi\"\"\"", { "say \"hi\"" }, __func__, __LINE__);
    CheckReads<std::string>("\"\"\"\"", { "\"" }, __func__, __LINE__);
    CheckReads<std::string>(" \t\"x\" \t, y", { "x", "y" }, __func__, __LINE__);
    CheckReads<std::string>("a,\"b\"", { "a", "b" }, __func__, __LINE__);
    CheckReads<int>("\"1\", \" 2 \"", { 1, 2 }, __func__, __LINE__);
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_ListConverter, FromText_RejectsBadQuoting)
{
    // Arrange
    // An unclosed quote, and more than blanks after a closing one
    std::string const texts[] = {
        "\"", "\"a", "\"a, b", "x, \"y", "\"a\"\"", "\"a\"b", "\"a\" b, c", "x, \"y\"z", "\"a\"\"b",
    };

    // Act & Assert
    for (std::string const& text : texts)
        CheckRejected<std::string>(text, EIniCode::InvalidValue, __func__, __LINE__);

    // The messages name the item and the problem
    std::vector<std::string> value;
    TASWIniResult const unclosed = TASWIniConverter<std::vector<std::string> >::FromText("x, \"y", value, m_Options);
    TASWIniResult const after = TASWIniConverter<std::vector<std::string> >::FromText("\"a\"b", value, m_Options);
    CheckEquals(std::string("The value \"x, \"y\" isn't a list: its item 2 starts with a '\"' that isn't closed."),
        unclosed.Message, __func__, __LINE__, "an unclosed quote");
    CheckEquals(std::string("The value \"\"a\"b\" isn't a list: its item 1 has more than blanks after its closing "
        "'\"'."), after.Message, __func__, __LINE__, "text after a closing quote");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_ListConverter, FromText_SplitsAtTheDelimiter)
{
    // Act & Assert
    // Each item without the blanks around it; an empty item is one too
    CheckReads<std::string>("a", { "a" }, __func__, __LINE__);
    CheckReads<std::string>("a,b,c", { "a", "b", "c" }, __func__, __LINE__);
    CheckReads<std::string>(" a ,\tb\t, c d ", { "a", "b", "c d" }, __func__, __LINE__);
    CheckReads<std::string>("1,,2", { "1", "", "2" }, __func__, __LINE__);
    CheckReads<std::string>("a,", { "a", "" }, __func__, __LINE__);
    CheckReads<std::string>(",a", { "", "a" }, __func__, __LINE__);
    CheckReads<std::string>(",", { "", "" }, __func__, __LINE__);
    CheckReads<std::string>(" , ", { "", "" }, __func__, __LINE__);
    CheckReads<int>("1, -2, 0x3", { 1, -2, 3 }, __func__, __LINE__);
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_ListConverter, ItemTypes_BoolEnumAndNumbers)
{
    // Act & Assert
    // std::vector<bool>'s items are proxies; the options reach the items' converters
    CheckReads<bool>("true, no, 1", { true, false, true }, __func__, __LINE__);
    CheckWrites<bool>({ true, false }, "true, false", __func__, __LINE__);
    m_Options.BoolFormat = EBoolFormat::YesNo;
    CheckWrites<bool>({ true, false }, "yes, no", __func__, __LINE__);
    CheckRoundTrip<bool>({ false, true, true }, __func__, __LINE__);

    CheckReads<EColor>("Red, lime, 40", { EColor::Red, EColor::Green, EColor::Blue }, __func__, __LINE__);
    CheckWrites<EColor>({ EColor::Blue, EColor::Green }, "Blue, Green", __func__, __LINE__);
    CheckRoundTrip<double>({ 0.1, -2.5, 1e300 }, __func__, __LINE__);
    CheckRoundTrip<std::int64_t>({ std::numeric_limits<std::int64_t>::min(), 0 }, __func__, __LINE__);
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_ListConverter, Nested_ListsOfLists)
{
    // Arrange
    typedef std::vector<std::vector<int> > TGrid;
    typedef std::vector<std::vector<std::string> > TTable;

    // Act & Assert
    // The inner lists are items like any other, quoted when they hold the delimiter (or are the only one and empty)
    CheckWrites<std::vector<int> >({ { 1, 2 }, { 3, 4 } }, "\"1, 2\", \"3, 4\"", __func__, __LINE__);
    CheckReads<std::vector<int> >("\"1, 2\", \"3, 4\"", TGrid({ { 1, 2 }, { 3, 4 } }), __func__, __LINE__);
    CheckWrites<std::vector<int> >({ { 1 }, {} }, "1,", __func__, __LINE__);
    CheckWrites<std::vector<int> >({ {} }, "\"\"", __func__, __LINE__);
    CheckRoundTrip<std::vector<int> >({ {} }, __func__, __LINE__);
    CheckRoundTrip<std::vector<int> >({ {}, {} }, __func__, __LINE__);
    CheckRoundTrip<std::vector<int> >({ { 1 }, { 2, 3 }, {} }, __func__, __LINE__);
    CheckRoundTrip<std::vector<std::string> >(TTable({ { "a, b", "c" }, { "\"q\"" }, { "" } }), __func__, __LINE__);
    CheckRoundTrip<std::vector<std::vector<std::string> > >({ TTable({ { "x, \"y\"" }, {} }), TTable() }, __func__,
        __LINE__);
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_ListConverter, RoundTrip_EveryCombinationOfTrickyItems)
{
    // Arrange
    std::vector<std::vector<std::string> > const lists = ListsOfItems(3);

    // Act & Assert
    for (char const delimiter : { ',', ';', '|' })
    {
        m_Options.ListDelimiter = delimiter;
        for (std::vector<std::string> const& list : lists)
            CheckRoundTrip(list, __func__, __LINE__);
    }
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_ListConverter, ToText_FailingItemFailsTheList)
{
    // Arrange
    std::string text = "old";

    // Act
    TASWIniResult const result = TASWIniConverter<std::vector<EColor> >::ToText({ EColor::Red, EColor::Unlisted },
        text, m_Options);

    // Assert
    CheckEquals(EIniCode::InvalidValue, result.Code, __func__, __LINE__, "the item's code should be the list's");
    CheckEquals(std::string("The list can't be written: item 2: The value \"7\" has no name in the enum's "
        "ASWIniEnumNames(), so it can't be written."), result.Message, __func__, __LINE__,
        "the message should name the item that failed");
    CheckEquals(std::string("old"), text, __func__, __LINE__, "the text should be left as it was");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_ListConverter, ToText_JoinsWithTheDelimiterAndASpace)
{
    // Act & Assert
    // Written() starts from "old", so these also check that the text is replaced
    CheckWrites<std::string>({}, "", __func__, __LINE__);
    CheckWrites<std::string>({ "a" }, "a", __func__, __LINE__);
    CheckWrites<std::string>({ "a", "b", "c" }, "a, b, c", __func__, __LINE__);
    CheckWrites<std::string>({ "a b", "c\td" }, "a b, c\td", __func__, __LINE__);
    CheckWrites<int>({ 1, -2, 3 }, "1, -2, 3", __func__, __LINE__);

    // No space before an empty item, so the text never ends with one
    CheckWrites<std::string>({ "a", "" }, "a,", __func__, __LINE__);
    CheckWrites<std::string>({ "", "a" }, ", a", __func__, __LINE__);
    CheckWrites<std::string>({ "a", "", "b" }, "a,, b", __func__, __LINE__);
    CheckWrites<std::string>({ "", "" }, ",", __func__, __LINE__);
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_ListConverter, ToText_QuotesOnlyWhenNeeded)
{
    // Act & Assert
    // The delimiter or a '"', blanks at either end, or an empty only item
    CheckWrites<std::string>({ "a, b", "c" }, "\"a, b\", c", __func__, __LINE__);
    CheckWrites<std::string>({ "say \"hi\"" }, "\"say \"\"hi\"\"\"", __func__, __LINE__);
    CheckWrites<std::string>({ "5\" disk" }, "\"5\"\" disk\"", __func__, __LINE__);
    CheckWrites<std::string>({ " a", "b " }, "\" a\", \"b \"", __func__, __LINE__);
    CheckWrites<std::string>({ "\ta", "b\t" }, "\"\ta\", \"b\t\"", __func__, __LINE__);
    CheckWrites<std::string>({ " " }, "\" \"", __func__, __LINE__);
    CheckWrites<std::string>({ "" }, "\"\"", __func__, __LINE__);

    // Another delimiter is an ordinary character
    CheckWrites<std::string>({ "a;b", "c|d" }, "a;b, c|d", __func__, __LINE__);
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
ASW_TEST(TTest_ASWIni_TextConverter, Path_FromTextIsUtf8OnEveryPlatform)
{
    // Arrange
    // "cafe" with an e acute (U+00E9), and a grinning face (U+1F600, a surrogate pair in UTF-16)
    std::string const text = "caf\xC3\xA9 \xF0\x9F\x98\x80";
    std::filesystem::path value;

    // Act
    TASWIniResult const result = TASWIniConverter<std::filesystem::path>::FromText(text, value,
        TASWIniValueOptions());

    // Assert
    // Not the ANSI code page, as a narrow string is with some Windows standard libraries
    CheckTrue(result.IsOk(), __func__, __LINE__, "the path should be read: " + result.Message);
#if defined(_WIN32)
    CheckTrue(value.wstring() == std::wstring(L"caf\x00E9 \xD83D\xDE00"), __func__, __LINE__,
        "the path should be the text's characters");
#else
    CheckTrue(value.string() == text, __func__, __LINE__, "the path should be the text's bytes");
#endif
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_TextConverter, Path_FromTextKeepsTheTextExactly)
{
    // Arrange
    // Blanks, quotes and comment prefixes stay, and the separators are the platform's (bcc32c makes each '/' in a
    // Windows path a '\')
    std::string const texts[] = {
        std::string(), " a ", "\"q\"", "x=y ; z",
#if defined(_WIN32)
        "C:\\Temp\\App Data\\app.ini", "..\\a, b",
#else
        "/home/user/.config/app.ini", "../a, b",
#endif
    };

    // Act & Assert
    // The previous value is replaced
    for (std::string const& text : texts)
    {
        std::filesystem::path value = "previous";
        TASWIniResult const result = TASWIniConverter<std::filesystem::path>::FromText(text, value,
            TASWIniValueOptions());

        CheckTrue(result.IsOk(), __func__, __LINE__, "\"" + text + "\" should be read: " + result.Message);
        CheckEquals(text, ASWIni::Detail::PathToUtf8(value), __func__, __LINE__, "the path should be the text exactly");
    }
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_TextConverter, Path_FromTextRejectsInvalidUtf8)
{
    // Arrange
    // A truncated sequence, a byte that is never UTF-8, an encoded surrogate, an overlong form, and a value above
    // U+10FFFF
    std::string const texts[] = { "\xC3", "a\xFF" "b", "\xED\xA0\x80", "\xC0\xAF", "\xF4\x90\x80\x80" };

    // Act & Assert
    for (std::string const& text : texts)
    {
        std::filesystem::path value = "previous";
        TASWIniResult const result = TASWIniConverter<std::filesystem::path>::FromText(text, value,
            TASWIniValueOptions());

        CheckEquals(EIniCode::InvalidValue, result.Code, __func__, __LINE__, "invalid UTF-8 should be InvalidValue");
        CheckEquals("The value " + ASWIni::Detail::MessageText(text) + " isn't a path: it isn't valid UTF-8.",
            result.Message, __func__, __LINE__, "the message should quote the text and say why");
        CheckTrue(value == std::filesystem::path("previous"), __func__, __LINE__,
            "invalid UTF-8 should leave the path as it was");
    }
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_TextConverter, Path_RoundTripsInAList)
{
    // Arrange
    // Recent files: with blanks, the list's delimiter, non-ASCII characters, and an empty path
#if defined(_WIN32)
    std::vector<std::filesystem::path> const paths = {
        std::filesystem::path(L"C:\\My Files\\a, b.ini"), std::filesystem::path(L"D:\\caf\x00E9\\\xD83D\xDE00.txt"),
        std::filesystem::path(), std::filesystem::path(L" x "),
    };
#else
    std::vector<std::filesystem::path> const paths = {
        std::filesystem::path("/home/my files/a, b.ini"), std::filesystem::path("/caf\xC3\xA9/\xF0\x9F\x98\x80.txt"),
        std::filesystem::path(), std::filesystem::path(" x "),
    };
#endif
    std::string text;
    std::vector<std::filesystem::path> readBack;

    // Act
    TASWIniResult const written = TASWIniConverter<std::vector<std::filesystem::path> >::ToText(paths, text,
        TASWIniValueOptions());
    TASWIniResult const read = TASWIniConverter<std::vector<std::filesystem::path> >::FromText(text, readBack,
        TASWIniValueOptions());

    // Assert
    CheckTrue(written.IsOk() && read.IsOk(), __func__, __LINE__, "the list should be written and read: " +
        written.Message + read.Message);
    CheckTrue(readBack == paths, __func__, __LINE__, "the list \"" + text + "\" should read back as the paths");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_TextConverter, Path_ToTextRejectsWhatWouldNotReadBack)
{
    // Arrange
    // Windows: a path with an unpaired surrogate (UTF-16 that isn't valid, which NTFS allows), high and low; elsewhere,
    // a path with bytes that aren't UTF-8
#if defined(_WIN32)
    std::wstring high = L"a";
    high += static_cast<wchar_t>(0xD800);
    high += L"b";
    std::wstring low = L"a";
    low += static_cast<wchar_t>(0xDC00);
    std::filesystem::path const paths[] = { std::filesystem::path(high), std::filesystem::path(low) };
#else
    std::filesystem::path const paths[] = { std::filesystem::path("a\xFF" "b"), std::filesystem::path("\xC3") };
#endif

    // Act & Assert
    for (std::filesystem::path const& path : paths)
    {
        std::string text = "old";
        TASWIniResult const result = TASWIniConverter<std::filesystem::path>::ToText(path, text,
            TASWIniValueOptions());

        CheckEquals(EIniCode::InvalidValue, result.Code, __func__, __LINE__,
            "a path that wouldn't read back should be InvalidValue");
        CheckContains(result.Message, "is a path that isn't valid Unicode, so it can't be written to read back the "
            "same.", __func__, __LINE__, "the message should say why");
        CheckEquals(std::string("old"), text, __func__, __LINE__, "the text should be left as it was");
    }
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_TextConverter, Path_ToTextWritesUtf8)
{
    // Arrange
    // A path as the platform holds it: on Windows in UTF-16, with its own separators
    struct TCase
    {
        std::filesystem::path Path;
        std::string Expected;
    };

#if defined(_WIN32)
    TCase const cases[] = {
        { std::filesystem::path(L"caf\x00E9 \xD83D\xDE00"), "caf\xC3\xA9 \xF0\x9F\x98\x80" },
        { std::filesystem::path(L"C:\\Temp\\app.ini"), "C:\\Temp\\app.ini" },
        { std::filesystem::path(), "" },
    };
#else
    TCase const cases[] = {
        { std::filesystem::path("caf\xC3\xA9 \xF0\x9F\x98\x80"), "caf\xC3\xA9 \xF0\x9F\x98\x80" },
        { std::filesystem::path("/tmp/app.ini"), "/tmp/app.ini" },
        { std::filesystem::path(), "" },
    };
#endif

    // Act & Assert
    // Written() starts from "old", so these also check that the text is replaced
    for (TCase const& c : cases)
        CheckEquals(c.Expected, Written(c.Path), __func__, __LINE__, "the path should be written as UTF-8");
}
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
#if ASWINI_HAS_U8STRING
ASW_TEST(TTest_ASWIni_TextConverter, U8String_FromTextKeepsTheBytesExactly)
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
        std::u8string value = U8("previous");
        TASWIniResult const result = TASWIniConverter<std::u8string>::FromText(text, value, TASWIniValueOptions());

        CheckTrue(result.IsOk(), __func__, __LINE__, "reading a std::u8string should never fail");
        CheckTrue(value == U8(text), __func__, __LINE__, "the value should be the text's bytes exactly");
    }
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_TextConverter, U8String_RoundTripsInAList)
{
    // Arrange
    std::vector<std::u8string> const values = { U8("a, b"), U8("\xC3\xA9t\xC3\xA9"), U8(""), U8(" \"q\" ") };
    std::string text;
    std::vector<std::u8string> readBack;

    // Act
    TASWIniResult const written = TASWIniConverter<std::vector<std::u8string> >::ToText(values, text,
        TASWIniValueOptions());
    TASWIniResult const read = TASWIniConverter<std::vector<std::u8string> >::FromText(text, readBack,
        TASWIniValueOptions());

    // Assert
    CheckTrue(written.IsOk() && read.IsOk(), __func__, __LINE__, "the list should be written and read");
    CheckTrue(readBack == values, __func__, __LINE__, "the list \"" + text + "\" should read back as the values");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_TextConverter, U8String_ToTextWritesTheBytesExactly)
{
    // Arrange
    std::string const values[] = { std::string(), " a ", std::string("a\0b", 3), "\xC3\xA9t\xC3\xA9" };

    // Act & Assert
    for (std::string const& bytes : values)
    {
        std::string text = "previous";
        TASWIniResult const result = TASWIniConverter<std::u8string>::ToText(U8(bytes), text, TASWIniValueOptions());

        CheckTrue(result.IsOk(), __func__, __LINE__, "writing a std::u8string should never fail");
        CheckEquals(bytes, text, __func__, __LINE__, "the text should be the value's bytes exactly");
    }
}
//---------------------------------------------------------------------------
#endif


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
ASW_REGISTER_TEST_GROUP_IN_SUITE(ASWUnitTests::TTest_ASWIni_EnumConverter, "Converters")
ASW_REGISTER_TEST_GROUP_IN_SUITE(ASWUnitTests::TTest_ASWIni_FloatConverter, "Converters")
ASW_REGISTER_TEST_GROUP_IN_SUITE(ASWUnitTests::TTest_ASWIni_IntegerConverter, "Converters")
ASW_REGISTER_TEST_GROUP_IN_SUITE(ASWUnitTests::TTest_ASWIni_ListConverter, "Converters")
ASW_REGISTER_TEST_GROUP_IN_SUITE(ASWUnitTests::TTest_ASWIni_TextConverter, "Converters")
