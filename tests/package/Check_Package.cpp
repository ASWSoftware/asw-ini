/* **************************************************************************
Check_Package.cpp
Author: Anthony S. West - ASW Software

Check of the installed CMake package: tests/package/CMakeLists.txt builds this file against an ASWIni installed with
`cmake --install` and found with find_package(), and running it returns 0 when the package works.

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

#include <iostream>
#include <string>
//---------------------------------------------------------------------------
#include "ASWIni_Version.h"
//---------------------------------------------------------------------------

#if !defined(ASWINI_PACKAGE_VERSION)
#error Define ASWINI_PACKAGE_VERSION
#endif

int main()
{
    // The version find_package() found (from ASWIniConfigVersion.cmake) must be the installed headers' version
    std::string const headerVersion = std::to_string(ASWIni::VersionMajor) + "." +
        std::to_string(ASWIni::VersionMinor) + "." + std::to_string(ASWIni::VersionPatch);
    if (headerVersion != ASWINI_PACKAGE_VERSION)
    {
        std::cerr << "The package version " << ASWINI_PACKAGE_VERSION << " isn't the headers' version "
        << headerVersion << "\n";
        return 1;
    }

    std::cout << "ASWIni " << ASWIni::Version << " package works\n";

    return 0;
}
