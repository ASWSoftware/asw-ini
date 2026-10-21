# Guarantees: errors, threads, lifetimes and saving

What ASWIni promises about failures, threads, object lifetimes and the files it saves. The header
comments have the details of each class and operation.

## Problems are returned and reported, not thrown

- `Load()`, `LoadText()`, `Save()`, `SaveAs()` and every other operation that returns a
  `TASWIniResult` never throw. A failure is returned (the result converts to `false`, with its
  `Code`, a `Message` that is a complete sentence, and the file path, line and system error when
  they apply) and reported to the log callback as an error. An unexpected exception inside one (in
  practice `std::bad_alloc`, or one an application's converter or validator throws) is returned as
  `UnexpectedException`, with nothing changed.
- A failed operation changes nothing: a load that fails keeps the document and the file path it
  had, a save that fails leaves the file, the document, the file path and `IsModified()` as they
  were, and an edit that fails leaves the document as it was. The one exception is a missing file
  on `Load()` (an application's first run): the document is emptied (and given the settings'
  defaults) and the path becomes the file path, so `Save()` creates the file; the result is
  `FileNotFound`, reported as a warning.
- Warnings (a malformed line, a duplicate key, a backup that couldn't be made, ...) don't make an
  operation fail; they are only reported.
- The operations that have no result to carry an error may throw `std::bad_alloc`: the lookups that
  return strings or names, `GetDocument()`, `ToText()`, `RemoveKey()` and `RemoveSection()` (whose
  `false` means there was nothing to remove), and `GetValue()` and a setting's `Get()`, which may
  also throw what a converter or validator throws. `TryGetValue()` returns its failure and reports
  nothing, since the caller decides what a failure means.
- An exception thrown by the log callback is dropped. A message that can't be made (out of memory)
  is dropped.

The library's tests make each allocation of loading, saving, editing and applying the settings fail
in turn, and check that the result is `UnexpectedException` with nothing changed, or a success that
is complete.

## Thread safety

- An INI object (`TASWBaseIni`, and a settings class derived from it) may be used from any number
  of threads at once, with no locking by the application. Each operation is complete in itself: a
  lookup sees the document before or after an edit or a load, never during one.
- Lookups share a reader/writer lock, and edits take it alone. `Load()` reads and parses the file
  without that lock and then swaps the new document in, and `Save()` writes the file without it, so
  lookups and edits go on while a file is read or written. A second lock keeps `Load()`,
  `LoadText()`, `Save()`, `SaveAs()` and `SetOptions()` from overlapping.
- An edit made while a save is writing the file isn't in that save, and `IsModified()` stays `true`
  after it, so the next save writes it.
- The operations give out copies (strings, names, `GetDocument()`'s copy of the document), never
  pointers into the document, which another thread may change.
- A setting's `Get()`, `Set()`, `Reset()` and `IsPresent()` are the owner's operations, as safe as
  them. Several calls are separate operations, though: another thread may see some of several
  `Set()`s done and not the others. There is no edit of several keys under one lock in 1.0.
- `TASWIniDocument` on its own isn't thread-safe (it is a plain value: copy it, or guard it).
- On RAD Studio Win32 (bcc32c) the locks are Windows slim reader/writer locks, since that
  compiler's `std::mutex` and `std::shared_mutex` wait on a kernel mutex for every lock;
  `ASWINI_WINDOWS_LOCKS` says which are used. Everywhere else they are the standard library's.
- Two processes that use the same file aren't coordinated (there is no cross-process file lock in
  1.0): each save replaces the whole file, so the last save wins, and a load reads either the old or
  the new file, never a mix.

## Which thread runs a callback, and what it may do

- The log callback is called on the thread of the operation that reports, after the operation's
  work, with none of the object's locks held. So it may call the object, even `Load()`, `Save()` or
  `SetLogCallback()`, without a deadlock. A `Get()` or `GetValue()` that reports a value that isn't
  valid calls it on the reading thread.
- Messages are reported in the order they happened; a load reports its own warnings first, then
  what applying the settings changed.
- Any thread may replace the callback at any time; a report that started before may still go to
  the callback it replaced.
- Converters, validators and an override of the protected `ReadFile()` or `WriteFile()` run while
  `Load()` or `Save()` holds the object's file lock (but not the document's lock). They must not
  call the object's `Load()`, `LoadText()`, `Save()`, `SaveAs()` or `SetOptions()`, which would
  wait for that lock forever. A validator should only look at the value it gets.

## Lifetime

- An INI object can't be copied or moved. Its sections and settings must not outlive it, which
  holds when they are members of the settings class; local ones are destroyed before it. A section
  is declared before its settings.
- Sections and settings are bound to the object from their constructors to their destructors. One
  made or destroyed while another thread loads waits for the load, so a load never applies a
  setting that is half made or half destroyed.
- Whatever the log callback captures must outlive the object, or the callback must be replaced
  first (e.g. `SetLogCallback(nullptr)`) before it goes.
- A `TASWIniDocument`'s pointers and views (`FindKey()`, `FindSection()`, a line's
  `GetValue()`) are valid until the document changes.

## Saving and crashes

- By default a save is atomic: the text is written to a temporary file in the same folder, flushed
  to the disk, and then replaces the file (with `ReplaceFileW()`, or `MoveFileExW()` where that
  can't be used, on Windows; with `rename()` elsewhere, flushing the folder after it). So the file
  is always either the old one or the new one, whole, even when the application crashes while
  saving, and the flushes keep it so after a power failure too. A temporary file left by a failed
  save is deleted (a warning when it can't be).
- The replaced file keeps its attributes and security on Windows, and its permissions (and, when
  allowed, its owner) elsewhere, where a symbolic link stays one and the file it points to is
  replaced.
- With `Saving.Backup`, the previous file is first copied to `<file>.bak`; a backup that fails is a
  warning, and the save goes on.
- With `Saving.AtomicSave` off (for a folder that can't be written to), the file is overwritten in
  place, and a crash while saving can leave it incomplete.
- A file that is loaded and saved without changes is written back byte for byte, and an edit
  changes only the lines it is about (see [Format.md](Format.md)).
- The library writes only when `Save()` or `SaveAs()` is called; nothing is saved on its own, e.g.
  in a destructor.

## Independent of the locale and the platform

- Values are converted the same way in every locale: numbers never have a `,` decimal mark or digit
  separators, whatever the C or C++ locale says.
- Section and key names compare with the ASCII letters' case ignored, the same for every user.
- File paths are UTF-8 on every platform (`PathToUtf8()` and `Utf8ToPath()` convert a
  `std::filesystem::path`), and files are read and written in binary mode, so a file written on one
  platform reads the same on another.
- What a value converter writes always reads back as the same value (e.g. the shortest text of a
  `double` that reads back exactly), on every supported compiler.

## Codes

`EIniCode` says what happened, in a `TASWIniResult` and in a `TASWIniMessage`. Its values never
change, so an application may store or compare them, and new codes are only added at the end.
`ToString(code)` gives its name.

| Value | Code | Reported as | When |
|---|---|---|---|
| 0 | `Ok` | | Success |
| 1 | `InvalidArgument` | Error | An argument that can't be used: an empty file path, a name or value that wouldn't read back, the section `""` where a header is needed |
| 2 | `InvalidOptions` | Error | `TASWIniOptions::Validate()` refused the options (see [Options.md](Options.md)) |
| 3 | `NoFilePath` | Error | `Save()` with no file path (use `SaveAs()`) |
| 4 | `FileNotFound` | Error; Warning for `Load()` | The file or a folder in its path doesn't exist |
| 5 | `AccessDenied` | Error | The operating system denied access to the file or its folder |
| 6 | `FileTooLarge` | Error | The file is larger than `Reading.MaxFileSize` |
| 7 | `OpenFailed` | Error | The file couldn't be opened, for another reason (e.g. it is a folder on Linux; Windows reports a folder as `AccessDenied`) |
| 8 | `ReadFailed` | Error | Reading the file failed |
| 9 | `UnsupportedEncoding` | Error | The file starts with a UTF-16 or UTF-32 byte order mark |
| 10 | `ParseFailed` | Error | A line couldn't be parsed, with `Reading.StrictParsing` |
| 11 | `CreateFolderFailed` | Error | The file's folder doesn't exist and couldn't be created |
| 12 | `WriteFailed` | Error | Writing the file, or its temporary file, failed |
| 13 | `ReplaceFailed` | Error | The temporary file couldn't replace the file (e.g. a read-only file) |
| 14 | `UnexpectedException` | Error | An exception was caught, e.g. `std::bad_alloc` |
| 15 | `MalformedLine` | Warning | A line is neither a section header, a key, a comment nor blank; it is kept as it is |
| 16 | `UnclosedSection` | Warning | A section header has no `]` |
| 17 | `DuplicateKey` | Warning | A key appears more than once in a section |
| 18 | `DuplicateSection` | Warning | A section appears more than once |
| 19 | `InvalidUtf8` | Warning | The text isn't valid UTF-8; it is kept byte for byte |
| 20 | `BackupFailed` | Warning | The backup couldn't be made; the file was saved anyway |
| 21 | `TempFileCleanupFailed` | Warning | The temporary file of a failed save couldn't be deleted |
| 22 | `NotFound` | Error; not reported by `TryGetValue()` | The section or key doesn't exist |
| 23 | `AlreadyExists` | Error | A rename to the name of another section or key |
| 24 | `InvalidValue` | Error; Warning when a load or a read gives the default | The text isn't a value of the type, a validator refused it, or the value can't be written |
| 25 | `ValueOutOfRange` | Error; Warning when a load or a read gives the default | The number is outside its type's range, or its setting's |
| 26 | `KeyAdded` | Info | A load added a setting's missing key with its default value |
| 27 | `KeyRenamed` | Info | A load renamed an earlier name of a setting's missing key to it |
