/* **************************************************************************
Test_ASWIni_Platform.cpp
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
#include "Test_ASWIni_Platform.h"
//---------------------------------------------------------------------------
#include "ASWUnitTests_Registry.h"
//---------------------------------------------------------------------------
#include <string>
//---------------------------------------------------------------------------
#include "ASWIni_Platform.h"
//---------------------------------------------------------------------------

// The feature macros must be usable in #if, as 0 or 1
#if ASWINI_HAS_U8STRING != 0 && ASWINI_HAS_U8STRING != 1
#error "ASWINI_HAS_U8STRING must be 0 or 1"
#endif

#if ASWINI_HAS_U8STRING
static_assert(sizeof(std::u8string::value_type) == 1, "std::u8string should hold char8_t");
#endif

namespace ASWUnitTests
{

/////////////////////////////////////////////////////////////////////////////
// TTest_ASWIni_Platform
/////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------
TTest_ASWIni_Platform::TTest_ASWIni_Platform()
    : inherited("ASWIni_Platform_Tests")
{
}
//---------------------------------------------------------------------------
TTest_ASWIni_Platform::~TTest_ASWIni_Platform()
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_Platform::SetUp_Group()
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_Platform::SetUp_Test(ITestCase& /*testCase*/)
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_Platform::TearDown_Group()
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_Platform::TearDown_Test(ITestCase& /*testCase*/)
{
}
//---------------------------------------------------------------------------

// /////// Begin tests after this line ///////////////////////

//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_Platform, HasU8String_OnFromCpp20)
{
    // Arrange
    // Every compiler the library supports has std::u8string at C++20 (MSVC gives the language version in _MSVC_LANG)
#if defined(_MSVC_LANG)
    long const language = static_cast<long>(_MSVC_LANG);
#else
    long const language = static_cast<long>(__cplusplus);
#endif
    int const expected = language >= 202002L ? 1 : 0;

    // Act
    int const hasU8String = ASWINI_HAS_U8STRING;

    // Assert
    CheckEquals(expected, hasU8String, __func__, __LINE__, "ASWINI_HAS_U8STRING should be 1 from C++20 on (C++" +
        std::to_string(language) + ")");
}
//---------------------------------------------------------------------------

} // namespace ASWUnitTests

//---------------------------------------------------------------------------
ASW_REGISTER_TEST_GROUP(ASWUnitTests::TTest_ASWIni_Platform)
