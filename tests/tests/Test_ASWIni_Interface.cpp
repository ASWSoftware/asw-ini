/* **************************************************************************
Test_ASWIni_Interface.cpp
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
#include "Test_ASWIni_Interface.h"
//---------------------------------------------------------------------------
#include "ASWUnitTests_Registry.h"
//---------------------------------------------------------------------------
#include <cstddef>
#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>
//---------------------------------------------------------------------------
#include "ASWIni_Interface.h"
#include "ASWIni_Version.h"
//---------------------------------------------------------------------------

// An interface: abstract, and neither copyable nor movable
static_assert(std::is_abstract_v<ASWIni::IASWIni>, "IASWIni should be abstract");
static_assert(!std::is_copy_constructible_v<ASWIni::IASWIni>, "IASWIni shouldn't be copyable");
static_assert(!std::is_copy_assignable_v<ASWIni::IASWIni>, "IASWIni shouldn't be copy-assignable");
static_assert(!std::is_move_constructible_v<ASWIni::IASWIni>, "IASWIni shouldn't be movable");
static_assert(!std::is_move_assignable_v<ASWIni::IASWIni>, "IASWIni shouldn't be move-assignable");

namespace ASWUnitTests
{

namespace
{

// The types of the tests' own converters (see the specializations of TASWIniConverter below)

// A temperature, which has no default constructor: GetValue() needs none. Its converter is careless: it sets the
// degrees to AbsoluteZero when it fails, which GetValue() must not return.
struct TCelsius
{
    explicit TCelsius(int degrees)
        : Degrees(degrees)
    {
    }

    static constexpr int AbsoluteZero = -273;

    int Degrees;
};

// A value whose converter always throws: a std::runtime_error for the text "std" and when StdException is set, else
// an int, which isn't a std::exception
struct TBroken
{
    bool StdException = true;
};

} // namespace

} // namespace ASWUnitTests

//---------------------------------------------------------------------------

namespace ASWIni
{

// The converters of the tests' own types, which an application adds as these do

template <>
struct TASWIniConverter<ASWUnitTests::TBroken>
{
    static TASWIniResult FromText(std::string_view text, ASWUnitTests::TBroken& /*value*/,
        TASWIniValueOptions const& /*options*/)
    {
        if (text == "std")
            throw std::runtime_error("The converter is broken.");

        throw 42;
    }

    static TASWIniResult ToText(ASWUnitTests::TBroken const& value, std::string& /*text*/,
        TASWIniValueOptions const& /*options*/)
    {
        if (value.StdException)
            throw std::runtime_error("The converter is broken.");

        throw 42;
    }
};

template <>
struct TASWIniConverter<ASWUnitTests::TCelsius>
{
    static TASWIniResult FromText(std::string_view text, ASWUnitTests::TCelsius& value,
        TASWIniValueOptions const& options)
    {
        int degrees = 0;
        TASWIniResult const result = TASWIniConverter<int>::FromText(text, degrees, options);
        value.Degrees = result.IsOk() ? degrees : ASWUnitTests::TCelsius::AbsoluteZero;

        return result;
    }

    static TASWIniResult ToText(ASWUnitTests::TCelsius const& value, std::string& text,
        TASWIniValueOptions const& options)
    {
        return TASWIniConverter<int>::ToText(value.Degrees, text, options);
    }
};

} // namespace ASWIni

//---------------------------------------------------------------------------

namespace ASWUnitTests
{

using ASWIni::EBoolFormat;
using ASWIni::EIniCode;
using ASWIni::ESeverity;
using ASWIni::TASWIniConverter;
using ASWIni::TASWIniDocument;
using ASWIni::TASWIniEnumName;
using ASWIni::TASWIniLogCallback;
using ASWIni::TASWIniMessage;
using ASWIni::TASWIniOptions;
using ASWIni::TASWIniResult;
using ASWIni::TASWIniValueOptions;

namespace
{

// An enum with names, for SetValue()'s conversion failure: Unlisted has no name, so it can't be written
enum class EColor
{
    Red,
    Green,
    Unlisted,
};

auto const& ASWIniEnumNames(EColor)
{
    static constexpr TASWIniEnumName<EColor> names[] = { { EColor::Red, "Red" }, { EColor::Green, "Green" } };

    return names;
}


/////////////////////////////////////////////////////////////////////////////
// TFakeIni
//
// An IASWIni that keeps the value texts in a map (names compared as they are), and records what the typed templates
// ask of it: each SetText() call, each GetValueOptions() call and each message reported. The operations the templates
// don't use do as little as they can.
/////////////////////////////////////////////////////////////////////////////
class TFakeIni : public ASWIni::IASWIni
{
protected:
    // Adds 'message' to Reported
    void Report(TASWIniMessage const& message) const override;

public: // What the tests set and check
    std::map<std::pair<std::string, std::string>, std::string> Texts; // The value texts by section and key
    TASWIniValueOptions ValueOptions; // What GetValueOptions() gives
    mutable std::size_t ValueOptionsCalls = 0; // How often GetValueOptions() was called
    std::vector<std::string> SetTextCalls; // Each SetText() call, as "[section] key=text"
    mutable std::vector<TASWIniMessage> Reported; // Each message reported, in order

public:
    TFakeIni() = default;

    TASWIniResult AddSection(std::string_view section) override;
    TASWIniDocument GetDocument() const override;
    std::string GetFileComment() const override;
    std::string GetFilePath() const override;
    std::string GetKeyComment(std::string_view section, std::string_view key) const override;
    std::vector<std::string> GetKeyNames(std::string_view section) const override;
    TASWIniOptions GetOptions() const override;
    std::string GetSectionComment(std::string_view section) const override;
    std::vector<std::string> GetSectionNames(std::string_view prefix) const override;
    TASWIniValueOptions GetValueOptions() const override;
    std::string_view GetVersionStr() const noexcept override;
    bool HasKey(std::string_view section, std::string_view key) const override;
    bool HasSection(std::string_view section) const override;
    bool IsModified() const override;
    TASWIniResult Load(std::string_view filePath) override;
    TASWIniResult LoadText(std::string_view text) override;
    bool RemoveKey(std::string_view section, std::string_view key) override;
    bool RemoveSection(std::string_view section) override;
    TASWIniResult RenameKey(std::string_view section, std::string_view key, std::string_view newKey) override;
    TASWIniResult RenameSection(std::string_view section, std::string_view newSection) override;
    TASWIniResult Save() override;
    TASWIniResult SaveAs(std::string_view filePath) override;
    TASWIniResult SetFileComment(std::string_view text) override;
    TASWIniResult SetKeyComment(std::string_view section, std::string_view key, std::string_view text) override;
    void SetLogCallback(TASWIniLogCallback callback) override;
    TASWIniResult SetOptions(TASWIniOptions const& options) override;
    TASWIniResult SetSectionComment(std::string_view section, std::string_view text) override;

    // Records the call; fails with InvalidArgument for an empty key (without reporting it, as the templates must
    // leave that to SetText()), else sets the text in Texts
    TASWIniResult SetText(std::string_view section, std::string_view key, std::string_view text) override;

    std::string ToText() const override;
    bool TryGetText(std::string_view section, std::string_view key, std::string& text) const override;
};

//---------------------------------------------------------------------------

std::string ConverterMessage(std::string_view text);
std::string ListText(std::vector<int> const& items);

//---------------------------------------------------------------------------
/*
    ConverterMessage

    The message TASWIniConverter<int> fails to read 'text' with.
*/
std::string ConverterMessage(std::string_view text)
{
    int value = 0;

    return TASWIniConverter<int>::FromText(text, value, TASWIniValueOptions()).Message;
}
//---------------------------------------------------------------------------
/*
    ListText

    'items' as a message shows them, e.g. "{1, 2, 3}".
*/
std::string ListText(std::vector<int> const& items)
{
    std::string text = "{";
    for (std::size_t i = 0; i < items.size(); ++i)
    {
        if (i > 0)
            text += ", ";
        text += std::to_string(items[i]);
    }

    text += "}";

    return text;
}
//---------------------------------------------------------------------------


/////////////////////////////////////////////////////////////////////////////
// TFakeIni
/////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------
TASWIniResult TFakeIni::AddSection(std::string_view /*section*/)
{
    return TASWIniResult();
}
//---------------------------------------------------------------------------
TASWIniDocument TFakeIni::GetDocument() const
{
    return TASWIniDocument();
}
//---------------------------------------------------------------------------
std::string TFakeIni::GetFileComment() const
{
    return std::string();
}
//---------------------------------------------------------------------------
std::string TFakeIni::GetFilePath() const
{
    return std::string();
}
//---------------------------------------------------------------------------
std::string TFakeIni::GetKeyComment(std::string_view /*section*/, std::string_view /*key*/) const
{
    return std::string();
}
//---------------------------------------------------------------------------
std::vector<std::string> TFakeIni::GetKeyNames(std::string_view /*section*/) const
{
    return std::vector<std::string>();
}
//---------------------------------------------------------------------------
TASWIniOptions TFakeIni::GetOptions() const
{
    TASWIniOptions options;
    options.Values = ValueOptions;

    return options;
}
//---------------------------------------------------------------------------
std::string TFakeIni::GetSectionComment(std::string_view /*section*/) const
{
    return std::string();
}
//---------------------------------------------------------------------------
std::vector<std::string> TFakeIni::GetSectionNames(std::string_view /*prefix*/) const
{
    return std::vector<std::string>();
}
//---------------------------------------------------------------------------
TASWIniValueOptions TFakeIni::GetValueOptions() const
{
    ++ValueOptionsCalls;

    return ValueOptions;
}
//---------------------------------------------------------------------------
std::string_view TFakeIni::GetVersionStr() const noexcept
{
    return ASWIni::Version;
}
//---------------------------------------------------------------------------
bool TFakeIni::HasKey(std::string_view section, std::string_view key) const
{
    return Texts.count(std::make_pair(std::string(section), std::string(key))) > 0;
}
//---------------------------------------------------------------------------
bool TFakeIni::HasSection(std::string_view section) const
{
    for (auto const& text : Texts)
    {
        if (text.first.first == section)
            return true;
    }

    return section.empty();
}
//---------------------------------------------------------------------------
bool TFakeIni::IsModified() const
{
    return false;
}
//---------------------------------------------------------------------------
TASWIniResult TFakeIni::Load(std::string_view /*filePath*/)
{
    return TASWIniResult();
}
//---------------------------------------------------------------------------
TASWIniResult TFakeIni::LoadText(std::string_view /*text*/)
{
    return TASWIniResult();
}
//---------------------------------------------------------------------------
bool TFakeIni::RemoveKey(std::string_view section, std::string_view key)
{
    return Texts.erase(std::make_pair(std::string(section), std::string(key))) > 0;
}
//---------------------------------------------------------------------------
bool TFakeIni::RemoveSection(std::string_view /*section*/)
{
    return false;
}
//---------------------------------------------------------------------------
TASWIniResult TFakeIni::RenameKey(std::string_view /*section*/, std::string_view /*key*/,
    std::string_view /*newKey*/)
{
    return TASWIniResult();
}
//---------------------------------------------------------------------------
TASWIniResult TFakeIni::RenameSection(std::string_view /*section*/, std::string_view /*newSection*/)
{
    return TASWIniResult();
}
//---------------------------------------------------------------------------
void TFakeIni::Report(TASWIniMessage const& message) const
{
    Reported.push_back(message);
}
//---------------------------------------------------------------------------
TASWIniResult TFakeIni::Save()
{
    return TASWIniResult();
}
//---------------------------------------------------------------------------
TASWIniResult TFakeIni::SaveAs(std::string_view /*filePath*/)
{
    return TASWIniResult();
}
//---------------------------------------------------------------------------
TASWIniResult TFakeIni::SetFileComment(std::string_view /*text*/)
{
    return TASWIniResult();
}
//---------------------------------------------------------------------------
TASWIniResult TFakeIni::SetKeyComment(std::string_view /*section*/, std::string_view /*key*/,
    std::string_view /*text*/)
{
    return TASWIniResult();
}
//---------------------------------------------------------------------------
void TFakeIni::SetLogCallback(TASWIniLogCallback /*callback*/)
{
}
//---------------------------------------------------------------------------
TASWIniResult TFakeIni::SetOptions(TASWIniOptions const& options)
{
    ValueOptions = options.Values;

    return TASWIniResult();
}
//---------------------------------------------------------------------------
TASWIniResult TFakeIni::SetSectionComment(std::string_view /*section*/, std::string_view /*text*/)
{
    return TASWIniResult();
}
//---------------------------------------------------------------------------
TASWIniResult TFakeIni::SetText(std::string_view section, std::string_view key, std::string_view text)
{
    std::string call = "[";
    call += section;
    call += "] ";
    call += key;
    call += "=";
    call += text;
    SetTextCalls.push_back(call);
    if (key.empty())
    {
        TASWIniResult result;
        result.Code = EIniCode::InvalidArgument;
        result.Message = "The key name is empty.";

        return result;
    }

    Texts[std::make_pair(std::string(section), std::string(key))] = std::string(text);

    return TASWIniResult();
}
//---------------------------------------------------------------------------
std::string TFakeIni::ToText() const
{
    return std::string();
}
//---------------------------------------------------------------------------
bool TFakeIni::TryGetText(std::string_view section, std::string_view key, std::string& text) const
{
    auto const found = Texts.find(std::make_pair(std::string(section), std::string(key)));
    if (found == Texts.end())
        return false;

    text = found->second;

    return true;
}
//---------------------------------------------------------------------------

} // namespace

//---------------------------------------------------------------------------

/////////////////////////////////////////////////////////////////////////////
// TTest_ASWIni_Interface
/////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------
TTest_ASWIni_Interface::TTest_ASWIni_Interface()
    : inherited("ASWIni_Interface_Tests")
{
}
//---------------------------------------------------------------------------
TTest_ASWIni_Interface::~TTest_ASWIni_Interface()
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_Interface::SetUp_Group()
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_Interface::SetUp_Test(ITestCase& /*testCase*/)
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_Interface::TearDown_Group()
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_Interface::TearDown_Test(ITestCase& /*testCase*/)
{
}
//---------------------------------------------------------------------------

// /////// Begin tests after this line ///////////////////////

//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_Interface, GetValue_ConverterExceptionIsThrown)
{
    // Arrange
    TFakeIni ini;
    ini.Texts[{ "Broken", "Value" }] = "std";
    auto const getValue = [&ini]()
        {
            static_cast<void>(ini.GetValue("Broken", "Value", TBroken()));
        };

    // Act & Assert
    CheckThrows<std::runtime_error>(getValue, __func__, __LINE__, "the converter's exception should pass through",
        "The converter is broken.");
    CheckEquals(std::size_t(0), ini.Reported.size(), __func__, __LINE__, "nothing should be reported");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_Interface, GetValue_ConvertsTheValue)
{
    // Arrange
    TFakeIni ini;
    ini.Texts[{ "Window", "Width" }] = "640";
    ini.Texts[{ "Window", "Maximized" }] = "yes";
    ini.Texts[{ "Window", "Zoom" }] = "1.5";
    ini.Texts[{ "Window", "Color" }] = "green";
    ini.Texts[{ "Window", "Columns" }] = "10, 20, 30";
    ini.Texts[{ "Window", "Title" }] = "My \"App\"";
    ini.Texts[{ "", "Global" }] = "-7";

    // Act
    int const width = ini.GetValue("Window", "Width", 0);
    bool const maximized = ini.GetValue("Window", "Maximized", false);
    double const zoom = ini.GetValue("Window", "Zoom", 1.0);
    EColor const color = ini.GetValue("Window", "Color", EColor::Red);
    std::vector<int> const columns = ini.GetValue("Window", "Columns", std::vector<int>());
    std::string const title = ini.GetValue("Window", "Title", std::string());
    long long const global = ini.GetValue("", "Global", 0LL);

    // Assert
    CheckEquals(640, width, __func__, __LINE__, "an int should be read");
    CheckTrue(maximized, __func__, __LINE__, "a bool should be read");
    CheckTrue(zoom == 1.5, __func__, __LINE__, "a double should be read");
    CheckTrue(color == EColor::Green, __func__, __LINE__, "an enum should be read by name");
    CheckEquals(std::string("{10, 20, 30}"), ListText(columns), __func__, __LINE__, "a list should be read");
    CheckEquals(std::string("My \"App\""), title, __func__, __LINE__, "a std::string should be the text");
    CheckEquals(-7LL, global, __func__, __LINE__, "a key before the first section should be read");
    CheckEquals(std::size_t(0), ini.Reported.size(), __func__, __LINE__, "nothing should be reported");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_Interface, GetValue_InvalidValueGivesTheDefaultAndAWarning)
{
    // Arrange
    TFakeIni ini;
    ini.Texts[{ "Window", "Width" }] = "wide";

    // Act
    int const width = ini.GetValue("Window", "Width", 640);

    // Assert
    CheckEquals(640, width, __func__, __LINE__, "a value that doesn't convert should give the default");
    CheckEquals(std::size_t(1), ini.Reported.size(), __func__, __LINE__, "one message should be reported");
    if (ini.Reported.size() == 1)
    {
        TASWIniMessage const& message = ini.Reported.front();
        CheckEquals(ESeverity::Warning, message.Severity, __func__, __LINE__, "the message should be a Warning");
        CheckEquals(EIniCode::InvalidValue, message.Code, __func__, __LINE__, "the message should have the "
            "converter's code");
        CheckEquals(ConverterMessage("wide"), message.Text, __func__, __LINE__,
            "the message should have the converter's text");
        CheckEquals(std::string("Window"), message.Section, __func__, __LINE__, "the message should name the section");
        CheckEquals(std::string("Width"), message.Key, __func__, __LINE__, "the message should name the key");
        CheckEquals(std::string(), message.FilePath, __func__, __LINE__, "the file path should be left to Report()");
        CheckEquals(std::size_t(0), message.Line, __func__, __LINE__, "the message should have no line");
    }
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_Interface, GetValue_MissingKeyGivesTheDefault)
{
    // Arrange
    TFakeIni ini;
    ini.Texts[{ "Window", "Width" }] = "640";

    // Act
    int const height = ini.GetValue("Window", "Height", 480);
    int const width = ini.GetValue("Dialog", "Width", 320);
    std::vector<int> const columns = ini.GetValue("Window", "Columns", std::vector<int>{ 1, 2 });

    // Assert
    CheckEquals(480, height, __func__, __LINE__, "a missing key should give the default");
    CheckEquals(320, width, __func__, __LINE__, "a key of a missing section should give the default");
    CheckEquals(std::string("{1, 2}"), ListText(columns), __func__, __LINE__, "a missing list should be the default");
    CheckEquals(std::size_t(0), ini.Reported.size(), __func__, __LINE__, "a missing key should report nothing");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_Interface, GetValue_OutOfRangeValueIsAWarning)
{
    // Arrange
    TFakeIni ini;
    ini.Texts[{ "Limits", "Level" }] = "300";

    // Act
    std::uint8_t const level = ini.GetValue("Limits", "Level", std::uint8_t(5));

    // Assert
    CheckEquals(5, static_cast<int>(level), __func__, __LINE__, "a value out of range should give the default");
    CheckEquals(std::size_t(1), ini.Reported.size(), __func__, __LINE__, "one message should be reported");
    if (ini.Reported.size() == 1)
    {
        CheckEquals(ESeverity::Warning, ini.Reported.front().Severity, __func__, __LINE__,
            "the message should be a Warning");
        CheckEquals(EIniCode::ValueOutOfRange, ini.Reported.front().Code, __func__, __LINE__,
            "the message should have the converter's code");
    }
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_Interface, GetValue_TextDefaultGivesTheText)
{
    // Arrange
    TFakeIni ini;
    ini.Texts[{ "App", "Name" }] = " My App ";
    char const* const nullText = nullptr;
    char buffer[] = "buffer";
    char* const pointer = buffer;
    static_assert(std::is_same_v<decltype(ini.GetValue("App", "Name", "x")), std::string>,
        "a string literal default should give a std::string");
    static_assert(std::is_same_v<decltype(ini.GetValue("App", "Name", pointer)), std::string>,
        "a char pointer default should give a std::string");
    static_assert(std::is_same_v<decltype(ini.GetValue("App", "Name", std::string_view())), std::string>,
        "a std::string_view default should give a std::string");

    // Act
    std::string const name = ini.GetValue("App", "Name", "Default");
    std::string const literal = ini.GetValue("App", "Missing", "Default");
    std::string const fromNull = ini.GetValue("App", "Missing", nullText);
    std::string const fromBuffer = ini.GetValue("App", "Missing", buffer);
    std::string const fromPointer = ini.GetValue("App", "Missing", pointer);
    std::string const fromView = ini.GetValue("App", "Missing", std::string_view("view"));

    // Assert
    CheckEquals(std::string(" My App "), name, __func__, __LINE__, "the text should be the value as it is");
    CheckEquals(std::string("Default"), literal, __func__, __LINE__, "a missing key should give a literal default");
    CheckEquals(std::string(), fromNull, __func__, __LINE__, "a nullptr default should be empty");
    CheckEquals(std::string("buffer"), fromBuffer, __func__, __LINE__, "a char array default should be its text");
    CheckEquals(std::string("buffer"), fromPointer, __func__, __LINE__, "a char pointer default should be its text");
    CheckEquals(std::string("view"), fromView, __func__, __LINE__, "a std::string_view default should be its text");
    CheckEquals(std::size_t(0), ini.ValueOptionsCalls, __func__, __LINE__,
        "a text needs no converter, so no value options");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_Interface, GetValue_TypeWithoutDefaultConstructor)
{
    // Arrange
    static_assert(!std::is_default_constructible_v<TCelsius>, "TCelsius should have no default constructor");
    TFakeIni ini;
    ini.Texts[{ "Room", "Temperature" }] = "21";
    ini.Texts[{ "Room", "Invalid" }] = "warm";

    // Act
    TCelsius const temperature = ini.GetValue("Room", "Temperature", TCelsius(18));
    TCelsius const missing = ini.GetValue("Room", "Missing", TCelsius(18));
    TCelsius const invalid = ini.GetValue("Room", "Invalid", TCelsius(18));

    // Assert
    CheckEquals(21, temperature.Degrees, __func__, __LINE__, "an application's converter should read the value");
    CheckEquals(18, missing.Degrees, __func__, __LINE__, "a missing key should give the default");
    CheckEquals(18, invalid.Degrees, __func__, __LINE__,
        "a value that doesn't convert should give the default, whatever the converter left");
    CheckEquals(std::size_t(1), ini.Reported.size(), __func__, __LINE__, "the invalid value should be reported");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_Interface, GetValue_UsesTheGivenValueOptions)
{
    // Arrange
    TFakeIni ini;
    ini.Texts[{ "Window", "Columns" }] = "10; 20, 30";
    TASWIniValueOptions options;
    options.ListDelimiter = ';';

    // Act
    std::vector<std::string> const columns = ini.GetValue("Window", "Columns", std::vector<std::string>(), options);

    // Assert
    CheckEquals(std::size_t(2), columns.size(), __func__, __LINE__, "the list should be split at the given delimiter");
    if (columns.size() == 2)
        CheckEquals(std::string("20, 30"), columns[1], __func__, __LINE__, "the default delimiter should be text");

    CheckEquals(std::size_t(0), ini.ValueOptionsCalls, __func__, __LINE__,
        "the object's value options shouldn't be asked for");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_Interface, GetValue_UsesTheValueOptions)
{
    // Arrange
    TFakeIni ini;
    ini.Texts[{ "Window", "Columns" }] = "10; 20, 30";
    ini.ValueOptions.ListDelimiter = ';';

    // Act
    std::vector<std::string> const columns = ini.GetValue("Window", "Columns", std::vector<std::string>());

    // Assert
    CheckEquals(std::size_t(2), columns.size(), __func__, __LINE__,
        "the list should be split at the object's delimiter");
    CheckEquals(std::size_t(1), ini.ValueOptionsCalls, __func__, __LINE__,
        "the object's value options should be asked for once");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_Interface, SetValue_ConversionFailureIsReported)
{
    // Arrange
    TFakeIni ini;
    std::string text;
    std::string const converterMessage = TASWIniConverter<EColor>::ToText(EColor::Unlisted, text,
        TASWIniValueOptions()).Message;

    // Act
    TASWIniResult const result = ini.SetValue("Window", "Color", EColor::Unlisted);

    // Assert
    std::string const expected = "The key \"Color\" of the section \"Window\" can't be set: " + converterMessage;
    CheckEquals(EIniCode::InvalidValue, result.Code, __func__, __LINE__, "the result should have the converter's code");
    CheckEquals(expected, result.Message, __func__, __LINE__,
        "the message should name the key and section, then give the converter's");
    CheckEquals(std::size_t(0), ini.SetTextCalls.size(), __func__, __LINE__, "nothing should be set");
    CheckEquals(std::size_t(1), ini.Reported.size(), __func__, __LINE__, "the failure should be reported once");
    if (ini.Reported.size() == 1)
    {
        TASWIniMessage const& message = ini.Reported.front();
        CheckEquals(ESeverity::Error, message.Severity, __func__, __LINE__, "the message should be an Error");
        CheckEquals(EIniCode::InvalidValue, message.Code, __func__, __LINE__, "the message should have the code");
        CheckEquals(expected, message.Text, __func__, __LINE__, "the message should have the result's text");
        CheckEquals(std::string(), message.Section, __func__, __LINE__,
            "the section should only be in the text, as in every failed result's message");
        CheckEquals(std::string(), message.Key, __func__, __LINE__, "the key should only be in the text");
    }
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_Interface, SetValue_ConverterExceptionIsReported)
{
    // Arrange
    TFakeIni ini;
    TBroken other;
    other.StdException = false;

    // Act
    TASWIniResult const stdResult = ini.SetValue("Broken", "Value", TBroken());
    TASWIniResult const otherResult = ini.SetValue("Broken", "Value", other, TASWIniValueOptions());

    // Assert
    CheckEquals(EIniCode::UnexpectedException, stdResult.Code, __func__, __LINE__,
        "a std::exception should be UnexpectedException");
    CheckEquals(std::string("The key \"Value\" of the section \"Broken\" can't be set: The converter is broken."),
        stdResult.Message, __func__, __LINE__, "the message should give what()");
    CheckEquals(EIniCode::UnexpectedException, otherResult.Code, __func__, __LINE__,
        "another exception should be UnexpectedException");
    CheckEquals(std::string("The key \"Value\" of the section \"Broken\" can't be set: An exception that isn't a "
        "std::exception was thrown."), otherResult.Message, __func__, __LINE__,
        "the message should say what was thrown");
    CheckEquals(std::size_t(2), ini.Reported.size(), __func__, __LINE__, "each failure should be reported");
    if (ini.Reported.size() == 2)
    {
        CheckEquals(ESeverity::Error, ini.Reported[0].Severity, __func__, __LINE__, "the message should be an Error");
        CheckEquals(stdResult.Message, ini.Reported[0].Text, __func__, __LINE__,
            "the first message should have the first result's text");
        CheckEquals(otherResult.Message, ini.Reported[1].Text, __func__, __LINE__,
            "the second message should have the second result's text");
    }
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_Interface, SetValue_ConvertsTheValue)
{
    // Arrange
    TFakeIni ini;

    // Act
    TASWIniResult const width = ini.SetValue("Window", "Width", 640);
    TASWIniResult const maximized = ini.SetValue("Window", "Maximized", true);
    TASWIniResult const zoom = ini.SetValue("Window", "Zoom", 0.1);
    TASWIniResult const color = ini.SetValue("Window", "Color", EColor::Green);
    TASWIniResult const columns = ini.SetValue("Window", "Columns", std::vector<int>{ 10, 20 });
    TASWIniResult const title = ini.SetValue("Window", "Title", std::string(" My App "));
    TASWIniResult const temperature = ini.SetValue("Room", "Temperature", TCelsius(-5));

    // Assert
    CheckTrue(width.IsOk() && maximized.IsOk() && zoom.IsOk() && color.IsOk() && columns.IsOk() && title.IsOk() &&
        temperature.IsOk(), __func__, __LINE__, "every value should be set");
    std::vector<std::string> const expected = {
        "[Window] Width=640", "[Window] Maximized=true", "[Window] Zoom=0.1", "[Window] Color=Green",
        "[Window] Columns=10, 20", "[Window] Title= My App ", "[Room] Temperature=-5",
    };
    CheckEquals(expected.size(), ini.SetTextCalls.size(), __func__, __LINE__,
        "each value should be set with SetText()");
    for (std::size_t i = 0; i < expected.size() && i < ini.SetTextCalls.size(); ++i)
        CheckEquals(expected[i], ini.SetTextCalls[i], __func__, __LINE__, "the text should be the converter's");

    CheckEquals(std::size_t(0), ini.Reported.size(), __func__, __LINE__, "nothing should be reported");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_Interface, SetValue_SetTextFailureIsReturned)
{
    // Arrange
    TFakeIni ini;

    // Act
    TASWIniResult const typed = ini.SetValue("Window", "", 640);
    TASWIniResult const text = ini.SetValue("Window", "", "text");

    // Assert
    CheckEquals(EIniCode::InvalidArgument, typed.Code, __func__, __LINE__, "SetText()'s failure should be returned");
    CheckEquals(std::string("The key name is empty."), typed.Message, __func__, __LINE__,
        "SetText()'s message should be returned as it is");
    CheckEquals(EIniCode::InvalidArgument, text.Code, __func__, __LINE__,
        "SetText()'s failure should be returned for a text too");
    CheckEquals(std::size_t(0), ini.Reported.size(), __func__, __LINE__, "SetText() should report its own failure");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_Interface, SetValue_TextIsSetAsItIs)
{
    // Arrange
    TFakeIni ini;
    char const* const nullText = nullptr;
    char buffer[] = "buffer";
    char* const pointer = buffer;
    std::string const view = " view ";

    // Act
    TASWIniResult const literal = ini.SetValue("App", "Literal", "a, b");
    TASWIniResult const fromNull = ini.SetValue("App", "Null", nullText);
    TASWIniResult const fromBuffer = ini.SetValue("App", "Buffer", buffer);
    TASWIniResult const fromPointer = ini.SetValue("App", "Pointer", pointer);
    TASWIniResult const fromView = ini.SetValue("App", "View", std::string_view(view));

    // Assert
    CheckTrue(literal.IsOk() && fromNull.IsOk() && fromBuffer.IsOk() && fromPointer.IsOk() && fromView.IsOk(),
        __func__, __LINE__, "every text should be set");
    std::vector<std::string> const expected = {
        "[App] Literal=a, b", "[App] Null=", "[App] Buffer=buffer", "[App] Pointer=buffer", "[App] View= view ",
    };
    CheckEquals(expected.size(), ini.SetTextCalls.size(), __func__, __LINE__, "each text should be set with SetText()");
    for (std::size_t i = 0; i < expected.size() && i < ini.SetTextCalls.size(); ++i)
        CheckEquals(expected[i], ini.SetTextCalls[i], __func__, __LINE__, "the text should be set as it is");

    CheckEquals(std::size_t(0), ini.ValueOptionsCalls, __func__, __LINE__,
        "a text needs no converter, so no value options");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_Interface, SetValue_UsesTheGivenValueOptions)
{
    // Arrange
    TFakeIni ini;
    ini.ValueOptions.BoolFormat = EBoolFormat::YesNo;
    TASWIniValueOptions options;
    options.BoolFormat = EBoolFormat::OnOff;

    // Act
    TASWIniResult const result = ini.SetValue("Window", "Maximized", true, options);

    // Assert
    CheckTrue(result.IsOk(), __func__, __LINE__, "the value should be set");
    CheckEquals(std::size_t(1), ini.SetTextCalls.size(), __func__, __LINE__, "the value should be set with SetText()");
    if (ini.SetTextCalls.size() == 1)
        CheckEquals(std::string("[Window] Maximized=on"), ini.SetTextCalls[0], __func__, __LINE__,
            "the value should be written as the given options say");

    CheckEquals(std::size_t(0), ini.ValueOptionsCalls, __func__, __LINE__,
        "the object's value options shouldn't be asked for");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_Interface, SetValue_UsesTheValueOptions)
{
    // Arrange
    TFakeIni ini;
    ini.ValueOptions.BoolFormat = EBoolFormat::YesNo;

    // Act
    TASWIniResult const result = ini.SetValue("Window", "Maximized", true);

    // Assert
    CheckTrue(result.IsOk(), __func__, __LINE__, "the value should be set");
    CheckEquals(std::size_t(1), ini.SetTextCalls.size(), __func__, __LINE__, "the value should be set with SetText()");
    if (ini.SetTextCalls.size() == 1)
        CheckEquals(std::string("[Window] Maximized=yes"), ini.SetTextCalls[0], __func__, __LINE__,
            "the value should be written as the object's options say");

    CheckEquals(std::size_t(1), ini.ValueOptionsCalls, __func__, __LINE__,
        "the object's value options should be asked for once");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_Interface, TryGetValue_ConverterExceptionIsReturned)
{
    // Arrange
    TFakeIni ini;
    ini.Texts[{ "Broken", "Std" }] = "std";
    ini.Texts[{ "Broken", "Other" }] = "other";
    TBroken value;
    value.StdException = false;

    // Act
    TASWIniResult const stdResult = ini.TryGetValue("Broken", "Std", value);
    TASWIniResult const otherResult = ini.TryGetValue("Broken", "Other", value, TASWIniValueOptions());

    // Assert
    CheckEquals(EIniCode::UnexpectedException, stdResult.Code, __func__, __LINE__,
        "a std::exception should be UnexpectedException");
    CheckEquals(std::string("The key \"Std\" of the section \"Broken\" can't be read: The converter is broken."),
        stdResult.Message, __func__, __LINE__, "the message should give what()");
    CheckEquals(EIniCode::UnexpectedException, otherResult.Code, __func__, __LINE__,
        "another exception should be UnexpectedException");
    CheckEquals(std::string("The key \"Other\" of the section \"Broken\" can't be read: An exception that isn't a "
        "std::exception was thrown."), otherResult.Message, __func__, __LINE__,
        "the message should say what was thrown");
    CheckFalse(value.StdException, __func__, __LINE__, "the value should stay as it was");
    CheckEquals(std::size_t(0), ini.Reported.size(), __func__, __LINE__, "nothing should be reported");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_Interface, TryGetValue_ConvertsTheValue)
{
    // Arrange
    TFakeIni ini;
    ini.Texts[{ "Window", "Width" }] = "640";
    ini.Texts[{ "Window", "Color" }] = "Green";
    ini.Texts[{ "Window", "Title" }] = " My App ";
    int width = 0;
    EColor color = EColor::Red;
    std::string title;

    // Act
    TASWIniResult const widthResult = ini.TryGetValue("Window", "Width", width);
    TASWIniResult const colorResult = ini.TryGetValue("Window", "Color", color);
    TASWIniResult const titleResult = ini.TryGetValue("Window", "Title", title);

    // Assert
    CheckTrue(widthResult.IsOk() && colorResult.IsOk() && titleResult.IsOk(), __func__, __LINE__,
        "every value should be read");
    CheckEquals(640, width, __func__, __LINE__, "an int should be read");
    CheckTrue(color == EColor::Green, __func__, __LINE__, "an enum should be read");
    CheckEquals(std::string(" My App "), title, __func__, __LINE__, "a std::string should be the text as it is");
    CheckEquals(std::size_t(0), ini.Reported.size(), __func__, __LINE__, "nothing should be reported");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_Interface, TryGetValue_InvalidValueKeepsTheValue)
{
    // Arrange
    TFakeIni ini;
    ini.Texts[{ "Window", "Width" }] = "wide";
    ini.Texts[{ "Limits", "Level" }] = "300";
    int width = 640;
    std::uint8_t level = 5;

    // Act
    TASWIniResult const widthResult = ini.TryGetValue("Window", "Width", width);
    TASWIniResult const levelResult = ini.TryGetValue("Limits", "Level", level);

    // Assert
    CheckEquals(EIniCode::InvalidValue, widthResult.Code, __func__, __LINE__,
        "the result should have the converter's code");
    CheckEquals("The key \"Width\" of the section \"Window\" can't be read: " + ConverterMessage("wide"),
        widthResult.Message, __func__, __LINE__, "the message should name the key and section, then give the "
        "converter's");
    CheckEquals(640, width, __func__, __LINE__, "the value should stay as it was");
    CheckEquals(EIniCode::ValueOutOfRange, levelResult.Code, __func__, __LINE__,
        "a value out of range should be ValueOutOfRange");
    CheckEquals(5, static_cast<int>(level), __func__, __LINE__, "the value out of range should stay as it was");
    CheckEquals(std::size_t(0), ini.Reported.size(), __func__, __LINE__, "nothing should be reported");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_Interface, TryGetValue_MissingKeyIsNotFound)
{
    // Arrange
    TFakeIni ini;
    ini.Texts[{ "Window", "Width" }] = "640";
    int height = 480;

    // Act
    TASWIniResult const result = ini.TryGetValue("Window", "Height", height);

    // Assert
    CheckEquals(EIniCode::NotFound, result.Code, __func__, __LINE__, "a missing key should be NotFound");
    CheckEquals(std::string("The key \"Height\" of the section \"Window\" doesn't exist."), result.Message, __func__,
        __LINE__, "the message should name the key and section");
    CheckEquals(480, height, __func__, __LINE__, "the value should stay as it was");
    CheckEquals(std::size_t(0), ini.Reported.size(), __func__, __LINE__, "nothing should be reported");
    CheckEquals(std::size_t(0), ini.ValueOptionsCalls, __func__, __LINE__,
        "the value options should only be asked for when the key exists");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_Interface, TryGetValue_UsesTheGivenValueOptions)
{
    // Arrange
    TFakeIni ini;
    ini.Texts[{ "Window", "Columns" }] = "10; 20";
    TASWIniValueOptions options;
    options.ListDelimiter = ';';
    std::vector<int> columns;

    // Act
    TASWIniResult const result = ini.TryGetValue("Window", "Columns", columns, options);

    // Assert
    CheckTrue(result.IsOk(), __func__, __LINE__, "the list should be read");
    CheckEquals(std::string("{10, 20}"), ListText(columns), __func__, __LINE__,
        "the list should be split at the given delimiter");
    CheckEquals(std::size_t(0), ini.ValueOptionsCalls, __func__, __LINE__,
        "the object's value options shouldn't be asked for");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_Interface, TryGetValue_UsesTheValueOptions)
{
    // Arrange
    TFakeIni ini;
    ini.Texts[{ "Window", "Columns" }] = "10; 20";
    ini.ValueOptions.ListDelimiter = ';';
    std::vector<int> columns;

    // Act
    TASWIniResult const result = ini.TryGetValue("Window", "Columns", columns);

    // Assert
    CheckTrue(result.IsOk(), __func__, __LINE__, "the list should be read");
    CheckEquals(std::string("{10, 20}"), ListText(columns), __func__, __LINE__,
        "the list should be split at the object's delimiter");
    CheckEquals(std::size_t(1), ini.ValueOptionsCalls, __func__, __LINE__,
        "the object's value options should be asked for once");
}
//---------------------------------------------------------------------------

} // namespace ASWUnitTests

//---------------------------------------------------------------------------
ASW_REGISTER_TEST_GROUP(ASWUnitTests::TTest_ASWIni_Interface)
