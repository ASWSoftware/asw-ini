/* **************************************************************************
Test_ASWIni_File.cpp
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
#include "Test_ASWIni_File.h"
//---------------------------------------------------------------------------
#include "ASWUnitTests_Registry.h"
//---------------------------------------------------------------------------
#include <filesystem>
#include <string>
#include <system_error>
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
#else
#  include <sys/stat.h>
#endif
//---------------------------------------------------------------------------
#include "ASWIni_File.h"
#include "ASWIni_Options.h"
#include "ASWIni_Types.h"
//---------------------------------------------------------------------------
#include "UT_Helper_TempDir.h"
//---------------------------------------------------------------------------

namespace ASWUnitTests
{

namespace
{

std::string TrickyBytes();

//---------------------------------------------------------------------------
/*
    TrickyBytes

    Bytes a text-mode read or write would change: CRLF, a lone CR, a NUL, a Ctrl+Z (end of file to some C runtimes),
    a UTF-8 BOM and a byte above 0x7F
*/
std::string TrickyBytes()
{
    return std::string("\xEF\xBB\xBF[General]\r\nKey = 1\rB\n\x1A\0x\xFF", 28);
}
//---------------------------------------------------------------------------

} // namespace

//---------------------------------------------------------------------------

/////////////////////////////////////////////////////////////////////////////
// TTest_ASWIni_File
/////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------
TTest_ASWIni_File::TTest_ASWIni_File()
    : inherited("ASWIni_File_Tests")
{
}
//---------------------------------------------------------------------------
TTest_ASWIni_File::~TTest_ASWIni_File()
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_File::SetUp_Group()
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_File::SetUp_Test(ITestCase& /*testCase*/)
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_File::TearDown_Group()
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_File::TearDown_Test(ITestCase& /*testCase*/)
{
}
//---------------------------------------------------------------------------

// /////// Begin tests after this line ///////////////////////

//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_File, PathToUtf8_RoundTripsAscii)
{
    // Arrange
    std::string const name = "app settings.ini";
    std::string const pathText = "folder/sub folder/app settings.ini";

    // Act
    std::string const roundTrip = ASWIni::Detail::PathToUtf8(ASWIni::Detail::Utf8ToPath(name));
    std::filesystem::path const path = ASWIni::Detail::Utf8ToPath(pathText);

    // Assert. A path is compared by its elements: bcc32c's library turns '/' into '\' in a path made from a wide
    // string, which the other libraries keep as written.
    CheckEquals(name, roundTrip, __func__, __LINE__, "an ASCII name should convert back to the same text");
    CheckTrue(path == std::filesystem::path(pathText), __func__, __LINE__,
        "a path with separators should have the same elements");
    CheckTrue(ASWIni::Detail::Utf8ToPath("").empty(), __func__, __LINE__, "an empty text should be an empty path");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_File, ReadWholeFile_EmptyFile)
{
    // Arrange
    TTempDir const folder;
    AssertFalse(folder.Path().empty(), __func__, __LINE__, "the test folder should be created");
    std::filesystem::path const path = folder.Path() / "empty.ini";
    AssertTrue(WriteTestFile(path, ""), __func__, __LINE__, "the test file should be written");
    std::string contents = "old";

    // Act
    ASWIni::TASWIniResult const result = ASWIni::Detail::ReadWholeFile(path, 100, contents);

    // Assert
    CheckTrue(result.IsOk(), __func__, __LINE__, "an empty file should be read: " + result.Message);
    CheckEmpty(contents, __func__, __LINE__, "the contents should be empty");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_File, ReadWholeFile_EmptyPath)
{
    // Arrange
    std::string contents;

    // Act
    ASWIni::TASWIniResult const result = ASWIni::Detail::ReadWholeFile(std::filesystem::path(), 100, contents);

    // Assert
    CheckEquals(ASWIni::EIniCode::InvalidArgument, result.Code, __func__, __LINE__,
        "an empty path should be an invalid argument");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_File, ReadWholeFile_Folder)
{
    // Arrange
    TTempDir const folder;
    AssertFalse(folder.Path().empty(), __func__, __LINE__, "the test folder should be created");
    std::string contents;

    // Act
    ASWIni::TASWIniResult const result = ASWIni::Detail::ReadWholeFile(folder.Path(), 100, contents);

    // Assert: Windows can't open a folder as a file (AccessDenied); elsewhere it opens, but isn't a file (OpenFailed)
    CheckFalse(result.IsOk(), __func__, __LINE__, "a folder should not be read as a file");
    CheckTrue(result.Code == ASWIni::EIniCode::AccessDenied || result.Code == ASWIni::EIniCode::OpenFailed, __func__,
        __LINE__, std::string("the code should be AccessDenied or OpenFailed, not ") + ASWIni::ToString(result.Code));
    CheckNotEmpty(result.Message, __func__, __LINE__, "the failure should be described");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_File, ReadWholeFile_LargerThanMaxSize)
{
    // Arrange
    TTempDir const folder;
    AssertFalse(folder.Path().empty(), __func__, __LINE__, "the test folder should be created");
    std::filesystem::path const path = folder.Path() / "large.ini";
    AssertTrue(WriteTestFile(path, "0123456789A"), __func__, __LINE__, "the test file should be written");
    std::string contents;

    // Act
    ASWIni::TASWIniResult const tooLarge = ASWIni::Detail::ReadWholeFile(path, 10, contents);
    std::string const contentsAfterFailure = contents;
    ASWIni::TASWIniResult const exactSize = ASWIni::Detail::ReadWholeFile(path, 11, contents);

    // Assert
    CheckEquals(ASWIni::EIniCode::FileTooLarge, tooLarge.Code, __func__, __LINE__,
        "an 11-byte file should be too large for a 10-byte limit");
    CheckContains(tooLarge.Message, "11 bytes", __func__, __LINE__, "the message should give the file's size");
    CheckEmpty(contentsAfterFailure, __func__, __LINE__, "nothing should be read from a file that is too large");
    CheckTrue(exactSize.IsOk(), __func__, __LINE__, "a file of exactly the limit should be read: " + exactSize.Message);
    CheckEquals(std::string("0123456789A"), contents, __func__, __LINE__, "the whole file should be read");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_File, ReadWholeFile_MissingFile)
{
    // Arrange
    TTempDir const folder;
    AssertFalse(folder.Path().empty(), __func__, __LINE__, "the test folder should be created");
    std::filesystem::path const path = folder.Path() / "missing.ini";
    std::string contents;

    // Act
    ASWIni::TASWIniResult const result = ASWIni::Detail::ReadWholeFile(path, 100, contents);

    // Assert
    CheckEquals(ASWIni::EIniCode::FileNotFound, result.Code, __func__, __LINE__, "the code should be FileNotFound");
    CheckNotEquals(0, result.SystemError, __func__, __LINE__, "the system's error code should be kept");
    CheckEquals(ASWIni::Detail::PathToUtf8(path), result.FilePath, __func__, __LINE__, "the path should be kept");
    CheckContains(result.Message, "missing.ini", __func__, __LINE__, "the message should name the file");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_File, ReadWholeFile_MissingFolder)
{
    // Arrange
    TTempDir const folder;
    AssertFalse(folder.Path().empty(), __func__, __LINE__, "the test folder should be created");
    std::string contents;

    // Act
    ASWIni::TASWIniResult const result = ASWIni::Detail::ReadWholeFile(folder.Path() / "no" / "app.ini", 100,
        contents);

    // Assert
    CheckEquals(ASWIni::EIniCode::FileNotFound, result.Code, __func__, __LINE__,
        "a missing folder should be FileNotFound");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_File, ReadWholeFile_ReadsExactBytes)
{
    // Arrange
    TTempDir const folder;
    AssertFalse(folder.Path().empty(), __func__, __LINE__, "the test folder should be created");
    std::filesystem::path const path = folder.Path() / "bytes.ini";
    AssertTrue(WriteTestFile(path, TrickyBytes()), __func__, __LINE__, "the test file should be written");
    std::string contents;

    // Act
    ASWIni::TASWIniResult const result = ASWIni::Detail::ReadWholeFile(path, 100, contents);

    // Assert
    CheckTrue(result.IsOk(), __func__, __LINE__, "the file should be read: " + result.Message);
    CheckEquals(TrickyBytes(), contents, __func__, __LINE__, "every byte should be read unchanged");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_File, Utf8ToPath_ConvertsFourByteSequence)
{
    // Arrange: U+1F600, a surrogate pair in UTF-16
    std::string const utf8 = "\xF0\x9F\x98\x80.ini";

    // Act
    std::filesystem::path const path = ASWIni::Detail::Utf8ToPath(utf8);

    // Assert
#if defined(_WIN32)
    CheckTrue(path.wstring() == std::wstring(L"\xD83D\xDE00.ini"), __func__, __LINE__,
        "the wide path should hold the surrogate pair");
#else
    CheckEquals(utf8, path.string(), __func__, __LINE__, "the path should hold the UTF-8 bytes");
#endif
    CheckEquals(utf8, ASWIni::Detail::PathToUtf8(path), __func__, __LINE__, "the path should convert back");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_File, Utf8ToPath_ConvertsNonAscii)
{
    // Arrange: "cafe" with U+00E9, and the Greek lambda U+03BB
    std::string const utf8 = "caf\xC3\xA9_\xCE\xBB.ini";

    // Act
    std::filesystem::path const path = ASWIni::Detail::Utf8ToPath(utf8);

    // Assert
#if defined(_WIN32)
    CheckTrue(path.wstring() == std::wstring(L"caf\x00E9_\x03BB.ini"), __func__, __LINE__,
        "the wide path should hold U+00E9 and U+03BB");
#else
    CheckEquals(utf8, path.string(), __func__, __LINE__, "the path should hold the UTF-8 bytes");
#endif
    CheckEquals(utf8, ASWIni::Detail::PathToUtf8(path), __func__, __LINE__, "the path should convert back");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_File, WriteWholeFile_Backup_CopiesPreviousFile)
{
    // Arrange
    TTempDir const folder;
    AssertFalse(folder.Path().empty(), __func__, __LINE__, "the test folder should be created");
    std::filesystem::path const path = folder.Path() / "app.ini";
    AssertTrue(WriteTestFile(path, "old"), __func__, __LINE__, "the test file should be written");
    ASWIni::TASWIniSavingOptions options;
    options.Backup = true;
    std::vector<ASWIni::TASWIniMessage> warnings;

    // Act
    ASWIni::TASWIniResult const result = ASWIni::Detail::WriteWholeFile(path, "new", options, warnings);

    // Assert
    std::string contents;
    std::string backup;
    CheckTrue(result.IsOk(), __func__, __LINE__, "the file should be saved: " + result.Message);
    CheckEmpty(warnings, __func__, __LINE__, "there should be no warnings");
    CheckTrue(ReadTestFile(path, contents), __func__, __LINE__, "the file should be readable");
    CheckEquals(std::string("new"), contents, __func__, __LINE__, "the file should have the new contents");
    CheckTrue(ReadTestFile(folder.Path() / "app.ini.bak", backup), __func__, __LINE__, "the backup should exist");
    CheckEquals(std::string("old"), backup, __func__, __LINE__, "the backup should have the previous contents");

    // Act: a second save replaces the older backup
    ASWIni::TASWIniResult const second = ASWIni::Detail::WriteWholeFile(path, "newer", options, warnings);

    // Assert
    CheckTrue(second.IsOk(), __func__, __LINE__, "the second save should succeed: " + second.Message);
    CheckTrue(ReadTestFile(folder.Path() / "app.ini.bak", backup), __func__, __LINE__, "the backup should exist");
    CheckEquals(std::string("new"), backup, __func__, __LINE__, "the backup should be replaced");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_File, WriteWholeFile_Backup_SkippedForNewFile)
{
    // Arrange
    TTempDir const folder;
    AssertFalse(folder.Path().empty(), __func__, __LINE__, "the test folder should be created");
    ASWIni::TASWIniSavingOptions options;
    options.Backup = true;
    std::vector<ASWIni::TASWIniMessage> warnings;

    // Act
    ASWIni::TASWIniResult const result = ASWIni::Detail::WriteWholeFile(folder.Path() / "new.ini", "x", options,
        warnings);

    // Assert
    CheckTrue(result.IsOk(), __func__, __LINE__, "the file should be saved: " + result.Message);
    CheckEmpty(warnings, __func__, __LINE__, "there is nothing to back up, which isn't a warning");
    CheckEquals(1, static_cast<int>(CountEntries(folder.Path())), __func__, __LINE__,
        "the folder should only hold the file");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_File, WriteWholeFile_CreatesFile)
{
    // Arrange
    TTempDir const folder;
    AssertFalse(folder.Path().empty(), __func__, __LINE__, "the test folder should be created");
    std::filesystem::path const path = folder.Path() / "app.ini";
    std::vector<ASWIni::TASWIniMessage> warnings;

    // Act
    ASWIni::TASWIniResult const result = ASWIni::Detail::WriteWholeFile(path, TrickyBytes(),
        ASWIni::TASWIniSavingOptions(), warnings);

    // Assert
    std::string contents;
    CheckTrue(result.IsOk(), __func__, __LINE__, "the file should be saved: " + result.Message);
    CheckEmpty(warnings, __func__, __LINE__, "there should be no warnings");
    CheckTrue(ReadTestFile(path, contents), __func__, __LINE__, "the file should be readable");
    CheckEquals(TrickyBytes(), contents, __func__, __LINE__, "every byte should be written unchanged");
    CheckEquals(1, static_cast<int>(CountEntries(folder.Path())), __func__, __LINE__,
        "no temporary file should be left");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_File, WriteWholeFile_CreatesMissingFolders)
{
    // Arrange
    TTempDir const folder;
    AssertFalse(folder.Path().empty(), __func__, __LINE__, "the test folder should be created");
    std::filesystem::path const path = folder.Path() / "Company" / "App" / "app.ini";
    std::vector<ASWIni::TASWIniMessage> warnings;

    // Act
    ASWIni::TASWIniResult const result = ASWIni::Detail::WriteWholeFile(path, "x", ASWIni::TASWIniSavingOptions(),
        warnings);

    // Assert
    std::string contents;
    CheckTrue(result.IsOk(), __func__, __LINE__, "the file should be saved: " + result.Message);
    CheckTrue(ReadTestFile(path, contents), __func__, __LINE__, "the file should be in the new folders");
    CheckEquals(std::string("x"), contents, __func__, __LINE__, "the file should have its contents");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_File, WriteWholeFile_EmptyPath)
{
    // Arrange
    std::vector<ASWIni::TASWIniMessage> warnings;

    // Act
    ASWIni::TASWIniResult const result = ASWIni::Detail::WriteWholeFile(std::filesystem::path(), "x",
        ASWIni::TASWIniSavingOptions(), warnings);

    // Assert
    CheckEquals(ASWIni::EIniCode::InvalidArgument, result.Code, __func__, __LINE__,
        "an empty path should be an invalid argument");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_File, WriteWholeFile_FailedReplace_KeepsTargetAndRemovesTemp)
{
    // Arrange: a folder where the file should be, so the temporary file can be written but can't replace it
    TTempDir const folder;
    AssertFalse(folder.Path().empty(), __func__, __LINE__, "the test folder should be created");
    std::filesystem::path const path = folder.Path() / "app.ini";
    std::error_code error;
    AssertTrue(std::filesystem::create_directory(path, error), __func__, __LINE__, "the blocking folder");
    AssertTrue(WriteTestFile(path / "inside.txt", "kept"), __func__, __LINE__, "a file in the blocking folder");
    std::vector<ASWIni::TASWIniMessage> warnings;

    // Act
    ASWIni::TASWIniResult const result = ASWIni::Detail::WriteWholeFile(path, "x", ASWIni::TASWIniSavingOptions(),
        warnings);

    // Assert
    std::string inside;
    CheckEquals(ASWIni::EIniCode::ReplaceFailed, result.Code, __func__, __LINE__, "the code should be ReplaceFailed");
    CheckNotEquals(0, result.SystemError, __func__, __LINE__, "the system's error code should be kept");
    CheckEmpty(warnings, __func__, __LINE__, "the temporary file should be deleted without a warning");
    CheckEquals(1, static_cast<int>(CountEntries(folder.Path())), __func__, __LINE__,
        "no temporary file should be left");
    CheckTrue(ReadTestFile(path / "inside.txt", inside), __func__, __LINE__, "the folder should be untouched");
    CheckEquals(std::string("kept"), inside, __func__, __LINE__, "the folder's file should be unchanged");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_File, WriteWholeFile_InPlace_OverwritesAndTruncates)
{
    // Arrange
    TTempDir const folder;
    AssertFalse(folder.Path().empty(), __func__, __LINE__, "the test folder should be created");
    std::filesystem::path const path = folder.Path() / "app.ini";
    AssertTrue(WriteTestFile(path, "a much longer previous content"), __func__, __LINE__,
        "the test file should be written");
    ASWIni::TASWIniSavingOptions options;
    options.AtomicSave = false;
    std::vector<ASWIni::TASWIniMessage> warnings;

    // Act
    ASWIni::TASWIniResult const result = ASWIni::Detail::WriteWholeFile(path, "short", options, warnings);

    // Assert
    std::string contents;
    CheckTrue(result.IsOk(), __func__, __LINE__, "the file should be saved: " + result.Message);
    CheckTrue(ReadTestFile(path, contents), __func__, __LINE__, "the file should be readable");
    CheckEquals(std::string("short"), contents, __func__, __LINE__, "the old contents should be cut off");
    CheckEquals(1, static_cast<int>(CountEntries(folder.Path())), __func__, __LINE__,
        "no other file should be made");
}
//---------------------------------------------------------------------------
#if defined(_WIN32)
ASW_TEST(TTest_ASWIni_File, WriteWholeFile_KeepsHiddenAttribute)
{
    // Arrange
    TTempDir const folder;
    AssertFalse(folder.Path().empty(), __func__, __LINE__, "the test folder should be created");
    std::filesystem::path const path = folder.Path() / "hidden.ini";
    AssertTrue(WriteTestFile(path, "old"), __func__, __LINE__, "the test file should be written");
    AssertTrue(::SetFileAttributesW(path.c_str(), FILE_ATTRIBUTE_HIDDEN) != 0, __func__, __LINE__,
        "the file should be made hidden");
    std::vector<ASWIni::TASWIniMessage> warnings;

    // Act: atomically, then in place
    ASWIni::TASWIniResult const atomic = ASWIni::Detail::WriteWholeFile(path, "new", ASWIni::TASWIniSavingOptions(),
        warnings);
    DWORD const attributesAfterAtomic = ::GetFileAttributesW(path.c_str());
    ASWIni::TASWIniSavingOptions inPlaceOptions;
    inPlaceOptions.AtomicSave = false;
    ASWIni::TASWIniResult const inPlace = ASWIni::Detail::WriteWholeFile(path, "newer", inPlaceOptions, warnings);
    DWORD const attributesAfterInPlace = ::GetFileAttributesW(path.c_str());

    // Assert
    std::string contents;
    CheckTrue(atomic.IsOk(), __func__, __LINE__, "the atomic save should succeed: " + atomic.Message);
    CheckTrue((attributesAfterAtomic & FILE_ATTRIBUTE_HIDDEN) != 0, __func__, __LINE__,
        "the file should still be hidden after an atomic save");
    CheckTrue(inPlace.IsOk(), __func__, __LINE__, "the in-place save should succeed: " + inPlace.Message);
    CheckTrue((attributesAfterInPlace & FILE_ATTRIBUTE_HIDDEN) != 0, __func__, __LINE__,
        "the file should still be hidden after an in-place save");
    CheckTrue(ReadTestFile(path, contents), __func__, __LINE__, "the file should be readable");
    CheckEquals(std::string("newer"), contents, __func__, __LINE__, "the file should have the last contents");
}
#endif
//---------------------------------------------------------------------------
#if !defined(_WIN32)
ASW_TEST(TTest_ASWIni_File, WriteWholeFile_KeepsPermissions)
{
    // Arrange: a private file, which an atomic save must not make readable by others
    TTempDir const folder;
    AssertFalse(folder.Path().empty(), __func__, __LINE__, "the test folder should be created");
    std::filesystem::path const path = folder.Path() / "private.ini";
    AssertTrue(WriteTestFile(path, "old"), __func__, __LINE__, "the test file should be written");
    AssertEquals(0, ::chmod(path.c_str(), 0600), __func__, __LINE__, "chmod 0600");
    std::vector<ASWIni::TASWIniMessage> warnings;

    // Act
    ASWIni::TASWIniResult const result = ASWIni::Detail::WriteWholeFile(path, "new", ASWIni::TASWIniSavingOptions(),
        warnings);

    // Assert
    struct stat status;
    CheckTrue(result.IsOk(), __func__, __LINE__, "the file should be saved: " + result.Message);
    AssertEquals(0, ::stat(path.c_str(), &status), __func__, __LINE__, "stat");
    CheckEquals(0600, static_cast<int>(status.st_mode & 07777), __func__, __LINE__,
        "the file should keep its 0600 permissions");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_File, WriteWholeFile_KeepsSymbolicLink)
{
    // Arrange: app.ini is a link to real.ini
    TTempDir const folder;
    AssertFalse(folder.Path().empty(), __func__, __LINE__, "the test folder should be created");
    std::filesystem::path const real = folder.Path() / "real.ini";
    std::filesystem::path const link = folder.Path() / "app.ini";
    AssertTrue(WriteTestFile(real, "old"), __func__, __LINE__, "the test file should be written");
    std::error_code error;
    std::filesystem::create_symlink(real, link, error);
    AssertFalse(static_cast<bool>(error), __func__, __LINE__, "the link should be created: " + error.message());
    std::vector<ASWIni::TASWIniMessage> warnings;

    // Act
    ASWIni::TASWIniResult const result = ASWIni::Detail::WriteWholeFile(link, "new", ASWIni::TASWIniSavingOptions(),
        warnings);

    // Assert
    std::string contents;
    CheckTrue(result.IsOk(), __func__, __LINE__, "the file should be saved: " + result.Message);
    CheckTrue(std::filesystem::is_symlink(link, error), __func__, __LINE__, "the link should still be a link");
    CheckTrue(ReadTestFile(real, contents), __func__, __LINE__, "the linked file should be readable");
    CheckEquals(std::string("new"), contents, __func__, __LINE__, "the linked file should have the new contents");
}
#endif
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_File, WriteWholeFile_NonAsciiPath)
{
    // Arrange: a folder and a file name with U+00E9 and U+03BB
    TTempDir const folder;
    AssertFalse(folder.Path().empty(), __func__, __LINE__, "the test folder should be created");
    std::filesystem::path const path = folder.Path() / ASWIni::Detail::Utf8ToPath("caf\xC3\xA9") /
        ASWIni::Detail::Utf8ToPath("\xCE\xBB.ini");
    std::vector<ASWIni::TASWIniMessage> warnings;
    std::string contents;

    // Act
    ASWIni::TASWIniResult const written = ASWIni::Detail::WriteWholeFile(path, "x", ASWIni::TASWIniSavingOptions(),
        warnings);
    ASWIni::TASWIniResult const read = ASWIni::Detail::ReadWholeFile(path, 100, contents);

    // Assert
    CheckTrue(written.IsOk(), __func__, __LINE__, "the file should be saved: " + written.Message);
    CheckTrue(read.IsOk(), __func__, __LINE__, "the file should be read: " + read.Message);
    CheckEquals(std::string("x"), contents, __func__, __LINE__, "the file should have its contents");
#if defined(_WIN32)
    std::error_code error;
    CheckTrue(std::filesystem::exists(folder.Path() / L"caf\x00E9" / L"\x03BB.ini", error), __func__, __LINE__,
        "the file should have the wide name");
#endif
}
//---------------------------------------------------------------------------
#if defined(_WIN32)
ASW_TEST(TTest_ASWIni_File, WriteWholeFile_ReadOnlyFile_Fails)
{
    // Arrange
    TTempDir const folder;
    AssertFalse(folder.Path().empty(), __func__, __LINE__, "the test folder should be created");
    std::filesystem::path const path = folder.Path() / "readonly.ini";
    AssertTrue(WriteTestFile(path, "old"), __func__, __LINE__, "the test file should be written");
    AssertTrue(::SetFileAttributesW(path.c_str(), FILE_ATTRIBUTE_READONLY) != 0, __func__, __LINE__,
        "the file should be made read-only");
    std::vector<ASWIni::TASWIniMessage> warnings;

    // Act
    ASWIni::TASWIniResult const result = ASWIni::Detail::WriteWholeFile(path, "new", ASWIni::TASWIniSavingOptions(),
        warnings);

    // Assert
    std::string contents;
    CheckEquals(ASWIni::EIniCode::ReplaceFailed, result.Code, __func__, __LINE__, "the code should be ReplaceFailed");
    CheckEquals(static_cast<int>(ERROR_ACCESS_DENIED), result.SystemError, __func__, __LINE__,
        "the system error should be access denied");
    CheckTrue(ReadTestFile(path, contents), __func__, __LINE__, "the file should still be readable");
    CheckEquals(std::string("old"), contents, __func__, __LINE__, "the file should be unchanged");
    CheckEquals(1, static_cast<int>(CountEntries(folder.Path())), __func__, __LINE__,
        "no temporary file should be left");

    // So the folder can be deleted
    ::SetFileAttributesW(path.c_str(), FILE_ATTRIBUTE_NORMAL);
}
#endif
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_File, WriteWholeFile_ReplacesFile)
{
    // Arrange
    TTempDir const folder;
    AssertFalse(folder.Path().empty(), __func__, __LINE__, "the test folder should be created");
    std::filesystem::path const path = folder.Path() / "app.ini";
    AssertTrue(WriteTestFile(path, "a longer previous content"), __func__, __LINE__, "the test file should be written");
    std::vector<ASWIni::TASWIniMessage> warnings;

    // Act
    ASWIni::TASWIniResult const result = ASWIni::Detail::WriteWholeFile(path, "new", ASWIni::TASWIniSavingOptions(),
        warnings);

    // Assert
    std::string contents;
    CheckTrue(result.IsOk(), __func__, __LINE__, "the file should be saved: " + result.Message);
    CheckEmpty(warnings, __func__, __LINE__, "there should be no warnings");
    CheckTrue(ReadTestFile(path, contents), __func__, __LINE__, "the file should be readable");
    CheckEquals(std::string("new"), contents, __func__, __LINE__, "the file should have only the new contents");
    CheckEquals(1, static_cast<int>(CountEntries(folder.Path())), __func__, __LINE__,
        "no temporary file should be left");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_File, WriteWholeFile_WithoutCreateFolders_FailsForMissingFolder)
{
    // Arrange
    TTempDir const folder;
    AssertFalse(folder.Path().empty(), __func__, __LINE__, "the test folder should be created");
    ASWIni::TASWIniSavingOptions options;
    options.CreateFolders = false;
    std::vector<ASWIni::TASWIniMessage> warnings;

    // Act
    ASWIni::TASWIniResult const result = ASWIni::Detail::WriteWholeFile(folder.Path() / "App" / "app.ini", "x",
        options, warnings);

    // Assert
    CheckEquals(ASWIni::EIniCode::FileNotFound, result.Code, __func__, __LINE__,
        "a missing folder should be FileNotFound");
    CheckEquals(0, static_cast<int>(CountEntries(folder.Path())), __func__, __LINE__,
        "no folder should be created");
}
//---------------------------------------------------------------------------

} // namespace ASWUnitTests

//---------------------------------------------------------------------------
ASW_REGISTER_TEST_GROUP(ASWUnitTests::TTest_ASWIni_File)
