/* **************************************************************************
Check_Usage.cpp
Author: Anthony S. West - ASW Software

Check of the installed CMake package: tests/package/CMakeLists.txt builds this file against an ASWIni installed with
`cmake --install` and found with find_package(), and running it returns 0 when the installed library works: every
installed header compiles, and a settings class reads and writes a text and a file (so the library, its file layer
and the threading library it links to are all there).

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

#include <filesystem>
#include <iostream>
#include <string>
#include <system_error>
#include <vector>
//---------------------------------------------------------------------------
#include "ASWIni_Base.h"
#include "ASWIni_Converters.h"
#include "ASWIni_Document.h"
#include "ASWIni_File.h"
#include "ASWIni_Interface.h"
#include "ASWIni_Options.h"
#include "ASWIni_Platform.h"
#include "ASWIni_Settings.h"
#include "ASWIni_Types.h"
#include "ASWIni_Utils.h"
#include "ASWIni_Version.h"
//---------------------------------------------------------------------------

namespace
{

// The problems found; main() returns 1 when there are any
unsigned ProblemCount = 0;

/////////////////////////////////////////////////////////////////////////////
// TCheckSettings
//
// A settings class with one section and one setting
/////////////////////////////////////////////////////////////////////////////
class TCheckSettings : public ASWIni::TASWBaseIni
{
public:
    ASWIni::TASWIniSection General{ *this, "General", "Written by the ASWIni package check." };
    ASWIni::TASWIniSetting<int> Count{ General, "Count", 3, "From 0 to 9.",
        ASWIni::TASWIniSettingOptions<int>().SetRange(0, 9) };
};

//---------------------------------------------------------------------------

// Counts a problem, saying what it is, when 'condition' is false
void Check(bool condition, char const* what);

//---------------------------------------------------------------------------
/*
    Check

    Counts a problem, saying what it is, when 'condition' is false.
*/
void Check(bool condition, char const* what)
{
    if (condition)
        return;

    ++ProblemCount;
    std::cerr << "Problem: " << what << "\n";
}
//---------------------------------------------------------------------------

} // namespace

//---------------------------------------------------------------------------

int main()
{
    // A text: the setting is added to an empty document, read, set, and refused outside its range
    TCheckSettings settings;
    Check(settings.LoadText("").IsOk(), "an empty text should load");
    Check(settings.Count.Get() == 3, "the setting should have its default value");
    Check(settings.IsModified(), "loading should add the setting");
    Check(settings.Count.Set(5).IsOk(), "the setting should be set");
    Check(settings.Count.Set(10).Code == ASWIni::EIniCode::ValueOutOfRange, "10 should be out of the range");
    Check(settings.ToText().find("Count = 5") != std::string::npos, "the text should have the value set");

    // A file, in a folder of the temp folder that the save creates
    std::error_code error;
    std::filesystem::path const folder = std::filesystem::temp_directory_path(error) / "ASWIniPackageCheck";
    std::filesystem::remove_all(folder, error);
    std::string const path = ASWIni::PathToUtf8(folder / "check.ini");
    Check(settings.SaveAs(path).IsOk(), "the file should be saved");

    TCheckSettings loaded;
    Check(loaded.Load(path).IsOk(), "the file should load");
    Check(loaded.Count.Get() == 5, "the loaded setting should have the value saved");
    Check(!loaded.IsModified(), "a file that has every setting shouldn't need saving");
    Check(loaded.ToText() == settings.ToText(), "the file should load as it was saved");
    std::filesystem::remove_all(folder, error);

    // The document on its own
    ASWIni::TASWIniDocument document;
    std::vector<ASWIni::TASWIniMessage> messages;
    Check(document.Parse("[A]\nkey = value\n", ASWIni::TASWIniOptions(), messages).IsOk(), "the text should parse");
    Check(document.ToText() == "[A]\nkey = value\n", "the text should be written back as it was");

    if (ProblemCount != 0)
        return 1;

    std::cout << "ASWIni " << ASWIni::Version << ": settings, texts and files work\n";

    return 0;
}
