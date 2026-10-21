/* **************************************************************************
Test_ASWIni_Settings.cpp
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
#include "Test_ASWIni_Settings.h"
//---------------------------------------------------------------------------
#include "ASWUnitTests_Registry.h"
//---------------------------------------------------------------------------
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>
//---------------------------------------------------------------------------
#include "ASWIni_Base.h"
#include "ASWIni_Converters.h"
#include "ASWIni_Document.h"
#include "ASWIni_File.h"
#include "ASWIni_Settings.h"
#include "UT_Helper_FailingAlloc.h"
#include "UT_Helper_TempDir.h"
#include "UT_Helper_Threads.h"
//---------------------------------------------------------------------------

// Neither copyable nor movable: settings and sections are bound to their owner by address
static_assert(!std::is_copy_constructible_v<ASWIni::TASWIniSection>, "TASWIniSection shouldn't be copyable");
static_assert(!std::is_move_constructible_v<ASWIni::TASWIniSection>, "TASWIniSection shouldn't be movable");
static_assert(!std::is_copy_constructible_v<ASWIni::TASWIniSetting<int> >, "TASWIniSetting shouldn't be copyable");
static_assert(!std::is_move_constructible_v<ASWIni::TASWIniSetting<int> >, "TASWIniSetting shouldn't be movable");

// A range needs operator<
static_assert(ASWIni::Detail::IsLessComparable<int>, "an int should have operator<");
static_assert(ASWIni::Detail::IsLessComparable<std::string>, "a std::string should have operator<");

namespace ASWUnitTests
{

namespace
{

// An enum with names, of which Unnamed has none
enum class ELevel : int
{
    Low = 0,
    High = 1,
    Unnamed = 2,
};

auto const& ASWIniEnumNames(ELevel)
{
    static constexpr ASWIni::TASWIniEnumName<ELevel> names[] = { { ELevel::Low, "Low" }, { ELevel::High, "High" } };

    return names;
}

// An application's type with a converter of its own (below) and no operator<
struct TTemperature
{
    explicit TTemperature(int degrees)
        : Degrees(degrees)
    {
    }

    int Degrees;
};

} // namespace

} // namespace ASWUnitTests

//---------------------------------------------------------------------------

namespace ASWIni
{

/////////////////////////////////////////////////////////////////////////////
// TASWIniConverter<TTemperature>
//
// The degrees as an int
/////////////////////////////////////////////////////////////////////////////
template <>
struct TASWIniConverter<ASWUnitTests::TTemperature>
{
    static TASWIniResult FromText(std::string_view text, ASWUnitTests::TTemperature& value,
        TASWIniValueOptions const& options)
    {
        return TASWIniConverter<int>::FromText(text, value.Degrees, options);
    }

    static TASWIniResult ToText(ASWUnitTests::TTemperature const& value, std::string& text,
        TASWIniValueOptions const& options)
    {
        return TASWIniConverter<int>::ToText(value.Degrees, text, options);
    }
};

} // namespace ASWIni

//---------------------------------------------------------------------------

static_assert(!ASWIni::Detail::IsLessComparable<ASWUnitTests::TTemperature>, "TTemperature should have no operator<");

namespace ASWUnitTests
{

using ASWIni::EBoolFormat;
using ASWIni::ECommentPolicy;
using ASWIni::EIniCode;
using ASWIni::ELineEnding;
using ASWIni::ESeverity;
using ASWIni::PathToUtf8;
using ASWIni::TASWBaseIni;
using ASWIni::TASWIniConverter;
using ASWIni::TASWIniDocument;
using ASWIni::TASWIniLogCallback;
using ASWIni::TASWIniMessage;
using ASWIni::TASWIniOptions;
using ASWIni::TASWIniResult;
using ASWIni::TASWIniSection;
using ASWIni::TASWIniSetting;
using ASWIni::TASWIniSettingOptions;
using ASWIni::TASWIniValueOptions;

namespace
{

/////////////////////////////////////////////////////////////////////////////
// TAppSettings
//
// The settings of an application, SweepThemMines, declared as its settings class declared them by hand on an earlier
// INI library, except that the days to keep the logs were called "LogDays" in an earlier version
/////////////////////////////////////////////////////////////////////////////
class TAppSettings : public TASWBaseIni
{
private:
    // The validator of a text that can't be empty
    static std::string NotEmpty(std::string const& value);

public:
    TASWIniSection General{ *this, "General" };
    TASWIniSetting<std::string> ImagesPath{ General, "ImagesPath", "", "Leave blank for default (exe directory)." };
    TASWIniSetting<bool> EnableCheats{ General, "EnableCheats", false, "",
        TASWIniSettingOptions<bool>().SetOptional() };
    TASWIniSetting<bool> UseQuestionMarksInit{ General, "UseQuestionMarksInit", true };
    TASWIniSetting<std::string> DirLogs{ General, "DirLogs", "%localAppData%\\ASWSoftware\\SweepThemMines\\Logs\\", "",
        TASWIniSettingOptions<std::string>().SetValidator(NotEmpty) };
    TASWIniSetting<std::string> LogPrefix{ General, "LogPrefix", "GeneralSTM_", "",
        TASWIniSettingOptions<std::string>().SetValidator(NotEmpty) };
    TASWIniSetting<unsigned> LogLevel{ General, "LogLevel", 0, "Valid range for LogLevel: 0-4. 0=System/forced logs "
        "only, 1=errors/warnings, 2=medium, 3=heavy, 4=debug/verbose",
        TASWIniSettingOptions<unsigned>().SetRange(0, 4) };
    TASWIniSetting<unsigned> NDaysRetainLogs{ General, "NDaysRetainLogs", 30,
        "Valid value for NDaysRetainLogs: 0 to N days. 0=retain forever.",
        TASWIniSettingOptions<unsigned>().AddOldKey("LogDays") };
};

//---------------------------------------------------------------------------

std::string BindingsProblem(TASWIniDocument const& document, std::size_t stayers, std::size_t binders);
TASWIniLogCallback Recorder(std::vector<TASWIniMessage>& messages);
void UseLf(TASWBaseIni& ini);
std::string WholeSettingsProblem(TASWIniDocument const& document, std::size_t writers, std::size_t others);

//---------------------------------------------------------------------------
/*
    BindingsProblem

    What is wrong with 'document', which a load in Bindings_ComeAndGoWhileLoading gave: "" when the section "Stays"
    has its comment and its keys Key0 to Key<stayers - 1>, each with its value (its number) and its comment, and when
    the section of each binder (Binder0 to Binder<binders - 1>) that is there has its comment and its key with its
    comment.
*/
std::string BindingsProblem(TASWIniDocument const& document, std::size_t stayers, std::size_t binders)
{
    if (document.GetSectionComment("Stays") != "Settings that stay")
        return "the section that stays should have its comment";

    for (std::size_t i = 0; i < stayers; ++i)
    {
        std::string const key = "Key" + std::to_string(i);
        ASWIni::TASWIniDocumentLine const* const line = document.FindKey("Stays", key);
        bool const whole = line != nullptr && line->GetValue() == std::to_string(i) &&
            document.GetKeyComment("Stays", key) == "A comment";
        if (!whole)
            return key + " should be there with its value and its comment";
    }

    for (std::size_t i = 0; i < binders; ++i)
    {
        std::string const section = "Binder" + std::to_string(i);
        if (document.FindSection(section) == nullptr)
            continue;

        bool const whole = document.GetSectionComment(section) == "A binder's section" &&
            document.FindKey(section, "Key") != nullptr && document.GetKeyComment(section, "Key") == "A binder's key";
        if (!whole)
            return "the section " + section + " should have its comment and its key with its comment";
    }

    return std::string();
}
//---------------------------------------------------------------------------
/*
    Recorder

    A log callback that adds each message to 'messages'.
*/
TASWIniLogCallback Recorder(std::vector<TASWIniMessage>& messages)
{
    auto const record = [&messages](TASWIniMessage const& message)
        {
            messages.push_back(message);
        };

    return record;
}
//---------------------------------------------------------------------------
/*
    UseLf

    Gives 'ini' options that write every line with LF, so its text is the same on every platform.
*/
void UseLf(TASWBaseIni& ini)
{
    TASWIniOptions options = ini.GetOptions();
    options.Writing.LineEnding = ELineEnding::LF;
    static_cast<void>(ini.SetOptions(options));
}
//---------------------------------------------------------------------------
/*
    WholeSettingsProblem

    What is wrong with 'document', a copy of the object of GetAndSet_SeeWholeSettingsWhileLoading: "" when the sections
    "Values" and "Others" have their comments, the keys WriterKey(0) to WriterKey(writers - 1) of "Values" and Key0 to
    Key<others - 1> of "Others" are there with their comments, and the key "Scratch", when it is there, has its comment.
*/
std::string WholeSettingsProblem(TASWIniDocument const& document, std::size_t writers, std::size_t others)
{
    bool const sectionComments = document.GetSectionComment("Values") == "The counts" &&
        document.GetSectionComment("Others") == "Other settings";
    if (!sectionComments)
        return "each section should have its comment";

    for (std::size_t i = 0; i < writers; ++i)
    {
        bool const whole = document.FindKey("Values", WriterKey(i)) != nullptr &&
            document.GetKeyComment("Values", WriterKey(i)) == "A count";
        if (!whole)
            return WriterKey(i) + " should be there with its comment";
    }

    for (std::size_t i = 0; i < others; ++i)
    {
        std::string const key = "Key" + std::to_string(i);
        bool const whole = document.FindKey("Others", key) != nullptr &&
            document.GetKeyComment("Others", key) == "A comment";
        if (!whole)
            return key + " should be there with its comment";
    }

    bool const scratchWhole = document.FindKey("Values", "Scratch") == nullptr ||
        document.GetKeyComment("Values", "Scratch") == "The scratch key";
    if (!scratchWhole)
        return "the scratch key should have its comment";

    return std::string();
}
//---------------------------------------------------------------------------


/////////////////////////////////////////////////////////////////////////////
// TAppSettings
/////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------
std::string TAppSettings::NotEmpty(std::string const& value)
{
    return value.empty() ? "it can't be empty" : "";
}
//---------------------------------------------------------------------------

} // namespace


/////////////////////////////////////////////////////////////////////////////
// TTest_ASWIni_SettingsAccess
/////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------
TTest_ASWIni_SettingsAccess::TTest_ASWIni_SettingsAccess()
    : inherited("ASWIni_SettingsAccess_Tests")
{
}
//---------------------------------------------------------------------------
TTest_ASWIni_SettingsAccess::~TTest_ASWIni_SettingsAccess()
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_SettingsAccess::SetUp_Group()
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_SettingsAccess::SetUp_Test(ITestCase& /*testCase*/)
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_SettingsAccess::TearDown_Group()
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_SettingsAccess::TearDown_Test(ITestCase& /*testCase*/)
{
}
//---------------------------------------------------------------------------

// /////// Begin tests after this line ///////////////////////

//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_SettingsAccess, Get_InvalidValueGivesTheDefault)
{
    // Arrange
    std::string const text = "[General]\nWidth = wide\nLevel = 9\n";
    TASWBaseIni ini;
    static_cast<void>(ini.LoadText(text));
    std::vector<TASWIniMessage> messages;
    ini.SetLogCallback(Recorder(messages));
    TASWIniSection const general(ini, "General");
    TASWIniSetting<int> const width(general, "Width", 640);
    TASWIniSetting<ELevel> const level(general, "Level", ELevel::Unnamed);
    int number = 0;
    std::string const widthProblem = TASWIniConverter<int>::FromText("wide", number, TASWIniValueOptions()).Message;
    ELevel read = ELevel::Low;
    std::string const levelProblem = TASWIniConverter<ELevel>::FromText("9", read, TASWIniValueOptions()).Message;

    // Act
    int const widthValue = width.Get();
    ELevel const levelValue = level.Get();

    // Assert
    CheckEquals(640, widthValue, __func__, __LINE__, "a value that doesn't convert should give the default");
    CheckTrue(levelValue == ELevel::Unnamed, __func__, __LINE__, "a name that isn't the enum's should give the default");
    AssertEquals(2, static_cast<int>(messages.size()), __func__, __LINE__, "each value should be reported once");
    CheckEquals(ESeverity::Warning, messages[0].Severity, __func__, __LINE__, "a value refused is a Warning");
    CheckEquals(EIniCode::InvalidValue, messages[0].Code, __func__, __LINE__, "the code should be the converter's");
    CheckEquals(widthProblem + " The default value \"640\" is used.", messages[0].Text, __func__, __LINE__,
        "the text should give the converter's problem and the default value");
    CheckEquals(std::string("General"), messages[0].Section, __func__, __LINE__, "the message should name the section");
    CheckEquals(std::string("Width"), messages[0].Key, __func__, __LINE__, "the message should name the key");
    CheckEquals(levelProblem + " The default value is used.", messages[1].Text, __func__, __LINE__,
        "a default value that can't be written should be left out of the text");
    CheckEquals(text, ini.ToText(), __func__, __LINE__, "Get() shouldn't change the document");
    CheckFalse(ini.IsModified(), __func__, __LINE__, "Get() shouldn't set the modified flag");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_SettingsAccess, Get_ReadsTheDocument)
{
    // Arrange
    TASWBaseIni ini;
    static_cast<void>(ini.LoadText("[values]\nnumber = 0x10\nEnabled = on\nName =  spaced  \nLevel = high\n"
        "Items = 1, 2, 3\n"));
    TASWIniSection const values(ini, "Values");
    TASWIniSetting<int> const number(values, "Number", 0);
    TASWIniSetting<bool> const enabled(values, "Enabled", false);
    TASWIniSetting<std::string> const name(values, "Name", "none");
    TASWIniSetting<ELevel> const level(values, "Level", ELevel::Low);
    TASWIniSetting<std::vector<int> > const items(values, "Items", std::vector<int>());

    // Act & Assert
    CheckEquals(16, number.Get(), __func__, __LINE__, "the value should be read, with the names' case ignored");
    CheckTrue(enabled.Get(), __func__, __LINE__, "a bool should be read in any of its forms");
    CheckEquals(std::string("spaced"), name.Get(), __func__, __LINE__, "a text should be the document's value");
    CheckTrue(level.Get() == ELevel::High, __func__, __LINE__, "an enum should be read by name");
    CheckTrue(items.Get() == std::vector<int>{ 1, 2, 3 }, __func__, __LINE__, "a list should be read");
    CheckTrue(number.IsPresent(), __func__, __LINE__, "a key in the document should be present");

    static_cast<void>(ini.SetText("Values", "Number", "5"));
    CheckEquals(5, number.Get(), __func__, __LINE__, "a raw edit of the key should be seen at once");

    CheckTrue(ini.RemoveKey("Values", "Number"), __func__, __LINE__, "the key should be removed");
    CheckEquals(0, number.Get(), __func__, __LINE__, "a removed key should give the default");
    CheckFalse(number.IsPresent(), __func__, __LINE__, "a removed key shouldn't be present");

    static_cast<void>(ini.LoadText(""));
    CheckFalse(enabled.Get(), __func__, __LINE__, "a document loaded again should be read");
    CheckEquals(std::string("none"), name.Get(), __func__, __LINE__, "an empty document should give the defaults");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_SettingsAccess, New_GivesTheDefaults)
{
    // Arrange
    TASWBaseIni ini;
    std::vector<TASWIniMessage> messages;
    ini.SetLogCallback(Recorder(messages));
    TASWIniSection const general(ini, "General", "The general settings");
    TASWIniSetting<int> const width(general, "Width", 640, "In pixels");
    TASWIniSetting<std::string> const name(general, "Name", "app");

    // Act
    int const widthValue = width.Get();
    std::string const nameValue = name.Get();

    // Assert
    CheckEquals(640, widthValue, __func__, __LINE__, "a missing key should give the default");
    CheckEquals(std::string("app"), nameValue, __func__, __LINE__, "a missing key should give the default");
    CheckFalse(width.IsPresent(), __func__, __LINE__, "a missing key shouldn't be present");
    CheckTrue(&general.GetOwner() == &ini, __func__, __LINE__, "the section should keep its owner");
    CheckEquals(std::string("General"), general.GetName(), __func__, __LINE__, "the section should keep its name");
    CheckEquals(std::string("The general settings"), general.GetComment(), __func__, __LINE__,
        "the section should keep its comment");
    CheckEquals(std::string("General"), width.GetSectionName(), __func__, __LINE__,
        "the setting should keep its section's name");
    CheckEquals(std::string("Width"), width.GetKey(), __func__, __LINE__, "the setting should keep its key");
    CheckEquals(std::string("In pixels"), width.GetComment(), __func__, __LINE__,
        "the setting should keep its comment");
    CheckEquals(std::string(), name.GetComment(), __func__, __LINE__, "a setting without a comment should have none");
    CheckEquals(640, width.GetDefault(), __func__, __LINE__, "the setting should keep its default");
    CheckTrue(width.GetOptions().GetMin() == nullptr && width.GetOptions().GetMax() == nullptr, __func__, __LINE__,
        "a setting should have no range by default");
    CheckFalse(static_cast<bool>(width.GetOptions().GetValidator()), __func__, __LINE__,
        "a setting should have no validator by default");
    CheckTrue(width.GetOptions().GetValues() == nullptr, __func__, __LINE__,
        "a setting should use its owner's value options by default");
    CheckEquals(std::string(), ini.ToText(), __func__, __LINE__, "declaring settings shouldn't change the document");
    CheckFalse(ini.IsModified(), __func__, __LINE__, "declaring settings shouldn't set the modified flag");
    CheckTrue(messages.empty(), __func__, __LINE__, "nothing should be reported");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_SettingsAccess, OutOfMemory_SetChangesNothing)
{
    // Arrange
    if (!CanFailAllocations())
        Skip(__func__, __LINE__, "allocations can't be made to fail in this build (see CanFailAllocations())");

    std::string const text = "[General]\nKept = 1\n";
    std::size_t failures = 0;

    // Act & Assert
    // Each allocation of a Set() that adds a key with its comment fails in turn, on a new object, until the Set()
    // makes no more. A failed allocation fails the Set(), or, when the standard library copes with it (bcc32c's has a
    // few such allocations), doesn't.
    for (std::size_t number = 1; number < 1000; ++number)
    {
        TASWBaseIni ini;
        UseLf(ini);
        static_cast<void>(ini.LoadText(text));
        TASWIniSection const general(ini, "General");
        TASWIniSetting<int> added(general, "Added", 0, "A comment");
        std::string const which = " (allocation " + std::to_string(number) + ")";
        TASWIniResult result;
        bool failed = false;
        {
            TFailingAlloc failingAlloc(number);
            result = added.Set(5);
            failed = failingAlloc.HasFailed();
        }

        if (result.IsOk())
        {
            CheckEquals(text + "; A comment\nAdded = 5\n", ini.ToText(), __func__, __LINE__,
                "a Set() that succeeded should be complete" + which);
        }
        else
        {
            ++failures;
            CheckTrue(failed, __func__, __LINE__, "Set() should only fail when an allocation fails" + which);
            CheckEquals(EIniCode::UnexpectedException, result.Code, __func__, __LINE__,
                "a failed allocation should be UnexpectedException" + which);
            CheckEquals(text, ini.ToText(), __func__, __LINE__, "a failed Set() should change nothing" + which);
            CheckFalse(ini.IsModified(), __func__, __LINE__,
                "a failed Set() shouldn't set the modified flag" + which);
        }

        if (!failed)
            break;
    }

    CheckGreaterThan(failures, std::size_t(0), __func__, __LINE__, "at least one allocation should have failed");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_SettingsAccess, Range_ChecksGetAndSet)
{
    // Arrange
    TASWBaseIni ini;
    UseLf(ini);
    static_cast<void>(ini.LoadText("[General]\nLevel = 7\n"));
    std::vector<TASWIniMessage> messages;
    ini.SetLogCallback(Recorder(messages));
    TASWIniSection const general(ini, "General");
    TASWIniSetting<int> level(general, "Level", 2, "", TASWIniSettingOptions<int>().SetRange(0, 4));
    TASWIniSetting<unsigned> lowest(general, "Lowest", 1, "", TASWIniSettingOptions<unsigned>().SetMin(1));
    TASWIniSetting<double> highest(general, "Highest", 0.5, "", TASWIniSettingOptions<double>().SetMax(1.0));

    // Act
    int const read = level.Get();
    TASWIniResult const belowRange = level.Set(-1);
    TASWIniResult const aboveRange = level.Set(5);
    TASWIniResult const top = level.Set(4);
    TASWIniResult const bottom = level.Set(0);
    TASWIniResult const belowMin = lowest.Set(0);
    TASWIniResult const aboveMax = highest.Set(1.5);

    // Assert
    AssertTrue(level.GetOptions().GetMin() != nullptr && level.GetOptions().GetMax() != nullptr, __func__, __LINE__,
        "the setting should keep its range");
    CheckEquals(0, *level.GetOptions().GetMin(), __func__, __LINE__, "the range should start at its Min");
    CheckEquals(4, *level.GetOptions().GetMax(), __func__, __LINE__, "the range should end at its Max");
    CheckTrue(lowest.GetOptions().GetMax() == nullptr, __func__, __LINE__, "a range with only a Min has no Max");
    CheckEquals(2, read, __func__, __LINE__, "a value out of the range should give the default");
    CheckEquals(EIniCode::ValueOutOfRange, belowRange.Code, __func__, __LINE__, "a value below should be refused");
    CheckEquals(EIniCode::ValueOutOfRange, aboveRange.Code, __func__, __LINE__, "a value above should be refused");
    CheckEquals(std::string("The key \"Level\" of the section \"General\" can't be set: The value \"5\" is out of the "
        "setting's range: it must be 0 to 4."), aboveRange.Message, __func__, __LINE__,
        "the message should name the key and give the range");
    CheckTrue(top.IsOk() && bottom.IsOk(), __func__, __LINE__, "the range's ends should be in it");
    CheckEquals(0, level.Get(), __func__, __LINE__, "the last value set should be read");
    CheckEquals(EIniCode::ValueOutOfRange, belowMin.Code, __func__, __LINE__, "a value below Min should be refused");
    CheckEndsWith(belowMin.Message, std::string("\"0\" is out of the setting's range: it must be at least 1."),
        __func__, __LINE__, "a range with only a Min should say so");
    CheckEquals(EIniCode::ValueOutOfRange, aboveMax.Code, __func__, __LINE__, "a value above Max should be refused");
    CheckEndsWith(aboveMax.Message, std::string("\"1.5\" is out of the setting's range: it must be at most 1."),
        __func__, __LINE__, "a range with only a Max should say so");
    CheckEquals(std::string("[General]\nLevel = 0\n"), ini.ToText(), __func__, __LINE__,
        "only the values in the range should be written");
    AssertEquals(5, static_cast<int>(messages.size()), __func__, __LINE__,
        "the Warning of Get() and each failed Set() should be reported");
    CheckEquals(ESeverity::Warning, messages[0].Severity, __func__, __LINE__, "Get() should report a Warning");
    CheckEquals(EIniCode::ValueOutOfRange, messages[0].Code, __func__, __LINE__, "Get() should report the code");
    CheckEquals(std::string("The value \"7\" is out of the setting's range: it must be 0 to 4. The default value \"2\" "
        "is used."), messages[0].Text, __func__, __LINE__, "Get()'s Warning should give the range and the default");
    CheckEquals(ESeverity::Error, messages[2].Severity, __func__, __LINE__, "a failed Set() should be an Error");
    CheckEquals(aboveRange.Message, messages[2].Text, __func__, __LINE__, "the Error should have the result's text");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_SettingsAccess, Reset_SetsTheDefault)
{
    // Arrange
    TASWBaseIni ini;
    UseLf(ini);
    static_cast<void>(ini.LoadText("[General]\nWidth = 800\n"));
    TASWIniSection const general(ini, "General");
    TASWIniSetting<int> width(general, "Width", 640);

    // Act
    TASWIniResult const result = width.Reset();

    // Assert
    CheckTrue(result.IsOk(), __func__, __LINE__, "Reset() should succeed");
    CheckEquals(640, width.Get(), __func__, __LINE__, "the value should be the default");
    CheckEquals(std::string("[General]\nWidth = 640\n"), ini.ToText(), __func__, __LINE__,
        "the default should be written to the document");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_SettingsAccess, Set_AddsTheKeyWithItsComment)
{
    // Arrange
    TASWBaseIni ini;
    UseLf(ini);
    static_cast<void>(ini.LoadText("[General]\n; The user's note\nKept = 1\nPlain = 2\n"));
    TASWIniSection const general(ini, "General");
    TASWIniSetting<int> kept(general, "Kept", 0, "The code's comment");
    TASWIniSetting<int> plain(general, "Plain", 0, "Plain's comment");
    TASWIniSetting<int> added(general, "Added", 0, "First line\nSecond line");
    TASWIniSetting<int> bare(general, "Bare", 0);
    TASWIniSection const other(ini, "Other");
    TASWIniSetting<int> elsewhere(other, "Key", 0, "In a new section");

    // Act
    TASWIniResult const keptResult = kept.Set(10);
    TASWIniResult const plainResult = plain.Set(20);
    TASWIniResult const addedResult = added.Set(30);
    TASWIniResult const bareResult = bare.Set(40);
    TASWIniResult const elsewhereResult = elsewhere.Set(50);

    // Assert
    CheckTrue(keptResult.IsOk() && plainResult.IsOk() && addedResult.IsOk() && bareResult.IsOk() &&
        elsewhereResult.IsOk(), __func__, __LINE__, "each Set() should succeed");
    CheckEquals(std::string("[General]\n; The user's note\nKept = 10\nPlain = 20\n; First line\n; Second line\n"
        "Added = 30\nBare = 40\n\n[Other]\n; In a new section\nKey = 50\n"), ini.ToText(), __func__, __LINE__,
        "a key Set() adds should get the setting's comment, and a key that exists should keep its own");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_SettingsAccess, Set_CommentThatCantBeWrittenChangesNothing)
{
    // Arrange
    // ";x" written after ";" is ";;x", which reads back as the comment "x" after the longer prefix ";;"
    std::string const text = "[General]\nExisting = 1\n";
    TASWBaseIni ini;
    TASWIniOptions options;
    options.Reading.CommentPrefixes = { ";", ";;" };
    options.Writing.CommentPrefix = ";";
    options.Writing.LineEnding = ELineEnding::LF;
    static_cast<void>(ini.SetOptions(options));
    static_cast<void>(ini.LoadText(text));
    std::vector<TASWIniMessage> messages;
    ini.SetLogCallback(Recorder(messages));
    TASWIniSection const general(ini, "General");
    TASWIniSetting<int> added(general, "Added", 0, ";x");
    TASWIniSetting<int> existing(general, "Existing", 0, ";x");

    // Act
    TASWIniResult const addedResult = added.Set(5);
    std::string const textAfterAdded = ini.ToText();
    bool const modifiedAfterAdded = ini.IsModified();
    TASWIniResult const existingResult = existing.Set(2);

    // Assert
    CheckEquals(EIniCode::InvalidArgument, addedResult.Code, __func__, __LINE__,
        "a new key whose comment can't be written should fail");
    CheckEquals(text, textAfterAdded, __func__, __LINE__, "the failed Set() shouldn't add the key without its comment");
    CheckFalse(modifiedAfterAdded, __func__, __LINE__, "the failed Set() shouldn't set the modified flag");
    AssertEquals(1, static_cast<int>(messages.size()), __func__, __LINE__, "the failure should be reported once");
    CheckEquals(ESeverity::Error, messages[0].Severity, __func__, __LINE__, "the failure should be an Error");
    CheckEquals(addedResult.Message, messages[0].Text, __func__, __LINE__, "the Error should have the result's text");
    CheckTrue(existingResult.IsOk(), __func__, __LINE__, "a key that exists doesn't get the comment, so it is set");
    CheckEquals(std::string("[General]\nExisting = 2\n"), ini.ToText(), __func__, __LINE__,
        "only the value of the key that exists should change");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_SettingsAccess, Set_FailuresAreReported)
{
    // Arrange
    TASWBaseIni ini;
    std::vector<TASWIniMessage> messages;
    ini.SetLogCallback(Recorder(messages));
    TASWIniSection const general(ini, "General");
    TASWIniSetting<ELevel> level(general, "Level", ELevel::Low);
    TASWIniSetting<int> badKey(general, "a=b", 0);
    auto const throwStd = [](int const& /*value*/) -> std::string
        {
            throw std::runtime_error("broken");
        };
    auto const throwInt = [](int const& /*value*/) -> std::string
        {
            throw 42;
        };
    TASWIniSetting<int> stdThrower(general, "StdThrower", 0, "", TASWIniSettingOptions<int>().SetValidator(throwStd));
    TASWIniSetting<int> intThrower(general, "IntThrower", 0, "", TASWIniSettingOptions<int>().SetValidator(throwInt));
    std::string unnamedText;
    std::string const unnamedProblem = TASWIniConverter<ELevel>::ToText(ELevel::Unnamed, unnamedText,
        TASWIniValueOptions()).Message;

    // Act
    TASWIniResult const unnamed = level.Set(ELevel::Unnamed);
    TASWIniResult const invalidKey = badKey.Set(1);
    TASWIniResult const stdException = stdThrower.Set(1);
    TASWIniResult const otherException = intThrower.Set(1);

    // Assert
    CheckEquals(EIniCode::InvalidValue, unnamed.Code, __func__, __LINE__, "a value that can't be written should fail");
    CheckEquals("The key \"Level\" of the section \"General\" can't be set: " + unnamedProblem, unnamed.Message,
        __func__, __LINE__, "the message should name the key and give the converter's problem");
    CheckEquals(EIniCode::InvalidArgument, invalidKey.Code, __func__, __LINE__,
        "a key that can't be written should fail as SetText() does");
    CheckEquals(EIniCode::UnexpectedException, stdException.Code, __func__, __LINE__,
        "a validator's exception should be UnexpectedException");
    CheckEquals(std::string("The key \"StdThrower\" of the section \"General\" can't be set: broken"),
        stdException.Message, __func__, __LINE__, "the message should name the key and give what() of the exception");
    CheckEquals(EIniCode::UnexpectedException, otherException.Code, __func__, __LINE__,
        "an exception that isn't a std::exception should be UnexpectedException");
    CheckContains(otherException.Message, std::string("isn't a std::exception"), __func__, __LINE__,
        "the message should say what was thrown");
    CheckEquals(std::string(), ini.ToText(), __func__, __LINE__, "the failures should change nothing");
    AssertEquals(4, static_cast<int>(messages.size()), __func__, __LINE__, "each failure should be reported once");
    CheckEquals(ESeverity::Error, messages[0].Severity, __func__, __LINE__, "a failure should be an Error");
    CheckEquals(unnamed.Message, messages[0].Text, __func__, __LINE__, "the Error should have the result's text");
    CheckEquals(invalidKey.Message, messages[1].Text, __func__, __LINE__, "SetText()'s failure should be reported");
    CheckEquals(stdException.Message, messages[2].Text, __func__, __LINE__, "the exception should be reported");
    CheckEquals(otherException.Message, messages[3].Text, __func__, __LINE__, "the exception should be reported");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_SettingsAccess, Set_WritesTheDocument)
{
    // Arrange
    TASWBaseIni ini;
    UseLf(ini);
    TASWIniSection const values(ini, "Values");
    TASWIniSetting<int> number(values, "Number", 0);
    TASWIniSetting<unsigned> count(values, "Count", 0);
    TASWIniSetting<bool> enabled(values, "Enabled", false);
    TASWIniSetting<double> ratio(values, "Ratio", 0.0);
    TASWIniSetting<std::string> name(values, "Name", "");
    TASWIniSetting<ELevel> level(values, "Level", ELevel::Low);
    TASWIniSetting<std::vector<int> > items(values, "Items", std::vector<int>());
    TASWIniSetting<std::filesystem::path> file(values, "File", std::filesystem::path());

    // Act
    bool const allOk = number.Set(-42).IsOk() && count.Set(7).IsOk() && enabled.Set(true).IsOk() &&
        ratio.Set(0.5).IsOk() && name.Set("some name").IsOk() && level.Set(ELevel::High).IsOk() &&
        items.Set({ 1, 2, 3 }).IsOk() && file.Set(std::filesystem::path("app.log")).IsOk();

    // Assert
    CheckTrue(allOk, __func__, __LINE__, "each Set() should succeed");
    CheckEquals(std::string("[Values]\nNumber = -42\nCount = 7\nEnabled = true\nRatio = 0.5\nName = some name\n"
        "Level = High\nItems = 1, 2, 3\nFile = app.log\n"), ini.ToText(), __func__, __LINE__,
        "each value should be written with its converter, in the order set");
    CheckTrue(ini.IsModified(), __func__, __LINE__, "the document should be modified");
    CheckEquals(-42, number.Get(), __func__, __LINE__, "the value set should be read back");
    CheckEquals(7u, count.Get(), __func__, __LINE__, "the value set should be read back");
    CheckTrue(enabled.Get(), __func__, __LINE__, "the value set should be read back");
    CheckEquals(std::string("some name"), name.Get(), __func__, __LINE__, "the value set should be read back");
    CheckTrue(level.Get() == ELevel::High, __func__, __LINE__, "the value set should be read back");
    CheckTrue(items.Get() == std::vector<int>{ 1, 2, 3 }, __func__, __LINE__, "the value set should be read back");
    CheckTrue(file.Get() == std::filesystem::path("app.log"), __func__, __LINE__, "the value set should be read back");
    CheckTrue(number.IsPresent(), __func__, __LINE__, "a key Set() added should be present");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_SettingsAccess, Types_WithoutOperatorLessWork)
{
    // Arrange
    TASWBaseIni ini;
    UseLf(ini);
    auto const physical = [](TTemperature const& value)
        {
            return value.Degrees < -273 ? "it is below absolute zero" : "";
        };
    TASWIniSection const room(ini, "Room");
    TASWIniSetting<TTemperature> temperature(room, "Temperature", TTemperature(20), "",
        TASWIniSettingOptions<TTemperature>().SetValidator(physical));

    // Act
    TASWIniResult const valid = temperature.Set(TTemperature(21));
    TASWIniResult const refused = temperature.Set(TTemperature(-300));

    // Assert
    CheckTrue(valid.IsOk(), __func__, __LINE__, "a value of an application's type should be set");
    CheckEquals(EIniCode::InvalidValue, refused.Code, __func__, __LINE__, "the validator should refuse a value");
    CheckEquals(std::string("[Room]\nTemperature = 21\n"), ini.ToText(), __func__, __LINE__,
        "the value should be written with the application's converter");
    CheckEquals(21, temperature.Get().Degrees, __func__, __LINE__, "the value should be read back");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_SettingsAccess, Validator_ChecksGetAndSet)
{
    // Arrange
    TASWBaseIni ini;
    UseLf(ini);
    static_cast<void>(ini.LoadText("[General]\nPrefix =\nEven = 3\n"));
    std::vector<TASWIniMessage> messages;
    ini.SetLogCallback(Recorder(messages));
    auto const notEmpty = [](std::string const& value)
        {
            return value.empty() ? "it can't be empty" : "";
        };
    std::vector<int> checked;
    auto const even = [&checked](int const& value)
        {
            checked.push_back(value);

            return value % 2 != 0 ? "it must be even." : "";
        };
    TASWIniSection const general(ini, "General");
    TASWIniSetting<std::string> prefix(general, "Prefix", "App_", "",
        TASWIniSettingOptions<std::string>().SetValidator(notEmpty));
    TASWIniSetting<int> evenNumber(general, "Even", 2, "", TASWIniSettingOptions<int>().SetValidator(even));

    // Act
    std::string const readPrefix = prefix.Get();
    TASWIniResult const emptyPrefix = prefix.Set("");
    TASWIniResult const validPrefix = prefix.Set("Log_");
    int const readEven = evenNumber.Get();
    TASWIniResult const odd = evenNumber.Set(5);
    TASWIniResult const validEven = evenNumber.Set(4);

    // Assert
    CheckEquals(std::string("App_"), readPrefix, __func__, __LINE__, "a value the validator refuses gives the default");
    AssertGreaterThan(messages.size(), std::size_t(0), __func__, __LINE__, "Get() should report the refused value");
    CheckEquals(ESeverity::Warning, messages[0].Severity, __func__, __LINE__, "Get() should report a Warning");
    CheckEquals(EIniCode::InvalidValue, messages[0].Code, __func__, __LINE__, "a refused value is InvalidValue");
    CheckEquals(std::string("The value \"\" isn't valid: it can't be empty. The default value \"App_\" is used."),
        messages[0].Text, __func__, __LINE__, "the Warning should give the validator's problem and the default");
    CheckEquals(EIniCode::InvalidValue, emptyPrefix.Code, __func__, __LINE__, "Set() should be refused");
    CheckEquals(std::string("The key \"Prefix\" of the section \"General\" can't be set: The value \"\" isn't valid: "
        "it can't be empty."), emptyPrefix.Message, __func__, __LINE__, "the message should give the problem");
    CheckTrue(validPrefix.IsOk(), __func__, __LINE__, "a value the validator accepts should be set");
    CheckEquals(2, readEven, __func__, __LINE__, "a value the validator refuses gives the default");
    CheckEndsWith(odd.Message, std::string("The value \"5\" isn't valid: it must be even."), __func__, __LINE__,
        "a problem that ends with a period shouldn't get another one");
    CheckTrue(validEven.IsOk(), __func__, __LINE__, "a value the validator accepts should be set");
    CheckTrue(checked == std::vector<int>{ 3, 5, 4 }, __func__, __LINE__,
        "the validator should be given each value read or set");
    CheckEquals(std::string("[General]\nPrefix = Log_\nEven = 4\n"), ini.ToText(), __func__, __LINE__,
        "only the values the validator accepts should be written");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_SettingsAccess, ValueOptions_AreTheSettingsOwnOrTheOwners)
{
    // Arrange
    TASWBaseIni ini;
    UseLf(ini);
    TASWIniValueOptions oneZero;
    oneZero.BoolFormat = EBoolFormat::OneZero;
    oneZero.ListDelimiter = ';';
    TASWIniSection const general(ini, "General");
    TASWIniSetting<bool> own(general, "Own", false, "", TASWIniSettingOptions<bool>().SetValues(oneZero));
    TASWIniSetting<bool> owners(general, "Owners", false);
    TASWIniSetting<std::vector<int> > list(general, "List", std::vector<int>(), "",
        TASWIniSettingOptions<std::vector<int> >().SetValues(oneZero));

    // Act
    bool const setTrue = own.Set(true).IsOk() && owners.Set(true).IsOk() && list.Set({ 1, 2 }).IsOk();
    std::string const textBefore = ini.ToText();
    TASWIniOptions yesNo = ini.GetOptions();
    yesNo.Values.BoolFormat = EBoolFormat::YesNo;
    static_cast<void>(ini.SetOptions(yesNo));
    bool const setFalse = own.Set(false).IsOk() && owners.Set(false).IsOk();

    // Assert
    CheckTrue(setTrue && setFalse, __func__, __LINE__, "each Set() should succeed");
    AssertTrue(own.GetOptions().GetValues() != nullptr, __func__, __LINE__, "the setting should keep its value options");
    CheckEquals(EBoolFormat::OneZero, own.GetOptions().GetValues()->BoolFormat, __func__, __LINE__,
        "the setting should keep its value options");
    CheckEquals(std::string("[General]\nOwn = 1\nOwners = true\nList = 1; 2\n"), textBefore, __func__, __LINE__,
        "a setting with value options of its own should be written with them, another with the owner's");
    CheckEquals(std::string("[General]\nOwn = 0\nOwners = no\nList = 1; 2\n"), ini.ToText(), __func__, __LINE__,
        "a setting without value options of its own should follow the owner's");
    CheckTrue(list.Get() == std::vector<int>{ 1, 2 }, __func__, __LINE__,
        "a setting should be read with its own value options");
}
//---------------------------------------------------------------------------


/////////////////////////////////////////////////////////////////////////////
// TTest_ASWIni_SettingsLoad
/////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------
TTest_ASWIni_SettingsLoad::TTest_ASWIni_SettingsLoad()
    : inherited("ASWIni_SettingsLoad_Tests")
{
}
//---------------------------------------------------------------------------
TTest_ASWIni_SettingsLoad::~TTest_ASWIni_SettingsLoad()
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_SettingsLoad::SetUp_Group()
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_SettingsLoad::SetUp_Test(ITestCase& /*testCase*/)
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_SettingsLoad::TearDown_Group()
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_SettingsLoad::TearDown_Test(ITestCase& /*testCase*/)
{
}
//---------------------------------------------------------------------------

// /////// Begin tests after this line ///////////////////////

//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_SettingsLoad, AppSettings_OldFileIsBroughtUpToDate)
{
    // Arrange
    // A file as the application's earlier settings class wrote it (CRLF, "key=value", ";comment"), which a user then
    // edited: an empty DirLogs, a LogLevel out of range, and the days to keep the logs still under their old name
    std::string const oldText = "[General]\r\n;Leave blank for default (exe directory).\r\nImagesPath=\r\nDirLogs=\r\n"
        "LogPrefix=GeneralSTM_\r\n;Valid range for LogLevel: 0-4. 0=System/forced logs only, 1=errors/warnings, "
        "2=medium, 3=heavy, 4=debug/verbose\r\nLogLevel=9\r\n;Days to keep the logs.\r\nLogDays=14\r\n";
    TTempDir const folder;
    AssertFalse(folder.Path().empty(), __func__, __LINE__, "the temporary folder should be created");
    std::filesystem::path const file = folder.Path() / "SweepThemMines.ini";
    AssertTrue(WriteTestFile(file, oldText), __func__, __LINE__, "the old file should be written");
    std::string const path = PathToUtf8(file);
    TAppSettings settings;
    std::vector<TASWIniMessage> messages;
    settings.SetLogCallback(Recorder(messages));

    // Act
    // As the application would: load, save when needed, read the settings; and then load the file saved
    TASWIniResult const loaded = settings.Load(path);
    bool const modified = settings.IsModified();
    TASWIniResult const saved = modified ? settings.Save() : TASWIniResult();
    std::string written;
    bool const read = ReadTestFile(file, written);
    TAppSettings again;
    std::vector<TASWIniMessage> againMessages;
    again.SetLogCallback(Recorder(againMessages));
    TASWIniResult const loadedAgain = again.Load(path);

    // Assert
    CheckTrue(loaded.IsOk(), __func__, __LINE__, "the old file should load");
    CheckTrue(modified, __func__, __LINE__, "the old file should need saving");
    CheckTrue(saved.IsOk() && read, __func__, __LINE__, "the file should be saved");
    CheckEquals(std::string("[General]\r\n;Leave blank for default (exe directory).\r\nImagesPath=\r\n"
        "DirLogs=%localAppData%\\ASWSoftware\\SweepThemMines\\Logs\\\r\nLogPrefix=GeneralSTM_\r\n;Valid range for "
        "LogLevel: 0-4. 0=System/forced logs only, 1=errors/warnings, 2=medium, 3=heavy, 4=debug/verbose\r\n"
        "LogLevel=0\r\n; Valid value for NDaysRetainLogs: 0 to N days. 0=retain forever.\r\nNDaysRetainLogs=14\r\n"
        "UseQuestionMarksInit = true\r\n"), written, __func__, __LINE__,
        "only the lines that needed it should change, and the missing key should be added in the file's line ending");
    AssertEquals(4, static_cast<int>(messages.size()), __func__, __LINE__, "each change should be reported");
    CheckEquals(EIniCode::KeyAdded, messages[0].Code, __func__, __LINE__, "the missing key should be added");
    CheckEquals(std::string("UseQuestionMarksInit"), messages[0].Key, __func__, __LINE__,
        "the changes should be reported in the order the settings are declared");
    CheckEquals(EIniCode::InvalidValue, messages[1].Code, __func__, __LINE__, "the empty DirLogs should be refused");
    CheckEquals(EIniCode::ValueOutOfRange, messages[2].Code, __func__, __LINE__, "LogLevel should be out of range");
    CheckEquals(EIniCode::KeyRenamed, messages[3].Code, __func__, __LINE__, "LogDays should be renamed");
    CheckEquals(std::string(), settings.ImagesPath.Get(), __func__, __LINE__, "ImagesPath should be the file's");
    CheckFalse(settings.EnableCheats.Get(), __func__, __LINE__, "the optional EnableCheats should be its default");
    CheckFalse(settings.EnableCheats.IsPresent(), __func__, __LINE__, "the optional EnableCheats shouldn't be added");
    CheckTrue(settings.UseQuestionMarksInit.Get(), __func__, __LINE__, "UseQuestionMarksInit should be its default");
    CheckEquals(settings.DirLogs.GetDefault(), settings.DirLogs.Get(), __func__, __LINE__,
        "DirLogs should be its default");
    CheckEquals(0u, settings.LogLevel.Get(), __func__, __LINE__, "LogLevel should be its default");
    CheckEquals(14u, settings.NDaysRetainLogs.Get(), __func__, __LINE__, "NDaysRetainLogs should keep the old value");
    CheckTrue(loadedAgain.IsOk(), __func__, __LINE__, "the saved file should load");
    CheckFalse(again.IsModified(), __func__, __LINE__, "the saved file shouldn't need saving");
    CheckTrue(againMessages.empty(), __func__, __LINE__, "loading the saved file should report nothing");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_SettingsLoad, Bindings_FollowTheirLifetimes)
{
    // Arrange
    TASWBaseIni ini;
    UseLf(ini);
    TASWIniSection const general(ini, "General");
    TASWIniSetting<int> const kept(general, "Kept", 1);
    {
        TASWIniSection const gone(ini, "General", "The comment of a section that is gone");
        TASWIniSetting<int> const goneSetting(general, "Gone", 2);
    }

    // Act
    static_cast<void>(ini.LoadText(""));
    std::string const textBefore = ini.ToText();
    TASWIniSetting<int> const later(general, "Later", 3);
    std::string const textDeclared = ini.ToText();
    static_cast<void>(ini.LoadText(""));

    // Assert
    CheckEquals(std::string("[General]\nKept = 1\n"), textBefore, __func__, __LINE__,
        "a setting or section that was destroyed should no longer be applied");
    CheckEquals(textBefore, textDeclared, __func__, __LINE__, "declaring a setting shouldn't change the document");
    CheckEquals(std::string("[General]\nKept = 1\nLater = 3\n"), ini.ToText(), __func__, __LINE__,
        "a setting declared later should be applied by the next load");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_SettingsLoad, Comments_CodeWinsReplacesTheFilesComments)
{
    // Arrange
    TASWBaseIni ini;
    UseLf(ini);
    std::vector<TASWIniMessage> messages;
    ini.SetLogCallback(Recorder(messages));
    TASWIniSection const file(ini, "", "The code's file comment");
    TASWIniSection const general(ini, "General", "The code's section comment");
    TASWIniSetting<int> const first(general, "A", 0, "The code's key comment\nin two lines");
    TASWIniSetting<int> const second(general, "B", 0);
    TASWIniSection const missing(ini, "Missing", "A section that isn't in the file");
    TASWIniSection const other(ini, "Other");

    // Act
    TASWIniResult const result = ini.LoadText("; The user's file comment\n\n; The user's section comment\n"
        "[General]\n; The user's key comment\nA = 1\n; The user's note, which the code has no comment for\nB = 2\n"
        "\n; The user's note on a section the code has no comment for\n[Other]\n");
    std::string const text = ini.ToText();
    bool const modified = ini.IsModified();
    static_cast<void>(ini.LoadText(text));

    // Assert
    CheckTrue(result.IsOk(), __func__, __LINE__, "the load should succeed");
    CheckEquals(std::string("; The code's file comment\n\n; The code's section comment\n[General]\n"
        "; The code's key comment\n; in two lines\nA = 1\n; The user's note, which the code has no comment for\n"
        "B = 2\n\n; The user's note on a section the code has no comment for\n[Other]\n"), text, __func__, __LINE__,
        "the code's comments should replace the file's, and a comment the code doesn't declare should stay");
    CheckTrue(modified, __func__, __LINE__, "the replaced comments should set the modified flag");
    CheckTrue(messages.empty(), __func__, __LINE__, "replacing a comment isn't reported");
    CheckEquals(text, ini.ToText(), __func__, __LINE__, "a document with the code's comments should stay as it is");
    CheckFalse(ini.IsModified(), __func__, __LINE__, "a document with the code's comments isn't modified");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_SettingsLoad, Comments_FileWinsKeepsTheFilesComments)
{
    // Arrange
    TASWBaseIni ini;
    TASWIniOptions options;
    options.Writing.LineEnding = ELineEnding::LF;
    options.Writing.CommentPolicy = ECommentPolicy::FileWins;
    static_cast<void>(ini.SetOptions(options));
    TASWIniSection const file(ini, "", "The code's file comment");
    TASWIniSection const general(ini, "General", "The code's section comment");
    TASWIniSetting<int> const first(general, "A", 0, "The code's key comment");
    TASWIniSetting<int> const third(general, "C", 0, "C's comment");
    TASWIniSetting<int> const fifth(general, "E", 5, "E's comment");
    TASWIniSection const bare(ini, "Bare", "Bare's comment");
    TASWIniSetting<int> const fourth(bare, "D", 0);

    // Act
    static_cast<void>(ini.LoadText("; The user's file comment\n\n; The user's section comment\n[General]\n"
        "; The user's key comment\nA = 1\nC = 3\n\n[Bare]\nD = 4\n"));
    std::string const withComments = ini.ToText();
    static_cast<void>(ini.LoadText("[General]\nA = 1\nC = 3\nE = 5\n"));

    // Assert
    CheckEquals(std::string("; The user's file comment\n\n; The user's section comment\n[General]\n"
        "; The user's key comment\nA = 1\n; C's comment\nC = 3\n; E's comment\nE = 5\n\n; Bare's comment\n[Bare]\n"
        "D = 4\n"), withComments, __func__, __LINE__,
        "the file's comments should stay, and the code's should be written only where there is none");
    CheckEquals(std::string("; The code's file comment\n\n; The code's section comment\n[General]\n"
        "; The code's key comment\nA = 1\n; C's comment\nC = 3\n; E's comment\nE = 5\n\n; Bare's comment\n[Bare]\n"
        "D = 0\n"), ini.ToText(), __func__, __LINE__, "a file without comments should get all the code's");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_SettingsLoad, Load_AddsMissingKeysInDeclarationOrder)
{
    // Arrange
    TASWBaseIni ini;
    UseLf(ini);
    std::vector<TASWIniMessage> messages;
    ini.SetLogCallback(Recorder(messages));
    TASWIniSection const general(ini, "General");
    TASWIniSetting<int> const first(general, "A", 1, "A's comment");
    TASWIniSetting<int> const second(general, "B", 0);
    TASWIniSetting<std::string> const third(general, "C", "three");
    TASWIniSection const window(ini, "Window");
    TASWIniSetting<int> const width(window, "Width", 640);

    // Act
    TASWIniResult const result = ini.LoadText("[General]\nB = 2\n");
    std::string const text = ini.ToText();
    bool const modified = ini.IsModified();
    std::size_t const reported = messages.size();
    static_cast<void>(ini.LoadText(text));

    // Assert
    CheckTrue(result.IsOk(), __func__, __LINE__, "the load should succeed");
    CheckEquals(std::string("[General]\nB = 2\n; A's comment\nA = 1\nC = three\n\n[Window]\nWidth = 640\n"), text,
        __func__, __LINE__, "the missing keys and sections should be added in the order they are declared");
    CheckTrue(modified, __func__, __LINE__, "adding keys should set the modified flag");
    AssertEquals(3, static_cast<int>(reported), __func__, __LINE__, "each added key should be reported");
    CheckEquals(ESeverity::Info, messages[0].Severity, __func__, __LINE__, "an added key is an Info message");
    CheckEquals(EIniCode::KeyAdded, messages[0].Code, __func__, __LINE__, "an added key is KeyAdded");
    CheckEquals(std::string("The key \"A\" of the section \"General\" is missing: it is added with its default value "
        "\"1\"."), messages[0].Text, __func__, __LINE__, "the message should name the key and the default value");
    CheckEquals(std::string("General"), messages[0].Section, __func__, __LINE__, "the message should name the section");
    CheckEquals(std::string("A"), messages[0].Key, __func__, __LINE__, "the message should name the key");
    CheckEquals(std::string("C"), messages[1].Key, __func__, __LINE__, "the keys should be reported in order");
    CheckEquals(std::string("Window"), messages[2].Section, __func__, __LINE__, "the keys should be reported in order");
    CheckEquals(text, ini.ToText(), __func__, __LINE__, "a document with every setting should stay as it is");
    CheckFalse(ini.IsModified(), __func__, __LINE__, "a document with every setting isn't modified");
    CheckEquals(3, static_cast<int>(messages.size()), __func__, __LINE__,
        "a document with every setting reports nothing");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_SettingsLoad, Load_FailureAppliesNothing)
{
    // Arrange
    TASWBaseIni ini;
    TASWIniOptions options;
    options.Reading.StrictParsing = true;
    options.Writing.LineEnding = ELineEnding::LF;
    static_cast<void>(ini.SetOptions(options));
    static_cast<void>(ini.LoadText("[General]\nA = 5\n"));
    std::vector<TASWIniMessage> messages;
    ini.SetLogCallback(Recorder(messages));
    TASWIniSection const general(ini, "General");
    TASWIniSetting<int> const first(general, "A", 1);
    TASWIniSetting<int> const second(general, "B", 2);

    // Act
    TASWIniResult const result = ini.LoadText("[General]\nthis line has no separator\n");

    // Assert
    CheckEquals(EIniCode::ParseFailed, result.Code, __func__, __LINE__, "a malformed line should fail the load");
    CheckEquals(std::string("[General]\nA = 5\n"), ini.ToText(), __func__, __LINE__,
        "a failed load shouldn't change the document, nor apply the settings to it");
    CheckFalse(ini.IsModified(), __func__, __LINE__, "a failed load shouldn't set the modified flag");
    AssertEquals(1, static_cast<int>(messages.size()), __func__, __LINE__, "only the failure should be reported");
    CheckEquals(EIniCode::ParseFailed, messages[0].Code, __func__, __LINE__, "the failure should be reported");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_SettingsLoad, Load_InvalidDefaultIsAnError)
{
    // Arrange
    TASWBaseIni ini;
    UseLf(ini);
    std::vector<TASWIniMessage> messages;
    ini.SetLogCallback(Recorder(messages));
    TASWIniSection const general(ini, "General");
    TASWIniSetting<int> const level(general, "Level", 9, "", TASWIniSettingOptions<int>().SetRange(0, 4));
    TASWIniSetting<ELevel> const unnamed(general, "Unnamed", ELevel::Unnamed);
    std::string unnamedText;
    std::string const unnamedProblem = TASWIniConverter<ELevel>::ToText(ELevel::Unnamed, unnamedText,
        TASWIniValueOptions()).Message;
    std::string const text = "[General]\nLevel = 7\n";

    // Act
    static_cast<void>(ini.LoadText(text));

    // Assert
    CheckEquals(text, ini.ToText(), __func__, __LINE__, "a setting whose default isn't valid should leave its key");
    CheckFalse(ini.IsModified(), __func__, __LINE__, "the document shouldn't be modified");
    AssertEquals(2, static_cast<int>(messages.size()), __func__, __LINE__, "each default should be reported");
    CheckEquals(ESeverity::Error, messages[0].Severity, __func__, __LINE__, "a default that isn't valid is an Error");
    CheckEquals(EIniCode::ValueOutOfRange, messages[0].Code, __func__, __LINE__, "the code should be the range's");
    CheckEquals(std::string("The key \"Level\" of the section \"General\" can't be given its default value: The value "
        "\"9\" is out of the setting's range: it must be 0 to 4."), messages[0].Text, __func__, __LINE__,
        "the message should name the key and give the problem");
    CheckEquals(std::string("Level"), messages[0].Key, __func__, __LINE__, "the message should name the key");
    CheckEquals(EIniCode::InvalidValue, messages[1].Code, __func__, __LINE__, "the code should be the converter's");
    CheckEndsWith(messages[1].Text, unnamedProblem, __func__, __LINE__, "the message should give the problem");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_SettingsLoad, Load_KeyWinsOverItsOldKeys)
{
    // Arrange
    TASWBaseIni ini;
    UseLf(ini);
    std::vector<TASWIniMessage> messages;
    ini.SetLogCallback(Recorder(messages));
    TASWIniSection const general(ini, "General");
    TASWIniSetting<int> const width(general, "Width", 640, "", TASWIniSettingOptions<int>().AddOldKey("W"));
    std::string const text = "[General]\nW = 800\nWidth = 900\n";

    // Act
    TASWIniResult const result = ini.LoadText(text);

    // Assert
    CheckTrue(result.IsOk(), __func__, __LINE__, "the load should succeed");
    CheckEquals(text, ini.ToText(), __func__, __LINE__, "the key and its old key should both stay as they are");
    CheckFalse(ini.IsModified(), __func__, __LINE__, "the document shouldn't be modified");
    CheckTrue(messages.empty(), __func__, __LINE__, "nothing should be reported");
    CheckEquals(900, width.Get(), __func__, __LINE__, "the setting should read its own key");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_SettingsLoad, Load_LeavesAValidDocumentAsItIs)
{
    // Arrange
    // Mixed line breaks, other spacing, a ':' separator and a hex number: valid values are never written again
    std::string const text = "; File comment\r\n\r\n; General's comment\r\n[General]\n; A's comment\nA = 0x10\n"
        "  B=yes\r\nC: some text\n";
    TASWBaseIni ini;
    std::vector<TASWIniMessage> messages;
    ini.SetLogCallback(Recorder(messages));
    TASWIniSection const file(ini, "", "File comment");
    TASWIniSection const general(ini, "General", "General's comment");
    TASWIniSetting<int> const first(general, "A", 0, "A's comment");
    TASWIniSetting<bool> const second(general, "B", false);
    TASWIniSetting<std::string> const third(general, "C", "");

    // Act
    TASWIniResult const result = ini.LoadText(text);

    // Assert
    CheckTrue(result.IsOk(), __func__, __LINE__, "the load should succeed");
    CheckEquals(text, ini.ToText(), __func__, __LINE__, "a document that holds every setting should stay byte for byte");
    CheckFalse(ini.IsModified(), __func__, __LINE__, "the document shouldn't be modified");
    CheckTrue(messages.empty(), __func__, __LINE__, "nothing should be reported");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_SettingsLoad, Load_MessagesNameTheFileLoaded)
{
    // Arrange
    // The callback saves the object to another file when the first message comes, which makes that the object's file
    // path before the messages of applying the settings are reported
    TTempDir const folder;
    AssertFalse(folder.Path().empty(), __func__, __LINE__, "the temporary folder should be created");
    std::string const path = PathToUtf8(folder.Path() / "app.ini");
    std::string const otherPath = PathToUtf8(folder.Path() / "other.ini");
    TASWBaseIni ini;
    UseLf(ini);
    TASWIniSection const general(ini, "General");
    TASWIniSetting<int> const first(general, "A", 1);
    TASWIniSetting<int> const second(general, "B", 2);
    std::vector<TASWIniMessage> messages;
    TASWIniResult savedAs;
    auto const callback = [&](TASWIniMessage const& message)
        {
            messages.push_back(message);
            if (messages.size() == 1)
                savedAs = ini.SaveAs(otherPath);
        };
    ini.SetLogCallback(callback);

    // Act
    TASWIniResult const loaded = ini.Load(path);

    // Assert
    CheckEquals(EIniCode::FileNotFound, loaded.Code, __func__, __LINE__, "a missing file is FileNotFound");
    CheckTrue(savedAs.IsOk(), __func__, __LINE__, "the callback should save the object to the other file");
    CheckEquals(otherPath, ini.GetFilePath(), __func__, __LINE__, "the other file should be the object's file path");
    AssertEquals(3, static_cast<int>(messages.size()), __func__, __LINE__,
        "the missing file and each key added should be reported");
    for (TASWIniMessage const& message : messages)
        CheckEquals(path, message.FilePath, __func__, __LINE__, "each message should name the file loaded");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_SettingsLoad, Load_MissingFileGetsEverySetting)
{
    // Arrange
    TTempDir const folder;
    AssertFalse(folder.Path().empty(), __func__, __LINE__, "the temporary folder should be created");
    std::string const path = PathToUtf8(folder.Path() / "app.ini");
    TASWBaseIni ini;
    UseLf(ini);
    std::vector<TASWIniMessage> messages;
    ini.SetLogCallback(Recorder(messages));
    TASWIniSection const file(ini, "", "My application's settings");
    TASWIniSection const general(ini, "General");
    TASWIniSetting<int> const first(general, "A", 1, "A's comment");
    TASWIniSetting<std::string> const second(general, "B", "x");

    // Act
    TASWIniResult const loaded = ini.Load(path);
    bool const modified = ini.IsModified();
    TASWIniResult const saved = ini.Save();
    std::string written;
    bool const read = ReadTestFile(folder.Path() / "app.ini", written);
    TASWBaseIni again;
    TASWIniSection const againFile(again, "", "My application's settings");
    TASWIniSection const againGeneral(again, "General");
    TASWIniSetting<int> const againFirst(againGeneral, "A", 1, "A's comment");
    TASWIniSetting<std::string> const againSecond(againGeneral, "B", "x");
    TASWIniResult const loadedAgain = again.Load(path);

    // Assert
    CheckEquals(EIniCode::FileNotFound, loaded.Code, __func__, __LINE__, "a missing file is FileNotFound");
    CheckTrue(modified, __func__, __LINE__, "the settings added should set the modified flag");
    CheckTrue(saved.IsOk() && read, __func__, __LINE__, "Save() should create the file");
    CheckEquals(std::string("; My application's settings\n\n[General]\n; A's comment\nA = 1\nB = x\n"), written,
        __func__, __LINE__, "the file should hold every setting with its comment");
    AssertEquals(3, static_cast<int>(messages.size()), __func__, __LINE__,
        "the missing file and each key added should be reported");
    CheckEquals(EIniCode::FileNotFound, messages[0].Code, __func__, __LINE__, "the missing file should come first");
    CheckEquals(EIniCode::KeyAdded, messages[1].Code, __func__, __LINE__, "then each key added");
    CheckEquals(path, messages[1].FilePath, __func__, __LINE__, "a key added should be reported with the file path");
    CheckTrue(loadedAgain.IsOk(), __func__, __LINE__, "the file saved should load");
    CheckFalse(again.IsModified(), __func__, __LINE__, "the file saved should hold every setting");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_SettingsLoad, Load_OldKeyThatCantBeRenamedIsAnError)
{
    // Arrange
    TASWBaseIni ini;
    UseLf(ini);
    std::vector<TASWIniMessage> messages;
    ini.SetLogCallback(Recorder(messages));
    TASWIniSection const general(ini, "General");
    TASWIniSetting<int> const badKey(general, "a=b", 1, "", TASWIniSettingOptions<int>().AddOldKey("Old"));
    TASWIniSetting<int> const good(general, "Good", 2, "", TASWIniSettingOptions<int>().AddOldKey("Older"));

    // Act
    TASWIniResult const result = ini.LoadText("[General]\nOld = 5\nOlder = 3\n");

    // Assert
    CheckTrue(result.IsOk(), __func__, __LINE__, "the load should succeed");
    CheckEquals(std::string("[General]\nOld = 5\nGood = 3\n"), ini.ToText(), __func__, __LINE__,
        "an old key that can't be renamed should stay as it is, and the next setting should still be applied");
    AssertEquals(2, static_cast<int>(messages.size()), __func__, __LINE__,
        "the failure should be reported once, then the next key renamed");
    CheckEquals(ESeverity::Error, messages[0].Severity, __func__, __LINE__, "a key that can't be renamed is an Error");
    CheckEquals(EIniCode::InvalidArgument, messages[0].Code, __func__, __LINE__, "the code should be RenameKey()'s");
    CheckEquals(std::string("General"), messages[0].Section, __func__, __LINE__, "the message should name the section");
    CheckEquals(std::string("a=b"), messages[0].Key, __func__, __LINE__, "the message should name the setting's key");
    CheckEquals(EIniCode::KeyRenamed, messages[1].Code, __func__, __LINE__, "the next key should still be renamed");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_SettingsLoad, Load_OptionalKeyIsNotAdded)
{
    // Arrange
    TASWBaseIni ini;
    UseLf(ini);
    std::vector<TASWIniMessage> messages;
    ini.SetLogCallback(Recorder(messages));
    TASWIniSection const general(ini, "General");
    TASWIniSetting<bool> cheats(general, "EnableCheats", false, "", TASWIniSettingOptions<bool>().SetOptional());

    // Act
    static_cast<void>(ini.LoadText("[General]\nOther = 1\n"));
    std::string const missingText = ini.ToText();
    bool const missingModified = ini.IsModified();
    bool const missingReported = !messages.empty();
    static_cast<void>(ini.LoadText("[General]\nEnableCheats = maybe\n"));
    std::string const invalidText = ini.ToText();
    TASWIniResult const set = cheats.Set(true);

    // Assert
    CheckEquals(std::string("[General]\nOther = 1\n"), missingText, __func__, __LINE__,
        "a missing optional key shouldn't be added");
    CheckFalse(missingModified, __func__, __LINE__, "the document shouldn't be modified");
    CheckFalse(missingReported, __func__, __LINE__, "a missing optional key shouldn't be reported");
    CheckEquals(std::string("[General]\nEnableCheats = false\n"), invalidText, __func__, __LINE__,
        "an optional key that is there should be applied as any other");
    AssertEquals(1, static_cast<int>(messages.size()), __func__, __LINE__, "the invalid value should be reported");
    CheckEquals(ESeverity::Warning, messages[0].Severity, __func__, __LINE__, "the invalid value is a Warning");
    CheckTrue(set.IsOk(), __func__, __LINE__, "Set() should write an optional setting");
    CheckEquals(std::string("[General]\nEnableCheats = true\n"), ini.ToText(), __func__, __LINE__,
        "Set() should write an optional setting");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_SettingsLoad, Load_RenamedKeyIsAppliedAsAnyOther)
{
    // Arrange
    TASWBaseIni ini;
    TASWIniOptions options;
    options.Writing.LineEnding = ELineEnding::LF;
    options.Writing.CommentPolicy = ECommentPolicy::FileWins;
    static_cast<void>(ini.SetOptions(options));
    std::vector<TASWIniMessage> messages;
    ini.SetLogCallback(Recorder(messages));
    TASWIniSection const general(ini, "General");
    TASWIniSetting<int> const level(general, "LogLevel", 2, "From 0 to 4",
        TASWIniSettingOptions<int>().SetRange(0, 4).AddOldKey("Level"));
    TASWIniSetting<bool> const cheats(general, "EnableCheats", false, "",
        TASWIniSettingOptions<bool>().SetOptional().AddOldKey("Cheats"));
    TASWIniSetting<int> const width(general, "Width", 640, "In pixels", TASWIniSettingOptions<int>().AddOldKey("W"));

    // Act
    TASWIniResult const result = ini.LoadText("[General]\nLevel = 9\nCheats = maybe\n; The user's note\nW = 800\n");

    // Assert
    CheckTrue(result.IsOk(), __func__, __LINE__, "the load should succeed");
    CheckEquals(std::string("[General]\n; From 0 to 4\nLogLevel = 2\nEnableCheats = false\n; The user's note\n"
        "Width = 800\n"), ini.ToText(), __func__, __LINE__,
        "a renamed key's value should be checked and its comment written as for any key, also for an optional one");
    AssertEquals(5, static_cast<int>(messages.size()), __func__, __LINE__,
        "each key renamed and each value replaced should be reported");
    CheckEquals(EIniCode::KeyRenamed, messages[0].Code, __func__, __LINE__, "the rename should come first");
    CheckEquals(EIniCode::ValueOutOfRange, messages[1].Code, __func__, __LINE__, "then the value replaced");
    CheckEquals(ESeverity::Warning, messages[1].Severity, __func__, __LINE__, "a value replaced is a Warning");
    CheckEquals(std::string("LogLevel"), messages[1].Key, __func__, __LINE__, "the warning should name the new key");
    CheckEquals(EIniCode::KeyRenamed, messages[2].Code, __func__, __LINE__, "an optional key should be renamed");
    CheckEquals(std::string("EnableCheats"), messages[2].Key, __func__, __LINE__, "the message should name the key");
    CheckEquals(EIniCode::InvalidValue, messages[3].Code, __func__, __LINE__,
        "an optional key's renamed value should be checked");
    CheckEquals(EIniCode::KeyRenamed, messages[4].Code, __func__, __LINE__, "the last key should be renamed");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_SettingsLoad, Load_RenamesAnOldKey)
{
    // Arrange
    TASWBaseIni ini;
    UseLf(ini);
    std::vector<TASWIniMessage> messages;
    ini.SetLogCallback(Recorder(messages));
    TASWIniSection const general(ini, "General");
    TASWIniSetting<int> const days(general, "NDaysRetainLogs", 7, "Days to keep the logs",
        TASWIniSettingOptions<int>().AddOldKey("LogDays").AddOldKey("DaysToKeepLogs"));
    TASWIniSetting<int> const width(general, "Width", 640, "",
        TASWIniSettingOptions<int>().AddOldKey("W").AddOldKey("SizeX"));
    TASWIniSetting<int> const height(general, "Height", 480, "", TASWIniSettingOptions<int>().AddOldKey("H"));

    // Act
    // An old key is looked for in the setting's own section only
    TASWIniResult const result = ini.LoadText("[General]\n; The user's note\n  logdays: 30\nSizeX = 800\nW = 900\n"
        "\n[Other]\nH = 1\n");
    std::string const text = ini.ToText();
    bool const modified = ini.IsModified();
    int const daysValue = days.Get();
    int const widthValue = width.Get();
    std::size_t const reported = messages.size();
    static_cast<void>(ini.LoadText(text));

    // Assert
    CheckTrue(result.IsOk(), __func__, __LINE__, "the load should succeed");
    CheckEquals(std::string("[General]\n  ; Days to keep the logs\n  NDaysRetainLogs: 30\nSizeX = 800\nWidth = 900\n"
        "Height = 480\n\n[Other]\nH = 1\n"), text, __func__, __LINE__,
        "the first old key in the setting's order should be renamed in place, keeping its spacing and value");
    CheckTrue(modified, __func__, __LINE__, "renaming keys should set the modified flag");
    CheckEquals(30, daysValue, __func__, __LINE__, "the setting should read the renamed key's value");
    CheckEquals(900, widthValue, __func__, __LINE__, "the setting should read the renamed key's value");
    AssertEquals(3, static_cast<int>(reported), __func__, __LINE__, "each key renamed or added should be reported");
    CheckEquals(ESeverity::Info, messages[0].Severity, __func__, __LINE__, "a renamed key is an Info message");
    CheckEquals(EIniCode::KeyRenamed, messages[0].Code, __func__, __LINE__, "a renamed key is KeyRenamed");
    CheckEquals(std::string("The key \"NDaysRetainLogs\" of the section \"General\" is missing: its earlier name "
        "\"LogDays\" is renamed to it."), messages[0].Text, __func__, __LINE__,
        "the message should name the key and its earlier name");
    CheckEquals(std::string("General"), messages[0].Section, __func__, __LINE__, "the message should name the section");
    CheckEquals(std::string("NDaysRetainLogs"), messages[0].Key, __func__, __LINE__,
        "the message should name the setting's key");
    CheckEquals(std::string("Width"), messages[1].Key, __func__, __LINE__, "the keys should be reported in order");
    CheckEquals(EIniCode::KeyAdded, messages[2].Code, __func__, __LINE__,
        "a key whose old key is in another section should be added");
    CheckEquals(text, ini.ToText(), __func__, __LINE__, "a document with the new keys should stay as it is");
    CheckFalse(ini.IsModified(), __func__, __LINE__, "a document with the new keys isn't modified");
    CheckEquals(3, static_cast<int>(messages.size()), __func__, __LINE__,
        "a document with the new keys reports nothing");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_SettingsLoad, Load_ReplacesAnInvalidValueWithTheDefault)
{
    // Arrange
    TASWBaseIni ini;
    UseLf(ini);
    std::vector<TASWIniMessage> messages;
    ini.SetLogCallback(Recorder(messages));
    auto const notEmpty = [](std::string const& value)
        {
            return value.empty() ? "it can't be empty" : "";
        };
    TASWIniSection const general(ini, "General");
    TASWIniSetting<int> const width(general, "Width", 640, "In pixels");
    TASWIniSetting<int> const level(general, "Level", 2, "", TASWIniSettingOptions<int>().SetRange(0, 4));
    TASWIniSetting<std::string> const prefix(general, "Prefix", "App_", "",
        TASWIniSettingOptions<std::string>().SetValidator(notEmpty));
    TASWIniValueOptions oneZero;
    oneZero.BoolFormat = EBoolFormat::OneZero;
    TASWIniSetting<bool> const flag(general, "Flag", true, "", TASWIniSettingOptions<bool>().SetValues(oneZero));
    int number = 0;
    std::string const widthProblem = TASWIniConverter<int>::FromText("wide", number, TASWIniValueOptions()).Message;

    // Act
    TASWIniResult const result = ini.LoadText("[General]\n; In pixels\nWidth = wide\nLevel = 9\nPrefix =\n"
        "Flag = maybe\n");

    // Assert
    CheckTrue(result.IsOk(), __func__, __LINE__, "the load should succeed");
    CheckEquals(std::string("[General]\n; In pixels\nWidth = 640\nLevel = 2\nPrefix = App_\nFlag = 1\n"),
        ini.ToText(), __func__, __LINE__,
        "each value that isn't valid should be replaced by the default, written with the setting's value options");
    CheckTrue(ini.IsModified(), __func__, __LINE__, "the values replaced should set the modified flag");
    AssertEquals(4, static_cast<int>(messages.size()), __func__, __LINE__, "each value should be reported");
    CheckEquals(ESeverity::Warning, messages[0].Severity, __func__, __LINE__, "a value replaced is a Warning");
    CheckEquals(EIniCode::InvalidValue, messages[0].Code, __func__, __LINE__, "the code should be the converter's");
    CheckEquals(widthProblem + " The default value \"640\" is used.", messages[0].Text, __func__, __LINE__,
        "the text should give the problem and the default");
    CheckEquals(std::string("Width"), messages[0].Key, __func__, __LINE__, "the message should name the key");
    CheckEquals(EIniCode::ValueOutOfRange, messages[1].Code, __func__, __LINE__, "the code should be the range's");
    CheckEquals(std::string("The value \"9\" is out of the setting's range: it must be 0 to 4. The default value "
        "\"2\" is used."), messages[1].Text, __func__, __LINE__, "the text should give the range and the default");
    CheckEquals(EIniCode::InvalidValue, messages[2].Code, __func__, __LINE__, "a refused value is InvalidValue");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_SettingsLoad, Load_WriteFailuresAreErrors)
{
    // Arrange
    // ";x" written after ";" is ";;x", which reads back as the comment "x" after the longer prefix ";;"
    TASWBaseIni ini;
    TASWIniOptions options;
    options.Reading.CommentPrefixes = { ";", ";;" };
    options.Writing.CommentPrefix = ";";
    options.Writing.LineEnding = ELineEnding::LF;
    static_cast<void>(ini.SetOptions(options));
    std::vector<TASWIniMessage> messages;
    ini.SetLogCallback(Recorder(messages));
    TASWIniSection const general(ini, "General");
    TASWIniSetting<int> const badKey(general, "a=b", 1);
    TASWIniSetting<int> const badComment(general, "Commented", 2, ";x");
    TASWIniSetting<int> const good(general, "Good", 3);

    // Act
    TASWIniResult const result = ini.LoadText("");

    // Assert
    CheckTrue(result.IsOk(), __func__, __LINE__, "the load should succeed");
    CheckEquals(std::string("[General]\nCommented = 2\nGood = 3\n"), ini.ToText(), __func__, __LINE__,
        "the keys that can be written should be added, the one whose comment can't be without it");
    AssertEquals(4, static_cast<int>(messages.size()), __func__, __LINE__, "each failure should be reported");
    CheckEquals(ESeverity::Error, messages[0].Severity, __func__, __LINE__, "a key that can't be written is an Error");
    CheckEquals(EIniCode::InvalidArgument, messages[0].Code, __func__, __LINE__, "the code should be SetValue()'s");
    CheckEquals(std::string("a=b"), messages[0].Key, __func__, __LINE__, "the message should name the key");
    CheckEquals(EIniCode::KeyAdded, messages[1].Code, __func__, __LINE__, "the key should be added");
    CheckEquals(ESeverity::Error, messages[2].Severity, __func__, __LINE__,
        "a comment that can't be written is an Error");
    CheckEquals(EIniCode::InvalidArgument, messages[2].Code, __func__, __LINE__, "the code should be SetKeyComment()'s");
    CheckEquals(std::string("Commented"), messages[2].Key, __func__, __LINE__, "the message should name the key");
    CheckEquals(EIniCode::KeyAdded, messages[3].Code, __func__, __LINE__, "the next key should still be added");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_SettingsLoad, OutOfMemory_LoadAppliesAllOrNothing)
{
    // Arrange
    if (!CanFailAllocations())
        Skip(__func__, __LINE__, "allocations can't be made to fail in this build (see CanFailAllocations())");

    // An old key renamed, a value replaced, a key added, and the comments of the keys and the section
    std::string const text = "[General]\nOld = 3\nB = bad\n";
    std::string const loadedText = "; General's comment\n[General]\n; A's comment\nA = 3\nB = 2\n; C's comment\n"
        "C = 5\n";
    std::size_t failures = 0;

    // Act & Assert
    // Each allocation of the load fails in turn, on a new object, until the load makes no more. A failed allocation
    // fails the load, which then changes nothing and reports only its failure (not what applying the settings did
    // before it failed), or, when the standard library copes with it, doesn't.
    for (std::size_t number = 1; number < 10000; ++number)
    {
        TASWBaseIni ini;
        UseLf(ini);
        static_cast<void>(ini.SetText("Before", "Key", "Value"));
        std::string const before = ini.ToText();
        TASWIniSection const general(ini, "General", "General's comment");
        TASWIniSetting<int> const first(general, "A", 1, "A's comment", TASWIniSettingOptions<int>().AddOldKey("Old"));
        TASWIniSetting<int> const second(general, "B", 2);
        TASWIniSetting<int> const third(general, "C", 5, "C's comment");
        // The callback keeps only the codes, in room made before, so it allocates nothing itself
        std::vector<EIniCode> codes;
        codes.reserve(10);
        auto const record = [&codes](TASWIniMessage const& message)
            {
                codes.push_back(message.Code);
            };
        ini.SetLogCallback(record);
        std::string const which = " (allocation " + std::to_string(number) + ")";
        TASWIniResult result;
        bool failed = false;
        {
            TFailingAlloc failingAlloc(number);
            result = ini.LoadText(text);
            failed = failingAlloc.HasFailed();
        }

        if (result.IsOk())
        {
            CheckEquals(loadedText, ini.ToText(), __func__, __LINE__,
                "a load that succeeded should have applied every setting" + which);
            CheckEquals(3, static_cast<int>(codes.size()), __func__, __LINE__,
                "a load that succeeded should report each change" + which);
        }
        else
        {
            ++failures;
            CheckTrue(failed, __func__, __LINE__, "the load should only fail when an allocation fails" + which);
            CheckEquals(EIniCode::UnexpectedException, result.Code, __func__, __LINE__,
                "a failed allocation should be UnexpectedException" + which);
            CheckEquals(before, ini.ToText(), __func__, __LINE__, "a failed load should change nothing" + which);
            CheckTrue(ini.IsModified(), __func__, __LINE__, "a failed load should keep the modified flag" + which);
            AssertEquals(1, static_cast<int>(codes.size()), __func__, __LINE__,
                "a failed load should report only its failure" + which);
            CheckEquals(EIniCode::UnexpectedException, codes[0], __func__, __LINE__,
                "the failure should be reported" + which);
        }

        if (!failed)
            break;
    }

    CheckGreaterThan(failures, std::size_t(0), __func__, __LINE__, "at least one allocation should have failed");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_SettingsLoad, SetOptions_DoesntApplyTheSettings)
{
    // Arrange
    TASWBaseIni ini;
    TASWIniSection const general(ini, "General", "A comment");
    TASWIniSetting<int> const width(general, "Width", 640, "A comment");
    TASWIniOptions options;
    options.Writing.LineEnding = ELineEnding::LF;

    // Act
    TASWIniResult const result = ini.SetOptions(options);

    // Assert
    CheckTrue(result.IsOk(), __func__, __LINE__, "SetOptions() should succeed");
    CheckEquals(std::string(), ini.ToText(), __func__, __LINE__, "SetOptions() shouldn't add the settings");
    CheckFalse(ini.IsModified(), __func__, __LINE__, "SetOptions() shouldn't set the modified flag");
}
//---------------------------------------------------------------------------


/////////////////////////////////////////////////////////////////////////////
// TTest_ASWIni_SettingsThreads
/////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------
TTest_ASWIni_SettingsThreads::TTest_ASWIni_SettingsThreads()
    : inherited("ASWIni_SettingsThreads_Tests")
{
}
//---------------------------------------------------------------------------
TTest_ASWIni_SettingsThreads::~TTest_ASWIni_SettingsThreads()
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_SettingsThreads::SetUp_Group()
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_SettingsThreads::SetUp_Test(ITestCase& /*testCase*/)
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_SettingsThreads::TearDown_Group()
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_SettingsThreads::TearDown_Test(ITestCase& /*testCase*/)
{
}
//---------------------------------------------------------------------------

// /////// Begin tests after this line ///////////////////////

//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_SettingsThreads, Bindings_ComeAndGoWhileLoading)
{
    // Arrange
    // Binder threads declare a section and a setting of their own and destroy them again, over and over, while a
    // loader loads a text that has none of the keys and checks each document it gives. Each load applies the settings
    // that stay, and a binder's section and setting whole or not at all: when its section is in the document, so is
    // its key, each with its comment. Many settings that stay make applying take long enough for bindings to come and
    // go while it runs. The binders go on until the loader is done (at least 'iterations' times, at most 'largest').
    std::size_t const binders = 2;
    std::size_t const stayers = 100;
    int const iterations = 300;
    int const largest = 1000000;
    int const loads = 200;
    std::atomic<bool> loading{ true };
    TASWBaseIni ini;
    UseLf(ini);
    TASWIniSection const stays(ini, "Stays", "Settings that stay");
    std::vector<std::unique_ptr<TASWIniSetting<int> > > settings;
    std::string expected = "; Settings that stay\n[Stays]\n";
    for (std::size_t i = 0; i < stayers; ++i)
    {
        std::string const key = "Key" + std::to_string(i);
        settings.push_back(std::make_unique<TASWIniSetting<int> >(stays, key, static_cast<int>(i), "A comment"));
        expected += "; A comment\n" + key + " = " + std::to_string(i) + "\n";
    }

    TThreadFailures failures;
    auto const binder = [&](std::size_t index)
        {
            std::mt19937 generator(ThreadsSeed + static_cast<std::uint32_t>(index));
            std::string const name = "Binder" + std::to_string(index);
            for (int i = 0; i < largest; ++i)
            {
                {
                    TASWIniSection const section(ini, name, "A binder's section");
                    TASWIniSetting<int> const setting(section, "Key", static_cast<int>(index), "A binder's key");
                    MaybeYield(generator);
                    if (setting.Get() != static_cast<int>(index))
                        failures.Add(name + "'s setting should read its default value");
                }

                if (i >= iterations && !loading)
                    break;

                MaybeYield(generator);
            }
        };
    auto const loader = [&](std::size_t index)
        {
            std::mt19937 generator(ThreadsSeed + static_cast<std::uint32_t>(index));
            for (int i = 0; i < loads; ++i)
            {
                TASWIniResult const result = ini.LoadText("");
                if (!result.IsOk())
                    failures.Add("LoadText() should succeed: " + result.Message);

                std::string const problem = BindingsProblem(ini.GetDocument(), stayers, binders);
                if (!problem.empty())
                    failures.Add("in a loaded document, " + problem);

                MaybeYield(generator);
            }

            loading = false;
        };
    std::vector<std::function<void()> > work;
    for (std::size_t i = 0; i < binders; ++i)
        work.push_back(std::bind(binder, i));

    work.push_back(std::bind(loader, binders));

    // Act
    // Then, with the binders' bindings gone, the object is loaded again
    RunTogether(work, failures);
    TASWIniResult const result = ini.LoadText("");

    // Assert
    CheckEquals(std::string(), failures.Summary(), __func__, __LINE__, "no thread should find a problem");
    CheckTrue(result.IsOk(), __func__, __LINE__, "the last load should succeed");
    CheckEquals(expected, ini.ToText(), __func__, __LINE__,
        "the last load should apply only the settings that stay");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_SettingsThreads, GetAndSet_SeeWholeSettingsWhileLoading)
{
    // Arrange
    // Writers count their own settings up with Set(), and a scratch thread removes the key of an optional setting and
    // sets it again, while a loader loads the saved file or a text whose values aren't valid and that lacks most keys,
    // a saver saves the file, and readers check what they see: each count's key is there and Get() gives a number in
    // range, and in a copy of the document each key that is there has its comment, since a load applies the settings
    // before anyone sees the document and Set() adds a key with its comment in one edit. Many other settings make
    // applying take long enough for a reader to see a document that isn't whole. The writers, the scratch thread and
    // the readers go on until the file threads are done (at least 'iterations' times, at most 'largest').
    std::size_t const writers = 3;
    std::size_t const readers = 2;
    std::size_t const others = 100;
    int const iterations = 300;
    int const largest = 1000000;
    int const fileOperations = 8;
    std::atomic<int> fileThreadsLeft{ 2 };
    TTempDir const folder;
    AssertFalse(folder.Path().empty(), __func__, __LINE__, "the temporary folder should be created");
    std::string const path = PathToUtf8(folder.Path() / "app.ini");
    TASWBaseIni ini;
    TASWIniSection const values(ini, "Values", "The counts");
    std::vector<std::unique_ptr<TASWIniSetting<int> > > counts;
    for (std::size_t i = 0; i < writers; ++i)
    {
        counts.push_back(std::make_unique<TASWIniSetting<int> >(values, WriterKey(i), 0, "A count",
            TASWIniSettingOptions<int>().SetRange(0, largest)));
    }

    TASWIniSetting<int> scratch(values, "Scratch", 0, "The scratch key",
        TASWIniSettingOptions<int>().SetOptional().SetRange(0, largest));
    TASWIniSection const other(ini, "Others", "Other settings");
    std::vector<std::unique_ptr<TASWIniSetting<int> > > fillers;
    for (std::size_t i = 0; i < others; ++i)
    {
        fillers.push_back(std::make_unique<TASWIniSetting<int> >(other, "Key" + std::to_string(i),
            static_cast<int>(i), "A comment"));
    }

    // Each load of 'invalid' reports a Warning for each count and the scratch key, and an Info for each other key
    std::string invalid = "[Values]\nScratch = -1\n";
    for (std::size_t i = 0; i < writers; ++i)
        invalid += WriterKey(i) + " = many\n";

    int const reportsPerInvalidLoad = static_cast<int>(writers + 1 + others);
    TASWIniResult const loaded = ini.LoadText("");
    TASWIniResult const saved = ini.SaveAs(path);
    AssertTrue(loaded.IsOk() && saved.IsOk(), __func__, __LINE__, "the settings should be saved");
    std::atomic<int> reports{ 0 };
    ini.SetLogCallback(CountingCallback(reports));
    TThreadFailures failures;
    auto const writer = [&](std::size_t index)
        {
            std::mt19937 generator(ThreadsSeed + static_cast<std::uint32_t>(index));
            for (int value = 1; value <= largest; ++value)
            {
                TASWIniResult const result = counts[index]->Set(value);
                if (!result.IsOk())
                    failures.Add("Set() should succeed: " + result.Message);

                if (value >= iterations && fileThreadsLeft == 0)
                    break;

                MaybeYield(generator);
            }
        };
    auto const scratcher = [&](std::size_t index)
        {
            std::mt19937 generator(ThreadsSeed + static_cast<std::uint32_t>(index));
            for (int i = 0; i < largest; ++i)
            {
                static_cast<void>(ini.RemoveKey("Values", "Scratch"));
                TASWIniResult const result = scratch.Set(i % 100);
                if (!result.IsOk())
                    failures.Add("Set() should succeed: " + result.Message);

                if (i >= iterations && fileThreadsLeft == 0)
                    break;

                MaybeYield(generator);
            }
        };
    auto const loader = [&](std::size_t index)
        {
            std::mt19937 generator(ThreadsSeed + static_cast<std::uint32_t>(index));
            for (int i = 0; i < fileOperations; ++i)
            {
                TASWIniResult const result = i % 2 == 0 ? ini.LoadText(invalid) : ini.Load(path);
                if (!result.IsOk())
                    failures.Add("a load should succeed: " + result.Message);

                MaybeYield(generator);
            }

            --fileThreadsLeft;
        };
    auto const saver = [&](std::size_t index)
        {
            std::mt19937 generator(ThreadsSeed + static_cast<std::uint32_t>(index));
            for (int i = 0; i < fileOperations; ++i)
            {
                TASWIniResult const result = ini.Save();
                if (!result.IsOk())
                    failures.Add("Save() should succeed: " + result.Message);

                MaybeYield(generator);
            }

            --fileThreadsLeft;
        };
    auto const reader = [&](std::size_t index)
        {
            std::mt19937 generator(ThreadsSeed + static_cast<std::uint32_t>(index));
            for (int i = 0; i < largest; ++i)
            {
                std::size_t const key = generator() % writers;
                if (!counts[key]->IsPresent())
                    failures.Add(WriterKey(key) + " should always be there");

                int const value = counts[key]->Get();
                if (value < 0 || value > largest)
                    failures.Add(WriterKey(key) + " should be a number in range: " + std::to_string(value));

                std::string const problem = WholeSettingsProblem(ini.GetDocument(), writers, others);
                if (!problem.empty())
                    failures.Add("in a copy of the document, " + problem);

                if (i >= iterations && fileThreadsLeft == 0)
                    break;

                MaybeYield(generator);
            }
        };
    std::vector<std::function<void()> > work;
    for (std::size_t i = 0; i < writers; ++i)
        work.push_back(std::bind(writer, i));

    work.push_back(std::bind(scratcher, writers));
    work.push_back(std::bind(loader, writers + 1));
    work.push_back(std::bind(saver, writers + 2));
    for (std::size_t i = 0; i < readers; ++i)
        work.push_back(std::bind(reader, writers + 3 + i));

    // Act
    // Then the object's text is loaded again, which needs nothing
    RunTogether(work, failures);
    TASWIniResult const reloaded = ini.LoadText(ini.ToText());

    // Assert
    CheckEquals(std::string(), failures.Summary(), __func__, __LINE__, "no thread should find a problem");
    CheckEquals(fileOperations / 2 * reportsPerInvalidLoad, reports.load(), __func__, __LINE__,
        "only the loads of the text that isn't valid should report, each what it changed");
    CheckTrue(reloaded.IsOk(), __func__, __LINE__, "the last load should succeed");
    CheckFalse(ini.IsModified(), __func__, __LINE__, "the object's text should hold every setting");
    CheckEquals(std::string(), WholeSettingsProblem(ini.GetDocument(), writers, others), __func__, __LINE__,
        "the document should hold every setting with its comment");
}
//---------------------------------------------------------------------------

} // namespace ASWUnitTests

//---------------------------------------------------------------------------
ASW_REGISTER_TEST_GROUP_IN_SUITE(ASWUnitTests::TTest_ASWIni_SettingsAccess, "Settings")
ASW_REGISTER_TEST_GROUP_IN_SUITE(ASWUnitTests::TTest_ASWIni_SettingsLoad, "Settings")
ASW_REGISTER_TEST_GROUP_IN_SUITE(ASWUnitTests::TTest_ASWIni_SettingsThreads, "Settings")
