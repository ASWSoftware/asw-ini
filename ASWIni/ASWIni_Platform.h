/* **************************************************************************
ASWIni_Platform.h
Author: Anthony S. West - ASW Software

An INI file reader/writer library.

What the compiler and its standard library offer beyond C++17, as macros the
library's optional C++20 additions are switched on with. Each one comes from
the standard library's feature-test macro, never from the language version or
the presence of a header alone. They are set by the code that includes this
header, so an application compiled at C++20 gets the additions even when the
library itself was built at C++17 (they are defined in the headers).

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

#ifndef ASWIni_PlatformH
#define ASWIni_PlatformH
//---------------------------------------------------------------------------
// <version> (C++20) defines the library's feature-test macros; before it, the headers of each feature do (<string>
// defines __cpp_lib_char8_t too). RAD Studio's 32-bit compiler (bcc32c) has no <version>.
#if defined(__has_include)
#  if __has_include(<version>)
#    include <version>
#  endif
#endif
#include <string>
//---------------------------------------------------------------------------

// Whether std::u8string (C++20) exists: 1 when the standard library has char8_t (__cpp_lib_char8_t), else 0
#if defined(__cpp_lib_char8_t)
#  define ASWINI_HAS_U8STRING 1
#else
#  define ASWINI_HAS_U8STRING 0
#endif
//---------------------------------------------------------------------------
#endif // #ifndef ASWIni_PlatformH
