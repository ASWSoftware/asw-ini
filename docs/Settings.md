# Settings classes

An application declares each of its settings once, as a member of its settings class: its section,
key, type, default value, comment and options. Loading the file brings it up to date, and each
setting reads and writes its value in the loaded document, from any thread. This replaces the code
that finds, converts, checks, repairs and comments each key by hand. `example/src/main.cpp` shows a
settings class in use.

## Declaring the settings

A settings class derives from `ASWIni::TASWBaseIni` (`ASWIni_Base.h`) and declares its sections
(`TASWIniSection`) and settings (`TASWIniSetting<T>`) as members (`ASWIni_Settings.h`):

```cpp
#include "ASWIni_Base.h"
#include "ASWIni_Settings.h"

class TAppSettings : public ASWIni::TASWBaseIni
{
private:
    // The validator of a text that can't be empty: "" when it is valid, else what is wrong with it
    static std::string NotEmpty(std::string const& value)
    {
        return value.empty() ? "it can't be empty" : "";
    }

public:
    // The section "" holds the keys before the first header; its comment is the file comment
    ASWIni::TASWIniSection FileTop{ *this, "", "Settings of MyApp." };

    ASWIni::TASWIniSection General{ *this, "General", "What the application looks like." };
    ASWIni::TASWIniSetting<std::string> UserName{ General, "UserName", "Guest", "The name in the title bar.",
        ASWIni::TASWIniSettingOptions<std::string>().SetValidator(NotEmpty) };
    // Not added to the file when it is missing: a hidden setting a user may add by hand
    ASWIni::TASWIniSetting<bool> EnableCheats{ General, "EnableCheats", false, "",
        ASWIni::TASWIniSettingOptions<bool>().SetOptional() };

    ASWIni::TASWIniSection Logging{ *this, "Logging" };
    ASWIni::TASWIniSetting<unsigned> LogLevel{ Logging, "LogLevel", 2, "From 0 (errors only) to 4 (everything).",
        ASWIni::TASWIniSettingOptions<unsigned>().SetRange(0, 4) };
    // Called "LogDays" in an earlier version of the application, so Load() renames that key
    ASWIni::TASWIniSetting<unsigned> NDaysRetainLogs{ Logging, "NDaysRetainLogs", 30,
        "How many days the logs are kept; 0 keeps them forever.",
        ASWIni::TASWIniSettingOptions<unsigned>().AddOldKey("LogDays") };

    TAppSettings()
    {
        // Options are set before the first Load(), which reads the file with them (see Options.md)
        ASWIni::TASWIniOptions options;
        options.Saving.Backup = true;
        static_cast<void>(SetOptions(options));
    }
};
```

- **A section** has its owner (the settings object, `*this`), its name and an optional comment,
  written above its header. The section `""` is the keys before the first header, and its comment
  is the file comment.
- **A setting** has its section, its key, its default value, an optional comment (written above
  the key) and options. Its type is any type `TASWIniConverter` converts: strings, `bool`, every
  integer type, `float` and `double`, enums, `std::filesystem::path`, `std::u8string` (C++20),
  `std::vector` of any of them, and your own types (see [Your own types](#your-own-types)). Their
  text in the file is described in [Format.md](Format.md).
- Declare a section before its settings. Sections and settings are bound to their owner from their
  constructors to their destructors, in the order they are declared, which is the order Load()
  applies them in.
- Neither may outlive its owner, and neither can be copied or moved. Both hold for members of the
  settings class. A setting copies its section's name, so it never reads the section object after
  it is made.

### The options of a setting

`TASWIniSettingOptions<T>` is given in one expression, since each setter returns it:

| Option | What it does |
|---|---|
| `SetRange(min, max)`, `SetMin(min)`, `SetMax(max)` | The values the setting takes, compared with `operator<` (numbers, enums, strings, ...). A value outside them is out of range. |
| `SetValidator(function)` | A function that gets a value and returns `""` when it is valid, or else what is wrong with it, as the end of a sentence ("it can't be empty"). It must only look at the value, not call the INI object. |
| `SetOptional()` | A missing key is left missing when a file is loaded (e.g. a hidden setting a user may add); it is written only when `Set()` writes it. |
| `AddOldKey(key)` | An earlier name of the key, in the same section, renamed to the key when a file has it and not the key. Several may be added; the first one the file has is renamed. |
| `SetValues(values)` | Value options of the setting's own, instead of its owner's, e.g. a `bool` written as `1` and `0` (`EBoolFormat::OneZero`). |

A value is valid when it converts, is in the range and the validator accepts it. The default value
is expected to be valid.

## Using the settings

```cpp
TAppSettings settings;
settings.SetLogCallback(LogMessage); // Where warnings, errors and notes go (see Messages below)

// On the application's first run the file is missing: Load() returns FileNotFound, reported as a warning, and the
// document holds every setting with its default value
ASWIni::TASWIniResult const loaded = settings.Load(ASWIni::PathToUtf8(folder / "MyApp.ini"));
if (!loaded && loaded.Code != ASWIni::EIniCode::FileNotFound)
    return false; // A file that can't be read: reported to LogMessage too

if (settings.IsModified()) // Load() added or repaired something: the file needs saving
    static_cast<void>(settings.Save());

unsigned const level = settings.LogLevel.Get();
ASWIni::TASWIniResult const set = settings.LogLevel.Set(level + 1); // Fails (and reports it) above 4
```

- **`Get()`** reads the value from the owner's document. It gives the default value for a missing
  key, and for a value that isn't valid (e.g. one a raw edit made invalid), which it also reports as
  a warning that names the default.
- **`Set()`** writes the value to the document. A missing key is added with the setting's comment
  above it, in the same edit. A value out of the range fails with `ValueOutOfRange`, and one the
  validator refuses (or that can't be written) with `InvalidValue`; the failure is returned,
  reported as an error, and changes nothing.
- **`Reset()`** sets the default value. **`IsPresent()`** says whether the key is in the document,
  e.g. for an optional setting. `GetDefault()`, `GetKey()`, `GetSectionName()`, `GetComment()` and
  `GetOptions()` give the declaration.
- A setting holds no value of its own: it always agrees with the document, also after a `Load()`
  or a raw `SetText()` of its key. A `Get()` costs a lookup and a conversion (well under a
  microsecond); code that reads a setting in a tight loop keeps a copy.
- `Save()` writes the document; it doesn't need to write the settings, which are in the document
  already.

## What Load() does

When `Load()` or `LoadText()` has read a document, and before anyone sees it, each setting is
applied to it, in the order the settings are declared:

1. A missing key whose earlier name (`AddOldKey()`) the section has is renamed from it, keeping its
   line, value and comment, reported as a note (`KeyRenamed`), and is then applied as a key that is
   there. When the key is there too, the earlier name is left alone.
2. Any other missing key is added with the default value and the setting's comment, reported as a
   note (`KeyAdded`), unless the setting is optional. A missing section is added at the end of the
   file, so missing sections come in the order of their first setting, and missing keys after the
   section's last key, in the order they are declared.
3. A value that isn't valid is replaced by the default value, reported as a warning (the converter's
   code, `ValueOutOfRange` or `InvalidValue`) that says what was read and that the default is used.
   A valid value is never rewritten: `0x10` stays `0x10`.
4. The key's comment is set to the setting's, as the comment policy says (see
   [Comments](#comments)).

Then each section's comment is applied, for the sections that are in the document (applying never
adds a section for its comment alone), and the comment of the section `""` is the file comment.

So the loaded document holds every setting with a valid value, and `IsModified()` says whether
applying changed it: whether the file needs saving. A file that holds every setting, valid and with
the code's comments, stays byte for byte, and a missing file (the first run) gets everything. The
notes and warnings are reported after the load's own warnings, with the file's path, and a load
that fails (e.g. a file that can't be read, or out of memory) changes nothing and reports none of
them.

A default value that can't be written (an enum value with no name), or that the setting's own range
or validator refuses, is the application's bug: it is reported as an error, and the key is left as
it is.

Settings are applied by `Load()` and `LoadText()` only. Before the first load, a new settings
object's document is empty, and `Get()` gives the default values; `LoadText("")` fills it with
them. `SetOptions()` doesn't apply the settings, so set the options first, in the constructor.

## Comments

The comments a settings class declares (a setting's, a section's, and the section `""`'s, which is
the file comment) are written as `Writing.CommentPolicy` says when a file is loaded:

- `ECommentPolicy::CodeWins` (the default): the code's comment is written over the file's, so a
  comment edited by hand comes back as the code has it. A comment that reads the same already is
  left as it is (whatever prefix its lines use), so it doesn't make the document modified.
- `ECommentPolicy::FileWins`: the code's comment is written only where the file has none, e.g. for a
  key that was just added; a user's own comment stays.

A setting or section declared with no comment (`""`) never changes the file's comment, under either
policy. A key that `Set()` adds gets the setting's comment.

## Threads

- `Get()`, `Set()`, `Reset()` and `IsPresent()` may be called from any thread at any time, as the
  owner's own operations: each one is a single lookup or edit under the owner's lock, so it sees the
  document before or after another thread's edit or load, never during one.
- Several `Set()`s are separate edits: another thread may see some of them done and not the others,
  which is harmless for settings. There is no batch edit in 1.0.
- The converters and validators run while `Load()` holds the owner's file lock, so they must not
  call its `Load()`, `LoadText()`, `Save()`, `SaveAs()` or `SetOptions()`. A validator should only
  look at the value it gets.
- A section or setting made or destroyed while another thread loads waits for the load to finish.
- [Guarantees.md](Guarantees.md) describes the locks and the log callback in full.

## Settings and the raw API

A settings class is a `TASWBaseIni`, so the keys it doesn't declare are read and written by section
and key, with the typed `GetValue()`, `TryGetValue()` and `SetValue()`, and the string operations
(`HasKey()`, `GetSectionNames()`, `RemoveKey()`, the comments, ...). They see the settings' keys
too, and a setting sees their edits at once:

```cpp
// Window positions, one section per window, which the settings class doesn't declare
static_cast<void>(settings.SetValue("Window.Main", "Width", 1024));
for (std::string const& window : settings.GetSectionNames("Window."))
    RestoreWindow(window, settings.GetValue(window, "Width", 800));
```

Code that only reads and writes values can take an `ASWIni::IASWIni&` (`ASWIni_Interface.h`), the
interface `TASWBaseIni` implements.

Compared with doing it by hand: without settings, each key needs its own code to read it, check it,
repair it and keep its comment, e.g. for `LogLevel` alone

```cpp
unsigned level = 2;
ASWIni::TASWIniResult const read = ini.TryGetValue("Logging", "LogLevel", level);
if (!read || level > 4)
{
    if (read)
        LogMessageText("LogLevel is out of range; using 2");

    level = 2;
    static_cast<void>(ini.SetValue("Logging", "LogLevel", level));
    needsSave = true;
}

if (ini.GetKeyComment("Logging", "LogLevel") != "From 0 (errors only) to 4 (everything).")
{
    static_cast<void>(ini.SetKeyComment("Logging", "LogLevel", "From 0 (errors only) to 4 (everything)."));
    needsSave = true;
}
```

and the same again wherever it is written, while the setting is one declaration.

## Your own types

### Enums by name

An enum is written by name when `ASWIniEnumNames()`, declared next to it (in its namespace, or as a
friend in its class), returns its names; argument-dependent lookup finds it. The first name of a
value is written, and the others are read too, e.g. an earlier name. Define it, not only declare
it, before the enum's first setting, or the setting converts the enum as a number:

```cpp
enum class ETheme : int
{
    Light = 0,
    Dark = 1,
    System = 2,
};

inline auto const& ASWIniEnumNames(ETheme)
{
    static constexpr ASWIni::TASWIniEnumName<ETheme> names[] = {
        { ETheme::Light, "Light" }, { ETheme::Dark, "Dark" }, { ETheme::System, "System" },
        { ETheme::System, "Auto" }, // An earlier name: read, never written
    };

    return names;
}
```

A name is read in any case, and so is the number of a value that has one. An enum without
`ASWIniEnumNames()` is read and written as its underlying integer type.

### A converter of your own

Any other type gets a converter by specializing `ASWIni::TASWIniConverter`, with a `FromText()` and
a `ToText()` that write a text `FromText()` reads back. On failure they return the error (usually
`InvalidValue`) and leave the value or text as it was:

```cpp
struct TSize
{
    int Width = 0;
    int Height = 0;
};

namespace ASWIni
{

// A size written as "640x480"
template <>
struct TASWIniConverter<TSize>
{
    static TASWIniResult FromText(std::string_view text, TSize& value, TASWIniValueOptions const& options)
    {
        TSize size;
        std::size_t const x = text.find('x');
        if (x == std::string_view::npos ||
            !TASWIniConverter<int>::FromText(text.substr(0, x), size.Width, options) ||
            !TASWIniConverter<int>::FromText(text.substr(x + 1), size.Height, options))
        {
            TASWIniResult result;
            result.Code = EIniCode::InvalidValue;
            result.Message = "The value \"" + std::string(text) + "\" isn't a size, e.g. 640x480.";
            return result;
        }

        value = size;
        return TASWIniResult();
    }

    static TASWIniResult ToText(TSize const& value, std::string& text, TASWIniValueOptions const& /*options*/)
    {
        text = std::to_string(value.Width) + "x" + std::to_string(value.Height);
        return TASWIniResult();
    }
};

} // namespace ASWIni
```

Then `TASWIniSetting<TSize>`, `GetValue<TSize>()` and `std::vector<TSize>` work as for the built-in
types (a list's items need a default constructor, as `TSize` has). A range needs an `operator<`; a
validator can check anything else (e.g. a positive size).

## Messages

The INI object reports to its log callback (`SetLogCallback()`): the warnings of loading (a
malformed line, a value that isn't valid), the notes about what loading changed, each operation that
failed, and a `Get()` or `GetValue()` that gave its default for a value that isn't valid. Each
`TASWIniMessage` has a `Severity` (`Info`, `Warning` or `Error`), a `Code` (`EIniCode`, whose values
never change), a `Text` that is a complete sentence, and the `FilePath`, `Line`, `Section`, `Key`
and `SystemError` it is about. `ToString(message)` writes it as one line:

```text
Info KeyRenamed: The key "NDaysRetainLogs" of the section "Logging" is missing: its earlier name "LogDays" is renamed to it. (file "MyApp.ini", section "Logging", key "NDaysRetainLogs")
Warning ValueOutOfRange: The value "9" is out of the setting's range: it must be 0 to 4. The default value "2" is used. (file "MyApp.ini", section "Logging", key "LogLevel")
```

The messages about settings:

| Code | Severity | When |
|---|---|---|
| `KeyAdded` | Info | Load() added a missing key with its default value |
| `KeyRenamed` | Info | Load() renamed an earlier name of a missing key to it |
| `InvalidValue`, `ValueOutOfRange` | Warning | Load() replaced a value that isn't valid, or `Get()` gave the default for one |
| `InvalidValue`, `ValueOutOfRange` | Error | `Set()` failed |
| The failure's code | Error | A default value that can't be applied, or a key that can't be added or renamed |

The callback is called on the thread of the operation, after it, with none of the object's locks
held, so it may call the object, even `Load()` or `Save()`. On a first run, it gets a note for each
setting that isn't optional: filter on `Severity` to log only warnings and errors.

### Sending the messages to ASWLog

ASWIni doesn't depend on a logging library. With [ASWLog](https://github.com/ASWSoftware/asw-log),
a callback passes each message on at its severity:

```cpp
#include "ASWIni_Base.h"
#include "ASWLog_Interface.h"

// A log callback that writes ASWIni's messages to 'log', which must outlive the INI object (or its callback)
ASWIni::TASWIniLogCallback ToASWLog(ASWLog::IASWLog& log)
{
    return [&log](ASWIni::TASWIniMessage const& message)
        {
            std::string const text = ASWIni::ToString(message);
            switch (message.Severity)
            {
                case ASWIni::ESeverity::Info:
                    log.LogInfo(text);
                    break;

                case ASWIni::ESeverity::Warning:
                    log.LogWarn(text);
                    break;

                default:
                    log.LogError(text);
                    break;
            }
        };
}

// settings.SetLogCallback(ToASWLog(ASWLog::TASWFileLog::GetInstance()));
```
