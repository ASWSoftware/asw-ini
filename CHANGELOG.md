# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and this project
adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- The ASWIni version in `ASWIni_Version.h`, as macros (`ASWINI_VERSION_MAJOR`, `_MINOR`, `_PATCH`,
  `_PRERELEASE` and `_STRING`) and as constants in the `ASWIni` namespace (`VersionMajor`,
  `VersionMinor`, `VersionPatch`, `VersionPreRelease` and `Version`), starting at `1.0.0-dev.1`.
- The `ASWIni::ASWIni` CMake target (a static library that needs C++17), with install rules and a
  CMake package, so `find_package(ASWIni CONFIG)` finds an installed copy.
- `Deploy.bat`, which copies the library sources, build files and docs to a folder.
- The shared types in `ASWIni_Types.h`: `EIniCode` (what happened, with values that never change),
  `ESeverity`, `TASWIniMessage` (a warning or error for the application's log callback, with the
  file, line, section, key and system error it is about) and `TASWIniResult` (an operation's
  outcome, which converts to `true` when it succeeded). `ToString()` names a code or a severity, and
  writes a message as one line of text. New sources: `ASWIni_Types.cpp` and `ASWIni_Utils.cpp`.

[Unreleased]: https://github.com/ASWSoftware/asw-ini/commits/develop
