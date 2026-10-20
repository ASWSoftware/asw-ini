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
#include <exception>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
//---------------------------------------------------------------------------
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

#if ASWINI_WINDOWS_LOCKS

static_assert(sizeof(SRWLOCK) == sizeof(void*), "an SRWLOCK should fit in the void* the locks keep it in");

SRWLOCK* SlimLock(void*& lock) noexcept;

//---------------------------------------------------------------------------
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
bool TASWBaseIni::RemoveKey(std::string_view section, std::string_view key)
{
    std::lock_guard<Detail::TSharedMutex> lock(m_Mutex);

    return m_Document.RemoveKey(section, key);
}
//---------------------------------------------------------------------------
bool TASWBaseIni::RemoveSection(std::string_view section)
{
    std::lock_guard<Detail::TSharedMutex> lock(m_Mutex);

    return m_Document.RemoveSection(section);
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

} // namespace ASWIni
