# ASWIni - https://github.com/ASWSoftware/asw-ini

ASWIni is a thread-safe, dependency-free C++17 library for reading and writing INI files on Windows
and Linux, which keeps the file's comments and layout.

## Status

ASWIni is in development toward 1.0.0: the `develop` branch carries `1.0.0-dev.N` versions, and
the interface may change in any way until 1.0.0. [CHANGELOG.md](CHANGELOG.md) lists what each
change adds.

## Design

ASWIni is a folder of standard C++ sources, or one CMake target, with no third-party code. It builds
with RAD Studio (bcc32c at C++17, bcc64x at C++20), MSVC, MinGW, GCC and Clang. Its goals:

- **Nothing in the file is lost.** A file that is loaded and saved without changes is written back
  byte for byte: comments, blank lines, spacing, the order of sections and keys, and lines it
  doesn't understand. Changing a value changes only that line.
- **Settings in one place.** An application declares each setting once, with its section, key,
  type, default value, comment and valid range. Loading brings the file up to date (it adds missing
  keys with their comments, and replaces values that aren't valid with the defaults, saying why),
  and each setting reads and writes its value in the file's document, from any thread.
- **Same file on every system.** Files are read and written in binary mode, so the line ending
  (LF or CRLF) is chosen by the library, not by the operating system.
- **Safe to use from several threads**, and saving replaces the file atomically, so a crash while
  saving leaves the previous file intact.
- **Problems are reported, not thrown.** Warnings and errors (a malformed line, an invalid value, a
  file that can't be written) go to a log callback, so the application logs them its own way.

## Features

- Settings declared once, as members of an application's settings class (`ASWIni::TASWIniSection`
  and `ASWIni::TASWIniSetting<T>`), each with its section, key, type, default value and comment,
  and optionally a range, a validator, a format of its own (e.g. a `bool` written as `1` and `0`)
  and the earlier names of a renamed key.
- Loading brings the file up to date: a missing key is added with its default value and comment, a
  value that isn't valid is replaced by the default, an old key is renamed, and each change is
  reported, so `IsModified()` says whether the file needs saving.
- A file that is loaded and saved without changes is written back byte for byte. An edit changes
  only the lines it is about; a new key goes after the last key of its section, and a new section
  at the end of the file.
- Sections, keys, values and comments (the file's, a section's and a key's) in file order, with
  keys before the first section allowed. Names compare case-insensitively (ASCII) by default. A
  duplicate key or section is kept and reported, and the first of the keys wins (or the last, as an
  option).
- `=` and `:` separators, written as `key = value` by default; `;` and `#` comments, written as
  `; comment`; optional inline comments (`key = value ; note`) and quoted values.
- UTF-8, with or without a byte order mark (kept as the file had it). LF, CRLF and CR line endings
  on read; on write, the file's own (the platform's for a new file), or one chosen.
- Typed values, the same in any locale: strings, file paths (UTF-8 on every platform), booleans
  (`true`/`false`, `yes`/`no`, `on`/`off`, `1`/`0`), every integer type with range checks (decimal
  or `0x` hex), `float` and `double` (the shortest text that reads back the same), enums by name,
  `std::u8string` (C++20) and lists of any of them, plus your own types (`TASWIniConverter<T>`).
- Values by section and key too (`GetValue()`, `TryGetValue()` and `SetValue()`), e.g. for
  `Window.*` sections a settings class doesn't declare, and `ASWIni::TASWIniDocument`, the document
  on its own, for INI text without a file or a lock.
- Safe to use from any number of threads with no locking by the application. Loading and saving
  read and write the file without holding the lock that lookups and edits use.
- Atomic saves (a crash while saving leaves the previous file whole), an optional backup, missing
  folders created, a size limit on loads, and UTF-8 file paths on Windows and Linux
  (`ASWIni::PathToUtf8()` and `Utf8ToPath()` convert a `std::filesystem::path`).
- No exceptions from loading, saving or editing: each returns a `TASWIniResult`, and warnings and
  errors (with the file, line, section and key) go to the application's log callback.
  [docs/Settings.md](docs/Settings.md) has a callback that sends them to ASWLog.
- A CMake target and package, and an example program with CMake and RAD Studio (Win32 and Win64x)
  projects.

# Donations:

If you find this library helpful, donations are always appreciated:

PayPal: donate@aswsoftware.com

Bitcoin: 15rKqL1numHJyE36ottMbhs5cmCjJkuowV

# How to Use

## Quick start

```cpp
#include <iostream>

#include "ASWIni_Base.h"
#include "ASWIni_Settings.h"

class TAppSettings : public ASWIni::TASWBaseIni
{
public:
    // The section "" holds the keys before the first header; its comment is the file comment
    ASWIni::TASWIniSection FileTop{ *this, "", "Settings of MyApp." };

    ASWIni::TASWIniSection General{ *this, "General", "What the application looks like." };
    ASWIni::TASWIniSetting<std::string> UserName{ General, "UserName", "Guest", "The name in the title bar." };
    ASWIni::TASWIniSetting<bool> ShowToolbar{ General, "ShowToolbar", true };

    ASWIni::TASWIniSection Logging{ *this, "Logging" };
    ASWIni::TASWIniSetting<unsigned> LogLevel{ Logging, "LogLevel", 2, "From 0 (errors only) to 4 (everything).",
        ASWIni::TASWIniSettingOptions<unsigned>().SetRange(0, 4) };
    // Called "LogDays" in an earlier version of the application, so Load() renames that key
    ASWIni::TASWIniSetting<unsigned> NDaysRetainLogs{ Logging, "NDaysRetainLogs", 30,
        "How many days the logs are kept; 0 keeps them forever.",
        ASWIni::TASWIniSettingOptions<unsigned>().AddOldKey("LogDays") };
};

// Where the library reports warnings, errors and notes; an application writes them to its log
void LogMessage(ASWIni::TASWIniMessage const& message)
{
    std::cout << ASWIni::ToString(message) << "\n";
}

int main()
{
    TAppSettings settings;
    settings.SetLogCallback(LogMessage);

    // On the first run the file is missing: Load() returns FileNotFound, reported as a warning, and the document
    // holds every setting with its default value and comment
    ASWIni::TASWIniResult const loaded = settings.Load("MyApp.ini");
    if (!loaded && loaded.Code != ASWIni::EIniCode::FileNotFound)
        return 1; // A file that can't be read: reported to LogMessage too

    if (settings.IsModified()) // Load() added or repaired a key: the file needs saving
        static_cast<void>(settings.Save());

    std::cout << "Hello, " << settings.UserName.Get() << "\n";
    static_cast<void>(settings.ShowToolbar.Set(false));
    static_cast<void>(settings.LogLevel.Set(7)); // Out of range: fails, is reported as an error, changes nothing

    return settings.Save() ? 0 : 1;
}
```

writes `MyApp.ini` (with the platform's line endings):

```ini
; Settings of MyApp.

; What the application looks like.
[General]
; The name in the title bar.
UserName = Guest
ShowToolbar = false

[Logging]
; From 0 (errors only) to 4 (everything).
LogLevel = 2
; How many days the logs are kept; 0 keeps them forever.
NDaysRetainLogs = 30
```

On the next run the file holds every setting, so `Load()` changes nothing. When a user has edited
it, `Load()` repairs it and reports each change: with `LogLevel = 9` and `LogDays = 7` in the file,
it reports that the default value 2 is used for `LogLevel` and that `LogDays` is renamed to
`NDaysRetainLogs`, adds any missing key and comment, and `IsModified()` is `true`. Lines the
settings class doesn't declare (a user's own comments, keys and sections) stay as they are.

The `example/src/main.cpp` program shows more: every kind of setting, a file a user edited, two
threads, values the settings class doesn't declare, and the document on its own.
[docs/Settings.md](docs/Settings.md) describes settings classes in full.

## Adding ASWIni to a project

### Copied sources (any build system)

Add every `.cpp` file in `ASWIni/` to the project, and the `ASWIni` folder to its include path, and
compile as C++17 or later. `Deploy.bat <folder>` copies the library sources, the build files, the
docs and the example to a folder, e.g. for a release package or to copy into another project.

### RAD Studio

Add the `ASWIni/*.cpp` files to the project (Project > Add to Project) and the `ASWIni` folder to
the include path (Project > Options > Building > C++ Shared Options > Include path). RAD Studio 13.1
or later, with bcc32c (Win32) or the modern Clang toolchain (Win64x). A Win32 project must use the
multithreaded runtime (the project's `Multithreaded` option), or code that uses a mutex doesn't
link. `example/rad370/ASWIniExample.cbproj` (Win32 and Win64x, with `Build_*.bat` scripts) is a
working project to copy settings from.

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
`-DASWINI_BUILD_EXAMPLE=ON` and `-DASWINI_BUILD_TESTS=ON` also build the example and the unit
tests, and `ASWINI_WARNINGS` (on by default only there) compiles the library with extra warnings.

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

## API overview

**Classes.** Everything is in the `ASWIni` namespace; a name in `ASWIni::Detail` is internal.

| Class | Header | What it is |
|---|---|---|
| `TASWBaseIni` | `ASWIni_Base.h` | The INI object: a document behind a reader/writer lock, with its file, options and log callback. An application's settings class derives from it. |
| `IASWIni` | `ASWIni_Interface.h` | The interface `TASWBaseIni` implements, for code that only reads and writes values. |
| `TASWIniSection`, `TASWIniSetting<T>` | `ASWIni_Settings.h` | A section and a setting, declared as members of a settings class. `TASWIniSettingOptions<T>` gives a setting its range, validator and other options. |
| `TASWIniDocument` | `ASWIni_Document.h` | The lossless model of an INI text: its sections (`TASWIniDocumentSection`) and their lines (`TASWIniDocumentLine`). A copyable value, with no file and no lock. |
| `TASWIniOptions` | `ASWIni_Options.h` | How text is read, written and saved, and how values are converted. |
| `TASWIniConverter<T>` | `ASWIni_Converters.h` | Converts a value to and from its text; specialize it for a type of your own. An enum is written by name when it has `ASWIniEnumNames()` (`TASWIniEnumName<T>`). |
| `TASWIniResult`, `TASWIniMessage` | `ASWIni_Types.h` | The outcome of an operation, and a warning, error or note reported to the log callback, each with its `EIniCode`. |

`PathToUtf8()` and `Utf8ToPath()` (`ASWIni_File.h`) convert a `std::filesystem::path` to and from
the UTF-8 path text the library takes, and `ASWIni_Version.h` has the version.

**Files.** On a `TASWBaseIni`: `Load(path)`, `Save()`, `SaveAs(path)`, `IsModified()` and
`GetFilePath()`; `LoadText(text)` and `ToText()` for a text; `SetOptions()` (before `Load()`) and
`GetOptions()`; `SetLogCallback()`.

**Settings.** On a `TASWIniSetting<T>`: `Get()`, `Set(value)` (which fails for a value out of the
range or one the validator refuses), `Reset()` and `IsPresent()`. Its options, in one expression:
`TASWIniSettingOptions<T>().SetRange(min, max)`, and `SetMin()`, `SetMax()`, `SetValidator()`,
`SetOptional()`, `AddOldKey()` and `SetValues()`.

**Values by section and key**, e.g. of sections a settings class doesn't declare:
`GetValue(section, key, default)`, `TryGetValue(section, key, value)` (which returns why it
couldn't), `SetValue(section, key, value)`, and a value's text as it is with `TryGetText()` and
`SetText()`. `HasSection()`, `HasKey()`, `GetSectionNames(prefix)`, `GetKeyNames()`,
`AddSection()`, `RemoveSection()`, `RemoveKey()`, `RenameSection()` and `RenameKey()`. The comments:
`GetFileComment()`, `GetSectionComment()` and `GetKeyComment()`, and their `Set` methods.

**The document.** `GetDocument()` gives a copy of an INI object's document. A `TASWIniDocument` of
its own has `Parse()` and `ToText()`, the same lookups, edits and comments (with `FindSection()`,
`FindKey()` and a `SetValue()` that takes text), and its sections and lines (`GetSections()`).

**Results.** Each operation that can fail returns a `TASWIniResult` (`true` when it succeeded, else
with its `Code` and `Message`) and reports the failure to the log callback, without throwing.
`ToString()` names a code and writes a message as one line. The header comments document every
class and operation.

## Documentation

- [docs/Format.md](docs/Format.md): the INI format ASWIni reads and writes: lines, sections, keys,
  comments, quoted values, encoding and line endings, what an edit changes, and the text of each
  type of value.
- [docs/Options.md](docs/Options.md): every option, its default and what it does, and when options
  apply.
- [docs/Settings.md](docs/Settings.md): settings classes: declaring and using settings, what
  `Load()` does, comments, threads, the raw API alongside, your own types, the messages, and
  sending them to ASWLog.
- [docs/Guarantees.md](docs/Guarantees.md): what is returned and reported instead of thrown, thread
  safety, what callbacks may do, object lifetimes, saving and crashes, and every `EIniCode`.
- [CHANGELOG.md](CHANGELOG.md): the changes in each release.

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
