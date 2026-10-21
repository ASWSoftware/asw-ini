/* **************************************************************************
ASWIni_Base.h
Author: Anthony S. West - ASW Software

An INI file reader/writer library.

TASWBaseIni, the thread-safe INI object: a TASWIniDocument behind a
reader/writer lock, its options, the file it is loaded from and saved to,
and the log callback its warnings and errors go to, with the operations of
IASWIni. An application's settings class derives from it. Detail::TMutex
and Detail::TSharedMutex, its locks.

Requires C++ 17 or higher.

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

#pragma once

#ifndef ASWIni_BaseH
#define ASWIni_BaseH
//---------------------------------------------------------------------------
#include <cstddef>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <vector>
//---------------------------------------------------------------------------
#include "ASWIni_Document.h"
#include "ASWIni_Interface.h"
#include "ASWIni_Options.h"
#include "ASWIni_Types.h"
//---------------------------------------------------------------------------

// Whether the locks (Detail::TMutex, Detail::TSharedMutex) are Windows slim reader/writer locks (1) or the standard
// library's mutexes (0). RAD Studio Win32's (bcc32c) std::mutex and std::shared_mutex wait on a kernel mutex for every
// lock, which is about 1000 times slower than a slim reader/writer lock when threads contend for it. Set by the
// compiler, not by the application, since the locks' layout depends on it.
#if defined(__BORLANDC__) && !defined(_WIN64)
#  define ASWINI_WINDOWS_LOCKS 1
#else
#  define ASWINI_WINDOWS_LOCKS 0
#endif
//---------------------------------------------------------------------------

namespace ASWIni
{

// A section of an application's settings (ASWIni_Settings.h), which registers with its TASWBaseIni
class TASWIniSection;

// The internals of TASWBaseIni
namespace Detail
{

// The base of TASWIniSetting<T> (ASWIni_Settings.h), which registers with its TASWBaseIni, and which TASWBaseIni lets
// add a key with its comment in one edit
class TSettingBase;

/////////////////////////////////////////////////////////////////////////////
// TMutex
//
// A mutex, locked as a std::mutex is (std::lock_guard, std::unique_lock): a std::mutex, or a Windows slim
// reader/writer lock (see ASWINI_WINDOWS_LOCKS). In a ThreadSanitizer build its destructor tells ThreadSanitizer that
// the mutex is gone: libstdc++'s std::mutex never destroys its pthread mutex, so ThreadSanitizer would take a later
// mutex at the same address (e.g. another object's, on a reused stack) for this one, and report lock-order inversions
// between unrelated objects. Neither copyable nor movable.
/////////////////////////////////////////////////////////////////////////////
class TMutex
{
private:
#if ASWINI_WINDOWS_LOCKS
    void* m_Lock = nullptr; // An SRWLOCK, whose initial value (SRWLOCK_INIT) is a null pointer
#else
    std::mutex m_Mutex;
#endif

public:
    TMutex() = default;
    TMutex(TMutex const&) = delete;
    TMutex& operator=(TMutex const&) = delete;
    ~TMutex();

    // The names std::lock_guard and std::unique_lock call
    void lock();
    void unlock() noexcept;
};


/////////////////////////////////////////////////////////////////////////////
// TSharedMutex
//
// A reader/writer mutex, locked as a std::shared_mutex is (std::lock_guard and std::unique_lock for writing,
// std::shared_lock for reading): a std::shared_mutex, or a Windows slim reader/writer lock (see ASWINI_WINDOWS_LOCKS).
// In a ThreadSanitizer build its destructor tells ThreadSanitizer that the mutex is gone, as TMutex's does (with
// glibc, libstdc++'s std::shared_mutex never destroys its pthread rwlock either). Neither copyable nor movable.
/////////////////////////////////////////////////////////////////////////////
class TSharedMutex
{
private:
#if ASWINI_WINDOWS_LOCKS
    void* m_Lock = nullptr; // An SRWLOCK, whose initial value (SRWLOCK_INIT) is a null pointer
#else
    std::shared_mutex m_Mutex;
#endif

public:
    TSharedMutex() = default;
    TSharedMutex(TSharedMutex const&) = delete;
    TSharedMutex& operator=(TSharedMutex const&) = delete;
    ~TSharedMutex();

    // The names std::lock_guard, std::unique_lock and std::shared_lock call
    void lock();
    void lock_shared();
    void unlock() noexcept;
    void unlock_shared() noexcept;
};

} // namespace Detail


/////////////////////////////////////////////////////////////////////////////
// TASWBaseIni
//
// The thread-safe INI object: a TASWIniDocument (empty, with the default options, until a text or a file is loaded),
// its file path and the log callback, with IASWIni's operations, which any thread may call at any time. Each
// operation is complete in itself: a lookup sees the document before or after an edit or a load, never during one.
// Lookups share a reader/writer lock; edits take it alone. Load() reads and parses the file without that lock, and
// Save() writes it without that lock, so lookups and edits go on meanwhile; a second lock keeps loads, saves and
// SetOptions() from overlapping. Warnings and errors are collected while a lock is held, and reported after it is
// released (see TASWIniLogCallback). An application's settings class derives from it, and declares its settings
// (TASWIniSection, TASWIniSetting<T>), which Load() and LoadText() apply to the document they read before swapping it
// in. Neither copyable nor movable.
/////////////////////////////////////////////////////////////////////////////
class TASWBaseIni : public IASWIni
{
private:
    friend class Detail::TSettingBase;
    friend class TASWIniSection;

    typedef std::shared_ptr<TASWIniLogCallback const> TLogCallbackPtr;

private:
    mutable Detail::TSharedMutex m_Mutex; // Guards the members after it
    TASWIniDocument m_Document;
    std::string m_FilePath; // UTF-8
    TLogCallbackPtr m_LogCallback; // nullptr when there is none
    unsigned long long m_EditCount = 0; // Counts the edits that succeeded, so Save() can tell whether one was made

    // Held for the whole of Load(), LoadText(), Save(), SaveAs() and SetOptions(), so they never overlap; taken before
    // m_Mutex, never after it. Guards the members after it.
    Detail::TMutex m_FileMutex;

    // The settings and sections bound to the object (see ASWIni_Settings.h), in the order they were declared
    std::vector<Detail::TSettingBase const*> m_Settings;
    std::vector<TASWIniSection const*> m_Sections;

private:
    // The UnexpectedException result of the exception being handled (call it in a catch block): its message is "An
    // unexpected exception was thrown: " and what() for a std::exception. Without a message when it can't be made.
    static TASWIniResult CaughtException() noexcept;

private:
    // Applies the settings and then the sections bound to the object to 'document', a document Load() or LoadText()
    // read, before anyone else can see it (see TASWIniSetting and TASWIniSection), adding what they report to
    // 'messages'. Called with the file lock held. May throw (std::bad_alloc, or what a converter or a validator
    // throws).
    void ApplySettings(TASWIniDocument& document, std::vector<TASWIniMessage>& messages) const;

    // Applies 'edit', a function that takes a TASWIniDocument& and returns a TASWIniResult, to the document under the
    // lock; an exception becomes CaughtException(). Reports a failed result.
    template <typename TEdit>
    TASWIniResult Edit(TEdit const& edit);

    // Load()'s work, which may throw: adds the warnings to 'messages', and sets 'missing' when the file doesn't exist
    // and the document was emptied
    TASWIniResult LoadFile(std::string_view filePath, std::vector<TASWIniMessage>& messages, bool& missing);

    // Adds 'setting' or 'section' at the end of the ones bound to the object, under the file lock: a setting's or a
    // section's constructor calls it, once the object is complete
    void Register(Detail::TSettingBase const& setting);
    void Register(TASWIniSection const& section);

    // Reports each of 'messages', in order (with no lock held)
    void ReportAll(std::vector<TASWIniMessage> const& messages) const noexcept;

    // The work of Save() (when 'filePath' is nullptr) and SaveAs(*filePath), which may throw: adds the warnings to
    // 'messages'
    TASWIniResult SaveFile(std::string_view const* filePath, std::vector<TASWIniMessage>& messages);

    // Save() (when 'filePath' is nullptr) or SaveAs(*filePath): SaveFile() with an exception caught, and its warnings
    // and failure reported
    TASWIniResult SaveTo(std::string_view const* filePath);

    // SetText() for a setting (see TASWIniSetting::Set()): when it adds the key and 'comment' isn't empty, also sets
    // the key's comment to 'comment', in the same edit, so a failure of either changes nothing
    TASWIniResult SetSettingText(std::string_view section, std::string_view key, std::string_view text,
        std::string_view comment);

    // Removes 'setting' or 'section' from the ones bound to the object, under the file lock (so after a Load() that is
    // applying it): a setting's or a section's destructor calls it
    void Unregister(Detail::TSettingBase const& setting);
    void Unregister(TASWIniSection const& section);

protected:
    // Reads the whole file 'filePath' (UTF-8) into 'contents', failing with FileTooLarge if it has more than 'maxSize'
    // bytes, as Detail::ReadWholeFile() does (the default). Load() calls it with the file lock held but not the
    // document's lock, so it may call the object's lookups and edits, but not Load(), LoadText(), Save(), SaveAs() or
    // SetOptions(). A derived class may override it, e.g. for a test to make a failure the file system can't give
    // reliably. A FileNotFound result is a missing file (see Load()). May throw; Load() catches it.
    virtual TASWIniResult ReadFile(std::string const& filePath, std::size_t maxSize, std::string& contents);

    // Calls the log callback with 'message' (FilePath set to the file path when it is empty), unless there is none.
    // An exception the callback throws is dropped.
    void Report(TASWIniMessage const& message) const noexcept override;

    // Writes 'contents' to the file 'filePath' (UTF-8) as the saving options 'options' say, adding warnings (e.g.
    // BackupFailed) to 'warnings', as Detail::WriteWholeFile() does (the default). Save() and SaveAs() call it as
    // Load() calls ReadFile(). May throw; Save() and SaveAs() catch it.
    virtual TASWIniResult WriteFile(std::string const& filePath, std::string_view contents,
        TASWIniSavingOptions const& options, std::vector<TASWIniMessage>& warnings);

public:
    // An empty document with the default options, no file path and no log callback
    TASWBaseIni() = default;

    TASWIniResult AddSection(std::string_view section) override;
    TASWIniDocument GetDocument() const override;
    std::string GetFileComment() const override;
    std::string GetFilePath() const override;
    std::string GetKeyComment(std::string_view section, std::string_view key) const override;
    std::vector<std::string> GetKeyNames(std::string_view section) const override;
    TASWIniOptions GetOptions() const override;
    std::string GetSectionComment(std::string_view section) const override;
    std::vector<std::string> GetSectionNames(std::string_view prefix = std::string_view()) const override;
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
    TASWIniResult SetText(std::string_view section, std::string_view key, std::string_view text) override;
    std::string ToText() const override;
    bool TryGetText(std::string_view section, std::string_view key, std::string& text) const override;
};

} // namespace ASWIni

//---------------------------------------------------------------------------
#endif // #ifndef ASWIni_BaseH
