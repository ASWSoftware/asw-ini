# The INI format

What ASWIni reads and writes: the lines of an INI file, its sections, keys, values and comments, its
encoding and line endings, what an edit changes, and the text of each type of value. The defaults
read the common INI dialects (the Windows profile API's, Python's configparser, most Linux tools);
the options that change them are named in each section and described in
[Options.md](Options.md).

## Lines

Every line is one of five kinds (`ELineKind`). Any line may be indented with spaces and tabs (the
"blanks"), and blanks around names, values and comments aren't part of them.

| Kind | Example | What it is |
|---|---|---|
| `Blank` | | Empty, or only blanks |
| `Comment` | `; Valid range: 0-4` | A comment prefix (`;` or `#`), then the comment |
| `Section` | `[General]` | A section header |
| `KeyValue` | `Width = 640` | A key, a separator (`=` or `:`) and a value |
| `Raw` | `just some words` | A line that is none of these, kept as it is |

A `Raw` line is reported as a warning (`MalformedLine`, or `UnclosedSection` for a `[` without its
`]`), with its line number, and written back unchanged. With `Reading.StrictParsing`, the first one
fails the load instead (`ParseFailed`).

## Sections

```ini
TopKey = before the first header

[General]
Name = value

[ Window.Main ]   ; the header's own comment
Width = 640
```

- A header is `[`, the name, and `]`. Blanks around the name are not part of it, so the second
  header above is the section `Window.Main`. A dot is an ordinary character, but
  `GetSectionNames("Window.")` finds the sections whose names start with a prefix.
- A comment may follow the `]` (`[General] ; note`), whatever `Reading.InlineComments` says. Other
  text after it is reported as a `MalformedLine` warning, and the line is still the header.
- A header without its `]` is an `UnclosedSection` warning, and one with an empty name (`[ ]`) a
  `MalformedLine` warning; both are kept as `Raw` lines.
- The keys before the first header belong to the section named `""`, which every document has.
- Names compare with the ASCII letters' case ignored: `[general]` is `[General]`
  (`Reading.CaseSensitive` makes them case-sensitive). Other characters, UTF-8 included, compare
  exactly.
- A section that appears more than once is reported as a `DuplicateSection` warning, and all its
  headers count as one section: a key is looked for in each of them, in file order, and a new key
  goes into the first one.

## Keys and values

```ini
Width = 640
Window Title = My App
url = http://example.com:8080
time: 12:30
Empty =
```

- The key is the text before the first separator on the line, and the value the text after it,
  each without the blanks around it. The default separators are `=` and `:`
  (`Reading.Separators`); the first one on the line wins, so the value above is
  `http://example.com:8080`, and `time` is `12:30`.
- A key may hold blanks (`Window Title`). A value may be empty.
- A line with no separator, or with nothing before it, is a `MalformedLine` warning.
- Keys compare as section names do.
- A key that appears more than once in a section is reported as a `DuplicateKey` warning, and all
  its lines are kept. The first line's value is the key's value (as the Windows profile API reads
  it); `Reading.DuplicateKeys` can make it the last line's. Setting the value changes that line
  only, and removing the key removes every line of it.
- A new line is written as `key = value` (`Writing.Separator`, which keeps its blanks; e.g. `"="`
  writes `key=value`).

## Comments

A comment line starts with a comment prefix, after any indentation: `;` or `#` by default
(`Reading.CommentPrefixes`; add `//` for files that use it). New comment lines are written with
`; ` (`Writing.CommentPrefix`). Comment lines belong to what follows them:

```ini
; Settings of MyApp
; (a comment of two lines)

; How the window looks
[Window]
; In pixels
Width = 640
Height = 480
```

- The **file comment** is the comment lines at the start of the file when a blank line (or the end
  of the file) follows them: "Settings of MyApp" and "(a comment of two lines)", read as one text
  with the lines joined by `"\n"`.
- A **section comment** is the comment lines directly above a header, with no other line between
  them: "How the window looks".
- A **key comment** is the comment lines directly above a key's line: "In pixels". `Height` has no
  comment.

Setting a comment replaces those lines with one line for each line of the new text, written with
the comment prefix and the indentation of the key or header; an empty text removes them. A new file
comment gets a blank line under it. A setting's comment is written over one edited in the file, or
only where the file has none, as `Writing.CommentPolicy` says (see [Settings.md](Settings.md)).

### Inline comments

A comment after a value (`Width = 640 ; pixels`) is not recognized by default: values such as
connection strings and colors (`#FF0000`) hold the prefixes, so `Width = 640 ; pixels` has the value
`640 ; pixels`. With `Reading.InlineComments`, a comment prefix that follows a blank starts a
comment, so the value is `640`, but `Color = #FF0000` then has an empty value and the comment
`FF0000` (the `#` follows the blank after the `=`). Turn on `Reading.QuotedValues` too, and such a
value can be written in quotes (`Color = "#FF0000" ; red`); the library quotes a value that needs it
when it writes one. Setting the value of a line keeps its comment.

## Quoted values

With `Reading.QuotedValues` (off by default), a value in double quotes keeps its blanks and loses
its quotes, and `""` inside the quotes is one `"`:

```ini
Prefix = "  > "
Greeting = "say ""hi"""
Path = "C:\Program Files\App"
```

The values are `  > ` (with its blanks), `say "hi"` and `C:\Program Files\App`; a backslash is an
ordinary character, as everywhere in ASWIni. Only blanks (or, with `Reading.InlineComments`, blanks
and a comment) may follow the closing quote; otherwise the quotes are part of the value. A value is
written in quotes only when it needs them to read back: blanks at either end, a leading `"`, or,
with `Reading.InlineComments`, a comment prefix after a blank. Without `Reading.QuotedValues`,
quotes are ordinary characters, and setting a value that would need them (e.g. with a blank at the
end) fails with `InvalidArgument`.

## Encoding and line endings

- Files are UTF-8, with or without a byte order mark, which is kept when the file is saved
  (`Writing.Bom`). A file that starts with a UTF-16 or UTF-32 byte order mark is not read
  (`UnsupportedEncoding`). A file that isn't valid UTF-8 is read byte for byte and kept so, with
  one `InvalidUtf8` warning for its first such line.
- Lines may end with LF, CRLF or CR, even mixed in one file. Files are read and written in binary
  mode, so the library chooses the line endings, not the C runtime: each line keeps its own, and a
  new line gets the file's most common one, or the platform's in a new file (CRLF on Windows, LF
  elsewhere). `Writing.LineEnding` can give every line one line ending instead. A last line
  without a line break never gets one.
- A file larger than 16 MiB is not read (`FileTooLarge`; see `Reading.MaxFileSize`).

## What is kept, and what an edit changes

A file that is loaded and saved without changes is written back byte for byte: comments, blank
lines, spacing, indentation, line endings, the byte order mark, the order of sections and keys,
duplicates and `Raw` lines. An edit changes only the lines it is about:

- **A new value** replaces only the value in its line: the indentation, the key, the separator, the
  spacing around it and an inline comment stay, and a quoted value stays quoted. Setting the value
  a key has changes nothing.
- **A new key** goes after the last key of its section, with that key's indentation (right after
  the header in a section with no keys), as `key = value`.
- **A new section** goes at the end of the file, after a blank line
  (`Writing.BlankLineBeforeNewSection`).
- **Removing** a key or a section removes its lines with its comment.
- **Renaming** a key or a section changes only its name, in each of its lines.
- A name or a value that wouldn't read back as it is can't be written, and fails with
  `InvalidArgument`: a line break, a key that is empty, holds a separator or starts with a comment
  prefix or `[`, a section name with `]`, blanks around a name, or a value that needs quotes without
  `Reading.QuotedValues`.

For example, setting `Width` to 800, `Height` to 600 (a new key) and `[View]`'s `Zoom` to 1.5 (a
new section) in

```ini
[Window]
  Width:640   ; set by the installer

[Other]
```

gives (with `Reading.InlineComments`)

```ini
[Window]
  Width:800   ; set by the installer
  Height = 600

[Other]

[View]
Zoom = 1.5
```

## The text of each type of value

The typed operations (`GetValue()`, `SetValue()`, the settings) convert a value with
`TASWIniConverter<T>`. Conversions are the same in every locale: a number never has a `,` decimal
mark or digit separators. Reading ignores the blanks around a value, except for a string or a path.

| Type | Read | Written |
|---|---|---|
| `std::string` | The text as it is | The text as it is |
| `bool` | `true`/`false`, `yes`/`no`, `on`/`off`, `1`/`0`, in any case | `true`/`false` (`Values.BoolFormat`) |
| Integers | An optional sign, then decimal digits, or `0x` and hex digits: `42`, `-42`, `0x2A`; `010` is 10 | Decimal digits: `42`, `-42` |
| `float`, `double` | `1.5`, `.5`, `5.`, `1e5`, `-2.5E-3`, `inf`, `infinity`, `nan` | The shortest text that reads back the same: `0.1`, `3`, `1e+21`, `inf` |
| An enum with names | A name, in any case, or the number of a value that has one | The value's first name |
| An enum without names | As its underlying integer type | As its underlying integer type |
| `std::filesystem::path` | The text, as the path in UTF-8 | The path in UTF-8, separators as it holds them |
| `std::u8string` (C++20) | The text as it is | The text as it is |
| `std::vector<T>` | Items separated by `,` (`Values.ListDelimiter`) | `a, b, c` |

- The integers are every integral type, signed and unsigned, from `char` to `long long` and
  `unsigned long long` (so `std::int8_t` to `std::int64_t` and `std::uint8_t` to `std::uint64_t`),
  except `bool` and the wide and Unicode character types. So `char`, `std::int8_t` and
  `std::uint8_t` are numbers, not characters. A number outside the type's range is
  `ValueOutOfRange` (a negative one for an unsigned type too), and hex is a value, not a bit pattern
  (`0xFFFFFFFF` is out of range for `std::int32_t`, and fits a `std::uint32_t` or a
  `std::int64_t`).
- A floating-point number too large for its type is `ValueOutOfRange`, and one too small reads as
  0.
- An enum's names are declared next to it with `ASWIniEnumNames()` (see
  [Settings.md](Settings.md)); its other names are read but never written, e.g. an old name.
- A list item is the text up to the next delimiter, without the blanks around it, or an item in
  double quotes, which may hold the delimiter and blanks, with `""` for a `"`:
  `report.txt, "notes, draft.txt"` is the two items `report.txt` and `notes, draft.txt`. An empty
  text is an empty list. Items are written in quotes only when they need them. A list of lists
  works too: its items are lists, in quotes.
- A text that isn't a value of the type is `InvalidValue`, with a message that quotes it.
  `GetValue()` then gives its default (and reports a warning), and a setting gives its default
  value (see [Settings.md](Settings.md)).
- Other types get a converter by specializing `TASWIniConverter` (see [Settings.md](Settings.md)).
