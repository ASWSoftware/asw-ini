/* **************************************************************************
UT_Helper_FailingAlloc.h
Author: Anthony S. West - ASW Software

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
#ifndef UT_Helper_FailingAllocH
#define UT_Helper_FailingAllocH
//---------------------------------------------------------------------------
#include <cstddef>
//---------------------------------------------------------------------------

namespace ASWUnitTests
{

/////////////////////////////////////////////////////////////////////////////
// TFailingAlloc
//
// Makes an allocation of the current thread (or a few in a row) fail with std::bad_alloc, to test what code does when
// memory runs out. The test executable replaces the global operator new and delete (UT_Helper_FailingAlloc.cpp) with
// ones that use malloc() and free(), and that fail only while a TFailingAlloc is armed. Not in a sanitizer build, whose
// runtime has its own operator new (see CanFailAllocations()). Only allocations made through the executable's operator
// new count: code in a standard library DLL (e.g. MinGW's libstdc++-6.dll) calls its own. One object at a time.
/////////////////////////////////////////////////////////////////////////////
class TFailingAlloc
{
public:
    // Makes allocation number 'number' (1 for the next one) of the current thread fail, once, and the 'count' - 1
    // allocations after it too (e.g. 2, so an allocation that a standard library retries fails again)
    explicit TFailingAlloc(std::size_t number, std::size_t count = 1) noexcept;
    TFailingAlloc(TFailingAlloc const&) = delete;
    ~TFailingAlloc();

    TFailingAlloc& operator=(TFailingAlloc const&) = delete;

    // Whether an allocation failed (false when there were fewer allocations)
    bool HasFailed() const noexcept;
};

//---------------------------------------------------------------------------

// Whether a TFailingAlloc can make allocations fail: false in a sanitizer build, which keeps the runtime's operator new,
// and with MSVC's library with iterator debugging on (its Debug default), whose noexcept move constructors allocate,
// so a failed allocation there calls std::terminate()
bool CanFailAllocations() noexcept;

} // namespace ASWUnitTests

//---------------------------------------------------------------------------
#endif // #ifndef UT_Helper_FailingAllocH
