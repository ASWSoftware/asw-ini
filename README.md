# ASWIni - https://github.com/ASWSoftware/asw-ini

ASWIni is a thread-safe, dependency-free C++17 library for reading and writing INI files on Windows
and Linux, which keeps the file's comments and layout.

## Status

ASWIni is in development toward 1.0.0; the `develop` branch carries `1.0.0-dev.N` versions. So far
it has the project structure, the version (`ASWIni_Version.h`), the CMake target and package, and
the unit-test projects. [CHANGELOG.md](CHANGELOG.md) lists what each change adds. The interface may
change in any way until 1.0.0.

## Design

ASWIni is a folder of standard C++ sources, or one CMake target, with no third-party code. It builds
with RAD Studio (bcc32c at C++17, bcc64x at C++20), MSVC, MinGW, GCC and Clang. Its goals:

- **Nothing in the file is lost.** A file that is loaded and saved without changes is written back
  byte for byte: comments, blank lines, spacing, the order of sections and keys, and lines it
  doesn't understand. Changing a value changes only that line.
- **Settings in one place.** An application declares each setting once, with its section, key,
  type, default value, comment and valid range; loading fills it in (or uses the default and says
  why) and saving writes it back, with its comment.
- **Same file on every system.** Files are read and written in binary mode, so the line ending
  (LF or CRLF) is chosen by the library, not by the operating system.
- **Safe to use from several threads**, and saving replaces the file atomically, so a crash while
  saving leaves the previous file intact.
- **Problems are reported, not thrown.** Warnings and errors (a malformed line, an invalid value, a
  file that can't be written) go to a log callback, so the application logs them its own way.

## Planned for 1.0

- Sections, keys, values and comments, read and written in file order; keys before the first
  section; case-insensitive names.
- `=` and `:` separators (configurable), written as `key = value` by default; `;` and `#` comments;
  optional inline comments.
- LF, CRLF and CR line endings on read; the file's own (or the platform's) on write. UTF-8, with or
  without a BOM.
- Typed values: strings, booleans, integers with range checks, floating point, enumerations and
  lists, read and written independently of the locale, plus your own types.
- An interface, `ASWIni::IASWIni`, and its implementation, `ASWIni::TASWBaseIni`, which an
  application's settings class derives from.
- Atomic saves with an optional backup, and UTF-8 file paths on Windows and Linux.

# Donations:

If you find this library helpful, donations are always appreciated:

PayPal: donate@aswsoftware.com

Bitcoin: 15rKqL1numHJyE36ottMbhs5cmCjJkuowV

# How to Use

## Adding ASWIni to a project

### Copied sources (any build system)

Add every `.cpp` file in `ASWIni/` to the project, and the `ASWIni` folder to its include path, and
compile as C++17 or later. `Deploy.bat <folder>` copies the library sources, the build files and
the docs to a folder, e.g. for a release package or to copy into another project.

### RAD Studio

Add the `ASWIni/*.cpp` files to the project (Project > Add to Project) and the `ASWIni` folder to
the include path (Project > Options > Building > C++ Shared Options > Include path). RAD Studio 13.1
or later, with bcc32c (Win32) or the modern Clang toolchain (Win64x). A Win32 project must use the
multithreaded runtime (the project's `Multithreaded` option), or code that uses a mutex doesn't
link. `tests/rad370/ASWIniTests.cbproj` is a working project to copy settings from.

### CMake

The repository's root `CMakeLists.txt` defines the static library target `ASWIni::ASWIni`. Linking
to it adds the `ASWIni` include folder and C++17. Add it from a copy of the repository, such as a
git submodule:

```
add_subdirectory(third_party/asw-ini)
target_link_libraries(MyApp PRIVATE ASWIni::ASWIni)
```

or let CMake download it with `FetchContent`:

```
include(FetchContent)
FetchContent_Declare(ASWIni
    GIT_REPOSITORY https://github.com/ASWSoftware/asw-ini.git
    GIT_TAG develop     # A release tag once there is one, e.g. v1.0.0
    GIT_SUBMODULES ""   # Skip the unit-test framework submodule
)
FetchContent_MakeAvailable(ASWIni)
target_link_libraries(MyApp PRIVATE ASWIni::ASWIni)
```

Either way, only the library is built. When ASWIni is the top-level project,
`-DASWINI_BUILD_TESTS=ON` also builds the unit tests, and `ASWINI_WARNINGS` (on by default only
there) compiles the library with extra warnings.

To use a prebuilt copy instead, build and install the library from the repository root (a Debug
build is named `ASWInid`, so Debug and Release can share one install prefix):

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
cmake --install build --config Release --prefix C:/Libraries/ASWIni
```

and find it with `find_package` (pass the prefix in `CMAKE_PREFIX_PATH`):

```
find_package(ASWIni 1.0 CONFIG REQUIRED)
target_link_libraries(MyApp PRIVATE ASWIni::ASWIni)
```

The headers are installed to `<prefix>/include/ASWIni`, the target's include folder, so code
includes them as it does from the sources (`#include "ASWIni_Version.h"`). A requested version is
met by any later release with the same major version. The install rules are on by default only when
ASWIni is the top-level project; set `ASWINI_INSTALL` to change that. `tests/package` is a small
project that builds against an installed copy; CI runs it.

## Versions

ASWIni follows [Semantic Versioning](https://semver.org). Releases are tagged on `main` (e.g.
`v1.0.0`). Between releases, the `develop` branch carries the next planned version with a
pre-release, e.g. `1.0.0-dev.1`, which comes before `1.0.0`. `ASWIni/ASWIni_Version.h` has the
version as macros, for code that supports several ASWIni versions, and as constants in the `ASWIni`
namespace:

```
#include "ASWIni_Version.h"

#if ASWINI_VERSION_MAJOR > 1 || (ASWINI_VERSION_MAJOR == 1 && ASWINI_VERSION_MINOR >= 1)
    // Uses something added in 1.1 (also present in 1.1.0-dev.N builds)
#endif
```

# Unit Tests

Using the library only requires the `ASWIni` source. The unit tests in `tests` additionally use the
[ASWUnitTests](https://github.com/ASWSoftware/asw-unit-tests) framework, included as a git submodule
in `third_party/asw-unit-tests`. Clone with submodules:

```
git clone --recurse-submodules https://github.com/ASWSoftware/asw-ini.git
```

or, in an existing clone:

```
git submodule update --init
```

Build and run the tests with CMake from the repository root:

```
cmake -S tests/cmake -B tests/cmake/build
cmake --build tests/cmake/build --config Release
```

The executable is written to `tests/cmake/build/bin` (in a `Release` sub folder for multi-config
generators such as Visual Studio). The tests build at C++20 by default; add
`-DASWINI_CXX_STANDARD=17` (in another build folder) to build them, and the library, at C++17.

For RAD Studio, use `tests/rad370/ASWIniTests.cbproj` or its scripts: `Build_Win32_Debug.bat` and
`Build_Win32_Release.bat` (bcc32c, C++17), and `Build_Win64x_Debug.bat` and
`Build_Win64x_Release.bat` (bcc64x, C++20). Output is written to
`tests/build/bin/<Platform>/<Config>`. GitHub Actions builds and runs the tests on Windows and Linux
for each push and pull request to `main` and `develop`.

With GCC or Clang (on Linux), the tests can also be built with a sanitizer, which fails the run with
a report when it finds a data race (`thread`), or a memory error or undefined behavior (`address`,
which includes UndefinedBehaviorSanitizer):

```
cmake -S tests/cmake -B tests/cmake/build-tsan -DCMAKE_BUILD_TYPE=Release -DASWINI_SANITIZE=thread
cmake --build tests/cmake/build-tsan
```

CI runs both with Clang. `ASWINI_SANITIZE` applies to the library and everything that links to it.

CI also measures which lines of the library the tests run (a Linux GCC build with `--coverage`, read
by gcovr): the "Linux (GCC, coverage)" job shows a table on its summary page and keeps an HTML
report as the `coverage-report` artifact. It is informational and never fails the run.

On Windows with RAD Studio, the tests can also run in the ASWUnitTests VCL GUI runner, a window for
choosing tests and reading their results: build `tests/vcl/gui/rad370/ASWIniTests_VCL_GUI.cbproj`
(or its `Build_*.bat` scripts), which writes `ASWIniTests_VCL_GUI.exe` to
`tests/build/bin/<Platform>/<Config>`. `ASWIniTests_Group.groupproj` in the same folder opens it
together with the console test project. The GUI takes the console runner's command line options
plus a few of its own, e.g. `--run --exit` to run every test and exit with the console runner's exit
code; see the ASWUnitTests README's "VCL GUI Runner" section.

# Coding Standards

To use uncrustify (for coding standards (pretty formatting) for this repo):

1. Get `uncrustify` and ensure that the path to `uncrustify.exe` is added to the Windows `Path`
   environment variable. https://github.com/uncrustify/uncrustify

2. Run the following command locally to set the git hooks directory (uncrustify will run upon
   commit):
```
git config --local core.hooksPath .githooks/
```

# Contributing

Contributions are welcome. See [CONTRIBUTING.md](CONTRIBUTING.md) for how to propose a change, build
and test it, and report a bug.
