/* **************************************************************************
main.cpp
Author: Anthony S. West - ASW Software

Example for ASWIni: an application's settings class, its first run, a file a
user edited, settings used from two threads, values the class doesn't
declare, and the document model on its own. It writes ASWIniExample.ini in
the temp folder's ASWIniExample folder, starting afresh on each run, and
returns 0 when every step did what it shows. Run with --pause to wait for
Enter before exiting.

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
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <vector>
//---------------------------------------------------------------------------
#include "ASWIni_Base.h"
#include "ASWIni_Converters.h"
#include "ASWIni_Document.h"
#include "ASWIni_File.h"
#include "ASWIni_Options.h"
#include "ASWIni_Settings.h"
#include "ASWIni_Types.h"
#include "ASWIni_Version.h"
//---------------------------------------------------------------------------

namespace
{

// The steps that went differently from what they show; main() returns 1 when there are any
unsigned UnexpectedCount = 0;

// The application's color theme, a setting written by name (see ASWIniEnumNames())
enum class ETheme : int
{
    Light = 0,
    Dark = 1,
    System = 2,
};

// The names ETheme's values are written and read by (TASWIniEnumName): the first name of a value is written, and the
// others are read too, so "Auto", an earlier name of System, still reads. Found by argument-dependent lookup, and
// defined (not only declared) before ETheme's setting, so the converter sees that ETheme has names.
auto const& ASWIniEnumNames(ETheme)
{
    static constexpr ASWIni::TASWIniEnumName<ETheme> names[] = {
        { ETheme::Light, "Light" }, { ETheme::Dark, "Dark" }, { ETheme::System, "System" },
        { ETheme::System, "Auto" },
    };

    return names;
}

/////////////////////////////////////////////////////////////////////////////
// TAppSettings
//
// The application's settings, declared once each: its section, key, type, default value, comment and options. Load()
// brings the file up to date (missing keys added, invalid values replaced by the defaults, old keys renamed), and each
// setting's Get() and Set() read and write the loaded document, from any thread.
/////////////////////////////////////////////////////////////////////////////
class TAppSettings : public ASWIni::TASWBaseIni
{
private:
    // The validator of a text that can't be empty: "" when it is valid, else what is wrong with it
    static std::string NotEmpty(std::string const& value);

    // Value options that write a bool as 1 or 0 (every form is read)
    static ASWIni::TASWIniValueOptions OneZero();

public:
    // The section "" holds the keys before the first header; its comment is the file comment
    ASWIni::TASWIniSection FileTop{ *this, "", "Settings of the ASWIni example." };

    ASWIni::TASWIniSection General{ *this, "General", "What the application looks like." };
    ASWIni::TASWIniSetting<std::string> UserName{ General, "UserName", "Guest", "The name in the title bar.",
        ASWIni::TASWIniSettingOptions<std::string>().SetValidator(NotEmpty) };
    ASWIni::TASWIniSetting<ETheme> Theme{ General, "Theme", ETheme::System, "Light, Dark or System." };
    ASWIni::TASWIniSetting<bool> ShowToolbar{ General, "ShowToolbar", true, "1 to show the toolbar, 0 to hide it.",
        ASWIni::TASWIniSettingOptions<bool>().SetValues(OneZero()) };
    // Not added to the file when it is missing: a hidden setting a user may add by hand
    ASWIni::TASWIniSetting<bool> EnableCheats{ General, "EnableCheats", false, "",
        ASWIni::TASWIniSettingOptions<bool>().SetOptional() };

    ASWIni::TASWIniSection Logging{ *this, "Logging" };
    ASWIni::TASWIniSetting<unsigned> LogLevel{ Logging, "LogLevel", 2, "From 0 (errors only) to 4 (everything).",
        ASWIni::TASWIniSettingOptions<unsigned>().SetRange(0, 4) };
    // Called "LogDays" in an earlier version of the application, so Load() renames that key
    ASWIni::TASWIniSetting<unsigned> NDaysRetainLogs{ Logging, "NDaysRetainLogs", 30,
        "How many days the logs are kept; 0 keeps them forever.",
        ASWIni::TASWIniSettingOptions<unsigned>().AddOldKey("LogDays") };
    ASWIni::TASWIniSetting<std::filesystem::path> LogFolder{ Logging, "LogFolder", std::filesystem::path("logs"),
        "Relative to the folder of this file." };

    ASWIni::TASWIniSection View{ *this, "View" };
    ASWIni::TASWIniSetting<double> Zoom{ View, "Zoom", 1.0, "From 0.25 to 4.",
        ASWIni::TASWIniSettingOptions<double>().SetRange(0.25, 4.0) };
    ASWIni::TASWIniSetting<std::vector<std::string> > Columns{ View, "Columns",
        std::vector<std::string>{ "Name", "Size", "Date" }, "The columns shown, in order." };

public:
    TAppSettings();
};

//---------------------------------------------------------------------------

// Counts 'what' as unexpected, saying so, when 'condition' is false
void Check(bool condition, char const* what);

// Prints the file at 'path', each line indented
void PrintFile(std::filesystem::path const& path);

// Prints a step's heading
void PrintHeading(char const* heading);

// The log callback: prints a message the INI object reports (a warning, an error, or a note about a change)
void PrintMessage(ASWIni::TASWIniMessage const& message);

// Sets LogLevel to 0, 1, 2, 3, 4, 0, ... 1000 times, as another thread of the application might
void SetLogLevels(TAppSettings& settings);

// The steps of the example, in the order main() runs them
void Step1_FirstRun(TAppSettings& settings, std::filesystem::path const& file);
void Step2_UseTheSettings(TAppSettings& settings, std::filesystem::path const& file);
void Step3_AUserEditedTheFile(TAppSettings& settings, std::filesystem::path const& file);
void Step4_Threads(TAppSettings& settings);
void Step5_ValuesTheClassDoesntDeclare(TAppSettings& settings, std::filesystem::path const& file);
void Step6_TheDocument(TAppSettings const& settings);

// 'value' as the text of an INI value, as TASWIniConverter<T> writes it with the default value options
template <typename T>
std::string Text(T const& value);

// Writes 'text' to the file at 'path' as it is (binary mode, so "\n" stays LF); false when it can't
bool WriteFileText(std::filesystem::path const& path, std::string const& text);

//---------------------------------------------------------------------------
/*
    Check

    Counts 'what' as unexpected, saying so, when 'condition' is false.
*/
void Check(bool condition, char const* what)
{
    if (condition)
        return;

    ++UnexpectedCount;
    std::cout << "  UNEXPECTED: " << what << "\n";
}
//---------------------------------------------------------------------------
/*
    PrintFile

    Prints the file at 'path', each line indented, without its line break.
*/
void PrintFile(std::filesystem::path const& path)
{
    std::cout << "  " << ASWIni::PathToUtf8(path) << ":\n";
    std::ifstream file(path, std::ios::binary);
    std::string line;
    while (std::getline(file, line))
    {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        std::cout << "    | " << line << "\n";
    }
}
//---------------------------------------------------------------------------
/*
    PrintHeading

    Prints a step's heading, after a blank line.
*/
void PrintHeading(char const* heading)
{
    std::cout << "\n" << heading << "\n";
}
//---------------------------------------------------------------------------
/*
    PrintMessage

    The log callback. The INI object calls it on the thread of the operation, after the operation, with none of its
    locks held, so it may call the object, even Load() or Save(). An application would write the message to its log.
*/
void PrintMessage(ASWIni::TASWIniMessage const& message)
{
    std::cout << "  [log] " << ASWIni::ToString(message) << "\n";
}
//---------------------------------------------------------------------------
/*
    SetLogLevels

    Sets LogLevel to 0, 1, 2, 3, 4, 0, ... 1000 times, as another thread of the application might.
*/
void SetLogLevels(TAppSettings& settings)
{
    for (unsigned i = 0; i < 1000; ++i)
        static_cast<void>(settings.LogLevel.Set(i % 5));
}
//---------------------------------------------------------------------------
/*
    Step1_FirstRun

    Loads the file, which is missing on an application's first run: Load() fails with FileNotFound, reported as a
    warning, and the document is given every setting with its default value and comment, each reported as a note
    (KeyAdded). IsModified() then says the file needs saving.
*/
void Step1_FirstRun(TAppSettings& settings, std::filesystem::path const& file)
{
    PrintHeading("1. First run: the file is missing");

    ASWIni::TASWIniResult const loaded = settings.Load(ASWIni::PathToUtf8(file));
    Check(loaded.Code == ASWIni::EIniCode::FileNotFound, "the file should be missing");

    std::cout << "  IsModified(): " << Text(settings.IsModified()) << "\n";
    if (settings.IsModified())
    {
        ASWIni::TASWIniResult const saved = settings.Save();
        Check(saved.IsOk(), "the file should be saved");
    }

    PrintFile(file);
}
//---------------------------------------------------------------------------
/*
    Step2_UseTheSettings

    Reads and sets the settings. A value outside a setting's range (or one its validator refuses) isn't set: Set()
    returns why, and reports it as an error.
*/
void Step2_UseTheSettings(TAppSettings& settings, std::filesystem::path const& file)
{
    PrintHeading("2. Reading and setting values");

    std::cout << "  UserName: " << settings.UserName.Get() << "\n";
    std::cout << "  Theme: " << Text(settings.Theme.Get()) << "\n";
    std::cout << "  Zoom: " << Text(settings.Zoom.Get()) << "\n";
    std::cout << "  Columns: " << Text(settings.Columns.Get()) << "\n";

    Check(settings.Theme.Set(ETheme::Dark).IsOk(), "Theme should be set");
    Check(settings.Zoom.Set(1.5).IsOk(), "Zoom should be set");
    std::vector<std::string> columns = settings.Columns.Get();
    columns.push_back("Type, if known");
    Check(settings.Columns.Set(columns).IsOk(), "Columns should be set");

    ASWIni::TASWIniResult const refused = settings.LogLevel.Set(7);
    Check(refused.Code == ASWIni::EIniCode::ValueOutOfRange, "LogLevel 7 should be out of range");
    std::cout << "  LogLevel.Set(7): " << ASWIni::ToString(refused.Code) << "\n";
    std::cout << "  LogLevel: " << settings.LogLevel.Get() << "\n";

    Check(settings.Save().IsOk(), "the file should be saved");
    PrintFile(file);
}
//---------------------------------------------------------------------------
/*
    Step3_AUserEditedTheFile

    Loads a file that a user edited by hand, written by an earlier version of the application: an empty UserName, a
    theme by its earlier name, a LogLevel out of range, the earlier key LogDays, a comment changed by hand, the hidden
    EnableCheats, and a section of the user's own. Load() repairs what isn't valid, renames LogDays, adds the missing
    keys, writes the code's comments back (TASWIniWritingOptions::CommentPolicy), and keeps everything else as it was.
*/
void Step3_AUserEditedTheFile(TAppSettings& settings, std::filesystem::path const& file)
{
    PrintHeading("3. A user edited the file");

    std::string const edited =
        "[General]\n"
        "UserName =\n"
        "Theme = Auto\n"
        "ShowToolbar = no\n"
        "EnableCheats = yes\n"
        "\n"
        "[Logging]\n"
        "; Turned up to find a bug\n"
        "LogLevel = 9\n"
        "LogDays = 7\n"
        "\n"
        "[Plugins]\n"
        "; Added by the spell checker\n"
        "Spell = on\n";
    Check(WriteFileText(file, edited), "the edited file should be written");
    PrintFile(file);

    ASWIni::TASWIniResult const loaded = settings.Load(ASWIni::PathToUtf8(file));
    Check(loaded.IsOk(), "the edited file should load");

    std::cout << "  UserName: " << settings.UserName.Get() << "\n";
    std::cout << "  Theme: " << Text(settings.Theme.Get()) << "\n";
    std::cout << "  ShowToolbar: " << Text(settings.ShowToolbar.Get()) << "\n";
    std::cout << "  EnableCheats: " << Text(settings.EnableCheats.Get()) << "\n";
    std::cout << "  LogLevel: " << settings.LogLevel.Get() << "\n";
    std::cout << "  NDaysRetainLogs: " << settings.NDaysRetainLogs.Get() << "\n";
    Check(settings.EnableCheats.IsPresent() && settings.EnableCheats.Get(), "EnableCheats should be on");
    Check(settings.NDaysRetainLogs.Get() == 7, "LogDays should be renamed to NDaysRetainLogs");

    std::cout << "  IsModified(): " << Text(settings.IsModified()) << "\n";
    if (settings.IsModified())
        Check(settings.Save().IsOk(), "the file should be saved");

    PrintFile(file);
}
//---------------------------------------------------------------------------
/*
    Step4_Threads

    Reads LogLevel on this thread while another thread sets it. Each Get() sees the document before or after a
    Set(), never during one, so every value read is in the range.
*/
void Step4_Threads(TAppSettings& settings)
{
    PrintHeading("4. Two threads");

    std::thread setter(SetLogLevels, std::ref(settings));
    unsigned outOfRange = 0;
    for (unsigned i = 0; i < 1000; ++i)
    {
        if (settings.LogLevel.Get() > 4)
            ++outOfRange;
    }

    setter.join();

    Check(outOfRange == 0, "every LogLevel read should be in the range");
    std::cout << "  1000 reads while another thread set LogLevel 1000 times: " << outOfRange << " out of range\n";

    Check(settings.LogLevel.Reset().IsOk(), "LogLevel should be reset");
    std::cout << "  LogLevel.Reset(): LogLevel is " << settings.LogLevel.Get() << " again\n";
}
//---------------------------------------------------------------------------
/*
    Step5_ValuesTheClassDoesntDeclare

    Reads and writes values of sections the settings class doesn't declare, by section and key: typed values with
    GetValue(), TryGetValue() and SetValue(), names, comments and removal. GetValue() gives the default for a missing
    key, and for a value that doesn't convert (reported as a warning); TryGetValue() says why and reports nothing.
*/
void Step5_ValuesTheClassDoesntDeclare(TAppSettings& settings, std::filesystem::path const& file)
{
    PrintHeading("5. Values the settings class doesn't declare");

    Check(settings.SetValue("Window.Main", "Width", 1024).IsOk(), "Window.Main's Width should be set");
    Check(settings.SetValue("Window.Main", "Height", 768).IsOk(), "Window.Main's Height should be set");
    Check(settings.SetKeyComment("Window.Main", "Width", "In pixels").IsOk(), "the comment should be set");
    Check(settings.SetValue("Window.Find", "Width", 400).IsOk(), "Window.Find's Width should be set");
    for (std::string const& section : settings.GetSectionNames("Window."))
        std::cout << "  " << section << ": Width " << settings.GetValue(section, "Width", 0) << "\n";

    int height = 0;
    ASWIni::TASWIniResult const missing = settings.TryGetValue("Window.Find", "Height", height);
    std::cout << "  TryGetValue(\"Window.Find\", \"Height\"): " << missing.Message << "\n";

    std::vector<std::string> const recent{ "report.txt", "notes, draft.txt" };
    Check(settings.SetValue("RecentFiles", "Files", recent).IsOk(), "the recent files should be set");

    int const spellNumber = settings.GetValue("Plugins", "Spell", 0);
    bool const spell = settings.GetValue("Plugins", "Spell", false);
    std::cout << "  GetValue(\"Plugins\", \"Spell\", 0): " << spellNumber << "\n";
    std::cout << "  GetValue(\"Plugins\", \"Spell\", false): " << Text(spell) << "\n";

    Check(settings.RemoveSection("Window.Find"), "Window.Find should be removed");

    Check(settings.Save().IsOk(), "the file should be saved");
    PrintFile(file);
}
//---------------------------------------------------------------------------
/*
    Step6_TheDocument

    Uses a TASWIniDocument on its own, with no file and no lock: parses a text, sets a value (only that line changes)
    and goes through its lines. A malformed line is kept as it is, and reported. GetDocument() gives a copy of an INI
    object's document.
*/
void Step6_TheDocument(TAppSettings const& settings)
{
    PrintHeading("6. The document");

    ASWIni::TASWIniDocument document;
    std::vector<ASWIni::TASWIniMessage> messages;
    ASWIni::TASWIniResult const parsed = document.Parse(
        "; Colors of the editor\n"
        "\n"
        "[Colors]\n"
        "Back = white\n"
        "Fore:black\n"
        "just some words\n", ASWIni::TASWIniOptions(), messages);
    Check(parsed.IsOk(), "the text should be parsed");
    for (ASWIni::TASWIniMessage const& message : messages)
        PrintMessage(message);

    Check(document.SetValue("Colors", "Back", "navy").IsOk(), "Back should be set");
    for (ASWIni::TASWIniDocumentSection const& section : document.GetSections())
    {
        for (ASWIni::TASWIniDocumentLine const& line : section.GetLines())
            std::cout << "  " << ASWIni::ToString(line.GetKind()) << ": " << line.GetText() << "\n";
    }

    std::cout << "  The settings' document has " << settings.GetDocument().GetLineCount() << " lines\n";
}
//---------------------------------------------------------------------------
/*
    Text

    'value' as the text of an INI value, as TASWIniConverter<T> writes it with the default value options.
*/
template <typename T>
std::string Text(T const& value)
{
    std::string text;
    static_cast<void>(ASWIni::TASWIniConverter<T>::ToText(value, text, ASWIni::TASWIniValueOptions()));

    return text;
}
//---------------------------------------------------------------------------
/*
    WriteFileText

    Writes 'text' to the file at 'path' as it is (binary mode, so "\n" stays LF); false when it can't.
*/
bool WriteFileText(std::filesystem::path const& path, std::string const& text)
{
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file << text;
    file.close();

    return !file.fail();
}
//---------------------------------------------------------------------------

/////////////////////////////////////////////////////////////////////////////
// TAppSettings
/////////////////////////////////////////////////////////////////////////////
TAppSettings::TAppSettings()
{
    // The options are set before the first Load(), which reads the file with them
    ASWIni::TASWIniOptions options;
    options.Saving.Backup = true; // Each save first copies the file to ASWIniExample.ini.bak
    static_cast<void>(SetOptions(options));
}
//---------------------------------------------------------------------------
std::string TAppSettings::NotEmpty(std::string const& value)
{
    return value.empty() ? "it can't be empty" : "";
}
//---------------------------------------------------------------------------
ASWIni::TASWIniValueOptions TAppSettings::OneZero()
{
    ASWIni::TASWIniValueOptions values;
    values.BoolFormat = ASWIni::EBoolFormat::OneZero;

    return values;
}
//---------------------------------------------------------------------------

} // namespace

//---------------------------------------------------------------------------

int main(int argc, char* argv[])
{
    bool pause = false;
    for (int i = 1; i < argc; ++i)
    {
        if (std::string_view(argv[i]) == "--pause")
            pause = true;
    }

    std::cout << "ASWIni " << ASWIni::Version << " example\n";

    // The example's own folder in the temp folder, which the first save creates (TASWIniSavingOptions::CreateFolders).
    // The file of an earlier run is deleted, so each run starts with the first run.
    std::error_code error;
    std::filesystem::path const folder = std::filesystem::temp_directory_path(error) / "ASWIniExample";
    std::filesystem::path const file = folder / "ASWIniExample.ini";
    std::filesystem::remove(file, error);
    std::filesystem::remove(folder / "ASWIniExample.ini.bak", error);

    TAppSettings settings;
    settings.SetLogCallback(PrintMessage);

    Step1_FirstRun(settings, file);
    Step2_UseTheSettings(settings, file);
    Step3_AUserEditedTheFile(settings, file);
    Step4_Threads(settings);
    Step5_ValuesTheClassDoesntDeclare(settings, file);
    Step6_TheDocument(settings);

    std::cout << "\nThe file is " << ASWIni::PathToUtf8(file) << "\n";
    if (UnexpectedCount != 0)
        std::cout << UnexpectedCount << " steps went differently from what they show\n";

    if (pause)
    {
        std::cout << "Press Enter to exit...\n";
        std::cin.get();
    }

    return UnexpectedCount == 0 ? 0 : 1;
}
