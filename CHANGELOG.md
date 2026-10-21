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
- `Deploy.bat`, which copies the library sources, build files, docs and example to a folder.
- The shared types in `ASWIni_Types.h`: `EIniCode` (what happened, with values that never change),
  `ELineBreak` (`None`, `LF`, `CR` or `CRLF`: the line break that ends a line), `ESeverity`,
  `TASWIniMessage` (a warning or error for the application's log callback, with the file, line,
  section, key and system error it is about) and `TASWIniResult` (an operation's outcome, which
  converts to `true` when it succeeded). `ToString()` names a code, a line break or a severity, and
  writes a message as one line of text. New sources: `ASWIni_Types.cpp` and `ASWIni_Utils.cpp`.
- `TASWIniOptions` (`ASWIni_Options.h`), the settings for INI files in four groups:
  - `Reading` (`TASWIniReadingOptions`): the separators (`=` and `:`) and comment prefixes (`;`
    and `#`) read, inline comments and quoted values (both off), case-sensitive names (off), which
    duplicate key wins (the first), strict parsing (off) and the largest file loaded (16 MiB).
  - `Writing` (`TASWIniWritingOptions`): the separator (`" = "`) and comment prefix (`"; "`) of new
    lines, the line ending and byte order mark (both as the loaded file had them), a blank line
    before a new section (on), and whose comment a setting's key, a section and the file get when
    the file's differs from the code's (`CommentPolicy`: `ECommentPolicy::CodeWins`, the default,
    or `FileWins`, which writes the code's only where the file has none).
  - `Saving` (`TASWIniSavingOptions`): atomic saves (on), backups (off, `.bak`) and creating
    missing folders (on).
  - `Values` (`TASWIniValueOptions`): how a bool is written (`EBoolFormat`: `TrueFalse`, the
    default, `YesNo`, `OnOff` or `OneZero`) and the character between the items of a list (`,`).

  Each group's `Validate()` checks its own settings, and `TASWIniOptions::Validate()` also checks
  that a file written with them can be read back (e.g. `Writing.Separator` must be one of
  `Reading.Separators`); a problem is `EIniCode::InvalidOptions` with a message naming the setting.
  New source: `ASWIni_Options.cpp`.
- The file layer that loading and saving use (`ASWIni_File.cpp`, internal): whole files read and
  written in binary mode with the operating system's own file APIs, a size limit on reads, and
  saves that replace the file atomically (keeping its attributes on Windows, and its permissions
  and symbolic link elsewhere), with an optional backup and missing folders created. File paths
  are UTF-8: `ASWIni::PathToUtf8()` and `ASWIni::Utf8ToPath()` (`ASWIni_File.h`, its only public
  part) convert a `std::filesystem::path` to and from that form on every platform, e.g.
  `ini.Load(ASWIni::PathToUtf8(folder / "app.ini"))` (on Windows through the path's wide form,
  since a narrow `std::filesystem::path` string is in the ANSI code page with MSVC and libc++).
- `TASWIniDocument` (`ASWIni_Document.h`), the lossless model of an INI text: its sections in file
  order (`TASWIniDocumentSection`, the preamble before the first header first), each with its lines
  (`TASWIniDocumentLine`: the text as read, its line break, its kind (`ELineKind`: `Blank`,
  `Comment`, `Section`, `KeyValue` or `Raw`) and its key, value, separator, section name and
  comment), plus whether the text had a UTF-8 byte order mark and the line break new lines get. It
  keeps its `TASWIniOptions` (`GetOptions()`), which only change when it is parsed again, reset
  (`Reset()` empties it and gives it new options) or given new ones with `SetOptions()`, and
  whether an edit changed it (`IsModified()`, `ClearModified()`). It is copyable and not
  thread-safe. New source: `ASWIni_Document.cpp`.
- `TASWIniDocument::Parse()` reads UTF-8 text (with or without a byte order mark, with LF, CRLF or
  CR line breaks) as the options' `Reading` group says, keeping every line, and keeps the options.
  Lines that can't be parsed are kept as `Raw` lines and reported as warnings (`MalformedLine`,
  `UnclosedSection`), as are duplicate sections and keys and the first line that isn't valid UTF-8;
  each warning has its line number and section. With `Reading.QuotedValues`, a value in double
  quotes keeps its blanks and loses its quotes, and `""` inside them is one `"` (`"say ""hi"""` is
  `say "hi"`). It fails with `InvalidOptions` (any group, see `TASWIniOptions::Validate()`),
  `UnsupportedEncoding` (a UTF-16 or UTF-32 byte order mark) or, with `StrictParsing`,
  `ParseFailed` for the first malformed line, and then leaves the document and the messages
  unchanged.
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
  when it needs it to read back (with `Reading.QuotedValues`, each `"` in it written as `""`); a
  name or value that wouldn't read back as it is (e.g. a line break, a key with a separator, a
  section name with `]`) fails with `InvalidArgument` and changes nothing. Setting the value a key
  already has changes nothing, and doesn't set `IsModified()`.
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
- `TASWIniDocument::SetOptions()` gives a document new options and reads its text again with them,
  so its keys, values and comments are what the new reading options find (e.g. a `key: value` line
  becomes a key once `:` is a separator). The text stays byte for byte, and so do the byte order
  mark, the line break new lines get and `IsModified()`. The warnings of the new reading are
  reported as `Parse()` reports them; invalid options, or a malformed line with `StrictParsing`,
  fail and change nothing.
- `TASWIniConverter<T>` (`ASWIni_Converters.h`), which converts a typed value to and from the text
  of an INI value as `TASWIniOptions::Values` says, the same in any locale: `bool` (reads `true`,
  `false`, `yes`, `no`, `on`, `off`, `1` and `0` in any case; writes the `Values.BoolFormat` form),
  the integer types (every integral type except `bool`, `wchar_t`, `char8_t`, `char16_t` and
  `char32_t`, so `char`, `std::int8_t` and `std::uint8_t` are numbers; reads decimal or `0x` hex
  digits after an optional sign, checked against the type's range; writes decimal), `float` and
  `double` (read digits with an optional `.` and exponent after an optional sign, or `inf`,
  `infinity` or `nan`; a number too large is out of range, one too small reads as 0; written as the
  shortest text that reads back the same, as `std::to_chars()` writes it, e.g. `0.1`, `3`, `1e+21`),
  enums (by name when the application declares `ASWIniEnumNames()` next to the enum, which returns
  its names as `TASWIniEnumName<T>` entries: reads a name in any case, or the number of a listed
  value; writes the first name listed for the value, so the others are aliases that are only read;
  an enum without `ASWIniEnumNames()` converts as its underlying integer type), `std::string` (the
  text as it is), `std::filesystem::path` (the text is the path in UTF-8 on every platform; a text
  that isn't valid UTF-8, or a path that wouldn't read back the same, is `InvalidValue`),
  `std::u8string` (the bytes as they are; in a C++20 application, even with the library built at
  C++17, as `ASWINI_HAS_U8STRING` in the new `ASWIni_Platform.h` says) and `std::vector<T>` of any
  of them (a list: items separated by `Values.ListDelimiter`, written with the delimiter and a
  space; an item is in double quotes, with `""` for a `"`, only when it holds the delimiter or a
  `"`, has blanks at either end, or is the only item and empty; a `"` elsewhere in an item is
  ordinary; lists of lists work too). An application adds a converter for its own type by
  specializing the template. A text that isn't a value of the type fails with the new code
  `EIniCode::InvalidValue` (a number outside its type's range with `EIniCode::ValueOutOfRange`),
  with a message that quotes the text, and leaves the value as it was. New sources:
  `ASWIni_Converters.cpp` and `ASWIni_Platform.cpp`.

  RAD Studio Win32 (bcc32c) converts `float` and `double` with the C runtime instead of
  `std::to_chars()`/`std::from_chars()`, which give wrong results there, reading with Windows'
  Universal C Runtime (`ucrtbase.dll`) when it is there. On a Windows without it, about 1 in 12,000
  doubles written on another platform reads back 1 unit in the last place off. The macro
  `ASWINI_STD_FLOAT_CHARCONV` (defined as 0 when building the library) chooses the C runtime for
  another standard library without floating-point `<charconv>`.
- `IASWIni` (`ASWIni_Interface.h`), the interface of an INI object (which `TASWBaseIni`
  implements), so code that only reads and writes values can take an `IASWIni&`. Its string-based
  operations are each thread-safe and complete in themselves, and give out copies: the lookups,
  edits and comments of `TASWIniDocument`, with `TryGetText()` and `SetText()` for a value's text,
  `GetDocument()` (a copy), `ToText()`, `LoadText()`, `Load()`, `Save()`, `SaveAs()`, the options
  and `SetLogCallback()` (a `TASWIniLogCallback`). The typed `GetValue()`, `TryGetValue()` and
  `SetValue()` are built on them with `TASWIniConverter<T>` and the value options, or options given
  for one call. `GetValue()` gives the default for a missing key, and for a value that doesn't
  convert, which it reports to the log callback as a warning. `TryGetValue()` returns why it
  couldn't read the value (`EIniCode::NotFound` for a missing key, or the converter's code, with a
  message that names the section and key), reports nothing and leaves the value as it was.
  `SetValue()` fails, and reports it, when the converter can't write the value. A text (a string
  literal, a `char` array or pointer, or a `std::string_view`) is read and set as it is. The
  operations that return a `TASWIniResult` don't throw: an exception, e.g. one thrown by an
  application's converter, is returned as `EIniCode::UnexpectedException`. New source:
  `ASWIni_Interface.cpp`.
- `TASWBaseIni` (`ASWIni_Base.h`), the thread-safe INI object that implements `IASWIni`, which an
  application's settings class derives from: a `TASWIniDocument` behind a reader/writer lock, so any
  thread may call its operations at any time; lookups share the lock and edits take it alone.
  `Load()` reads and parses a file, and `LoadText()` a text, without holding the lock, and then
  swaps the new document in. A load that fails changes nothing, except for a missing file (an
  application's first run): it empties the document and becomes the file path, so `Save()` creates
  it, and is reported as a warning. `Save()` and `SaveAs()` write the text without holding the lock
  (atomically, by default), even when the document isn't modified, and clear the modified flag
  unless an edit was made meanwhile; `SaveAs()` makes the file the file path when it succeeds.
  Loads, saves and `SetOptions()`, which reads the text again with the new options, never overlap.
  `GetDocument()` gives a copy of the document. A derived class may override the protected
  `ReadFile()` and `WriteFile()`, which read and write the file. Every failed operation and every
  warning (e.g. of `Load()` or `Save()`, with the file's path) is reported to the log callback,
  after the locks are released, so the callback may call the object, even `Load()`, `Save()` or
  `SetLogCallback()`; an exception it throws is dropped. Any thread may replace the callback at any
  time; a report already under way may still go to the callback it replaced. An exception inside an
  operation that returns a `TASWIniResult` (in practice `std::bad_alloc`) is returned as
  `EIniCode::UnexpectedException`, with nothing changed. On RAD Studio Win32 (bcc32c) the locks are
  Windows slim reader/writer locks, since that compiler's `std::mutex` and `std::shared_mutex` wait
  on a kernel mutex for every lock (about 1000 times slower when threads contend);
  `ASWINI_WINDOWS_LOCKS` says which are used. The `ASWIni::ASWIni` CMake target now links to
  `Threads::Threads`, and the installed package finds it. New source: `ASWIni_Base.cpp`.
- `TASWIniSection` and `TASWIniSetting<T>` (`ASWIni_Settings.h`), which declare an application's
  settings in one place, as members of its settings class (derived from `TASWBaseIni`), instead of
  code that finds, checks and writes each key by hand:

  ```cpp
  class TAppSettings : public ASWIni::TASWBaseIni
  {
  public:
      ASWIni::TASWIniSection General{ *this, "General" };
      ASWIni::TASWIniSetting<unsigned> LogLevel{ General, "LogLevel", 2, "From 0 (errors) to 4 (all).",
          ASWIni::TASWIniSettingOptions<unsigned>().SetRange(0, 4) };
      ASWIni::TASWIniSetting<unsigned> NDaysRetainLogs{ General, "NDaysRetainLogs", 30, "0 = forever.",
          ASWIni::TASWIniSettingOptions<unsigned>().AddOldKey("LogDays") };
  };
  ```

  A section has its owner, name and comment. A setting has its section, key, type (any type
  `TASWIniConverter` converts), default value, comment and options (`TASWIniSettingOptions<T>`): a
  range (`SetRange()`, `SetMin()` or `SetMax()`, for a type that `operator<` compares), a validator
  that returns what is wrong with a value, `SetOptional()` for a key that isn't added when it is
  missing, value options of its own (e.g. a `bool` written as `1` and `0`), and the earlier names
  of a renamed key, in the same section (`AddOldKey()`). New source: `ASWIni_Settings.cpp`.

  A setting holds no value of its own. `Get()` reads the owner's document, giving the default value
  for a missing key, and for a value that doesn't convert, is out of the range or that the validator
  refuses (reported as a warning that names the default). `Set()` writes the document, adding a
  missing key with the setting's comment above it in the same edit, and fails, reports it and
  changes nothing for a value out of the range (`ValueOutOfRange`) or one the validator refuses
  (`InvalidValue`). `Reset()` sets the default value, and `IsPresent()` says whether the key is in
  the document. Any thread may call them, as the owner's operations.

  Sections and settings are bound to their owner from their constructors to their destructors, in
  the order they are declared. When `Load()` or `LoadText()` has read a document, before anyone
  sees it, each setting is applied to it: a missing key whose earlier name is there is renamed,
  keeping its line, value and comment, reported as an Info message with the new code
  `EIniCode::KeyRenamed` (when the key is there too, the old one is left alone); any other missing
  key is added with its default value and comment, reported as an Info message with the new code
  `EIniCode::KeyAdded`; a value that isn't valid is replaced by the default value, reported as a
  warning; and the comments of the keys, of the sections that are in the document and of the file
  (the comment of the section `""`) are set to the code's, as `Writing.CommentPolicy` says. So the
  loaded document holds every setting, and `IsModified()` says whether the file needs saving, e.g.
  on an application's first run, when the file is missing. A file that holds every setting with
  the code's comments stays byte for byte, and a load that fails (e.g. out of memory) changes
  nothing and reports none of these changes. A default value that can't be written, or that its
  own range or validator refuses, is reported as an error. `SetOptions()` doesn't apply the
  settings.
- An example program, `example/src/main.cpp`: an application's settings class (with a range, a
  validator, an enum written by name, a bool written as `1` and `0`, an optional setting and an
  old key), its first run, a file a user edited by hand, a setting used from two threads, values
  the class doesn't declare, and a `TASWIniDocument` on its own. It prints each step, the messages
  the library reports and the file, and returns 0 when every step did what it shows. Build it with
  `example/cmake`, with the root project and `-DASWINI_BUILD_EXAMPLE=ON`, or with RAD Studio's
  `example/rad370/ASWIniExample.cbproj` (Win32 and Win64x).

[Unreleased]: https://github.com/ASWSoftware/asw-ini/commits/develop
