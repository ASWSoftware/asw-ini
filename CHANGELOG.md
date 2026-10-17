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
  keeps its `TASWIniOptions` (`GetOptions()`), which only change when it is parsed again or reset
  (`Reset()` empties it and gives it new options), and whether an edit changed it (`IsModified()`,
  `ClearModified()`). It is copyable and not thread-safe. New source: `ASWIni_Document.cpp`.
- `TASWIniDocument::Parse()` reads UTF-8 text (with or without a byte order mark, with LF, CRLF or
  CR line breaks) as the options' `Reading` group says, keeping every line, and keeps the options.
  Lines that can't be parsed are kept as `Raw` lines and reported as warnings (`MalformedLine`,
  `UnclosedSection`), as are duplicate sections and keys and the first line that isn't valid UTF-8;
  each warning has its line number and section. It fails with `InvalidOptions` (any group, see
  `TASWIniOptions::Validate()`), `UnsupportedEncoding` (a UTF-16 or UTF-32 byte order mark) or,
  with `StrictParsing`, `ParseFailed` for the first malformed line, and then leaves the document
  and the messages unchanged.
- `TASWIniDocument::ToText()` writes the document back as text, as its `Writing` options say. With
  the defaults, a parsed text comes back byte for byte. `ELineEnding::Auto` keeps each line's own
  line break, while `LF`, `CRLF` and `Native` give every line that one. A last line without a line
  break never gets one. The UTF-8 byte order mark follows `Writing.Bom` (`Auto`: as the parsed text
  had it).
- Lookups by name in `TASWIniDocument`: `FindSection()` (the first section of that name; `""` is
  the part before the first header), `FindKey()` (a key's line, or `nullptr`), `GetSectionNames()`
  (optionally only the names that start with a prefix, e.g. `"Window."`) and `GetKeyNames()`.
  Names compare as `Reading.CaseSensitive` says, sections of the same name count as one, and of a
  key that appears more than once, `Reading.DuplicateKeys` picks the first or the last line.
- `TASWIniDocument::SetValue()` and `AddSection()`. Changing a key's value replaces only the value
  of the line `FindKey()` finds, so its indentation, separator, spacing, quotes and inline comment
  stay. A new key goes after the last key of its section (or right after the header, or in the
  part before the first header under the file comment) as `key = value` with `Writing.Separator`,
  and a new section goes at the end of the file, after a blank line
  (`Writing.BlankLineBeforeNewSection`). New lines get the file's line break. A value is quoted
  when it needs it to read back (with `Reading.QuotedValues`); a name or value that wouldn't read
  back as it is (e.g. a line break, a key with a separator, a section name with `]`) fails with
  `InvalidArgument` and changes nothing. Setting the value a key already has changes nothing, and
  doesn't set `IsModified()`.
- `TASWIniDocument::RemoveKey()`, `RemoveSection()`, `RenameKey()` and `RenameSection()`. A
  removal takes every line of the key, or every section of the name, with the comment lines
  directly above it (a removed section's lines that are the next section's comment stay), and
  returns whether anything was removed. A rename changes only the name, in every line or header
  of that name, and may change only its case. Renames fail with the new codes
  `EIniCode::NotFound` (no such key or section) and `EIniCode::AlreadyExists` (the new name is
  taken), or with `InvalidArgument` for a name that wouldn't read back or for the preamble (`""`),
  which has no header.
- Comments in `TASWIniDocument`: `GetFileComment()`/`SetFileComment()` (the comment lines at the
  start of the file, before a blank line), `GetKeyComment()`/`SetKeyComment()` (the comment lines
  directly above a key) and `GetSectionComment()`/`SetSectionComment()` (directly above a header).
  A comment reads as its lines' texts joined with `"\n"`. Setting one writes a line per line of the
  text (each trimmed) with `Writing.CommentPrefix` and the key's or header's indentation, replacing
  the old lines; an empty text removes them, and a new file comment gets a blank line under it. A
  text the comment already reads as changes nothing, whatever prefix its lines use. Inline and
  header comments (`k = v ; note`) stay as they are.
- After every edit, the text a `TASWIniDocument` writes reads back as the lines it holds. When an
  edit would put an empty line that ends with LF right after a line that ends with CR (which would
  read back as one CRLF line break), that line gets a CRLF line break instead.

[Unreleased]: https://github.com/ASWSoftware/asw-ini/commits/develop
