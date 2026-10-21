# Options reference

How INI text is read, written and saved, and how values are converted, is set by one
`ASWIni::TASWIniOptions` (`ASWIni_Options.h`). Its settings are in four groups: `Reading`,
`Writing`, `Saving` and `Values`. The defaults read the common INI dialects and write `key = value`
and `; comment` lines, atomically, so most applications change nothing, or a setting or two.

```cpp
ASWIni::TASWBaseIni ini;
ASWIni::TASWIniOptions options = ini.GetOptions();
options.Reading.InlineComments = true; // "Width = 640 ; pixels" has the value 640
options.Writing.Separator = "=";       // New lines are written as key=value
options.Saving.Backup = true;          // Each save keeps the previous file as app.ini.bak

ASWIni::TASWIniResult const set = ini.SetOptions(options);
if (!set)
    std::cerr << set.Message << "\n"; // What is wrong, naming the setting

ASWIni::TASWIniResult const loaded = ini.Load("app.ini");
```

The format each option changes is described in [Format.md](Format.md).

## When options apply

- An INI object (`TASWBaseIni`) reads, writes and saves with its options, which start as the
  defaults. `SetOptions()` replaces them, and reads the loaded text again with them, so a
  `key: value` line becomes a key once `:` is a separator; the text itself, the file path and
  `IsModified()` stay. Set the options before the first `Load()`: a settings class does it in its
  constructor (see [Settings.md](Settings.md)). `GetOptions()` gives a copy.
- `SetOptions()` checks the options first (see [Validation](#validation)), and invalid ones change
  nothing: it returns `InvalidOptions`, with a message that names the setting, and reports it to
  the log callback. A `TASWIniDocument`'s `Parse()`, `Reset()` and `SetOptions()` check them the
  same way.
- `Load()`, `LoadText()`, `Save()`, `SaveAs()` and `SetOptions()` never overlap, so a load or a
  save uses one set of options from start to end.
- The value options can also be given for one call (`GetValue()`, `TryGetValue()` and `SetValue()`
  take a `TASWIniValueOptions` after their other arguments) or for one setting
  (`TASWIniSettingOptions::SetValues()`), e.g. for a key whose bool is written as `1` and `0`:

```cpp
ASWIni::TASWIniValueOptions oneZero = ini.GetValueOptions();
oneZero.BoolFormat = ASWIni::EBoolFormat::OneZero;
ASWIni::TASWIniResult const result = ini.SetValue("Window", "Maximized", true, oneZero); // Maximized = 1
```

## `Reading`: how text is read (`TASWIniReadingOptions`)

| Setting | Default | What it does |
|---|---|---|
| `Separators` | `"=:"` | The characters that can separate a key from its value. The first of them on a line ends the key. |
| `CommentPrefixes` | `{ ";", "#" }` | The texts that start a comment line, after any indentation. Add `"//"` for files that use it. |
| `InlineComments` | `false` | Whether a comment prefix after a blank in a value starts a comment (`key = value ; note`). Off, since values such as connection strings and colors hold the prefixes. |
| `QuotedValues` | `false` | Whether a value in double quotes keeps its blanks and loses its quotes (`""` inside is one `"`), and whether a value that needs quotes is written in them. |
| `CaseSensitive` | `false` | Whether section and key names are case-sensitive. When not, they compare with the ASCII letters' case ignored. |
| `DuplicateKeys` | `FirstWins` | Which line's value a key that appears more than once in a section has: `EDuplicateKeys::FirstWins` (as the Windows profile API reads them) or `LastWins`. Every line is kept, and the duplicate is reported as a warning either way. |
| `StrictParsing` | `false` | Whether a line that can't be parsed fails the load (`ParseFailed`), instead of being kept as it is with a warning. |
| `MaxFileSize` | 16 MiB | The largest file a load reads, in bytes; a larger one fails with `FileTooLarge`. `DefaultMaxFileSize` is the default. |

`InlineComments` and `QuotedValues` also decide how values are written, so that a value always reads
back the same: with `QuotedValues`, a value with blanks at either end, a leading `"`, or (with
`InlineComments`) a comment prefix after a blank is written in quotes; without it, setting such a
value fails with `InvalidArgument`.

## `Writing`: how lines are written (`TASWIniWritingOptions`)

A line read from a file is written back as it was, unless its value changes, and then it keeps its
own separator and spacing. These settings are for the lines an edit adds, and for the text as a
whole.

| Setting | Default | What it does |
|---|---|---|
| `Separator` | `" = "` | What is written between the key and the value of a new line: one of `Reading.Separators`, with optional spaces or tabs around it, e.g. `"="`, `" = "` or `": "`. |
| `CommentPrefix` | `"; "` | What is written before the text of a new comment line, so the comment `Valid range: 0-4` is `; Valid range: 0-4`. It must start with one of `Reading.CommentPrefixes`. |
| `LineEnding` | `Auto` | The line endings of the text (`ELineEnding`): `Auto` keeps each line's own, and gives a new line the file's most common one (the platform's for a new file); `Native` gives every line the platform's (CRLF on Windows, LF elsewhere); `LF` and `CRLF` give every line that one. |
| `Bom` | `Auto` | Whether the text starts with a UTF-8 byte order mark (`EBomMode`): `Auto`, as the loaded file did (none for a new file), `Always` or `Never`. |
| `BlankLineBeforeNewSection` | `true` | Whether a new section is written after a blank line (unless it is the first thing in the file). |
| `CommentPolicy` | `CodeWins` | Whose comment a setting's key, a section and the file get when the loaded file's differs from the one the code declares (`ECommentPolicy`): `CodeWins`, the code's, written over a comment edited in the file, or `FileWins`, the file's, with the code's written only where the file has none. A comment the code doesn't declare is never changed. See [Settings.md](Settings.md). |

## `Saving`: how files are saved (`TASWIniSavingOptions`)

| Setting | Default | What it does |
|---|---|---|
| `AtomicSave` | `true` | Whether a save writes a temporary file in the same folder, flushes it to the disk and then replaces the file with it, so a failure or a crash while saving leaves the previous file whole. Turn it off only when the folder can't be written to: the file itself is then overwritten in place. |
| `Backup` | `false` | Whether a save first copies the previous file to its name followed by `BackupSuffix`, replacing an older backup. A backup that fails is reported as a warning (`BackupFailed`), and the file is still saved. |
| `BackupSuffix` | `".bak"` | Added to the file name for the backup, e.g. `app.ini.bak`. |
| `CreateFolders` | `true` | Whether a save creates the file's folder, and the folders above it, when they don't exist. When off, a save to a missing folder fails with `FileNotFound`. |

An atomic save keeps the file's attributes and security on Windows, and its permissions (and, when
allowed, its owner) elsewhere, where a file that is a symbolic link stays one: the file it points to
is replaced.

## `Values`: how values are converted (`TASWIniValueOptions`)

| Setting | Default | What it does |
|---|---|---|
| `BoolFormat` | `TrueFalse` | How a `bool` is written (`EBoolFormat`): `TrueFalse` (`true`/`false`), `YesNo`, `OnOff` or `OneZero` (`1`/`0`). Every form is read, whichever is written. |
| `ListDelimiter` | `','` | The character between the items of a list (a `std::vector`), which are written with it and one space (`a, b, c`). An item that holds it is written in double quotes. |

## Validation

Each group's `Validate()` checks its own settings, and `TASWIniOptions::Validate()` checks the four
groups and then that a file written with them reads back. Each returns `Ok`, or `InvalidOptions`
with a message about the first problem it finds. The rules:

- `Reading.Separators` isn't empty, and holds no space, tab, line break, NUL, `[`, `]` or `"`.
- `Reading.CommentPrefixes` isn't empty; no prefix is empty, holds a line break or NUL, starts with
  a space, a tab or `[`, or starts with one of the separators.
- `Reading.MaxFileSize` isn't 0.
- `Writing.Separator` is one character with only spaces or tabs around it, and that character is
  one of `Reading.Separators`.
- `Writing.CommentPrefix` isn't empty, holds no line break or NUL, and starts with one of
  `Reading.CommentPrefixes`.
- With `Saving.Backup`, `Saving.BackupSuffix` isn't empty and holds no `/`, `\`, `:`, line break or
  NUL.
- `Values.ListDelimiter` isn't a space, a tab, a line break, NUL or `"`, and with
  `Reading.InlineComments` it doesn't start one of `Reading.CommentPrefixes` (a list with an empty
  item would then hold a comment).
- Each enumeration (`DuplicateKeys`, `LineEnding`, `Bom`, `CommentPolicy`, `BoolFormat`) is one of
  its values, in case one was made from a number.

`ToString()` names a value of each enumeration, e.g. `ToString(ASWIni::ELineEnding::CRLF)` is
`"CRLF"`.
