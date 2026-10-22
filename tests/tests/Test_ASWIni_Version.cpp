/* **************************************************************************
Test_ASWIni_Version.cpp
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
#include "Test_ASWIni_Version.h"
//---------------------------------------------------------------------------
#include "ASWUnitTests_Registry.h"
//---------------------------------------------------------------------------
#include <string>
//---------------------------------------------------------------------------
#include "ASWIni_Version.h"
//---------------------------------------------------------------------------

// The version macros must be usable in #if, so code can check which version it is built against
#if ASWINI_VERSION_MAJOR < 0 || ASWINI_VERSION_MINOR < 0 || ASWINI_VERSION_PATCH < 0
#error "The ASWIni version macros must be usable in #if"
#endif

// The constants must be usable at compile time
static_assert(ASWIni::VersionMajor == ASWINI_VERSION_MAJOR, "VersionMajor should be ASWINI_VERSION_MAJOR");
static_assert(ASWIni::VersionMinor == ASWINI_VERSION_MINOR, "VersionMinor should be ASWINI_VERSION_MINOR");
static_assert(ASWIni::VersionPatch == ASWINI_VERSION_PATCH, "VersionPatch should be ASWINI_VERSION_PATCH");

namespace ASWUnitTests
{

/////////////////////////////////////////////////////////////////////////////
// TTest_ASWIni_Version
/////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------
TTest_ASWIni_Version::TTest_ASWIni_Version()
    : inherited("ASWIni_Version_Tests")
{
}
//---------------------------------------------------------------------------
TTest_ASWIni_Version::~TTest_ASWIni_Version()
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_Version::SetUp_Group()
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_Version::SetUp_Test(ITestCase& /*testCase*/)
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_Version::TearDown_Group()
{
}
//---------------------------------------------------------------------------
void TTest_ASWIni_Version::TearDown_Test(ITestCase& /*testCase*/)
{
}
//---------------------------------------------------------------------------

// /////// Begin tests after this line ///////////////////////

//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_Version, Constants_MatchMacros)
{
    // Arrange
    std::string const expectedPreRelease = ASWINI_VERSION_PRERELEASE;
    std::string const expectedVersion = ASWINI_VERSION_STRING;

    // Act
    std::string const preRelease = ASWIni::VersionPreRelease;
    std::string const version = ASWIni::Version;

    // Assert
    CheckEquals(expectedPreRelease, preRelease, __func__, __LINE__,
        "VersionPreRelease should be ASWINI_VERSION_PRERELEASE");
    CheckEquals(expectedVersion, version, __func__, __LINE__, "Version should be ASWINI_VERSION_STRING");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_Version, PreRelease_IsValidSemVer)
{
    // Arrange: a pre-release is empty (a release) or dot-separated identifiers, e.g. "dev.1". An identifier is
    // [0-9A-Za-z-], not empty, and has no leading zero if it is all digits (SemVer 2.0.0).
    std::string const identifier = "(?:0|[1-9][0-9]*|[0-9]*[A-Za-z-][0-9A-Za-z-]*)";

    // Act
    std::string const preRelease = ASWIni::VersionPreRelease;

    // Assert
    CheckMatches(preRelease, "(?:" + identifier + "(?:\\." + identifier + ")*)?", __func__, __LINE__,
        "ASWINI_VERSION_PRERELEASE should be empty or SemVer pre-release identifiers");
}
//---------------------------------------------------------------------------
ASW_TEST(TTest_ASWIni_Version, VersionString_MatchesParts)
{
    // Arrange
    std::string const preRelease = ASWIni::VersionPreRelease;
    std::string expected = std::to_string(ASWIni::VersionMajor) + "." + std::to_string(ASWIni::VersionMinor) + "." +
        std::to_string(ASWIni::VersionPatch);
    if (!preRelease.empty())
        expected += "-" + preRelease;

    // Act
    std::string const version = ASWIni::Version;

    // Assert
    CheckEquals(expected, version, __func__, __LINE__,
        "ASWINI_VERSION_STRING should be MAJOR.MINOR.PATCH, plus -PRERELEASE if there is one");
}
//---------------------------------------------------------------------------

} // namespace ASWUnitTests

//---------------------------------------------------------------------------
ASW_REGISTER_TEST_GROUP(ASWUnitTests::TTest_ASWIni_Version)
