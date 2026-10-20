/* **************************************************************************
ASWIni_Base.cpp
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
#include "ASWIni_Base.h"
//---------------------------------------------------------------------------
#include <cstddef>
#include <exception>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
//---------------------------------------------------------------------------
#include "ASWIni_File.h"
#include "ASWIni_Version.h"
//---------------------------------------------------------------------------
#if ASWINI_WINDOWS_LOCKS
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#endif
//---------------------------------------------------------------------------

// A ThreadSanitizer build, whose runtime has the annotation the locks' destructors use (declared here as in
// <sanitizer/tsan_interface.h>)
#if defined(__SANITIZE_THREAD__)
#  define ASWINI_THREAD_SANITIZER 1
#elif defined(__has_feature)
#  if __has_feature(thread_sanitizer)
#    define ASWINI_THREAD_SANITIZER 1
#  endif
#endif

#if defined(ASWINI_THREAD_SANITIZER) && !ASWINI_WINDOWS_LOCKS
extern "C" void __tsan_mutex_destroy(void* addr, unsigned flags);
#endif
//---------------------------------------------------------------------------

namespace ASWIni
{

namespace
{

// The message of the InvalidArgument result of Load() or SaveAs() with an empty file path
constexpr char const* EmptyFilePathText = "The file path is empty.";

#if ASWINI_WINDOWS_LOCKS
static_assert(sizeof(SRWLOCK) == sizeof(void*), "an SRWLOCK should fit in the void* the locks keep it in");
#endif

TASWIniResult Failure(EIniCode code, char const* message);
TASWIniMessage MissingFileWarning(TASWIniResult const& result);
void SetFilePaths(std::string const& filePath, TASWIniResult& result, std::vector<TASWIniMessage>& messages);

#if ASWINI_WINDOWS_LOCKS
SRWLOCK* SlimLock(void*& lock) noexcept;
#endif

//---------------------------------------------------------------------------
/*
    Failure

    A failed result with 'code' and 'message'.
*/
TASWIniResult Failure(EIniCode code, char const* message)
{
    TASWIniResult result;
    result.Code = code;
    result.Message = message;

    return result;
}
//---------------------------------------------------------------------------
/*
    MissingFileWarning

    The Warning message Load() reports for 'result', its FileNotFound result (the file it emptied the document for
    doesn't exist), with the result's code, text, file path and system error.
*/
TASWIniMessage MissingFileWarning(TASWIniResult const& result)
{
    TASWIniMessage message;
    message.Severity = ESeverity::Warning;
    message.Code = result.Code;
    message.Text = result.Message;
    message.FilePath = result.FilePath;
    message.SystemError = result.SystemError;

    return message;
}
//---------------------------------------------------------------------------
/*
    SetFilePaths

    Sets the file path of 'result', when it failed, and of each of 'messages' to 'filePath', the file they are about,
    where they have none, so their reports don't take the object's file path, which may be another file by then.
*/
void SetFilePaths(std::string const& filePath, TASWIniResult& result, std::vector<TASWIniMessage>& messages)
{
    if (!result.IsOk() && result.FilePath.empty())
        result.FilePath = filePath;

    for (TASWIniMessage& message : messages)
    {
        if (message.FilePath.empty())
            message.FilePath = filePath;
    }
}
//---------------------------------------------------------------------------
#if ASWINI_WINDOWS_LOCKS
/*
    SlimLock

    The SRWLOCK that 'lock', a lock's storage (a null pointer at first, which is SRWLOCK_INIT), holds.
*/
SRWLOCK* SlimLock(void*& lock) noexcept
{
    return reinterpret_cast<SRWLOCK*>(&lock);
}
//---------------------------------------------------------------------------
#endif

} // namespace

//---------------------------------------------------------------------------

namespace Detail
{

/////////////////////////////////////////////////////////////////////////////
// TMutex
/////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------
TMutex::~TMutex()
{
#if defined(ASWINI_THREAD_SANITIZER) && !ASWINI_WINDOWS_LOCKS
    // What the pthread_mutex_destroy() that libstdc++ leaves out would tell it (libc++ calls it too, which is harmless)
    __tsan_mutex_destroy(m_Mutex.native_handle(), 0);
#endif
}
//---------------------------------------------------------------------------
void TMutex::lock()
{
#if ASWINI_WINDOWS_LOCKS
    AcquireSRWLockExclusive(SlimLock(m_Lock));
#else
    m_Mutex.lock();
#endif
}
//---------------------------------------------------------------------------
void TMutex::unlock() noexcept
{
#if ASWINI_WINDOWS_LOCKS
    ReleaseSRWLockExclusive(SlimLock(m_Lock));
#else
    m_Mutex.unlock();
#endif
}
//---------------------------------------------------------------------------


/////////////////////////////////////////////////////////////////////////////
// TSharedMutex
/////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------
TSharedMutex::~TSharedMutex()
{
#if defined(ASWINI_THREAD_SANITIZER) && defined(_GLIBCXX_USE_PTHREAD_RWLOCK_T)
    // What the pthread_rwlock_destroy() that libstdc++ leaves out would tell it. Only libstdc++'s std::shared_mutex
    // on a pthread rwlock (_GLIBCXX_USE_PTHREAD_RWLOCK_T) has a native_handle(); libc++ builds its std::shared_mutex
    // on a std::mutex and condition variables, which it destroys.
    __tsan_mutex_destroy(m_Mutex.native_handle(), 0);
#endif
}
//---------------------------------------------------------------------------
void TSharedMutex::lock()
{
#if ASWINI_WINDOWS_LOCKS
    AcquireSRWLockExclusive(SlimLock(m_Lock));
#else
    m_Mutex.lock();
#endif
}
//---------------------------------------------------------------------------
void TSharedMutex::lock_shared()
{
#if ASWINI_WINDOWS_LOCKS
    AcquireSRWLockShared(SlimLock(m_Lock));
#else
    m_Mutex.lock_shared();
#endif
}
//---------------------------------------------------------------------------
void TSharedMutex::unlock() noexcept
{
#if ASWINI_WINDOWS_LOCKS
    ReleaseSRWLockExclusive(SlimLock(m_Lock));
#else
    m_Mutex.unlock();
#endif
}
//---------------------------------------------------------------------------
void TSharedMutex::unlock_shared() noexcept
{
#if ASWINI_WINDOWS_LOCKS
    ReleaseSRWLockShared(SlimLock(m_Lock));
#else
    m_Mutex.unlock_shared();
#endif
}
//---------------------------------------------------------------------------

} // namespace Detail

//---------------------------------------------------------------------------

/////////////////////////////////////////////////////////////////////////////
// TASWBaseIni
/////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------
TASWIniResult TASWBaseIni::AddSection(std::string_view section)
{
    auto const edit = [section](TASWIniDocument& document)
        {
            return document.AddSection(section);
        };

    return Edit(edit);
}
//---------------------------------------------------------------------------
TASWIniResult TASWBaseIni::CaughtException() noexcept
{
    TASWIniResult result;
    result.Code = EIniCode::UnexpectedException;

    try
    {
        try
        {
            // Not a throw expression, which bcc32c would warn about in a noexcept function
            std::rethrow_exception(std::current_exception());
        }
        catch (std::exception const& e)
        {
            result.Message = std::string("An unexpected exception was thrown: ") + e.what();
        }
        catch (...)
        {
            result.Message = UnknownExceptionText;
        }
    }
    catch (...)
    {
        // The message couldn't be made (std::bad_alloc); the code says what happened
    }

    return result;
}
//---------------------------------------------------------------------------
template <typename TEdit>
TASWIniResult TASWBaseIni::Edit(TEdit const& edit)
{
    TASWIniResult result;

    try
    {
        std::lock_guard<Detail::TSharedMutex> lock(m_Mutex);
        result = edit(m_Document);
        if (result.IsOk())
            ++m_EditCount;
    }
    catch (...)
    {
        result = CaughtException();
    }

    if (!result.IsOk())
        ReportFailure(result);

    return result;
}
//---------------------------------------------------------------------------
TASWIniDocument TASWBaseIni::GetDocument() const
{
    std::shared_lock<Detail::TSharedMutex> lock(m_Mutex);

    return m_Document;
}
//---------------------------------------------------------------------------
std::string TASWBaseIni::GetFileComment() const
{
    std::shared_lock<Detail::TSharedMutex> lock(m_Mutex);

    return m_Document.GetFileComment();
}
//---------------------------------------------------------------------------
std::string TASWBaseIni::GetFilePath() const
{
    std::shared_lock<Detail::TSharedMutex> lock(m_Mutex);

    return m_FilePath;
}
//---------------------------------------------------------------------------
std::string TASWBaseIni::GetKeyComment(std::string_view section, std::string_view key) const
{
    std::shared_lock<Detail::TSharedMutex> lock(m_Mutex);

    return m_Document.GetKeyComment(section, key);
}
//---------------------------------------------------------------------------
std::vector<std::string> TASWBaseIni::GetKeyNames(std::string_view section) const
{
    std::shared_lock<Detail::TSharedMutex> lock(m_Mutex);

    return m_Document.GetKeyNames(section);
}
//---------------------------------------------------------------------------
TASWIniOptions TASWBaseIni::GetOptions() const
{
    std::shared_lock<Detail::TSharedMutex> lock(m_Mutex);

    return m_Document.GetOptions();
}
//---------------------------------------------------------------------------
std::string TASWBaseIni::GetSectionComment(std::string_view section) const
{
    std::shared_lock<Detail::TSharedMutex> lock(m_Mutex);

    return m_Document.GetSectionComment(section);
}
//---------------------------------------------------------------------------
std::vector<std::string> TASWBaseIni::GetSectionNames(std::string_view prefix) const
{
    std::shared_lock<Detail::TSharedMutex> lock(m_Mutex);

    return m_Document.GetSectionNames(prefix);
}
//---------------------------------------------------------------------------
TASWIniValueOptions TASWBaseIni::GetValueOptions() const
{
    std::shared_lock<Detail::TSharedMutex> lock(m_Mutex);

    return m_Document.GetOptions().Values;
}
//---------------------------------------------------------------------------
std::string_view TASWBaseIni::GetVersionStr() const noexcept
{
    return Version;
}
//---------------------------------------------------------------------------
bool TASWBaseIni::HasKey(std::string_view section, std::string_view key) const
{
    std::shared_lock<Detail::TSharedMutex> lock(m_Mutex);

    return m_Document.FindKey(section, key) != nullptr;
}
//---------------------------------------------------------------------------
bool TASWBaseIni::HasSection(std::string_view section) const
{
    std::shared_lock<Detail::TSharedMutex> lock(m_Mutex);

    return m_Document.FindSection(section) != nullptr;
}
//---------------------------------------------------------------------------
bool TASWBaseIni::IsModified() const
{
    std::shared_lock<Detail::TSharedMutex> lock(m_Mutex);

    return m_Document.IsModified();
}
//---------------------------------------------------------------------------
TASWIniResult TASWBaseIni::Load(std::string_view filePath)
{
    std::vector<TASWIniMessage> messages;
    TASWIniResult result;
    bool missing = false;

    try
    {
        result = LoadFile(filePath, messages, missing);
    }
    catch (...)
    {
        result = CaughtException();
    }

    ReportAll(messages);

    // A missing file is in 'messages', as a Warning
    if (!result.IsOk() && !missing)
        ReportFailure(result);

    return result;
}
//---------------------------------------------------------------------------
TASWIniResult TASWBaseIni::LoadFile(std::string_view filePath, std::vector<TASWIniMessage>& messages, bool& missing)
{
    if (filePath.empty())
        return Failure(EIniCode::InvalidArgument, EmptyFilePathText);

    std::string path(filePath);
    std::lock_guard<Detail::TMutex> fileLock(m_FileMutex);
    TASWIniOptions options;
    {
        std::shared_lock<Detail::TSharedMutex> lock(m_Mutex);
        options = m_Document.GetOptions();
    }

    // Read and parsed without the lock, as LoadText() parses; an edit made meanwhile is replaced by the file
    std::string text;
    TASWIniDocument document;
    TASWIniResult result = ReadFile(path, options.Reading.MaxFileSize, text);
    bool const notFound = result.Code == EIniCode::FileNotFound;
    if (result.IsOk())
        result = document.Parse(text, options, messages);
    else if (notFound)
        static_cast<void>(document.Reset(options)); // The options are the document's, so they are valid

    SetFilePaths(path, result, messages);
    if (!result.IsOk() && !notFound)
        return result;

    // An application's first run has no file: the document is emptied and gets the path, so Save() creates the file
    if (notFound)
        messages.push_back(MissingFileWarning(result));

    {
        std::lock_guard<Detail::TSharedMutex> lock(m_Mutex);
        m_Document = std::move(document);
        m_FilePath = std::move(path);
    }

    missing = notFound;

    return result;
}
//---------------------------------------------------------------------------
TASWIniResult TASWBaseIni::LoadText(std::string_view text)
{
    std::vector<TASWIniMessage> messages;
    TASWIniResult result;

    try
    {
        std::lock_guard<Detail::TMutex> fileLock(m_FileMutex);
        TASWIniOptions options;
        {
            std::shared_lock<Detail::TSharedMutex> lock(m_Mutex);
            options = m_Document.GetOptions();
        }

        // Parsed without the lock, so lookups go on meanwhile; the options can't change, since SetOptions() waits for
        // the file lock
        TASWIniDocument document;
        result = document.Parse(text, options, messages);
        if (result.IsOk())
        {
            std::lock_guard<Detail::TSharedMutex> lock(m_Mutex);
            m_Document = std::move(document);
        }
    }
    catch (...)
    {
        result = CaughtException();
    }

    ReportAll(messages);

    if (!result.IsOk())
        ReportFailure(result);

    return result;
}
//---------------------------------------------------------------------------
TASWIniResult TASWBaseIni::ReadFile(std::string const& filePath, std::size_t maxSize, std::string& contents)
{
    return Detail::ReadWholeFile(Detail::Utf8ToPath(filePath), maxSize, contents);
}
//---------------------------------------------------------------------------
bool TASWBaseIni::RemoveKey(std::string_view section, std::string_view key)
{
    std::lock_guard<Detail::TSharedMutex> lock(m_Mutex);
    bool const removed = m_Document.RemoveKey(section, key);
    if (removed)
        ++m_EditCount;

    return removed;
}
//---------------------------------------------------------------------------
bool TASWBaseIni::RemoveSection(std::string_view section)
{
    std::lock_guard<Detail::TSharedMutex> lock(m_Mutex);
    bool const removed = m_Document.RemoveSection(section);
    if (removed)
        ++m_EditCount;

    return removed;
}
//---------------------------------------------------------------------------
TASWIniResult TASWBaseIni::RenameKey(std::string_view section, std::string_view key, std::string_view newKey)
{
    auto const edit = [section, key, newKey](TASWIniDocument& document)
        {
            return document.RenameKey(section, key, newKey);
        };

    return Edit(edit);
}
//---------------------------------------------------------------------------
TASWIniResult TASWBaseIni::RenameSection(std::string_view section, std::string_view newSection)
{
    auto const edit = [section, newSection](TASWIniDocument& document)
        {
            return document.RenameSection(section, newSection);
        };

    return Edit(edit);
}
//---------------------------------------------------------------------------
void TASWBaseIni::Report(TASWIniMessage const& message) const noexcept
{
    try
    {
        // The callback is called through a copy of the pointer, so SetLogCallback() may replace it meanwhile
        TLogCallbackPtr callback;
        std::string filePath;
        {
            std::shared_lock<Detail::TSharedMutex> lock(m_Mutex);
            callback = m_LogCallback;
            if (message.FilePath.empty())
                filePath = m_FilePath;
        }

        if (callback == nullptr)
            return;

        if (filePath.empty())
        {
            (*callback)(message);
        }
        else
        {
            TASWIniMessage withPath = message;
            withPath.FilePath = filePath;
            (*callback)(withPath);
        }
    }
    catch (...)
    {
        // The callback threw, or the message couldn't be made (std::bad_alloc): reporting never throws
    }
}
//---------------------------------------------------------------------------
void TASWBaseIni::ReportAll(std::vector<TASWIniMessage> const& messages) const noexcept
{
    for (TASWIniMessage const& message : messages)
        Report(message);
}
//---------------------------------------------------------------------------
TASWIniResult TASWBaseIni::Save()
{
    return SaveTo(nullptr);
}
//---------------------------------------------------------------------------
TASWIniResult TASWBaseIni::SaveAs(std::string_view filePath)
{
    return SaveTo(&filePath);
}
//---------------------------------------------------------------------------
TASWIniResult TASWBaseIni::SaveFile(std::string_view const* filePath, std::vector<TASWIniMessage>& messages)
{
    if (filePath != nullptr && filePath->empty())
        return Failure(EIniCode::InvalidArgument, EmptyFilePathText);

    std::lock_guard<Detail::TMutex> fileLock(m_FileMutex);
    std::string path;
    std::string text;
    TASWIniSavingOptions options;
    unsigned long long editCount = 0;
    {
        std::shared_lock<Detail::TSharedMutex> lock(m_Mutex);
        path = filePath != nullptr ? std::string(*filePath) : m_FilePath;
        if (path.empty())
            return Failure(EIniCode::NoFilePath, "There is no file path to save to; Load() or SaveAs() sets one.");

        text = m_Document.ToText();
        options = m_Document.GetOptions().Saving;
        editCount = m_EditCount;
    }

    // Written without the lock, so lookups and edits go on meanwhile
    TASWIniResult result = WriteFile(path, text, options, messages);
    SetFilePaths(path, result, messages);
    if (!result.IsOk())
        return result;

    std::lock_guard<Detail::TSharedMutex> lock(m_Mutex);

    // An edit made while the file was being written isn't in the file
    if (m_EditCount == editCount)
        m_Document.ClearModified();

    if (filePath != nullptr)
        m_FilePath = std::move(path);

    return result;
}
//---------------------------------------------------------------------------
TASWIniResult TASWBaseIni::SaveTo(std::string_view const* filePath)
{
    std::vector<TASWIniMessage> messages;
    TASWIniResult result;

    try
    {
        result = SaveFile(filePath, messages);
    }
    catch (...)
    {
        result = CaughtException();
    }

    ReportAll(messages);

    if (!result.IsOk())
        ReportFailure(result);

    return result;
}
//---------------------------------------------------------------------------
TASWIniResult TASWBaseIni::SetFileComment(std::string_view text)
{
    auto const edit = [text](TASWIniDocument& document)
        {
            return document.SetFileComment(text);
        };

    return Edit(edit);
}
//---------------------------------------------------------------------------
TASWIniResult TASWBaseIni::SetKeyComment(std::string_view section, std::string_view key, std::string_view text)
{
    auto const edit = [section, key, text](TASWIniDocument& document)
        {
            return document.SetKeyComment(section, key, text);
        };

    return Edit(edit);
}
//---------------------------------------------------------------------------
void TASWBaseIni::SetLogCallback(TASWIniLogCallback callback)
{
    TLogCallbackPtr newCallback;
    if (callback)
        newCallback = std::make_shared<TASWIniLogCallback const>(std::move(callback));

    {
        std::lock_guard<Detail::TSharedMutex> lock(m_Mutex);
        m_LogCallback.swap(newCallback);
    }

    // The old callback is destroyed here, with no lock held (unless a report is still calling it)
}
//---------------------------------------------------------------------------
TASWIniResult TASWBaseIni::SetOptions(TASWIniOptions const& options)
{
    std::vector<TASWIniMessage> messages;
    TASWIniResult result;

    try
    {
        // Read again under the lock, so an edit made meanwhile isn't lost
        std::lock_guard<Detail::TMutex> fileLock(m_FileMutex);
        std::lock_guard<Detail::TSharedMutex> lock(m_Mutex);
        result = m_Document.SetOptions(options, messages);
    }
    catch (...)
    {
        result = CaughtException();
    }

    ReportAll(messages);

    if (!result.IsOk())
        ReportFailure(result);

    return result;
}
//---------------------------------------------------------------------------
TASWIniResult TASWBaseIni::SetSectionComment(std::string_view section, std::string_view text)
{
    auto const edit = [section, text](TASWIniDocument& document)
        {
            return document.SetSectionComment(section, text);
        };

    return Edit(edit);
}
//---------------------------------------------------------------------------
TASWIniResult TASWBaseIni::SetSettingText(std::string_view section, std::string_view key, std::string_view text,
    std::string_view comment)
{
    auto const edit = [section, key, text, comment](TASWIniDocument& document)
        {
            if (comment.empty() || document.FindKey(section, key) != nullptr)
                return document.SetValue(section, key, text);

            // The key and its comment are added to a copy, which replaces the document only when both succeed
            TASWIniDocument changed = document;
            TASWIniResult result = changed.SetValue(section, key, text);
            if (result.IsOk())
                result = changed.SetKeyComment(section, key, comment);

            if (result.IsOk())
                document = std::move(changed);

            return result;
        };

    return Edit(edit);
}
//---------------------------------------------------------------------------
TASWIniResult TASWBaseIni::SetText(std::string_view section, std::string_view key, std::string_view text)
{
    auto const edit = [section, key, text](TASWIniDocument& document)
        {
            return document.SetValue(section, key, text);
        };

    return Edit(edit);
}
//---------------------------------------------------------------------------
std::string TASWBaseIni::ToText() const
{
    std::shared_lock<Detail::TSharedMutex> lock(m_Mutex);

    return m_Document.ToText();
}
//---------------------------------------------------------------------------
bool TASWBaseIni::TryGetText(std::string_view section, std::string_view key, std::string& text) const
{
    std::shared_lock<Detail::TSharedMutex> lock(m_Mutex);
    TASWIniDocumentLine const* const line = m_Document.FindKey(section, key);
    if (line == nullptr)
        return false;

    text = line->GetValue();

    return true;
}
//---------------------------------------------------------------------------
TASWIniResult TASWBaseIni::WriteFile(std::string const& filePath, std::string_view contents,
    TASWIniSavingOptions const& options, std::vector<TASWIniMessage>& warnings)
{
    return Detail::WriteWholeFile(Detail::Utf8ToPath(filePath), contents, options, warnings);
}
//---------------------------------------------------------------------------

} // namespace ASWIni
