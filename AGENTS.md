# AGENTS.md

## Scope

These instructions apply to the entire repository unless a more specific `AGENTS.md` is added in a subdirectory. Keep changes focused on the requested behavior and preserve existing public APIs unless a change is necessary.

## Repository Overview

ASWIni is a thread-safe, dependency-free C++17 library for reading and writing INI files on Windows and Linux. It keeps comments, blank lines and formatting when a file is loaded and saved, and lets an application declare its settings (section, key, type, default, comment) in one place.

- `ASWIni/` contains the library implementation (namespace `ASWIni`).
- `tests/` contains the unit tests: test modules (`Test_ASWIni_<Unit>`, the tests of `ASWIni_<Unit>`) and shared test helpers (`UT_Helper_<Topic>`) in `tests/tests/`, their CMake project in `tests/cmake/`, their RAD Studio console project in `tests/rad370/`, and their RAD Studio VCL GUI runner project in `tests/vcl/gui/rad370/` (built from the submodule's `vcl/gui/src`). `tests/package/` is a project that builds against an installed ASWIni with `find_package` (CI installs the library and runs it).
- `CMakeLists.txt` (the root) defines the `ASWIni::ASWIni` library target and its install rules; `cmake/ASWIniConfig.cmake.in` is the template of the installed package file. The project version is read from `ASWIni/ASWIni_Version.h`.
- `third_party/asw-unit-tests/` is the ASWUnitTests framework, a git submodule pinned to a release tag. Never edit files in it.
- `.github/workflows/ci.yml` builds and runs the unit tests on Windows (MSVC, MinGW) and Linux (GCC, Clang) at C++20, on Linux GCC at C++17, plus Linux Clang builds with ThreadSanitizer and with AddressSanitizer + UBSan (`ASWINI_SANITIZE`); the non-sanitizer jobs also check the installed CMake package (`tests/package/`). A Linux GCC coverage job (informational, it never fails the run) shows the library's line coverage on its summary page and uploads an HTML report.
- `Deploy.bat` copies the library sources, build files and docs to a folder; it lists every file explicitly.
- `README.md` contains repo details: features, quick start, project setup, versions and tests.
- `CHANGELOG.md` records notable changes per release (see Changelog below).
- `CONTRIBUTING.md` contains the guidelines for contributors (issues, branches, commit history, pull requests, and bug reports); it defers to this file for coding rules, so keep the two consistent.
- `.uncrustify.cfg` and `.githooks/` define the repository formatting workflow.

## Accuracy-First Engineering

Contributors and AI coding agents are expected to be expert, skeptical, and careful. Verify assumptions by inspecting nearby code, call sites, tests, project files, and relevant history or documentation when needed. Neither the human contributor nor the AI is infallible; treat requirements and proposed fixes as hypotheses to check, and prefer evidence from a focused build or test over intuition.

Before changing behavior:

1. Identify the code path that owns the behavior.
2. State or test a concrete hypothesis about the cause or intended result.
3. Make the smallest change that addresses the cause.
4. Run the narrowest relevant validation, then broader checks when practical.

Do not hide unrelated failures by changing tests or weakening diagnostics. Do not revert changes that were already present unless explicitly asked.

## Language and Toolchain Compatibility

New and modified code must compile as C++17, the library's minimum; RAD Studio's 32-bit compiler (bcc32c) only supports C++17. C++20 features are allowed only as optional additions behind a feature-test check, with the C++17 path still complete. Test the library feature-test macro (e.g. `__cpp_char8_t`, `__cpp_lib_concepts`) rather than `__has_include`, since a header can be present but unusable. bcc32c (Clang 5 with the Dinkumware library) has no `<version>` header and defines no `__cpp_lib_*` macros at all, even for the C++17 features it does have (`std::string_view`, `std::optional`, `std::shared_mutex`, `<filesystem>`, `std::from_chars`), so never require a feature-test macro for a C++17 feature; use one only to switch a C++20 addition on. The CMake tests and RAD Studio Win64x build at C++20, so validate code with a C++20 path on both a C++17 build (RAD Studio Win32, or CMake with `-DASWINI_CXX_STANDARD=17`) and a C++20 build.

New and modified code must also be compatible with:

- RAD Studio 13.1 and later: bcc32c (Win32, C++17) and the modern Clang tool-set bcc64x (Win64x, C++20).
- CMake
- JetBrains CLion
- Visual Studio
- Clang
- MinGW
- Windows and Linux

Prefer standard C++ and portable library facilities. Avoid compiler-, IDE-, or operating-system-specific extensions unless they are isolated behind a clear portability boundary. For OS-specific code, use valid preprocessor checks such as `#if defined(_WIN32)` and provide the corresponding Linux or portable path where appropriate. Keep headers self-contained and avoid relying on transitive includes.

Keep all source files (`.cpp`/`.h`) plain ASCII, including comments. MSVC reads a BOM-less file using the system code page by default, unlike GCC/Clang/RAD Studio's Clang-based compilers, which default to UTF-8. A literal non-ASCII character can therefore silently compile to the wrong value on MSVC while working correctly everywhere else, with no error or warning. If a string needs a specific non-ASCII character, use a numeric escape instead of typing the character literally (e.g. `"\xC3\xA9"` for UTF-8 bytes, or `L"\x00E9"` for a wide-string code point), splitting adjacent escapes into separate concatenated string literals if needed so one escape doesn't consume a following hex digit. Avoid `u8""` literals for non-ASCII test data.

## Library Design Rules

- Loading and saving never throw: failures are returned as a result and reported to the application's log callback. The log callback is always called with no internal lock held, so it may call back into the same object.
- Files are read and written in binary mode as whole files, so line endings are handled by the library, not by the C runtime. A file that is loaded and saved without changes is written back byte for byte.
- Paths are UTF-8 in the API. On Windows, build `std::filesystem::path` objects from wide strings (MinGW's libstdc++ converts a narrow string to a path as UTF-8, but libc++ and MSVC use the ANSI code page).
- Value conversions are locale-independent; section and key names compare case-insensitively with ASCII-only folding.

## Code Organization

- Insert new class methods in alphabetical order within their existing section. If the class has named or visibly separated sections, preserve those sections and alphabetize only within the relevant section.
- Insert new free functions in alphabetical order within their existing section. Apply the same section rule when functions are grouped.
- Follow the surrounding naming, indentation, brace, include, and comment style. Avoid unrelated formatting changes.
- Each class has a banner (a full line of `/` characters, `// TClassName`, and an optional description, then another full line of `/`) above its declaration in the `.h` file, and the same banner, without the description, above its first definition in the `.cpp` file. Close a namespace with `} // namespace Name`.
- In a `.cpp` file, put the free functions before the definitions of the class methods. Helpers used only in that file go in an anonymous namespace at the top: declare them first, in alphabetical order, then define them in the same order, each after a `//---` line and a block comment that names it and says what it does (`/*`, the function's name, a blank line, the description, `*/`). Public free functions and methods are described in the header, not again in the `.cpp`.
- The same order holds inside the anonymous namespace: its constants and variables, then the declarations of classes used only in that file (with their banners), then the helper functions' declarations and definitions, and last the definitions of those classes' methods (see `TNativeFile` in `ASWIni_File.cpp`).
- End the last enumerator of an enumeration with a comma.
- Separate with a blank line: the `case` blocks of a `switch`; an `if` whose body returns, breaks or continues from the statement after it; a block's closing `}` from the statement after it (not from `else` or `while`); and a function's final `return` from the statements before it.
- Keep declarations and definitions consistent, and update all relevant project or build files when adding, removing or renaming source files, including `Deploy.bat`, which copies an explicit list of files.
- Add or update focused tests for behavior changes, including error and platform-specific cases where applicable.

## Unit Tests

The unit tests use the ASWUnitTests framework from the `third_party/asw-unit-tests` submodule; run `git submodule update --init` if it is empty. Test modules self-register with `ASW_REGISTER_TEST_GROUP` at the end of their `.cpp`, so adding a test only touches `tests/` and its build files.

- Each test group derives from `TAutoTestGroup<itself>`, is named `"ASWIni_<Unit>_Tests"` (or `"ASWIni_<Group>_Tests"` for one of several groups of a unit, e.g. `"ASWIni_ReadingOptions_Tests"`) in its constructor, and defines each test with `ASW_TEST(ClassName, TestName)` in the group's `.cpp`, declared as a public `void Test_TestName();` in its header (in a `public: // Test methods, each defined with ASW_TEST` section, alphabetical), with no `RegisterTest()` call. Use `Test_ASWIni_Version.cpp/h` as the template.
- Pass `__func__, __LINE__` to every check (e.g. `CheckEquals(expected, actual, __func__, __LINE__, "message")`). bcc32c has no `std::source_location`, so the overloads without them don't exist in its build.
- When a unit's tests are clearer as several groups (e.g. one per struct, as in `Test_ASWIni_Options`), keep them in the unit's one `.cpp`/`.h`, each class with its own banner, and register each with `ASW_REGISTER_TEST_GROUP_IN_SUITE(ClassName, "<Unit>")`, so `--filter "<Unit>.*"` runs them all and the GUI runner shows them under one node. A unit with one group isn't put in a suite.
- Structure a test with `// Arrange`, `// Act` and `// Assert` comments, each on its own line (combined as `// Act & Assert` or `// Arrange & Act & Assert` when the steps are interleaved), with any explanation on the line after the comment, and write each check's message as what should be true.
- When adding a test module or a test helper, add its `.cpp`/`.h` to `tests/cmake/CMakeLists.txt`, `tests/rad370/ASWIniTests.cbproj`, and `tests/vcl/gui/rad370/ASWIniTests_VCL_GUI.cbproj`, and keep their include paths in sync. When adding a library source file, add it to the root `CMakeLists.txt`, both RAD Studio test projects, and `Deploy.bat`.
- Never open the GUI's main form (`ASWUnitTests_GUI_MainForm`) in the RAD Studio designer and save it, since that writes into the submodule.

## Building and Testing

Build the unit tests with CMake:

```text
cmake -S tests/cmake -B tests/cmake/build
cmake --build tests/cmake/build --config Release
```

and with `-DASWINI_CXX_STANDARD=17` in a second build folder for a C++17 build. The executable is written to `tests/cmake/build/bin` (in a `Release` sub folder for multi-config generators).

The RAD Studio scripts need the `Rad370` environment variable to point to the RAD Studio installation: `tests/rad370/Build_Win32_Debug.bat`, `Build_Win32_Release.bat`, `Build_Win64x_Debug.bat` and `Build_Win64x_Release.bat` build the console tests into `tests/build/bin/<Platform>/<Config>/`. The Debug configurations pass `--pause` when run from the IDE. The VCL GUI runner builds with the scripts of the same names in `tests/vcl/gui/rad370/` into the same output folders. To run every test in it without interaction, use `start /wait ASWIniTests_VCL_GUI.exe --run --exit --layout-ignore --report-junit results.xml`; its exit code matches the console runner's.

Tests should be run outside the debugger unless debugging an expected exception is intentional. Before considering a change complete:

- Build the affected configuration, at C++17 and at C++20 (RAD Studio Win32 and Win64x when available).
- Run the unit-test executable.
- Check compiler warnings and errors.
- Validate both Windows-specific and portable paths when the change touches platform code.

## Changelog

`CHANGELOG.md` follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and Semantic Versioning.

- Record each notable change under `## [Unreleased]`, in the matching `### Added`, `### Changed`, `### Deprecated`, `### Removed`, `### Fixed`, or `### Security` section.
- Describe the effect on someone using the library, not the commit. For a breaking change, say what existing code must change.
- The version lives only in `ASWIni/ASWIni_Version.h` (the `ASWINI_VERSION_*` macros; the constants come from them). `main` carries only release versions, with an empty `ASWINI_VERSION_PRERELEASE`. Between releases, `develop` carries the next planned version with a pre-release such as `dev.1` (e.g. `1.1.0-dev.1` after `1.0.0`), which sorts before that release; bump its number only to tell dev builds apart, and never bump the patch number on `develop`. A unit test checks that `ASWINI_VERSION_STRING` matches the parts.
- When preparing a release, set the release version in `ASWIni/ASWIni_Version.h` (empty pre-release; the number develop forecast may change, e.g. to a patch release or the next major), rename `[Unreleased]` to `[x.y.z] - YYYY-MM-DD`, start a new empty `[Unreleased]`, and update the compare links at the bottom of the file. After merging the release into `develop`, move `develop` to the next minor version with pre-release `dev.1`.

## Formatting and Review

Use the repository's `.uncrustify.cfg` configuration. The README documents how to enable the local Git hook with:

```text
git config --local core.hooksPath .githooks/
```

Wrap Markdown prose at 100 columns in every `.md` file except this one (which keeps one line per paragraph), including `README.md`, `CONTRIBUTING.md`, `CHANGELOG.md` and the files in `docs/`, re-wrapping the edited paragraph when a change makes a line too long. Don't split inline code or a link's text across lines. Code blocks and tables are exempt, and so is a line that only overflows because a single inline code span, link or URL can't be split.

RAD Studio project files (`.cbproj`) are stored as RAD Studio writes them (UTF-8 with a BOM, CRLF line endings; `-text` in `.gitattributes`). Batch files need CRLF line endings in the working tree.

Review the final diff for accidental file changes, ordering regressions, missing includes, platform assumptions, and tests that do not actually exercise the changed behavior. Do not commit changes unless the user explicitly requests a commit.
