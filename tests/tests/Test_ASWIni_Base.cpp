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
#include <filesystem>
#include <functional>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <type_traits>
#include <vector>
//---------------------------------------------------------------------------
#if defined(_WIN32)
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#endif
//---------------------------------------------------------------------------
#include "ASWIni_Base.h"
#include "ASWIni_File.h"
#include "ASWIni_Version.h"
#include "UT_Helper_FailingAlloc.h"
#include "UT_Helper_TempDir.h"
//---------------------------------------------------------------------------

// Neither copyable nor movable
static_assert(!std::is_copy_constructible_v<ASWIni::TASWBaseIni>, "TASWBaseIni shouldn't be copyable");
static_assert(!std::is_move_constructible_v<ASWIni::TASWBaseIni>, "TASWBaseIni shouldn't be movable");
static_assert(!std::is_copy_constructible_v<ASWIni::Detail::TMutex>, "TMutex shouldn't be copyable");
static_assert(!std::is_copy_constructible_v<ASWIni::Detail::TSharedMutex>, "TSharedMutex shouldn't be copyable");

// Load() swaps a document in, and gives the object the file path, after the point where an exception would make it
// change nothing
static_assert(std::is_nothrow_move_assignable_v<ASWIni::TASWIniDocument>,
    "a TASWIniDocument should move without throwing");
static_assert(std::is_nothrow_move_assignable_v<std::string>, "a std::string should move without throwing");

namespace ASWUnitTests
{

using ASWIni::Detail::PathToUtf8;
using ASWIni::EBoolFormat;
using ASWIni::EIniCode;
using ASWIni::ESeverity;
using ASWIni::TASWBaseIni;
using ASWIni::TASWIniDocument;
using ASWIni::TASWIniLogCallback;
using ASWIni::TASWIniMessage;
using ASWIni::TASWIniOptions;
using ASWIni::TASWIniResult;
using ASWIni::TASWIniSavingOptions;

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

// A text with bytes a text-mode read or write would change, which a load and a save keep: a byte order mark, CRLF,
// LF and CR line breaks, a tab, a Ctrl+Z, invalid UTF-8, and no line break at the end
constexpr char const* TrickyText =
    "\xEF\xBB\xBF; Settings\r\n"
    "[A]\r\n"
    "k = v\n"
    "\tx=1\r"
    "[B]\r\n"
    "name = caf\xC3\xA9\r\n"
    "bad = \xFF\x1A\r\n"
    "last = no line break";

// How long a test waits for another thread before it counts as stuck
constexpr std::chrono::seconds ThreadTimeout(10);


/////////////////////////////////////////////////////////////////////////////
// TReadCall
//
// The arguments of a call of TSeamIni::ReadFile()
/////////////////////////////////////////////////////////////////////////////
struct TReadCall
{
    std::string const& FilePath;
    std::size_t MaxSize;
    std::string& Contents;
};


/////////////////////////////////////////////////////////////////////////////
// TWriteCall
//
// The arguments of a call of TSeamIni::WriteFile()
/////////////////////////////////////////////////////////////////////////////
struct TWriteCall
{
    std::string const& FilePath;
    std::string_view Contents;
    TASWIniSavingOptions const& Options;
    std::vector<TASWIniMessage>& Warnings;
};


/////////////////////////////////////////////////////////////////////////////
// TSeamIni
//
// A TASWBaseIni whose ReadFile() and WriteFile() call OnRead and OnWrite instead of the defaults when they are set, so
// a test can see their arguments, call the object while a file is read or written, or make them fail
/////////////////////////////////////////////////////////////////////////////
class TSeamIni : public TASWBaseIni
{
public:
    std::function<TASWIniResult (TReadCall const& call)> OnRead;
    std::function<TASWIniResult (TWriteCall const& call)> OnWrite;

protected:
    TASWIniResult ReadFile(std::string const& filePath, std::size_t maxSize, std::string& contents) override;
    TASWIniResult WriteFile(std::string const& filePath, std::string_view contents,
        TASWIniSavingOptions const& options, std::vector<TASWIniMessage>& warnings) override;

public:
    // TASWBaseIni's WriteFile() with the arguments of 'call', which writes the file
    TASWIniResult DefaultWrite(TWriteCall const& call);
};

//---------------------------------------------------------------------------

TASWIniDocument ParsedDocument(std::string_view text, TASWIniOptions const& options = TASWIniOptions());
TASWIniLogCallback Recorder(std::vector<TASWIniMessage>& messages);
TASWIniResult SeamFailure(EIniCode code, std::string const& filePath, int systemError);

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
    SeamFailure

    A failed result with 'code', 'filePath' and 'systemError', as TSeamIni's OnRead or OnWrite may return it.
*/
TASWIniResult SeamFailure(EIniCode code, std::string const& filePath, int systemError)
{
    TASWIniResult result;
    result.Code = code;
    result.Message = "The seam failed.";
    result.FilePath = filePath;
    result.SystemError = systemError;

    return result;
}
//---------------------------------------------------------------------------


/////////////////////////////////////////////////////////////////////////////
// TSeamIni
/////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------
TASWIniResult TSeamIni::DefaultWrite(TWriteCall const& call)
{
    return TASWBaseIni::WriteFile(call.FilePath, call.Contents, call.Options, call.Warnings);
}
//---------------------------------------------------------------------------
TASWIniResult TSeamIni::ReadFile(std::string const& filePath, std::size_t maxSize, std::string& contents)
{
    TReadCall const call = { filePath, maxSize, contents };
    if (OnRead)
        return OnRead(call);

    return TASWBaseIni::ReadFile(filePath, maxSize, contents);
}
//---------------------------------------------------------------------------
TASWIniResult TSeamIni::WriteFile(std::string const& filePath, std::string_view contents,
    TASWIniSavingOptions const& options, std::vector<TASWIniMessage>& warnings)
{
    TWriteCall const call = { filePath, contents, options, warnings };
    if (OnWrite)
        return OnWrite(call);

    return DefaultWrite(call);
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
    TASWBaseIni ini;
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
    TASWBaseIni ini;
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
    TASWBaseIni ini;
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
    TASWBaseIni ini;
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
    TASWBaseIni ini;
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
    TASWBaseIni ini;
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
    TASWBaseIni ini;
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
    TASWBaseIni ini;

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
        Skip(__func__, __LINE__, "allocations can't be made to fail in this build (see CanFailAllocations())");

    std::size_t failures = 0;

    // Act & Assert
    // Each allocation of the edit fails in turn, on a new object, until the edit makes no more. A failed allocation
    // fails the edit, or, when the standard library copes with it (bcc32c's has a few such allocations), doesn't.
    for (std::size_t number = 1; number < 1000; ++number)
    {
        TASWBaseIni ini;
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
        Skip(__func__, __LINE__, "allocations can't be made to fail in this build (see CanFailAllocations())");

    std::size_t failures = 0;

    // Act & Assert
    // As OutOfMemory_EditChangesNothing does
    for (std::size_t number = 1; number < 10000; ++number)
    {
        TASWBaseIni ini;
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
        Skip(__func__, __LINE__, "allocations can't be made to fail in this build (see CanFailAllocations())");

    TASWIniOptions options;
    options.Reading.InlineComments = true;
    std::size_t failures = 0;

    // Act & Assert
    // As OutOfMemory_EditChangesNothing does
    for (std::size_t number = 1; number < 10000; ++number)
    {
        TASWBaseIni ini;
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
    TASWBaseIni ini;
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
    TASWBaseIni ini;
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
    TASWBaseIni ini;
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
    TASWBaseIni ini;
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
    TASWBaseIni ini;
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
    TASWBaseIni ini;
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
    TASWBaseIni ini;
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
    TASWBaseIni ini;
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
    TASWBaseIni ini;
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
    TASWBaseIni ini;
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
    TASWBaseIni ini;
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
        Skip(__func__, __LINE__, "allocations can't be made to fail in this build (see CanFailAllocations())");

    TASWBaseIni ini;
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
    TASWBaseIni ini;
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
    TASWBaseIni ini;
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


/////////////////////////////////////////////////////////////////////////////
// TTest_ASWIni_BaseFile
/////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------
TTest_ASWIni_BaseFile::TTest_ASWIni_BaseFile()
    : inherited("ASWIni_BaseFile_Tests")
{
}
//---------------------------------------------------------------------------
TTest_ASWIni_BaseFile::~TTest_ASWIni_BaseFile()
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_BaseFile::SetUp_Group()
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_BaseFile::SetUp_Test(ITestCase& /*testCase*/)
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_BaseFile::TearDown_Group()
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_BaseFile::TearDown_Test(ITestCase& /*testCase*/)
{
}
//---------------------------------------------------------------------------

// /////// Begin tests after this line ///////////////////////

//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_BaseFile, EmptyPath_IsInvalidArgument)
{
    // Arrange
    TSeamIni ini;
    static_cast<void>(ini.LoadText(SampleText));
    static_cast<void>(ini.SetText("Window", "Width", "800"));
    std::string const before = ini.ToText();
    std::vector<TASWIniMessage> messages;
    ini.SetLogCallback(Recorder(messages));
    int seamCalls = 0;
    auto const read = [&seamCalls](TReadCall const& /*call*/)
        {
            ++seamCalls;

            return TASWIniResult();
        };
    auto const write = [&seamCalls](TWriteCall const& /*call*/)
        {
            ++seamCalls;

            return TASWIniResult();
        };
    ini.OnRead = read;
    ini.OnWrite = write;

    // Act
    TASWIniResult const load = ini.Load("");
    TASWIniResult const saveAs = ini.SaveAs("");

    // Assert
    CheckEquals(EIniCode::InvalidArgument, load.Code, __func__, __LINE__,
        "Load() of an empty path should be InvalidArgument");
    CheckEquals(EIniCode::InvalidArgument, saveAs.Code, __func__, __LINE__,
        "SaveAs() of an empty path should be InvalidArgument");
    CheckEquals(0, seamCalls, __func__, __LINE__, "no file should be read or written");
    CheckEquals(before, ini.ToText(), __func__, __LINE__, "the document should be unchanged");
    CheckTrue(ini.IsModified(), __func__, __LINE__, "the modified flag should be unchanged");
    CheckEquals(std::string(), ini.GetFilePath(), __func__, __LINE__, "there should still be no file path");
    CheckEquals(std::size_t(2), messages.size(), __func__, __LINE__, "both failures should be reported");
    for (TASWIniMessage const& message : messages)
        CheckEquals(ESeverity::Error, message.Severity, __func__, __LINE__, "a failure should be an Error");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_BaseFile, FileOperations_DontOverlap)
{
    // Arrange
    // While a load reads (or a save writes) the file, it starts a save (or a load) on another thread and gives it time
    // to get to its own file access, which has to wait until the first operation is done
    TSeamIni ini;
    std::atomic<int> inside{ 0 };
    std::atomic<bool> overlapped{ false };
    std::function<void()> during;
    auto const access = [&]()
        {
            if (inside.fetch_add(1) != 0)
                overlapped = true;

            if (during)
            {
                std::function<void()> const start = during;
                during = nullptr;
                start();
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }

            --inside;
        };
    auto const read = [&access](TReadCall const& call)
        {
            access();
            call.Contents = SampleText;

            return TASWIniResult();
        };
    auto const write = [&access](TWriteCall const& /*call*/)
        {
            access();

            return TASWIniResult();
        };
    ini.OnRead = read;
    ini.OnWrite = write;
    std::thread other;
    auto const saveAs = [&ini]()
        {
            static_cast<void>(ini.SaveAs("app.ini"));
        };
    auto const load = [&ini]()
        {
            static_cast<void>(ini.Load("app.ini"));
        };
    auto const startSaveAs = [&other, &saveAs]()
        {
            other = std::thread(saveAs);
        };
    auto const startLoad = [&other, &load]()
        {
            other = std::thread(load);
        };

    // Act
    during = startSaveAs;
    TASWIniResult const loaded = ini.Load("app.ini");
    other.join();
    bool const saveWaited = !overlapped;
    during = startLoad;
    TASWIniResult const saved = ini.Save();
    other.join();
    bool const loadWaited = !overlapped;

    // Assert
    CheckTrue(loaded.IsOk() && saved.IsOk(), __func__, __LINE__, "the file should be loaded and saved");
    CheckTrue(saveWaited, __func__, __LINE__, "a save should wait for a load to finish");
    CheckTrue(loadWaited, __func__, __LINE__, "a load should wait for a save to finish");
    CheckEquals(std::string("app.ini"), ini.GetFilePath(), __func__, __LINE__, "the file path should be set");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_BaseFile, Load_FileErrorsChangeNothing)
{
    // Arrange
    TTempDir const folder;
    AssertFalse(folder.Path().empty(), __func__, __LINE__, "the test folder should be created");
    std::filesystem::path const large = folder.Path() / "large.ini";
    std::filesystem::path const utf16 = folder.Path() / "utf16.ini";
    AssertTrue(WriteTestFile(large, std::string(101, ';')), __func__, __LINE__, "the large file should be written");
    AssertTrue(WriteTestFile(utf16, std::string("\xFF\xFE[\0A\0]\0", 8)), __func__, __LINE__,
        "the UTF-16 file should be written");
    TASWBaseIni ini;
    TASWIniOptions options;
    options.Reading.MaxFileSize = 100;
    static_cast<void>(ini.SetOptions(options));
    static_cast<void>(ini.LoadText(SampleText));
    static_cast<void>(ini.SetText("Window", "Width", "800"));
    std::string const before = ini.ToText();
    std::vector<TASWIniMessage> messages;
    ini.SetLogCallback(Recorder(messages));

    // Act
    TASWIniResult const results[] = {
        ini.Load(PathToUtf8(large)), ini.Load(PathToUtf8(folder.Path())), ini.Load(PathToUtf8(utf16)),
    };

    // Assert
    // Windows can't open a folder as a file (AccessDenied); elsewhere it opens, but isn't a file (OpenFailed)
    CheckEquals(EIniCode::FileTooLarge, results[0].Code, __func__, __LINE__,
        "a file larger than MaxFileSize should be FileTooLarge");
    CheckTrue(results[1].Code == EIniCode::AccessDenied || results[1].Code == EIniCode::OpenFailed, __func__,
        __LINE__, "a folder should be AccessDenied or OpenFailed");
    CheckEquals(EIniCode::UnsupportedEncoding, results[2].Code, __func__, __LINE__,
        "a UTF-16 file should be UnsupportedEncoding");
    CheckEquals(PathToUtf8(utf16), results[2].FilePath, __func__, __LINE__,
        "the failure to parse the file should name it");
    CheckEquals(before, ini.ToText(), __func__, __LINE__, "the document should be unchanged");
    CheckTrue(ini.IsModified(), __func__, __LINE__, "the modified flag should be unchanged");
    CheckEquals(std::string(), ini.GetFilePath(), __func__, __LINE__, "there should still be no file path");
    CheckEquals(std::size_t(3), messages.size(), __func__, __LINE__, "each failure should be reported");
    for (std::size_t i = 0; i < messages.size() && i < 3; ++i)
    {
        std::string const which = " (load " + std::to_string(i) + ")";
        CheckEquals(ESeverity::Error, messages[i].Severity, __func__, __LINE__,
            "the failure should be an Error" + which);
        CheckEquals(results[i].Code, messages[i].Code, __func__, __LINE__, "the message should have the code" + which);
        CheckNotEmpty(messages[i].FilePath, __func__, __LINE__, "the message should name the file" + which);
        CheckEquals(results[i].FilePath, messages[i].FilePath, __func__, __LINE__,
            "the message should have the result's file path" + which);
        CheckEquals(results[i].SystemError, messages[i].SystemError, __func__, __LINE__,
            "the message should have the result's system error" + which);
    }
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_BaseFile, Load_MissingFileEmptiesTheDocument)
{
    // Arrange
    TTempDir const folder;
    AssertFalse(folder.Path().empty(), __func__, __LINE__, "the test folder should be created");
    std::string const path = PathToUtf8(folder.Path() / "app.ini");
    TASWBaseIni ini;
    TASWIniOptions options;
    options.Reading.InlineComments = true;
    static_cast<void>(ini.SetOptions(options));
    static_cast<void>(ini.LoadText(SampleText));
    static_cast<void>(ini.SetText("Window", "Width", "800"));
    std::vector<TASWIniMessage> messages;
    ini.SetLogCallback(Recorder(messages));

    // Act
    TASWIniResult const result = ini.Load(path);

    // Assert
    CheckEquals(EIniCode::FileNotFound, result.Code, __func__, __LINE__, "a missing file should be FileNotFound");
    CheckNotEquals(0, result.SystemError, __func__, __LINE__, "the system's error code should be kept");
    CheckEquals(std::string(), ini.ToText(), __func__, __LINE__, "the document should be emptied");
    CheckFalse(ini.IsModified(), __func__, __LINE__, "the emptied document shouldn't be modified");
    CheckEquals(path, ini.GetFilePath(), __func__, __LINE__, "the missing file should become the file path");
    CheckTrue(ini.GetOptions().Reading.InlineComments, __func__, __LINE__, "the options should be kept");
    CheckEquals(std::size_t(1), messages.size(), __func__, __LINE__, "the missing file should be reported once");
    if (messages.size() == 1)
    {
        CheckEquals(ESeverity::Warning, messages[0].Severity, __func__, __LINE__, "a missing file should be a Warning");
        CheckEquals(EIniCode::FileNotFound, messages[0].Code, __func__, __LINE__, "the warning should be FileNotFound");
        CheckEquals(result.Message, messages[0].Text, __func__, __LINE__, "the warning should have the result's text");
        CheckEquals(path, messages[0].FilePath, __func__, __LINE__, "the warning should name the file");
        CheckEquals(result.SystemError, messages[0].SystemError, __func__, __LINE__,
            "the warning should have the system error");
    }

    // Act
    static_cast<void>(ini.SetText("General", "Name", "My App"));
    TASWIniResult const saved = ini.Save();

    // Assert
    std::string contents;
    CheckTrue(saved.IsOk(), __func__, __LINE__, "Save() should create the file: " + saved.Message);
    CheckTrue(ReadTestFile(folder.Path() / "app.ini", contents), __func__, __LINE__, "the file should exist");
    CheckEquals(ini.ToText(), contents, __func__, __LINE__, "the file should have the document's text");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_BaseFile, Load_MissingFolderIsCreatedBySave)
{
    // Arrange
    TTempDir const folder;
    AssertFalse(folder.Path().empty(), __func__, __LINE__, "the test folder should be created");
    std::filesystem::path const file = folder.Path() / "Company" / "App" / "app.ini";
    TASWBaseIni ini;

    // Act
    TASWIniResult const loaded = ini.Load(PathToUtf8(file));
    static_cast<void>(ini.SetText("General", "Name", "My App"));
    TASWIniResult const saved = ini.Save();

    // Assert
    std::string contents;
    CheckEquals(EIniCode::FileNotFound, loaded.Code, __func__, __LINE__, "a missing folder should be FileNotFound");
    CheckTrue(saved.IsOk(), __func__, __LINE__, "Save() should create the folders and the file: " + saved.Message);
    CheckTrue(ReadTestFile(file, contents), __func__, __LINE__, "the file should exist");
    CheckEquals(ini.ToText(), contents, __func__, __LINE__, "the file should have the document's text");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_BaseFile, Load_ReadsTheFile)
{
    // Arrange
    // The path as an application may give it, with '/' on Windows too
    TTempDir const folder;
    AssertFalse(folder.Path().empty(), __func__, __LINE__, "the test folder should be created");
    AssertTrue(WriteTestFile(folder.Path() / "app.ini", WarningsText), __func__, __LINE__,
        "the test file should be written");
    std::string const path = PathToUtf8(folder.Path()) + "/app.ini";
    TASWBaseIni ini;
    static_cast<void>(ini.SetText("Old", "Key", "Value"));
    std::vector<TASWIniMessage> messages;
    ini.SetLogCallback(Recorder(messages));

    // Act
    TASWIniResult const result = ini.Load(path);
    TASWIniResult const failedEdit = ini.RenameSection("Missing", "Other");

    // Assert
    CheckTrue(result.IsOk(), __func__, __LINE__, "the file should be loaded: " + result.Message);
    CheckEquals(std::string(), result.FilePath, __func__, __LINE__, "a result that is Ok should be only that");
    CheckEquals(std::string(WarningsText), ini.ToText(), __func__, __LINE__, "the document should have the file's text");
    CheckFalse(ini.IsModified(), __func__, __LINE__, "a loaded document shouldn't be modified");
    CheckEquals(path, ini.GetFilePath(), __func__, __LINE__, "the file path should be the path as it was given");
    CheckEquals(EIniCode::NotFound, failedEdit.Code, __func__, __LINE__, "the rename should fail");
    CheckEquals(std::size_t(4), messages.size(), __func__, __LINE__,
        "the file's three warnings and the failed edit should be reported");
    for (TASWIniMessage const& message : messages)
        CheckEquals(path, message.FilePath, __func__, __LINE__, "each message should name the file: " + message.Text);

    if (messages.size() == 4)
    {
        CheckEquals(ESeverity::Warning, messages[0].Severity, __func__, __LINE__, "the first should be a Warning");
        CheckEquals(EIniCode::MalformedLine, messages[0].Code, __func__, __LINE__, "the first should be MalformedLine");
        CheckEquals(ESeverity::Error, messages[3].Severity, __func__, __LINE__, "the failed edit should be an Error");
    }
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_BaseFile, Load_ReadsWithoutTheLock)
{
    // Arrange
    // A lookup or an edit made while the file is read would wait forever if the document's lock were held (the CI's
    // test timeout turns that into a failure)
    TSeamIni ini;
    static_cast<void>(ini.LoadText(SampleText));
    std::string widthDuringLoad;
    TASWIniResult editDuringLoad;
    auto const read = [&](TReadCall const& call)
        {
            static_cast<void>(ini.TryGetText("Window", "Width", widthDuringLoad));
            editDuringLoad = ini.SetText("Window", "Width", "800");
            call.Contents = "[Loaded]\nKey = 1\n";

            return TASWIniResult();
        };
    ini.OnRead = read;

    // Act
    TASWIniResult const result = ini.Load("app.ini");

    // Assert
    CheckTrue(result.IsOk(), __func__, __LINE__, "the file should be loaded: " + result.Message);
    CheckEquals(std::string("640"), widthDuringLoad, __func__, __LINE__,
        "a lookup during the load should see the old document");
    CheckTrue(editDuringLoad.IsOk(), __func__, __LINE__, "an edit during the load should succeed");
    CheckEquals(std::string("[Loaded]\nKey = 1\n"), ini.ToText(), __func__, __LINE__,
        "the loaded file should replace the edited document");
    CheckFalse(ini.IsModified(), __func__, __LINE__, "a loaded document shouldn't be modified");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_BaseFile, OutOfMemory_LoadChangesNothing)
{
    // Arrange
    if (!CanFailAllocations())
        Skip(__func__, __LINE__, "allocations can't be made to fail in this build (see CanFailAllocations())");

    TTempDir const folder;
    AssertFalse(folder.Path().empty(), __func__, __LINE__, "the test folder should be created");
    AssertTrue(WriteTestFile(folder.Path() / "app.ini", SampleText), __func__, __LINE__,
        "the test file should be written");
    std::string const path = PathToUtf8(folder.Path() / "app.ini");
    std::size_t failures = 0;

    // Act & Assert
    // Each allocation of the load fails in turn, on a new object, until the load makes no more. A failed allocation
    // fails the load, or, when the standard library copes with it (bcc32c's has a few such allocations), doesn't.
    for (std::size_t number = 1; number < 10000; ++number)
    {
        TASWBaseIni ini;
        static_cast<void>(ini.SetText("Old", "Key", "Value"));
        std::string const before = ini.ToText();
        std::string const which = " (allocation " + std::to_string(number) + ")";
        TASWIniResult result;
        bool failed = false;
        {
            TFailingAlloc failingAlloc(number);
            result = ini.Load(path);
            failed = failingAlloc.HasFailed();
        }

        if (result.IsOk())
        {
            CheckEquals(std::string(SampleText), ini.ToText(), __func__, __LINE__,
                "a load that succeeded should have loaded the file" + which);
            CheckEquals(path, ini.GetFilePath(), __func__, __LINE__,
                "a load that succeeded should set the file path" + which);
            CheckFalse(ini.IsModified(), __func__, __LINE__, "a loaded document shouldn't be modified" + which);
        }
        else
        {
            ++failures;
            CheckTrue(failed, __func__, __LINE__, "the load should only fail when an allocation fails" + which);
            CheckEquals(EIniCode::UnexpectedException, result.Code, __func__, __LINE__,
                "a failed allocation should be UnexpectedException" + which);
            CheckEquals(before, ini.ToText(), __func__, __LINE__, "a failed load should change nothing" + which);
            CheckEquals(std::string(), ini.GetFilePath(), __func__, __LINE__,
                "a failed load should keep the file path" + which);
            CheckTrue(ini.IsModified(), __func__, __LINE__, "a failed load should keep the modified flag" + which);
        }

        if (!failed)
            break;
    }

    CheckGreaterThan(failures, std::size_t(0), __func__, __LINE__, "at least one allocation should have failed");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_BaseFile, OutOfMemory_SaveChangesNothing)
{
    // Arrange
    if (!CanFailAllocations())
        Skip(__func__, __LINE__, "allocations can't be made to fail in this build (see CanFailAllocations())");

    TTempDir const folder;
    AssertFalse(folder.Path().empty(), __func__, __LINE__, "the test folder should be created");
    std::filesystem::path const file = folder.Path() / "app.ini";
    std::size_t failures = 0;

    // Act & Assert
    // As OutOfMemory_LoadChangesNothing does, with two allocations in a row failing: bcc32c's library (Dinkumware)
    // retries a string allocation that fails, and a save only allocates strings. The save is atomic, so a failed one
    // leaves the file as it was.
    for (std::size_t number = 1; number < 10000; ++number)
    {
        AssertTrue(WriteTestFile(file, SampleText), __func__, __LINE__, "the test file should be written");
        TASWBaseIni ini;
        static_cast<void>(ini.Load(PathToUtf8(file)));
        static_cast<void>(ini.SetText("Window", "Width", "800"));
        std::string const text = ini.ToText();
        std::string const which = " (allocation " + std::to_string(number) + ")";
        TASWIniResult result;
        bool failed = false;
        {
            TFailingAlloc failingAlloc(number, 2);
            result = ini.Save();
            failed = failingAlloc.HasFailed();
        }

        std::string contents;
        CheckTrue(ReadTestFile(file, contents), __func__, __LINE__, "the file should be readable" + which);
        if (result.IsOk())
        {
            CheckEquals(text, contents, __func__, __LINE__, "a save that succeeded should write the text" + which);
            CheckFalse(ini.IsModified(), __func__, __LINE__, "a saved document shouldn't be modified" + which);
        }
        else
        {
            ++failures;
            CheckTrue(failed, __func__, __LINE__, "the save should only fail when an allocation fails" + which);
            CheckEquals(EIniCode::UnexpectedException, result.Code, __func__, __LINE__,
                "a failed allocation should be UnexpectedException" + which);
            CheckEquals(std::string(SampleText), contents, __func__, __LINE__,
                "a failed save should leave the file as it was" + which);
            CheckTrue(ini.IsModified(), __func__, __LINE__, "a failed save should keep the modified flag" + which);
        }

        if (!failed)
            break;
    }

    CheckGreaterThan(failures, std::size_t(0), __func__, __LINE__, "at least one allocation should have failed");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_BaseFile, SaveAs_SetsTheFilePath)
{
    // Arrange
    TTempDir const folder;
    AssertFalse(folder.Path().empty(), __func__, __LINE__, "the test folder should be created");
    std::string const first = PathToUtf8(folder.Path() / "first.ini");
    std::string const inMissingFolder = PathToUtf8(folder.Path() / "No" / "second.ini");
    TASWBaseIni ini;
    TASWIniOptions options;
    options.Saving.CreateFolders = false;
    static_cast<void>(ini.SetOptions(options));
    static_cast<void>(ini.LoadText(SampleText));
    std::vector<TASWIniMessage> messages;
    ini.SetLogCallback(Recorder(messages));

    // Act
    TASWIniResult const saved = ini.SaveAs(first);
    std::string const pathAfterSave = ini.GetFilePath();
    static_cast<void>(ini.SetText("Window", "Width", "800"));
    TASWIniResult const failed = ini.SaveAs(inMissingFolder);
    std::string const pathAfterFailure = ini.GetFilePath();
    bool const modifiedAfterFailure = ini.IsModified();
    TASWIniResult const savedAgain = ini.Save();

    // Assert
    std::string contents;
    CheckTrue(saved.IsOk(), __func__, __LINE__, "SaveAs() should write the file: " + saved.Message);
    CheckEquals(first, pathAfterSave, __func__, __LINE__, "the file should become the file path");
    CheckEquals(EIniCode::FileNotFound, failed.Code, __func__, __LINE__,
        "a missing folder without CreateFolders should be FileNotFound");
    CheckEquals(first, pathAfterFailure, __func__, __LINE__, "a failed SaveAs() should keep the file path");
    CheckTrue(modifiedAfterFailure, __func__, __LINE__, "a failed SaveAs() should keep the modified flag");
    CheckTrue(savedAgain.IsOk(), __func__, __LINE__, "Save() should write the file path: " + savedAgain.Message);
    CheckFalse(ini.IsModified(), __func__, __LINE__, "a saved document shouldn't be modified");
    CheckTrue(ReadTestFile(folder.Path() / "first.ini", contents), __func__, __LINE__, "the file should exist");
    CheckEquals(ini.ToText(), contents, __func__, __LINE__, "the file should have the document's text");
    CheckEquals(std::size_t(1), messages.size(), __func__, __LINE__, "only the failure should be reported");
    if (messages.size() == 1)
    {
        CheckEquals(ESeverity::Error, messages[0].Severity, __func__, __LINE__, "the failure should be an Error");
        CheckEquals(failed.FilePath, messages[0].FilePath, __func__, __LINE__,
            "the failure should name the file it couldn't write");
        CheckNotEquals(first, messages[0].FilePath, __func__, __LINE__,
            "the failure shouldn't name the object's file path");
    }
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_BaseFile, Save_EditDuringSaveKeepsTheModifiedFlag)
{
    // Arrange
    // WriteFile() makes an edit before it writes the text Save() made, so the file doesn't have the edit
    TTempDir const folder;
    AssertFalse(folder.Path().empty(), __func__, __LINE__, "the test folder should be created");
    std::filesystem::path const file = folder.Path() / "app.ini";
    TSeamIni ini;
    static_cast<void>(ini.LoadText(SampleText));
    static_cast<void>(ini.SetText("Window", "Width", "800"));
    std::function<void()> edit;
    auto const write = [&](TWriteCall const& call)
        {
            edit();

            return ini.DefaultWrite(call);
        };
    ini.OnWrite = write;
    auto const setKey = [&ini]()
        {
            static_cast<void>(ini.SetText("Window", "Height", "600"));
        };
    auto const removeKey = [&ini]()
        {
            static_cast<void>(ini.RemoveKey("Window.Tools", "Visible"));
        };
    auto const removeSection = [&ini]()
        {
            static_cast<void>(ini.RemoveSection("General"));
        };
    auto const failedEdit = [&ini]()
        {
            static_cast<void>(ini.RenameSection("Missing", "Other"));
        };
    std::string contents;

    // Act
    edit = setKey;
    std::string const textOfFirstSave = ini.ToText();
    TASWIniResult const first = ini.SaveAs(PathToUtf8(file));

    // Assert
    CheckTrue(first.IsOk(), __func__, __LINE__, "the file should be saved: " + first.Message);
    CheckTrue(ini.IsModified(), __func__, __LINE__, "a key set during the save should keep the modified flag");
    CheckTrue(ReadTestFile(file, contents), __func__, __LINE__, "the file should exist");
    CheckEquals(textOfFirstSave, contents, __func__, __LINE__, "the file should have the text before the edit");
    CheckEquals(std::string("600"), ini.GetValue("Window", "Height", ""), __func__, __LINE__,
        "the document should have the edit");

    // Act
    edit = removeKey;
    std::string const textOfSecondSave = ini.ToText();
    TASWIniResult const second = ini.Save();

    // Assert
    CheckTrue(second.IsOk(), __func__, __LINE__, "the file should be saved: " + second.Message);
    CheckTrue(ini.IsModified(), __func__, __LINE__, "a key removed during the save should keep the modified flag");
    CheckTrue(ReadTestFile(file, contents), __func__, __LINE__, "the file should exist");
    CheckEquals(textOfSecondSave, contents, __func__, __LINE__, "the file should have the text before the removal");

    // Act
    edit = removeSection;
    TASWIniResult const third = ini.Save();

    // Assert
    CheckTrue(third.IsOk(), __func__, __LINE__, "the file should be saved: " + third.Message);
    CheckTrue(ini.IsModified(), __func__, __LINE__,
        "a section removed during the save should keep the modified flag");

    // Act
    edit = failedEdit;
    TASWIniResult const fourth = ini.Save();

    // Assert
    CheckTrue(fourth.IsOk(), __func__, __LINE__, "the file should be saved: " + fourth.Message);
    CheckFalse(ini.IsModified(), __func__, __LINE__,
        "an edit that failed during the save changed nothing, so the flag should be cleared");
    CheckTrue(ReadTestFile(file, contents), __func__, __LINE__, "the file should exist");
    CheckEquals(ini.ToText(), contents, __func__, __LINE__, "the file should have the document's text");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_BaseFile, Save_NoFilePathFails)
{
    // Arrange
    TASWBaseIni ini;
    static_cast<void>(ini.LoadText(SampleText));
    static_cast<void>(ini.SetText("Window", "Width", "800"));
    std::vector<TASWIniMessage> messages;
    ini.SetLogCallback(Recorder(messages));

    // Act
    TASWIniResult const result = ini.Save();

    // Assert
    CheckEquals(EIniCode::NoFilePath, result.Code, __func__, __LINE__, "Save() without a file path should fail");
    CheckTrue(ini.IsModified(), __func__, __LINE__, "the modified flag should be unchanged");
    CheckEquals(std::size_t(1), messages.size(), __func__, __LINE__, "the failure should be reported");
    if (messages.size() == 1)
    {
        CheckEquals(ESeverity::Error, messages[0].Severity, __func__, __LINE__, "the failure should be an Error");
        CheckEquals(EIniCode::NoFilePath, messages[0].Code, __func__, __LINE__, "the message should be NoFilePath");
        CheckEquals(result.Message, messages[0].Text, __func__, __LINE__, "the message should have the result's text");
        CheckEquals(std::string(), messages[0].FilePath, __func__, __LINE__, "the message should name no file");
    }
}
//---------------------------------------------------------------------------
#if defined(_WIN32)
ASW_TEST(TTest_ASWIni_BaseFile, Save_ReadOnlyFileChangesNothing)
{
    // Arrange
    TTempDir const folder;
    AssertFalse(folder.Path().empty(), __func__, __LINE__, "the test folder should be created");
    std::filesystem::path const file = folder.Path() / "readonly.ini";
    AssertTrue(WriteTestFile(file, SampleText), __func__, __LINE__, "the test file should be written");
    std::string const path = PathToUtf8(file);
    TASWBaseIni ini;
    static_cast<void>(ini.Load(path));
    static_cast<void>(ini.SetText("Window", "Width", "800"));
    AssertTrue(::SetFileAttributesW(file.c_str(), FILE_ATTRIBUTE_READONLY) != 0, __func__, __LINE__,
        "the file should be made read-only");
    std::vector<TASWIniMessage> messages;
    ini.SetLogCallback(Recorder(messages));

    // Act
    TASWIniResult const result = ini.Save();
    std::string contents;
    bool const read = ReadTestFile(file, contents);

    // So the folder can be deleted
    ::SetFileAttributesW(file.c_str(), FILE_ATTRIBUTE_NORMAL);

    // Assert
    CheckEquals(EIniCode::ReplaceFailed, result.Code, __func__, __LINE__, "the code should be ReplaceFailed");
    CheckEquals(static_cast<int>(ERROR_ACCESS_DENIED), result.SystemError, __func__, __LINE__,
        "the system error should be access denied");
    CheckTrue(read, __func__, __LINE__, "the file should still be readable");
    CheckEquals(std::string(SampleText), contents, __func__, __LINE__, "the file should be unchanged");
    CheckTrue(ini.IsModified(), __func__, __LINE__, "the modified flag should be unchanged");
    CheckEquals(path, ini.GetFilePath(), __func__, __LINE__, "the file path should be unchanged");
    CheckEquals(std::size_t(1), messages.size(), __func__, __LINE__, "the failure should be reported");
    if (messages.size() == 1)
    {
        CheckEquals(ESeverity::Error, messages[0].Severity, __func__, __LINE__, "the failure should be an Error");
        CheckEquals(path, messages[0].FilePath, __func__, __LINE__, "the message should name the file");
        CheckEquals(result.SystemError, messages[0].SystemError, __func__, __LINE__,
            "the message should have the system error");
    }
}
#endif
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_BaseFile, Save_RoundTripsByteForByte)
{
    // Arrange
    TTempDir const folder;
    AssertFalse(folder.Path().empty(), __func__, __LINE__, "the test folder should be created");
    std::filesystem::path const source = folder.Path() / "source.ini";
    std::filesystem::path const copy = folder.Path() / "copy.ini";
    AssertTrue(WriteTestFile(source, TrickyText), __func__, __LINE__, "the test file should be written");
    TASWBaseIni ini;

    // Act
    TASWIniResult const loaded = ini.Load(PathToUtf8(source));
    TASWIniResult const saved = ini.Save();
    TASWIniResult const savedAs = ini.SaveAs(PathToUtf8(copy));

    // Assert
    std::string sourceContents;
    std::string copyContents;
    CheckTrue(loaded.IsOk(), __func__, __LINE__, "the file should be loaded: " + loaded.Message);
    CheckTrue(saved.IsOk(), __func__, __LINE__, "the file should be saved: " + saved.Message);
    CheckTrue(savedAs.IsOk(), __func__, __LINE__, "the copy should be saved: " + savedAs.Message);
    CheckTrue(ReadTestFile(source, sourceContents), __func__, __LINE__, "the file should be readable");
    CheckTrue(ReadTestFile(copy, copyContents), __func__, __LINE__, "the copy should be readable");
    CheckEquals(std::string(TrickyText), sourceContents, __func__, __LINE__,
        "the saved file should have the bytes it was loaded from");
    CheckEquals(std::string(TrickyText), copyContents, __func__, __LINE__,
        "the copy should have the bytes the file was loaded from");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_BaseFile, Save_WarningsAreReported)
{
    // Arrange
    // A folder where the backup should go, so the backup fails; and WriteFile() adds a warning that names no file
    TTempDir const folder;
    AssertFalse(folder.Path().empty(), __func__, __LINE__, "the test folder should be created");
    std::filesystem::path const file = folder.Path() / "app.ini";
    AssertTrue(WriteTestFile(file, SampleText), __func__, __LINE__, "the test file should be written");
    std::error_code error;
    AssertTrue(std::filesystem::create_directory(folder.Path() / "app.ini.bak", error), __func__, __LINE__,
        "the folder where the backup should go should be created");
    std::string const path = PathToUtf8(file);
    TSeamIni ini;
    TASWIniOptions options;
    options.Saving.Backup = true;
    static_cast<void>(ini.SetOptions(options));
    static_cast<void>(ini.Load(path));
    std::vector<TASWIniMessage> messages;
    ini.SetLogCallback(Recorder(messages));
    auto const write = [&ini](TWriteCall const& call)
        {
            TASWIniMessage warning;
            warning.Severity = ESeverity::Warning;
            warning.Code = EIniCode::TempFileCleanupFailed;
            warning.Text = "A warning that names no file.";
            call.Warnings.push_back(warning);

            return ini.DefaultWrite(call);
        };

    // Act
    static_cast<void>(ini.SetText("Window", "Width", "800"));
    TASWIniResult const saved = ini.Save();
    std::size_t const messagesOfFirstSave = messages.size();
    ini.OnWrite = write;
    TASWIniResult const savedAgain = ini.Save();

    // Assert
    std::string contents;
    CheckTrue(saved.IsOk(), __func__, __LINE__, "a failed backup shouldn't fail the save: " + saved.Message);
    CheckTrue(savedAgain.IsOk(), __func__, __LINE__, "the second save should succeed: " + savedAgain.Message);
    CheckFalse(ini.IsModified(), __func__, __LINE__, "a saved document shouldn't be modified");
    CheckTrue(ReadTestFile(file, contents), __func__, __LINE__, "the file should be readable");
    CheckEquals(ini.ToText(), contents, __func__, __LINE__, "the file should have the document's text");
    CheckEquals(std::size_t(1), messagesOfFirstSave, __func__, __LINE__, "the failed backup should be reported");
    CheckEquals(std::size_t(3), messages.size(), __func__, __LINE__,
        "the second save should report its two warnings");
    if (messages.size() == 3)
    {
        CheckEquals(ESeverity::Warning, messages[0].Severity, __func__, __LINE__, "the backup should be a Warning");
        CheckEquals(EIniCode::BackupFailed, messages[0].Code, __func__, __LINE__, "the warning should be BackupFailed");
        CheckEquals(path + ".bak", messages[0].FilePath, __func__, __LINE__,
            "the warning should keep the file it names, the backup");
        CheckEquals(EIniCode::TempFileCleanupFailed, messages[1].Code, __func__, __LINE__,
            "WriteFile()'s warning should be reported in order");
        CheckEquals(path, messages[1].FilePath, __func__, __LINE__,
            "a warning that names no file should name the saved file");
        CheckEquals(EIniCode::BackupFailed, messages[2].Code, __func__, __LINE__,
            "the second failed backup should be reported");
    }
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_BaseFile, Save_WritesAndClearsTheModifiedFlag)
{
    // Arrange
    TTempDir const folder;
    AssertFalse(folder.Path().empty(), __func__, __LINE__, "the test folder should be created");
    std::filesystem::path const file = folder.Path() / "app.ini";
    AssertTrue(WriteTestFile(file, SampleText), __func__, __LINE__, "the test file should be written");
    TASWBaseIni ini;
    static_cast<void>(ini.Load(PathToUtf8(file)));
    static_cast<void>(ini.SetText("Window", "Width", "800"));
    std::string contents;

    // Act
    TASWIniResult const saved = ini.Save();

    // Assert
    CheckTrue(saved.IsOk(), __func__, __LINE__, "the file should be saved: " + saved.Message);
    CheckEquals(std::string(), saved.FilePath, __func__, __LINE__, "a result that is Ok should be only that");
    CheckFalse(ini.IsModified(), __func__, __LINE__, "a saved document shouldn't be modified");
    CheckTrue(ReadTestFile(file, contents), __func__, __LINE__, "the file should be readable");
    CheckEquals(ini.ToText(), contents, __func__, __LINE__, "the file should have the document's text");
    CheckContains(contents, std::string("Width = 800"), __func__, __LINE__, "the file should have the edit");

    // Act
    // A document that isn't modified is saved too
    AssertTrue(WriteTestFile(file, "Changed by another program"), __func__, __LINE__, "the file should be changed");
    TASWIniResult const savedAgain = ini.Save();

    // Assert
    CheckTrue(savedAgain.IsOk(), __func__, __LINE__, "the file should be saved again: " + savedAgain.Message);
    CheckTrue(ReadTestFile(file, contents), __func__, __LINE__, "the file should be readable");
    CheckEquals(ini.ToText(), contents, __func__, __LINE__, "the file should have the document's text again");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_BaseFile, Seams_ExceptionIsUnexpectedException)
{
    // Arrange
    TSeamIni ini;
    static_cast<void>(ini.LoadText(SampleText));
    static_cast<void>(ini.SetText("Window", "Width", "800"));
    std::string const before = ini.ToText();
    std::vector<TASWIniMessage> messages;
    ini.SetLogCallback(Recorder(messages));
    auto const read = [](TReadCall const& /*call*/) -> TASWIniResult
        {
            throw std::runtime_error("The disk is gone.");
        };
    auto const write = [](TWriteCall const& /*call*/) -> TASWIniResult
        {
            throw 42;
        };
    ini.OnRead = read;
    ini.OnWrite = write;

    // Act
    TASWIniResult const load = ini.Load("app.ini");
    TASWIniResult const saveAs = ini.SaveAs("app.ini");

    // Assert
    CheckEquals(EIniCode::UnexpectedException, load.Code, __func__, __LINE__,
        "an exception from ReadFile() should be UnexpectedException");
    CheckContains(load.Message, std::string("The disk is gone."), __func__, __LINE__,
        "the message should say what was thrown");
    CheckEquals(EIniCode::UnexpectedException, saveAs.Code, __func__, __LINE__,
        "an exception from WriteFile() should be UnexpectedException");
    CheckEquals(before, ini.ToText(), __func__, __LINE__, "the document should be unchanged");
    CheckTrue(ini.IsModified(), __func__, __LINE__, "the modified flag should be unchanged");
    CheckEquals(std::string(), ini.GetFilePath(), __func__, __LINE__, "there should still be no file path");
    CheckEquals(std::size_t(2), messages.size(), __func__, __LINE__, "both failures should be reported");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_BaseFile, Seams_FailuresChangeNothing)
{
    // Arrange
    // The seams return 'next'; a failure without a file path gets the path of the file the operation was about
    TSeamIni ini;
    TASWIniResult next;
    auto const read = [&next](TReadCall const& call)
        {
            call.Contents = "[New]\n";

            return next;
        };
    auto const write = [&next](TWriteCall const& /*call*/)
        {
            return next;
        };
    ini.OnRead = read;
    ini.OnWrite = write;
    static_cast<void>(ini.LoadText(SampleText));
    static_cast<void>(ini.SaveAs("app.ini"));
    static_cast<void>(ini.SetText("Window", "Width", "800"));
    std::string const before = ini.ToText();
    std::vector<TASWIniMessage> messages;
    ini.SetLogCallback(Recorder(messages));

    // Act
    next = SeamFailure(EIniCode::WriteFailed, "", 0);
    TASWIniResult const writeFailed = ini.Save();
    next = SeamFailure(EIniCode::ReplaceFailed, "app.ini.tmp", 5);
    TASWIniResult const replaceFailed = ini.SaveAs("other.ini");
    next = SeamFailure(EIniCode::ReadFailed, "", 0);
    TASWIniResult const readFailed = ini.Load("other.ini");

    // Assert
    CheckEquals(EIniCode::WriteFailed, writeFailed.Code, __func__, __LINE__, "Save() should fail as WriteFile() did");
    CheckEquals(std::string("app.ini"), writeFailed.FilePath, __func__, __LINE__,
        "the failed Save() should name the file path");
    CheckEquals(EIniCode::ReplaceFailed, replaceFailed.Code, __func__, __LINE__,
        "SaveAs() should fail as WriteFile() did");
    CheckEquals(std::string("app.ini.tmp"), replaceFailed.FilePath, __func__, __LINE__,
        "a failure that names a file should keep it");
    CheckEquals(EIniCode::ReadFailed, readFailed.Code, __func__, __LINE__, "Load() should fail as ReadFile() did");
    CheckEquals(std::string("other.ini"), readFailed.FilePath, __func__, __LINE__,
        "the failed Load() should name the file it read");
    CheckEquals(before, ini.ToText(), __func__, __LINE__, "the document should be unchanged");
    CheckTrue(ini.IsModified(), __func__, __LINE__, "the modified flag should be unchanged");
    CheckEquals(std::string("app.ini"), ini.GetFilePath(), __func__, __LINE__, "the file path should be unchanged");
    CheckEquals(std::size_t(3), messages.size(), __func__, __LINE__, "each failure should be reported");
    if (messages.size() == 3)
    {
        CheckEquals(std::string("app.ini"), messages[0].FilePath, __func__, __LINE__,
            "the failed Save() should be reported with the file path");
        CheckEquals(std::string("app.ini.tmp"), messages[1].FilePath, __func__, __LINE__,
            "the failed SaveAs() should be reported with the file it names");
        CheckEquals(5, messages[1].SystemError, __func__, __LINE__, "the system error should be reported");
        CheckEquals(std::string("other.ini"), messages[2].FilePath, __func__, __LINE__,
            "the failed Load() should be reported with the file it read, not the file path");
    }
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_BaseFile, Seams_FileNotFoundIsAMissingFile)
{
    // Arrange
    // ReadFile() fails with FileNotFound for a file it names itself, and gives contents that aren't used
    TSeamIni ini;
    static_cast<void>(ini.LoadText(SampleText));
    std::vector<TASWIniMessage> messages;
    ini.SetLogCallback(Recorder(messages));
    auto const read = [](TReadCall const& call)
        {
            call.Contents = "[Unused]\n";

            return SeamFailure(EIniCode::FileNotFound, "elsewhere.ini", 2);
        };
    ini.OnRead = read;

    // Act
    TASWIniResult const result = ini.Load("app.ini");

    // Assert
    CheckEquals(EIniCode::FileNotFound, result.Code, __func__, __LINE__, "Load() should fail as ReadFile() did");
    CheckEquals(std::string(), ini.ToText(), __func__, __LINE__, "the document should be emptied");
    CheckEquals(std::string("app.ini"), ini.GetFilePath(), __func__, __LINE__,
        "the file Load() was given should become the file path");
    CheckEquals(std::size_t(1), messages.size(), __func__, __LINE__, "the missing file should be reported once");
    if (messages.size() == 1)
    {
        CheckEquals(ESeverity::Warning, messages[0].Severity, __func__, __LINE__, "a missing file should be a Warning");
        CheckEquals(std::string("elsewhere.ini"), messages[0].FilePath, __func__, __LINE__,
            "the warning should name the file ReadFile() named");
        CheckEquals(2, messages[0].SystemError, __func__, __LINE__, "the warning should have the system error");
    }
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_BaseFile, Seams_GetTheirArguments)
{
    // Arrange
    TSeamIni ini;
    TASWIniOptions options;
    options.Reading.MaxFileSize = 1000;
    options.Saving.AtomicSave = false;
    options.Saving.Backup = true;
    options.Saving.BackupSuffix = ".old";
    static_cast<void>(ini.SetOptions(options));
    std::string readPath;
    std::size_t readMaxSize = 0;
    std::string writePath;
    std::string writeText;
    TASWIniSavingOptions writeOptions;
    auto const read = [&](TReadCall const& call)
        {
            readPath = call.FilePath;
            readMaxSize = call.MaxSize;
            call.Contents = SampleText;

            return TASWIniResult();
        };
    auto const write = [&](TWriteCall const& call)
        {
            writePath = call.FilePath;
            writeText = std::string(call.Contents);
            writeOptions = call.Options;

            return TASWIniResult();
        };
    ini.OnRead = read;
    ini.OnWrite = write;

    // Act
    TASWIniResult const loaded = ini.Load("Folder/app.ini");
    static_cast<void>(ini.SetText("Window", "Width", "800"));
    TASWIniResult const saved = ini.Save();

    // Assert
    CheckTrue(loaded.IsOk() && saved.IsOk(), __func__, __LINE__, "the file should be loaded and saved");
    CheckEquals(std::string("Folder/app.ini"), readPath, __func__, __LINE__, "ReadFile() should get the path as given");
    CheckEquals(std::size_t(1000), readMaxSize, __func__, __LINE__, "ReadFile() should get MaxFileSize");
    CheckEquals(std::string("Folder/app.ini"), writePath, __func__, __LINE__, "WriteFile() should get the file path");
    CheckEquals(ini.ToText(), writeText, __func__, __LINE__, "WriteFile() should get the document's text");
    CheckTrue(writeOptions.Backup && !writeOptions.AtomicSave && writeOptions.BackupSuffix == ".old", __func__,
        __LINE__, "WriteFile() should get the saving options");
    CheckFalse(ini.IsModified(), __func__, __LINE__, "a saved document shouldn't be modified");
}
//---------------------------------------------------------------------------

} // namespace ASWUnitTests

//---------------------------------------------------------------------------
ASW_REGISTER_TEST_GROUP_IN_SUITE(ASWUnitTests::TTest_ASWIni_BaseAccess, "Base")
ASW_REGISTER_TEST_GROUP_IN_SUITE(ASWUnitTests::TTest_ASWIni_BaseCallback, "Base")
ASW_REGISTER_TEST_GROUP_IN_SUITE(ASWUnitTests::TTest_ASWIni_BaseFile, "Base")
