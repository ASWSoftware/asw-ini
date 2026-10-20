/* **************************************************************************
Test_ASWIni_Base.cpp
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
#include "Test_ASWIni_Base.h"
//---------------------------------------------------------------------------
#include "ASWUnitTests_Registry.h"
//---------------------------------------------------------------------------
#include <atomic>
#include <chrono>
#include <cstddef>
#include <functional>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <vector>
//---------------------------------------------------------------------------
#include "ASWIni_Base.h"
#include "ASWIni_Version.h"
#include "UT_Helper_FailingAlloc.h"
//---------------------------------------------------------------------------

// Neither copyable nor movable
static_assert(!std::is_copy_constructible_v<ASWIni::TASWBaseIni>, "TASWBaseIni shouldn't be copyable");
static_assert(!std::is_move_constructible_v<ASWIni::TASWBaseIni>, "TASWBaseIni shouldn't be movable");
static_assert(!std::is_copy_constructible_v<ASWIni::Detail::TMutex>, "TMutex shouldn't be copyable");
static_assert(!std::is_copy_constructible_v<ASWIni::Detail::TSharedMutex>, "TSharedMutex shouldn't be copyable");

namespace ASWUnitTests
{

using ASWIni::EBoolFormat;
using ASWIni::EIniCode;
using ASWIni::ESeverity;
using ASWIni::TASWIniDocument;
using ASWIni::TASWIniLogCallback;
using ASWIni::TASWIniMessage;
using ASWIni::TASWIniOptions;
using ASWIni::TASWIniResult;

namespace
{

// A text with comments, sections, keys and a section name with a dot
constexpr char const* SampleText =
    "; Settings of the app\r\n"
    "\r\n"
    "; The main window\r\n"
    "[Window]\r\n"
    "Width = 640\r\n"
    "; In pixels\r\n"
    "Height = 480\r\n"
    "\r\n"
    "[Window.Tools]\r\n"
    "Visible = yes\r\n"
    "\r\n"
    "[General]\r\n"
    "Name = My App ; not a comment\r\n";

// A text with three warnings: a malformed line (line 2), an unclosed section header (line 3) and a duplicate key
// (line 5)
constexpr char const* WarningsText =
    "[A]\n"
    "no separator\n"
    "[B\n"
    "k = 1\n"
    "k = 2\n";

// How long a test waits for another thread before it counts as stuck
constexpr std::chrono::seconds ThreadTimeout(10);


/////////////////////////////////////////////////////////////////////////////
// TTestIni
//
// A TASWBaseIni with Load(), Save() and SaveAs(), which these tests don't use, failing with InvalidArgument
/////////////////////////////////////////////////////////////////////////////
class TTestIni : public ASWIni::TASWBaseIni
{
public:
    TASWIniResult Load(std::string_view filePath) override;
    TASWIniResult Save() override;
    TASWIniResult SaveAs(std::string_view filePath) override;
};

//---------------------------------------------------------------------------

TASWIniDocument ParsedDocument(std::string_view text, TASWIniOptions const& options = TASWIniOptions());
TASWIniLogCallback Recorder(std::vector<TASWIniMessage>& messages);
TASWIniResult Unused();

//---------------------------------------------------------------------------
/*
    ParsedDocument

    A TASWIniDocument that has parsed 'text' with 'options'.
*/
TASWIniDocument ParsedDocument(std::string_view text, TASWIniOptions const& options)
{
    TASWIniDocument document;
    std::vector<TASWIniMessage> messages;
    static_cast<void>(document.Parse(text, options, messages));

    return document;
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
    Unused

    The result of TTestIni's file operations.
*/
TASWIniResult Unused()
{
    TASWIniResult result;
    result.Code = EIniCode::InvalidArgument;
    result.Message = "Not used by these tests.";

    return result;
}
//---------------------------------------------------------------------------


/////////////////////////////////////////////////////////////////////////////
// TTestIni
/////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------
TASWIniResult TTestIni::Load(std::string_view /*filePath*/)
{
    return Unused();
}
//---------------------------------------------------------------------------
TASWIniResult TTestIni::Save()
{
    return Unused();
}
//---------------------------------------------------------------------------
TASWIniResult TTestIni::SaveAs(std::string_view /*filePath*/)
{
    return Unused();
}
//---------------------------------------------------------------------------

} // namespace


/////////////////////////////////////////////////////////////////////////////
// TTest_ASWIni_BaseAccess
/////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------
TTest_ASWIni_BaseAccess::TTest_ASWIni_BaseAccess()
    : inherited("ASWIni_BaseAccess_Tests")
{
}
//---------------------------------------------------------------------------
TTest_ASWIni_BaseAccess::~TTest_ASWIni_BaseAccess()
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_BaseAccess::SetUp_Group()
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_BaseAccess::SetUp_Test(ITestCase& /*testCase*/)
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_BaseAccess::TearDown_Group()
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_BaseAccess::TearDown_Test(ITestCase& /*testCase*/)
{
}
//---------------------------------------------------------------------------

// /////// Begin tests after this line ///////////////////////

//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_BaseAccess, Edits_FailureChangesNothing)
{
    // Arrange
    TTestIni ini;
    static_cast<void>(ini.LoadText(SampleText));
    TASWIniDocument document = ParsedDocument(SampleText);

    // Act
    TASWIniResult const renameKey = ini.RenameKey("Window", "Depth", "Z");
    TASWIniResult const renameSection = ini.RenameSection("Window", "General");
    TASWIniResult const setText = ini.SetText("Window", "Bad=Key", "1");
    TASWIniResult const addSection = ini.AddSection("Bad]Name");
    TASWIniResult const sectionComment = ini.SetSectionComment("Missing", "Text");

    // Assert
    CheckEquals(document.RenameKey("Window", "Depth", "Z").Code, renameKey.Code, __func__, __LINE__,
        "a missing key should fail as the document's RenameKey() does");
    CheckEquals(EIniCode::NotFound, renameKey.Code, __func__, __LINE__, "a missing key should be NotFound");
    CheckEquals(document.RenameSection("Window", "General").Message, renameSection.Message, __func__, __LINE__,
        "an existing name should fail as the document's RenameSection() does");
    CheckEquals(EIniCode::InvalidArgument, setText.Code, __func__, __LINE__, "an invalid key should fail");
    CheckEquals(EIniCode::InvalidArgument, addSection.Code, __func__, __LINE__, "an invalid section name should fail");
    CheckEquals(EIniCode::NotFound, sectionComment.Code, __func__, __LINE__, "a missing section should fail");
    CheckEquals(std::string(SampleText), ini.ToText(), __func__, __LINE__, "the text should be unchanged");
    CheckFalse(ini.IsModified(), __func__, __LINE__, "the object shouldn't be modified");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_BaseAccess, Edits_MatchTheDocument)
{
    // Arrange
    TTestIni ini;
    static_cast<void>(ini.LoadText(SampleText));
    TASWIniDocument document = ParsedDocument(SampleText);
    std::vector<TASWIniResult> results;
    std::vector<TASWIniResult> expected;

    // Act
    results.push_back(ini.SetText("Window", "Width", "800"));
    results.push_back(ini.SetText("Window", "Left", "10"));
    results.push_back(ini.SetText("New", "Key", "Value"));
    results.push_back(ini.AddSection("Empty"));
    results.push_back(ini.RenameKey("Window", "Height", "Tall"));
    results.push_back(ini.RenameSection("General", "Main"));
    results.push_back(ini.SetFileComment("New file comment"));
    results.push_back(ini.SetSectionComment("Main", "The main section"));
    results.push_back(ini.SetKeyComment("Window", "Width", "Wider"));
    bool const removedKey = ini.RemoveKey("Window.Tools", "Visible");
    bool const removedSection = ini.RemoveSection("Empty");
    expected.push_back(document.SetValue("Window", "Width", "800"));
    expected.push_back(document.SetValue("Window", "Left", "10"));
    expected.push_back(document.SetValue("New", "Key", "Value"));
    expected.push_back(document.AddSection("Empty"));
    expected.push_back(document.RenameKey("Window", "Height", "Tall"));
    expected.push_back(document.RenameSection("General", "Main"));
    expected.push_back(document.SetFileComment("New file comment"));
    expected.push_back(document.SetSectionComment("Main", "The main section"));
    expected.push_back(document.SetKeyComment("Window", "Width", "Wider"));
    bool const expectedKey = document.RemoveKey("Window.Tools", "Visible");
    bool const expectedSection = document.RemoveSection("Empty");

    // Assert
    for (std::size_t i = 0; i < results.size(); ++i)
        CheckTrue(results[i].IsOk() && expected[i].IsOk(), __func__, __LINE__, "edit " + std::to_string(i) +
            " should succeed");

    CheckEquals(expectedKey, removedKey, __func__, __LINE__, "RemoveKey() should do what the document's does");
    CheckEquals(expectedSection, removedSection, __func__, __LINE__,
        "RemoveSection() should do what the document's does");
    CheckEquals(document.ToText(), ini.ToText(), __func__, __LINE__, "the edits should change the text as they do "
        "the document's");
    CheckTrue(ini.IsModified(), __func__, __LINE__, "the object should be modified");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_BaseAccess, GetDocument_IsACopy)
{
    // Arrange
    TTestIni ini;
    static_cast<void>(ini.LoadText(SampleText));

    // Act
    TASWIniDocument document = ini.GetDocument();
    static_cast<void>(document.SetValue("Window", "Width", "1"));

    // Assert
    CheckEquals(std::size_t(13), ini.GetDocument().GetLineCount(), __func__, __LINE__,
        "the copy should have the document's lines");
    std::string width;
    CheckTrue(ini.TryGetText("Window", "Width", width), __func__, __LINE__, "the key should exist");
    CheckEquals(std::string("640"), width, __func__, __LINE__, "editing the copy shouldn't change the object");
    CheckFalse(ini.IsModified(), __func__, __LINE__, "the object shouldn't be modified");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_BaseAccess, LoadText_FailureChangesNothing)
{
    // Arrange
    TTestIni ini;
    static_cast<void>(ini.LoadText(SampleText));
    static_cast<void>(ini.SetText("Window", "Width", "800"));
    std::string const before = ini.ToText();
    TASWIniOptions strict;
    strict.Reading.StrictParsing = true;
    static_cast<void>(ini.SetOptions(strict));

    // Act
    TASWIniResult const utf16 = ini.LoadText(std::string_view("\xFF\xFE[A]", 5));
    TASWIniResult const malformed = ini.LoadText("[A]\nno separator\n");

    // Assert
    CheckEquals(EIniCode::UnsupportedEncoding, utf16.Code, __func__, __LINE__, "UTF-16 should be UnsupportedEncoding");
    CheckEquals(EIniCode::ParseFailed, malformed.Code, __func__, __LINE__,
        "a malformed line should be ParseFailed with StrictParsing");
    CheckEquals(std::size_t(2), malformed.Line, __func__, __LINE__, "the result should have the malformed line");
    CheckEquals(before, ini.ToText(), __func__, __LINE__, "the document should be unchanged");
    CheckTrue(ini.IsModified(), __func__, __LINE__, "the modified flag should be unchanged");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_BaseAccess, LoadText_ReplacesTheDocument)
{
    // Arrange
    TTestIni ini;
    static_cast<void>(ini.SetText("Old", "Key", "Value"));

    // Act
    TASWIniResult const result = ini.LoadText(SampleText);

    // Assert
    CheckTrue(result.IsOk(), __func__, __LINE__, "the text should be loaded");
    CheckEquals(std::string(SampleText), ini.ToText(), __func__, __LINE__, "the text should be written back as it is");
    CheckFalse(ini.HasSection("Old"), __func__, __LINE__, "the old document should be gone");
    CheckFalse(ini.IsModified(), __func__, __LINE__, "a loaded document shouldn't be modified");
    CheckEquals(std::string(), ini.GetFilePath(), __func__, __LINE__, "there should still be no file path");

    // Act
    TASWIniResult const empty = ini.LoadText("");

    // Assert
    CheckTrue(empty.IsOk(), __func__, __LINE__, "an empty text should be loaded");
    CheckEquals(std::string(), ini.ToText(), __func__, __LINE__, "an empty text should empty the document");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_BaseAccess, LoadText_UsesTheOptions)
{
    // Arrange
    TTestIni ini;
    TASWIniOptions options;
    options.Reading.InlineComments = true;
    static_cast<void>(ini.SetOptions(options));

    // Act
    static_cast<void>(ini.LoadText(SampleText));

    // Assert
    std::string name;
    CheckTrue(ini.TryGetText("General", "Name", name), __func__, __LINE__, "the key should exist");
    CheckEquals(std::string("My App"), name, __func__, __LINE__, "the text should be read with the object's options");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_BaseAccess, Locks_WorkLikeStandardMutexes)
{
    // Arrange
    // Writers increment two counters under the exclusive lock, and a third under the mutex, each with a yield between
    // reading and writing it, so without the lock other writers' increments in between would be lost; readers check
    // under the shared lock that the two are equal. A second shared lock is taken while one is held, which an
    // exclusive lock would block.
    ASWIni::Detail::TSharedMutex shared;
    ASWIni::Detail::TMutex plain;
    long long first = 0;
    long long second = 0;
    long long plainCount = 0;
    std::atomic<long long> unequal{ 0 };
    int const iterations = 5000;
    auto const writer = [&]()
        {
            for (int i = 0; i < iterations; ++i)
            {
                {
                    std::lock_guard<ASWIni::Detail::TSharedMutex> lock(shared);
                    long long const value = first;
                    std::this_thread::yield();
                    first = value + 1;
                    std::this_thread::yield();
                    ++second;
                }

                std::lock_guard<ASWIni::Detail::TMutex> lock(plain);
                long long const count = plainCount;
                std::this_thread::yield();
                plainCount = count + 1;
            }
        };
    auto const reader = [&]()
        {
            for (int i = 0; i < iterations; ++i)
            {
                std::shared_lock<ASWIni::Detail::TSharedMutex> lock(shared);
                if (first != second)
                    ++unequal;
            }
        };

    // Act
    std::vector<std::thread> threads;
    for (int i = 0; i < 6; ++i)
    {
        if (i % 2 == 0)
            threads.emplace_back(writer);
        else
            threads.emplace_back(reader);
    }

    for (std::thread& thread : threads)
        thread.join();

    std::atomic<bool> secondReaderIn{ false };
    bool sharedTogether = false;
    {
        std::shared_lock<ASWIni::Detail::TSharedMutex> lock(shared);
        auto const secondReader = [&shared, &secondReaderIn]()
            {
                std::shared_lock<ASWIni::Detail::TSharedMutex> secondLock(shared);
                secondReaderIn = true;
            };
        std::thread thread(secondReader);
        auto const deadline = std::chrono::steady_clock::now() + ThreadTimeout;
        while (!secondReaderIn && std::chrono::steady_clock::now() < deadline)
            std::this_thread::yield();

        sharedTogether = secondReaderIn;
        lock.unlock();
        thread.join();
    }

    // Assert
    CheckEquals(3LL * iterations, first, __func__, __LINE__, "no exclusive increment should be lost");
    CheckEquals(3LL * iterations, plainCount, __func__, __LINE__, "no increment under the mutex should be lost");
    CheckEquals(0LL, unequal.load(), __func__, __LINE__, "a reader should never see a writer halfway");
    CheckTrue(sharedTogether, __func__, __LINE__, "two threads should hold the shared lock together");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_BaseAccess, Lookups_MatchTheDocument)
{
    // Arrange
    TTestIni ini;
    static_cast<void>(ini.LoadText(SampleText));
    TASWIniDocument const document = ParsedDocument(SampleText);
    std::string text = "unchanged";

    // Act & Assert
    CheckTrue(ini.HasSection("") && ini.HasSection("window") && ini.HasSection("Window.Tools"), __func__, __LINE__,
        "the sections should exist, compared without case");
    CheckFalse(ini.HasSection("Missing"), __func__, __LINE__, "a missing section shouldn't exist");
    CheckTrue(ini.HasKey("Window", "HEIGHT"), __func__, __LINE__, "a key should exist, compared without case");
    CheckFalse(ini.HasKey("Window", "Visible"), __func__, __LINE__, "a key of another section shouldn't exist");
    CheckTrue(ini.GetKeyNames("Window") == document.GetKeyNames("Window"), __func__, __LINE__,
        "the key names should be the document's");
    CheckTrue(ini.GetSectionNames() == document.GetSectionNames(), __func__, __LINE__,
        "the section names should be the document's");
    CheckTrue(ini.GetSectionNames("Window.") == std::vector<std::string>{ "Window.Tools" }, __func__, __LINE__,
        "the section names with a prefix should be the document's");
    CheckEquals(document.GetFileComment(), ini.GetFileComment(), __func__, __LINE__,
        "the file comment should be the document's");
    CheckEquals(std::string("The main window"), ini.GetSectionComment("Window"), __func__, __LINE__,
        "the section comment should be the document's");
    CheckEquals(std::string("In pixels"), ini.GetKeyComment("Window", "Height"), __func__, __LINE__,
        "the key comment should be the document's");
    CheckFalse(ini.TryGetText("Window", "Depth", text), __func__, __LINE__, "a missing key should have no text");
    CheckEquals(std::string("unchanged"), text, __func__, __LINE__, "the text should stay as it was");
    CheckTrue(ini.TryGetText("General", "Name", text), __func__, __LINE__, "the key should have a text");
    CheckEquals(std::string("My App ; not a comment"), text, __func__, __LINE__, "the text should be the value's");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_BaseAccess, New_IsEmptyWithTheDefaultOptions)
{
    // Arrange & Act
    TTestIni ini;

    // Assert
    TASWIniOptions const defaults;
    TASWIniOptions const options = ini.GetOptions();
    CheckEquals(std::string(), ini.ToText(), __func__, __LINE__, "a new object should have no text");
    CheckEquals(std::string(), ini.GetFilePath(), __func__, __LINE__, "a new object should have no file path");
    CheckFalse(ini.IsModified(), __func__, __LINE__, "a new object shouldn't be modified");
    CheckTrue(ini.HasSection(""), __func__, __LINE__, "the keys before the first section should always exist");
    CheckEquals(defaults.Reading.Separators, options.Reading.Separators, __func__, __LINE__,
        "the options should be the defaults");
    CheckEquals(defaults.Writing.Separator, options.Writing.Separator, __func__, __LINE__,
        "the options should be the defaults");
    CheckTrue(ini.GetValueOptions().BoolFormat == EBoolFormat::TrueFalse, __func__, __LINE__,
        "the value options should be the defaults");
    CheckEquals(std::string(ASWIni::Version), std::string(ini.GetVersionStr()), __func__, __LINE__,
        "the version should be the library's");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_BaseAccess, OutOfMemory_EditChangesNothing)
{
    // Arrange
    if (!CanFailAllocations())
        Skip(__func__, __LINE__, "allocations can't be made to fail in a sanitizer build");

    std::size_t failures = 0;

    // Act & Assert
    // Each allocation of the edit fails in turn, on a new object, until the edit makes no more. A failed allocation
    // fails the edit, or, when the standard library copes with it (bcc32c's has a few such allocations), doesn't.
    for (std::size_t number = 1; number < 1000; ++number)
    {
        TTestIni ini;
        static_cast<void>(ini.LoadText(SampleText));
        std::string const which = " (allocation " + std::to_string(number) + ")";
        TASWIniResult result;
        bool failed = false;
        {
            TFailingAlloc failingAlloc(number);
            result = ini.SetText("New Section", "Key", "Value");
            failed = failingAlloc.HasFailed();
        }

        if (result.IsOk())
        {
            CheckTrue(ini.HasKey("New Section", "Key") && ini.IsModified(), __func__, __LINE__,
                "an edit that succeeded should be complete" + which);
        }
        else
        {
            ++failures;
            CheckTrue(failed, __func__, __LINE__, "the edit should only fail when an allocation fails" + which);
            CheckEquals(EIniCode::UnexpectedException, result.Code, __func__, __LINE__,
                "a failed allocation should be UnexpectedException" + which);
            CheckStartsWith(result.Message, std::string("An unexpected exception was thrown: "), __func__, __LINE__,
                "the message should say what was thrown" + which);
            CheckEquals(std::string(SampleText), ini.ToText(), __func__, __LINE__,
                "a failed edit should change nothing" + which);
            CheckFalse(ini.IsModified(), __func__, __LINE__, "a failed edit shouldn't set the modified flag" + which);
        }

        if (!failed)
            break;
    }

    CheckGreaterThan(failures, std::size_t(0), __func__, __LINE__, "at least one allocation should have failed");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_BaseAccess, OutOfMemory_LoadTextChangesNothing)
{
    // Arrange
    if (!CanFailAllocations())
        Skip(__func__, __LINE__, "allocations can't be made to fail in a sanitizer build");

    std::size_t failures = 0;

    // Act & Assert
    // As OutOfMemory_EditChangesNothing does
    for (std::size_t number = 1; number < 10000; ++number)
    {
        TTestIni ini;
        static_cast<void>(ini.SetText("Old", "Key", "Value"));
        std::string const before = ini.ToText();
        std::string const which = " (allocation " + std::to_string(number) + ")";
        TASWIniResult result;
        bool failed = false;
        {
            TFailingAlloc failingAlloc(number);
            result = ini.LoadText(SampleText);
            failed = failingAlloc.HasFailed();
        }

        if (result.IsOk())
        {
            CheckEquals(std::string(SampleText), ini.ToText(), __func__, __LINE__,
                "a load that succeeded should have loaded the text" + which);
            CheckFalse(ini.IsModified(), __func__, __LINE__, "a loaded document shouldn't be modified" + which);
        }
        else
        {
            ++failures;
            CheckTrue(failed, __func__, __LINE__, "the load should only fail when an allocation fails" + which);
            CheckEquals(EIniCode::UnexpectedException, result.Code, __func__, __LINE__,
                "a failed allocation should be UnexpectedException" + which);
            CheckEquals(before, ini.ToText(), __func__, __LINE__, "a failed load should change nothing" + which);
            CheckTrue(ini.IsModified(), __func__, __LINE__, "a failed load should keep the modified flag" + which);
        }

        if (!failed)
            break;
    }

    CheckGreaterThan(failures, std::size_t(0), __func__, __LINE__, "at least one allocation should have failed");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_BaseAccess, OutOfMemory_SetOptionsChangesNothing)
{
    // Arrange
    if (!CanFailAllocations())
        Skip(__func__, __LINE__, "allocations can't be made to fail in a sanitizer build");

    TASWIniOptions options;
    options.Reading.InlineComments = true;
    std::size_t failures = 0;

    // Act & Assert
    // As OutOfMemory_EditChangesNothing does
    for (std::size_t number = 1; number < 10000; ++number)
    {
        TTestIni ini;
        static_cast<void>(ini.LoadText(SampleText));
        std::string const which = " (allocation " + std::to_string(number) + ")";
        TASWIniResult result;
        bool failed = false;
        {
            TFailingAlloc failingAlloc(number);
            result = ini.SetOptions(options);
            failed = failingAlloc.HasFailed();
        }

        std::string name;
        static_cast<void>(ini.TryGetText("General", "Name", name));
        if (result.IsOk())
        {
            CheckTrue(ini.GetOptions().Reading.InlineComments && name == "My App", __func__, __LINE__,
                "SetOptions() that succeeded should have read the text again with the options" + which);
        }
        else
        {
            ++failures;
            CheckTrue(failed, __func__, __LINE__, "SetOptions() should only fail when an allocation fails" + which);
            CheckEquals(EIniCode::UnexpectedException, result.Code, __func__, __LINE__,
                "a failed allocation should be UnexpectedException" + which);
            CheckFalse(ini.GetOptions().Reading.InlineComments, __func__, __LINE__,
                "a failed SetOptions() should keep the options" + which);
            CheckEquals(std::string("My App ; not a comment"), name, __func__, __LINE__,
                "a failed SetOptions() should keep the lines as they were read" + which);
        }

        if (!failed)
            break;
    }

    CheckGreaterThan(failures, std::size_t(0), __func__, __LINE__, "at least one allocation should have failed");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_BaseAccess, Remove_ReturnsWhetherAnythingWasRemoved)
{
    // Arrange
    TTestIni ini;
    static_cast<void>(ini.LoadText(SampleText));

    // Act
    bool const removedKey = ini.RemoveKey("Window", "Width");
    bool const removedAgain = ini.RemoveKey("Window", "Width");
    bool const removedSection = ini.RemoveSection("Window.Tools");
    bool const removedMissing = ini.RemoveSection("Missing");

    // Assert
    CheckTrue(removedKey, __func__, __LINE__, "an existing key should be removed");
    CheckFalse(removedAgain, __func__, __LINE__, "a removed key shouldn't be removed again");
    CheckTrue(removedSection, __func__, __LINE__, "an existing section should be removed");
    CheckFalse(removedMissing, __func__, __LINE__, "a missing section shouldn't be removed");
    CheckFalse(ini.HasKey("Window", "Width") || ini.HasSection("Window.Tools"), __func__, __LINE__,
        "the key and section should be gone");
    CheckTrue(ini.IsModified(), __func__, __LINE__, "the object should be modified");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_BaseAccess, SetOptions_InvalidOptionsChangeNothing)
{
    // Arrange
    TTestIni ini;
    static_cast<void>(ini.LoadText(SampleText));
    TASWIniOptions options;
    options.Reading.InlineComments = true;
    options.Reading.Separators = "";

    // Act
    TASWIniResult const result = ini.SetOptions(options);

    // Assert
    CheckEquals(EIniCode::InvalidOptions, result.Code, __func__, __LINE__, "invalid options should be InvalidOptions");
    CheckEquals(TASWIniOptions().Reading.Separators, ini.GetOptions().Reading.Separators, __func__, __LINE__,
        "the options should be unchanged");
    CheckFalse(ini.GetOptions().Reading.InlineComments, __func__, __LINE__, "the options should be unchanged");
    CheckEquals(std::string(SampleText), ini.ToText(), __func__, __LINE__, "the text should be unchanged");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_BaseAccess, SetOptions_ReadsTheTextAgain)
{
    // Arrange
    TTestIni ini;
    static_cast<void>(ini.LoadText(SampleText));
    static_cast<void>(ini.SetText("Window", "Width", "800"));
    std::string const before = ini.ToText();
    TASWIniOptions options;
    options.Reading.InlineComments = true;
    options.Values.BoolFormat = EBoolFormat::YesNo;

    // Act
    TASWIniResult const result = ini.SetOptions(options);

    // Assert
    std::string name;
    CheckTrue(result.IsOk(), __func__, __LINE__, "the options should be set");
    CheckTrue(ini.GetOptions().Reading.InlineComments, __func__, __LINE__, "the object should have the new options");
    CheckTrue(ini.GetValueOptions().BoolFormat == EBoolFormat::YesNo, __func__, __LINE__,
        "the value options should be the new ones");
    CheckTrue(ini.TryGetText("General", "Name", name), __func__, __LINE__, "the key should exist");
    CheckEquals(std::string("My App"), name, __func__, __LINE__, "the text should be read again with the new options");
    CheckEquals(before, ini.ToText(), __func__, __LINE__, "the text should be unchanged");
    CheckTrue(ini.IsModified(), __func__, __LINE__, "the modified flag should be kept");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_BaseAccess, TypedValues_UseTheValueOptions)
{
    // Arrange
    TTestIni ini;
    static_cast<void>(ini.LoadText(SampleText));
    TASWIniOptions options;
    options.Values.BoolFormat = EBoolFormat::OnOff;
    static_cast<void>(ini.SetOptions(options));

    // Act
    TASWIniResult const result = ini.SetValue("Window", "Maximized", true);
    int const width = ini.GetValue("Window", "Width", 0);
    bool const visible = ini.GetValue("Window.Tools", "Visible", false);

    // Assert
    std::string text;
    CheckTrue(result.IsOk(), __func__, __LINE__, "the value should be set");
    CheckTrue(ini.TryGetText("Window", "Maximized", text) && text == "on", __func__, __LINE__,
        "the bool should be written as the object's value options say");
    CheckEquals(640, width, __func__, __LINE__, "an int should be read");
    CheckTrue(visible, __func__, __LINE__, "a bool should be read");
}
//---------------------------------------------------------------------------


/////////////////////////////////////////////////////////////////////////////
// TTest_ASWIni_BaseCallback
/////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------
TTest_ASWIni_BaseCallback::TTest_ASWIni_BaseCallback()
    : inherited("ASWIni_BaseCallback_Tests")
{
}
//---------------------------------------------------------------------------
TTest_ASWIni_BaseCallback::~TTest_ASWIni_BaseCallback()
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_BaseCallback::SetUp_Group()
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_BaseCallback::SetUp_Test(ITestCase& /*testCase*/)
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_BaseCallback::TearDown_Group()
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_BaseCallback::TearDown_Test(ITestCase& /*testCase*/)
{
}
//---------------------------------------------------------------------------

// /////// Begin tests after this line ///////////////////////

//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_BaseCallback, Callback_ExceptionIsDropped)
{
    // Arrange
    TTestIni ini;
    std::vector<TASWIniMessage> messages;
    auto const throwing = [&messages](TASWIniMessage const& message)
        {
            messages.push_back(message);
            if (messages.size() == 1)
                throw std::runtime_error("The callback failed.");

            throw 42;
        };
    ini.SetLogCallback(throwing);

    // Act
    TASWIniResult const result = ini.LoadText(WarningsText);

    // Assert
    CheckTrue(result.IsOk(), __func__, __LINE__, "the text should be loaded");
    CheckEquals(std::size_t(3), messages.size(), __func__, __LINE__,
        "every message should be delivered, after the callback threw");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_BaseCallback, Callback_MayCallTheObject)
{
    // Arrange
    // A lock held while the callback runs would make these calls deadlock (the CI's test timeout turns that into a
    // failure). The SetOptions() it calls reads the text again, which reports the warning again, to the callback,
    // while it is still running.
    TTestIni ini;
    std::vector<std::string> seen;
    auto const reentrant = [&ini, &seen](TASWIniMessage const& message)
        {
            seen.push_back(message.Text);
            if (seen.size() > 1)
                return;

            seen.push_back(ini.GetValue("B", "k", "none"));
            static_cast<void>(ini.SetText("Callback", "Called", "yes"));
            static_cast<void>(ini.SetOptions(ini.GetOptions()));
            seen.push_back(ini.ToText());
        };
    ini.SetLogCallback(reentrant);

    // Act
    TASWIniResult const result = ini.LoadText("[A]\nno separator\n[B]\nk = 1\n");
    TASWIniResult const failed = ini.RenameSection("Missing", "Other");

    // Assert
    CheckTrue(result.IsOk(), __func__, __LINE__, "the text should be loaded");
    CheckEquals(EIniCode::NotFound, failed.Code, __func__, __LINE__, "the rename should fail");
    CheckEquals(std::size_t(5), seen.size(), __func__, __LINE__, "the callback should have run three times");
    if (seen.size() == 5)
    {
        CheckEquals(std::string("1"), seen[1], __func__, __LINE__, "the callback should see the loaded document");
        CheckEquals(seen[0], seen[2], __func__, __LINE__,
            "the callback's SetOptions() should report the warning again, inside the callback");
        CheckContains(seen[3], std::string("[Callback]"), __func__, __LINE__,
            "the callback's edit should be in the document");
        CheckEquals(failed.Message, seen[4], __func__, __LINE__, "the failed rename should be reported");
    }
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_BaseCallback, Callback_MayReplaceItself)
{
    // Arrange
    TTestIni ini;
    std::vector<TASWIniMessage> second;
    std::size_t firstCount = 0;
    auto const first = [&ini, &second, &firstCount](TASWIniMessage const& /*message*/)
        {
            ++firstCount;
            ini.SetLogCallback(Recorder(second));
        };
    ini.SetLogCallback(first);

    // Act
    static_cast<void>(ini.LoadText(WarningsText));

    // Assert
    CheckEquals(std::size_t(1), firstCount, __func__, __LINE__, "the first callback should get the first message");
    CheckEquals(std::size_t(2), second.size(), __func__, __LINE__, "the new callback should get the others");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_BaseCallback, FailedEdit_IsReportedAsAnError)
{
    // Arrange
    TTestIni ini;
    static_cast<void>(ini.LoadText(SampleText));
    std::vector<TASWIniMessage> messages;
    ini.SetLogCallback(Recorder(messages));

    // Act
    TASWIniResult const ok = ini.SetText("Window", "Width", "1");
    TASWIniResult const results[] = {
        ini.SetText("Window", "", "1"), ini.AddSection("Bad]"), ini.RenameKey("Window", "Depth", "Z"),
        ini.RenameSection("Missing", "Other"), ini.SetSectionComment("Missing", "Text"),
        ini.SetKeyComment("Window", "Depth", "Text"),
    };

    // Assert
    CheckTrue(ok.IsOk(), __func__, __LINE__, "the first edit should succeed");
    std::size_t const count = sizeof(results) / sizeof(results[0]);
    CheckEquals(count, messages.size(), __func__, __LINE__, "each failed edit, and only those, should be reported");
    for (std::size_t i = 0; i < count && i < messages.size(); ++i)
    {
        std::string const which = " (edit " + std::to_string(i) + ")";
        CheckFalse(results[i].IsOk(), __func__, __LINE__, "the edit should fail" + which);
        CheckEquals(ESeverity::Error, messages[i].Severity, __func__, __LINE__,
            "the message should be an Error" + which);
        CheckEquals(results[i].Code, messages[i].Code, __func__, __LINE__, "the message should have the code" + which);
        CheckEquals(results[i].Message, messages[i].Text, __func__, __LINE__,
            "the message should have the result's text" + which);
    }
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_BaseCallback, GetValue_WarningIsReported)
{
    // Arrange
    TTestIni ini;
    static_cast<void>(ini.LoadText(SampleText));
    std::vector<TASWIniMessage> messages;
    ini.SetLogCallback(Recorder(messages));

    // Act
    int const visible = ini.GetValue("Window.Tools", "Visible", 7);

    // Assert
    CheckEquals(7, visible, __func__, __LINE__, "a value that doesn't convert should give the default");
    CheckEquals(std::size_t(1), messages.size(), __func__, __LINE__, "the value should be reported");
    if (messages.size() == 1)
    {
        CheckEquals(ESeverity::Warning, messages[0].Severity, __func__, __LINE__, "the message should be a Warning");
        CheckEquals(EIniCode::InvalidValue, messages[0].Code, __func__, __LINE__, "the message should be InvalidValue");
        CheckEquals(std::string("Window.Tools"), messages[0].Section, __func__, __LINE__,
            "the message should name the section");
        CheckEquals(std::string("Visible"), messages[0].Key, __func__, __LINE__, "the message should name the key");
    }
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_BaseCallback, LoadText_WarningsAreReportedInOrder)
{
    // Arrange
    TTestIni ini;
    std::vector<TASWIniMessage> messages;
    ini.SetLogCallback(Recorder(messages));

    // Act
    TASWIniResult const result = ini.LoadText(WarningsText);
    TASWIniResult const failed = ini.LoadText(std::string_view("\xFF\xFE", 2));

    // Assert
    CheckTrue(result.IsOk(), __func__, __LINE__, "the text should be loaded");
    CheckEquals(std::size_t(4), messages.size(), __func__, __LINE__, "three warnings and an error should be reported");
    if (messages.size() == 4)
    {
        CheckEquals(EIniCode::MalformedLine, messages[0].Code, __func__, __LINE__, "the first should be MalformedLine");
        CheckEquals(std::size_t(2), messages[0].Line, __func__, __LINE__, "the first should be about line 2");
        CheckEquals(EIniCode::UnclosedSection, messages[1].Code, __func__, __LINE__,
            "the second should be UnclosedSection");
        CheckEquals(EIniCode::DuplicateKey, messages[2].Code, __func__, __LINE__, "the third should be DuplicateKey");
        CheckEquals(std::size_t(5), messages[2].Line, __func__, __LINE__, "the third should be about line 5");
        CheckEquals(ESeverity::Warning, messages[2].Severity, __func__, __LINE__, "a warning should be a Warning");
        CheckEquals(ESeverity::Error, messages[3].Severity, __func__, __LINE__, "the failure should be an Error");
        CheckEquals(failed.Code, messages[3].Code, __func__, __LINE__, "the failure should have the result's code");
    }
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_BaseCallback, Lookups_ReportNothing)
{
    // Arrange
    TTestIni ini;
    static_cast<void>(ini.LoadText(SampleText));
    std::vector<TASWIniMessage> messages;
    ini.SetLogCallback(Recorder(messages));
    std::string text;
    int number = 0;

    // Act
    static_cast<void>(ini.HasKey("Missing", "Key"));
    static_cast<void>(ini.TryGetText("Missing", "Key", text));
    static_cast<void>(ini.GetKeyComment("Missing", "Key"));
    static_cast<void>(ini.GetValue("Missing", "Key", 1));
    TASWIniResult const tryGet = ini.TryGetValue("Window.Tools", "Visible", number);
    bool const removed = ini.RemoveKey("Missing", "Key");

    // Assert
    CheckEquals(EIniCode::InvalidValue, tryGet.Code, __func__, __LINE__, "TryGetValue() should fail");
    CheckFalse(removed, __func__, __LINE__, "nothing should be removed");
    CheckEquals(std::size_t(0), messages.size(), __func__, __LINE__,
        "lookups, TryGetValue() and removals should report nothing");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_BaseCallback, OutOfMemory_IsReportedAsAnError)
{
    // Arrange
    if (!CanFailAllocations())
        Skip(__func__, __LINE__, "allocations can't be made to fail in a sanitizer build");

    TTestIni ini;
    std::vector<TASWIniMessage> messages;
    ini.SetLogCallback(Recorder(messages));

    // Act
    TASWIniResult result;
    {
        TFailingAlloc failingAlloc(1);
        result = ini.SetText("Section", "Key", "A value long enough not to fit in a string's own buffer");
    }

    // Assert
    CheckEquals(EIniCode::UnexpectedException, result.Code, __func__, __LINE__,
        "a failed allocation should be UnexpectedException");
    CheckEquals(std::size_t(1), messages.size(), __func__, __LINE__, "the failure should be reported");
    if (messages.size() == 1)
    {
        CheckEquals(ESeverity::Error, messages[0].Severity, __func__, __LINE__, "the message should be an Error");
        CheckEquals(result.Message, messages[0].Text, __func__, __LINE__, "the message should have the result's text");
    }
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_BaseCallback, SetLogCallback_EmptyTurnsReportingOff)
{
    // Arrange
    TTestIni ini;
    std::vector<TASWIniMessage> messages;
    ini.SetLogCallback(Recorder(messages));
    static_cast<void>(ini.RenameSection("Missing", "Other"));

    // Act
    ini.SetLogCallback(TASWIniLogCallback());
    TASWIniResult const result = ini.RenameSection("Missing", "Other");
    static_cast<void>(ini.LoadText(WarningsText));

    // Assert
    CheckEquals(EIniCode::NotFound, result.Code, __func__, __LINE__, "the rename should still fail");
    CheckEquals(std::size_t(1), messages.size(), __func__, __LINE__,
        "only the failure before the callback was removed should be reported");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_BaseCallback, SetOptions_WarningsAreReported)
{
    // Arrange
    // With '=' as the only separator, "a: 1" is a malformed line
    TTestIni ini;
    static_cast<void>(ini.LoadText("[A]\na: 1\n"));
    std::vector<TASWIniMessage> messages;
    ini.SetLogCallback(Recorder(messages));
    TASWIniOptions options;
    options.Reading.Separators = "=";
    options.Writing.Separator = "=";

    // Act
    TASWIniResult const result = ini.SetOptions(options);

    // Assert
    CheckTrue(result.IsOk(), __func__, __LINE__, "the options should be set");
    CheckEquals(std::size_t(1), messages.size(), __func__, __LINE__, "the warning should be reported");
    if (messages.size() == 1)
    {
        CheckEquals(EIniCode::MalformedLine, messages[0].Code, __func__, __LINE__,
            "the warning should be MalformedLine");
        CheckEquals(std::size_t(2), messages[0].Line, __func__, __LINE__, "the warning should be about line 2");
    }
}
//---------------------------------------------------------------------------

} // namespace ASWUnitTests

//---------------------------------------------------------------------------
ASW_REGISTER_TEST_GROUP_IN_SUITE(ASWUnitTests::TTest_ASWIni_BaseAccess, "Base")
ASW_REGISTER_TEST_GROUP_IN_SUITE(ASWUnitTests::TTest_ASWIni_BaseCallback, "Base")
