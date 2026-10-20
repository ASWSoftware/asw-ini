/* **************************************************************************
UT_Helper_FailingAlloc.cpp
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
#include "UT_Helper_FailingAlloc.h"
//---------------------------------------------------------------------------
#include <atomic>
#include <cstdlib>
#include <new>
#include <thread>
//---------------------------------------------------------------------------

// A sanitizer build: its runtime has its own operator new (with a static runtime, defining ours too can fail to link),
// so the replacement below is left out
#if defined(__SANITIZE_ADDRESS__) || defined(__SANITIZE_THREAD__)
#  define ASWINI_TESTS_SANITIZER 1
#elif defined(__has_feature)
#  if __has_feature(address_sanitizer) || __has_feature(thread_sanitizer) || __has_feature(memory_sanitizer)
#    define ASWINI_TESTS_SANITIZER 1
#  endif
#endif
//---------------------------------------------------------------------------

namespace ASWUnitTests
{

namespace
{

// The allocations left until the first that fails, counted on g_ArmedThread; 0 when no TFailingAlloc is armed
std::atomic<std::size_t> g_Countdown{ 0 };

// Whether an armed allocation failed
std::atomic<bool> g_Failed{ false };

// The thread whose allocations count; set before g_Countdown
std::thread::id g_ArmedThread;

// The allocations that fail in a row from the first one, less those that have; set before g_Countdown, and then only
// used by g_ArmedThread
std::size_t g_FailuresLeft = 0;

#if !defined(ASWINI_TESTS_SANITIZER)

//---------------------------------------------------------------------------

bool ShouldFailAllocation() noexcept;

//---------------------------------------------------------------------------
/*
    ShouldFailAllocation

    Whether the allocation being made should fail: it is the armed thread's, and one of those a TFailingAlloc was armed
    for (which then counts as failed). Called by the replaced operator new.
*/
bool ShouldFailAllocation() noexcept
{
    if (g_Countdown.load(std::memory_order_acquire) == 0 || std::this_thread::get_id() != g_ArmedThread)
        return false;

    // Only the armed thread gets here, so the count can't change between the load and the store
    std::size_t const left = g_Countdown.load(std::memory_order_relaxed);
    if (left > 1)
    {
        g_Countdown.store(left - 1, std::memory_order_relaxed);
        return false;
    }

    // The countdown stays at 1 until the last allocation in a row has failed
    g_Failed.store(true, std::memory_order_relaxed);
    if (--g_FailuresLeft == 0)
        g_Countdown.store(0, std::memory_order_relaxed);

    return true;
}
//---------------------------------------------------------------------------

#endif

} // namespace

//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
bool CanFailAllocations() noexcept
{
#if defined(ASWINI_TESTS_SANITIZER)
    return false;
#else
    return true;
#endif
}
//---------------------------------------------------------------------------


/////////////////////////////////////////////////////////////////////////////
// TFailingAlloc
/////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------
TFailingAlloc::TFailingAlloc(std::size_t number, std::size_t count) noexcept
{
    g_Failed.store(false, std::memory_order_relaxed);
    g_ArmedThread = std::this_thread::get_id();
    g_FailuresLeft = count;
    g_Countdown.store(count != 0 ? number : 0, std::memory_order_release);
}
//---------------------------------------------------------------------------
TFailingAlloc::~TFailingAlloc()
{
    g_Countdown.store(0, std::memory_order_release);
}
//---------------------------------------------------------------------------
bool TFailingAlloc::HasFailed() const noexcept
{
    return g_Failed.load(std::memory_order_relaxed);
}
//---------------------------------------------------------------------------

} // namespace ASWUnitTests

//---------------------------------------------------------------------------

#if !defined(ASWINI_TESTS_SANITIZER)

// The replaced global operator new and delete, for the whole test executable. The array forms and the nothrow
// operator new call these by default; the aligned forms keep their own (they allocate differently), which is fine
// since the library never over-aligns.

//---------------------------------------------------------------------------
void* operator new(std::size_t size)
{
    if (ASWUnitTests::ShouldFailAllocation())
        throw std::bad_alloc();

    void* const memory = std::malloc(size == 0 ? 1 : size);
    if (memory == nullptr)
        throw std::bad_alloc();

    return memory;
}
//---------------------------------------------------------------------------
void* operator new[](std::size_t size)
{
    return operator new(size);
}
//---------------------------------------------------------------------------
void operator delete(void* memory) noexcept
{
    std::free(memory);
}
//---------------------------------------------------------------------------
void operator delete(void* memory, std::size_t /*size*/) noexcept
{
    std::free(memory);
}
//---------------------------------------------------------------------------
void operator delete[](void* memory) noexcept
{
    std::free(memory);
}
//---------------------------------------------------------------------------
void operator delete[](void* memory, std::size_t /*size*/) noexcept
{
    std::free(memory);
}
//---------------------------------------------------------------------------

#endif
