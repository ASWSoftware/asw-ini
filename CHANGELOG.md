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
  `ELineBreak` (`None`, `LF`, `CR` or `CRLF`: the line break that ends a line), `ESeverity`,
  `TASWIniMessage` (a warning or error for the application's log callback, with the file, line,
  section, key and system error it is about) and `TASWIniResult` (an operation's outcome, which
  converts to `true` when it succeeded). `ToString()` names a code, a line break or a severity, and
  writes a message as one line of text. New sources: `ASWIni_Types.cpp` and `ASWIni_Utils.cpp`.
- `TASWIniOptions` (`ASWIni_Options.h`), the settings for INI files in three groups:
  - `Reading` (`TASWIniReadingOptions`): the separators (`=` and `:`) and comment prefixes (`;`
    and `#`) read, inline comments and quoted values (both off), case-sensitive names (off), which
    duplicate key wins (the first), strict parsing (off) and the largest file loaded (16 MiB).
  - `Writing` (`TASWIniWritingOptions`): the separator (`" = "`) and comment prefix (`"; "`) of new
    lines, the line ending and byte order mark (both as the loaded file had them), and a blank line
    before a new section (on).
  - `Saving` (`TASWIniSavingOptions`): atomic saves (on), backups (off, `.bak`) and creating
    missing folders (on).

  Each group's `Validate()` checks its own settings, and `TASWIniOptions::Validate()` also checks
  that a file written with them can be read back (e.g. `Writing.Separator` must be one of
  `Reading.Separators`); a problem is `EIniCode::InvalidOptions` with a message naming the setting.
  New source: `ASWIni_Options.cpp`.
- The file layer that loading and saving will use (`ASWIni_File.cpp`, internal): whole files read
  and written in binary mode with the operating system's own file APIs, a size limit on reads, and
  saves that replace the file atomically (keeping its attributes on Windows, and its permissions
  and symbolic link elsewhere), with an optional backup and missing folders created. File paths
  are UTF-8.
- `TASWIniDocument` (`ASWIni_Document.h`), the lossless model of an INI text: its sections in file
  order (`TASWIniDocumentSection`, the preamble before the first header first), each with its lines
  (`TASWIniDocumentLine`: the text as read, its line break, its kind (`ELineKind`: `Blank`,
  `Comment`, `Section`, `KeyValue` or `Raw`) and its key, value, separator, section name and
  comment), plus whether the text had a UTF-8 byte order mark and the line break new lines get. It
  is copyable and not thread-safe. New source: `ASWIni_Document.cpp`.
- `TASWIniDocument::Parse()` reads UTF-8 text (with or without a byte order mark, with LF, CRLF or
  CR line breaks) as the reading options say, keeping every line. Lines that can't be parsed are
  kept as `Raw` lines and reported as warnings (`MalformedLine`, `UnclosedSection`), as are
  duplicate sections and keys and the first line that isn't valid UTF-8; each warning has its line
  number and section. It fails with `InvalidOptions`, `UnsupportedEncoding` (a UTF-16 or UTF-32
  byte order mark) or, with `StrictParsing`, `ParseFailed` for the first malformed line, and then
  leaves the document and the messages unchanged.
- `TASWIniDocument::ToText()` writes the document back as text, as the writing options say. With
  the defaults, a parsed text comes back byte for byte. `ELineEnding::Auto` keeps each line's own
  line break, while `LF`, `CRLF` and `Native` give every line that one. A last line without a line
  break never gets one. The UTF-8 byte order mark follows `Writing.Bom` (`Auto`: as the parsed text
  had it).

[Unreleased]: https://github.com/ASWSoftware/asw-ini/commits/develop
