/* **************************************************************************
ASWIni_Interface.h
Author: Anthony S. West - ASW Software

An INI file reader/writer library.

IASWIni, the interface of an INI object (TASWBaseIni implements it): its
string-based operations, each thread-safe and complete in itself, and the
typed GetValue(), TryGetValue() and SetValue() built on them with the value
converters (TASWIniConverter). TASWIniLogCallback, the function an INI object
reports its warnings and errors to.

Requires C++ 17 or higher.

Copyright 2026 ASW Software

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    https://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

************************************************************************** */

#pragma once

#ifndef ASWIni_InterfaceH
#define ASWIni_InterfaceH
//---------------------------------------------------------------------------
#include <exception>
#include <functional>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>
//---------------------------------------------------------------------------
#include "ASWIni_Converters.h"
#include "ASWIni_Document.h"
#include "ASWIni_Options.h"
#include "ASWIni_Types.h"
//---------------------------------------------------------------------------

namespace ASWIni
{

// The function an INI object reports its messages to (see IASWIni::SetLogCallback()): the warnings of loading,
// reading options and saving, a value GetValue() couldn't convert, and each operation that failed. It is called on
// the thread of the operation, after it, with no lock of the object held, so it may call the object's methods
// (Load() and Save() too). An exception it throws is dropped.
typedef std::function<void (TASWIniMessage const& message)> TASWIniLogCallback;

//---------------------------------------------------------------------------

// Helpers of IASWIni's templates, and of the settings' (ASWIni_Settings.h)
namespace Detail
{

// A failed result with 'code' and the message "The key <key> of the section <section> <action>: <problem>", e.g.
// "The key "Width" of the section "Window" can't be read: The value "wide" isn't an integer."
TASWIniResult KeyFailure(std::string_view section, std::string_view key, char const* action, EIniCode code,
    std::string_view problem);

} // namespace Detail


/////////////////////////////////////////////////////////////////////////////
// IASWIni
//
// The interface of an INI object, which holds a TASWIniDocument and the file it was loaded from: string-based
// operations that an implementation (TASWBaseIni) provides, each thread-safe and complete in itself, and the typed
// GetValue(), TryGetValue() and SetValue() built on them. Code that only reads and writes values can take an IASWIni&.
// The operations give out copies (strings, names, a copy of the document), never pointers into the document, which
// another thread may change. A section named "" holds the keys before the first header.
// Every operation that fails with a TASWIniResult reports it to the log callback as an Error message too (except
// TryGetValue(), whose failure is an answer for the caller), with the result's code, text, file path, line and system
// error. Operations that return a TASWIniResult don't throw: an unexpected exception (std::bad_alloc, or one an
// application's converter throws) is returned as EIniCode::UnexpectedException with nothing changed. The others may
// throw std::bad_alloc (and GetValue() what a converter throws), since they have no result to carry it. Neither
// copyable nor movable.
/////////////////////////////////////////////////////////////////////////////
class IASWIni
{
private:
    // Enables a typed template for a T that isn't text: a string literal, a char array or pointer, or anything else
    // that converts to char const* goes to the text overloads instead
    template <typename T>
    using TIfNotText = std::enable_if_t<!std::is_convertible_v<T const&, char const*> >;

private:
    // The NotFound result of TryGetValue() for key 'key' of section 'section', which doesn't exist
    static TASWIniResult KeyNotFound(std::string_view section, std::string_view key);

    // TryGetValue() with 'options', or with GetValueOptions() when it is nullptr (only asked for when the key exists)
    template <typename T>
    TASWIniResult ReadValue(std::string_view section, std::string_view key, T& value,
        TASWIniValueOptions const* options) const
    {
        char const* const action = "can't be read";
        try
        {
            std::string text;
            if (!TryGetText(section, key, text))
                return KeyNotFound(section, key);

            TASWIniValueOptions const valueOptions = options != nullptr ? *options : GetValueOptions();
            TASWIniResult const result = TASWIniConverter<T>::FromText(text, value, valueOptions);
            if (!result.IsOk())
                return Detail::KeyFailure(section, key, action, result.Code, result.Message);

            return result;
        }
        catch (std::exception const& e)
        {
            return Detail::KeyFailure(section, key, action, EIniCode::UnexpectedException, e.what());
        }
        catch (...)
        {
            return Detail::KeyFailure(section, key, action, EIniCode::UnexpectedException, UnknownExceptionText);
        }
    }

    // Reports that GetValue() couldn't convert the value of key 'key' of section 'section': a Warning message with the
    // code and text of 'result', the converter's failure, and the section and key. A message that can't be made
    // (std::bad_alloc) is dropped.
    void ReportValueWarning(std::string_view section, std::string_view key,
        TASWIniResult const& result) const noexcept;

    // SetValue() with 'options', or with GetValueOptions() when it is nullptr
    template <typename T>
    TASWIniResult WriteValue(std::string_view section, std::string_view key, T const& value,
        TASWIniValueOptions const* options)
    {
        char const* const action = "can't be set";
        TASWIniResult result;
        try
        {
            std::string text;
            TASWIniValueOptions const valueOptions = options != nullptr ? *options : GetValueOptions();
            result = TASWIniConverter<T>::ToText(value, text, valueOptions);
            if (result.IsOk())
                return SetText(section, key, text);

            result = Detail::KeyFailure(section, key, action, result.Code, result.Message);
        }
        catch (std::exception const& e)
        {
            result = Detail::KeyFailure(section, key, action, EIniCode::UnexpectedException, e.what());
        }
        catch (...)
        {
            result = Detail::KeyFailure(section, key, action, EIniCode::UnexpectedException, UnknownExceptionText);
        }

        ReportFailure(result);

        return result;
    }

protected:
    // The problem an exception that isn't a std::exception is described with
    static constexpr char const* UnknownExceptionText = "An exception that isn't a std::exception was thrown.";

protected:
    IASWIni() = default;

    // Sends 'message' to the log callback, with FilePath set to GetFilePath() when it is empty. Never called with a
    // lock of the object held.
    virtual void Report(TASWIniMessage const& message) const noexcept = 0;

    // Reports 'result', which failed, as an Error message with its code, text, file path, line and system error. A
    // message that can't be made (std::bad_alloc) is dropped.
    void ReportFailure(TASWIniResult const& result) const noexcept;

public:
    IASWIni(IASWIni const&) = delete;
    IASWIni& operator=(IASWIni const&) = delete;
    virtual ~IASWIni() = default;

    // Adds an empty section named 'section' at the end, as TASWIniDocument::AddSection() does
    virtual TASWIniResult AddSection(std::string_view section) = 0;

    // A copy of the document, e.g. to go through its lines
    virtual TASWIniDocument GetDocument() const = 0;

    // The file comment, as TASWIniDocument::GetFileComment() gives it
    virtual std::string GetFileComment() const = 0;

    // The file Load() read or SaveAs() wrote last, which Save() writes (UTF-8, as it was given); "" before either
    virtual std::string GetFilePath() const = 0;

    // The comment of key 'key' of section 'section', as TASWIniDocument::GetKeyComment() gives it
    virtual std::string GetKeyComment(std::string_view section, std::string_view key) const = 0;

    // The keys of section 'section', as TASWIniDocument::GetKeyNames() gives them
    virtual std::vector<std::string> GetKeyNames(std::string_view section) const = 0;

    // The options the document is read, written and saved with, and values are converted with (see SetOptions())
    virtual TASWIniOptions GetOptions() const = 0;

    // The comment of section 'section', as TASWIniDocument::GetSectionComment() gives it
    virtual std::string GetSectionComment(std::string_view section) const = 0;

    // The names of the sections (with 'prefix', only those that start with it), as TASWIniDocument::GetSectionNames()
    // gives them
    virtual std::vector<std::string> GetSectionNames(std::string_view prefix = std::string_view()) const = 0;

    // GetOptions().Values, without copying the other options: what GetValue(), TryGetValue() and SetValue() convert
    // values with, unless they are given their own
    virtual TASWIniValueOptions GetValueOptions() const = 0;

    // The library's version (ASWIni::Version)
    virtual std::string_view GetVersionStr() const noexcept = 0;

    // Whether key 'key' of section 'section' exists
    virtual bool HasKey(std::string_view section, std::string_view key) const = 0;

    // Whether section 'section' exists ("" always does)
    virtual bool HasSection(std::string_view section) const = 0;

    // Whether an edit changed the document's text since it was loaded or saved (see TASWIniDocument::IsModified())
    virtual bool IsModified() const = 0;

    // Reads file 'filePath' (UTF-8) as LoadText() reads a text, and makes it the file path; its warnings are reported
    // with the file path. Fails with InvalidArgument for an empty path, the file errors (e.g. AccessDenied,
    // FileTooLarge), and LoadText()'s; then nothing changes. A missing file (FileNotFound) is the exception, since an
    // application's first run has none: the document is emptied, as a new one with the options, 'filePath' becomes
    // the file path, so Save() creates the file, and the failure is reported as a Warning, not an Error.
    virtual TASWIniResult Load(std::string_view filePath) = 0;

    // Replaces the document with 'text', read as TASWIniDocument::Parse() reads it, with the options; its warnings are
    // reported. Keeps the file path, and clears the modified flag (which a TASWBaseIni's settings may set again: see
    // TASWIniSetting). Fails as Parse() does (UnsupportedEncoding, or ParseFailed with StrictParsing), and then nothing
    // changes. LoadText("") empties the document.
    virtual TASWIniResult LoadText(std::string_view text) = 0;

    // Removes key 'key' of section 'section', as TASWIniDocument::RemoveKey() does; returns whether anything was
    // removed. May throw std::bad_alloc, since false would say there was nothing to remove.
    virtual bool RemoveKey(std::string_view section, std::string_view key) = 0;

    // Removes section 'section', as TASWIniDocument::RemoveSection() does; returns whether anything was removed. May
    // throw std::bad_alloc, since false would say there was nothing to remove.
    virtual bool RemoveSection(std::string_view section) = 0;

    // Renames key 'key' of section 'section' to 'newKey', as TASWIniDocument::RenameKey() does
    virtual TASWIniResult RenameKey(std::string_view section, std::string_view key, std::string_view newKey) = 0;

    // Renames section 'section' to 'newSection', as TASWIniDocument::RenameSection() does
    virtual TASWIniResult RenameSection(std::string_view section, std::string_view newSection) = 0;

    // Writes ToText() to the file path, as the options' Saving group says (atomically by default, so a failed save
    // leaves the file as it was), even when the document isn't modified; its warnings (e.g. BackupFailed) are
    // reported. Clears the modified flag, unless an edit succeeded while the file was being written. Fails with
    // NoFilePath when there is no file path (see SaveAs()), or the file errors (e.g. AccessDenied, WriteFailed); then
    // the document, the file path and the modified flag stay as they were.
    virtual TASWIniResult Save() = 0;

    // As Save(), to file 'filePath' (UTF-8), which becomes the file path when it succeeds. Fails with
    // InvalidArgument for an empty path.
    virtual TASWIniResult SaveAs(std::string_view filePath) = 0;

    // Sets the file comment, as TASWIniDocument::SetFileComment() does
    virtual TASWIniResult SetFileComment(std::string_view text) = 0;

    // Sets the comment of key 'key' of section 'section', as TASWIniDocument::SetKeyComment() does
    virtual TASWIniResult SetKeyComment(std::string_view section, std::string_view key, std::string_view text) = 0;

    // Sets the function messages are reported to (see TASWIniLogCallback); an empty one turns reporting off (the
    // default). A report that started before it returns may still go to the function it replaced.
    virtual void SetLogCallback(TASWIniLogCallback callback) = 0;

    // Gives the object 'options' and reads the document's text again with them, as TASWIniDocument::SetOptions()
    // does: the text, the file path and the modified flag stay. Its warnings are reported with the file path. Fails
    // with InvalidOptions (see TASWIniOptions::Validate()) or, with StrictParsing, ParseFailed; then nothing changes.
    virtual TASWIniResult SetOptions(TASWIniOptions const& options) = 0;

    // Sets the comment of section 'section', as TASWIniDocument::SetSectionComment() does
    virtual TASWIniResult SetSectionComment(std::string_view section, std::string_view text) = 0;

    // Sets the value of key 'key' of section 'section' to 'text' (the value's text, without quotes or a comment),
    // adding the key or the section when it is missing, as TASWIniDocument::SetValue() does
    virtual TASWIniResult SetText(std::string_view section, std::string_view key, std::string_view text) = 0;

    // The document as text, as TASWIniDocument::ToText() writes it
    virtual std::string ToText() const = 0;

    // Whether key 'key' of section 'section' exists; if it does, 'text' is set to its value's text (without quotes or
    // a comment, see TASWIniDocumentLine::GetValue()) in the line TASWIniDocument::FindKey() finds
    virtual bool TryGetText(std::string_view section, std::string_view key, std::string& text) const = 0;

public: // Typed values, built on the operations above
    // The value of key 'key' of section 'section', converted with TASWIniConverter<T> and the value options
    // (GetValueOptions(), or 'options'), or 'defaultValue' when the key doesn't exist or its value doesn't convert. A
    // value that doesn't convert is reported as a Warning message with the converter's code and text, and the section
    // and key. A text default (a string literal, a char array or pointer, or a std::string_view) gives a std::string,
    // the text as it is (a nullptr default is ""). May throw what the converter or the operations throw.
    template <typename T, typename = TIfNotText<T> >
    T GetValue(std::string_view section, std::string_view key, T const& defaultValue) const
    {
        return GetValue(section, key, defaultValue, GetValueOptions());
    }

    template <typename T, typename = TIfNotText<T> >
    T GetValue(std::string_view section, std::string_view key, T const& defaultValue,
        TASWIniValueOptions const& options) const
    {
        std::string text;
        if (!TryGetText(section, key, text))
            return defaultValue;

        T value = defaultValue;
        TASWIniResult const result = TASWIniConverter<T>::FromText(text, value, options);
        if (!result.IsOk())
        {
            ReportValueWarning(section, key, result);
            return defaultValue;
        }

        return value;
    }

    std::string GetValue(std::string_view section, std::string_view key, char const* defaultValue) const;
    std::string GetValue(std::string_view section, std::string_view key, std::string_view defaultValue) const;

    // Sets the value of key 'key' of section 'section' to 'value' converted with TASWIniConverter<T> and the value
    // options (GetValueOptions(), or 'options'), with SetText(). Fails with the converter's code (e.g. InvalidValue
    // for an enum value that has no name) and a message that names the section and key, or as SetText() does. A text
    // value (a string literal, a char array or pointer, or a std::string_view) is set as it is, with SetText() (a
    // nullptr value is "").
    template <typename T, typename = TIfNotText<T> >
    TASWIniResult SetValue(std::string_view section, std::string_view key, T const& value)
    {
        return WriteValue(section, key, value, nullptr);
    }

    template <typename T, typename = TIfNotText<T> >
    TASWIniResult SetValue(std::string_view section, std::string_view key, T const& value,
        TASWIniValueOptions const& options)
    {
        return WriteValue(section, key, value, &options);
    }

    TASWIniResult SetValue(std::string_view section, std::string_view key, char const* value);
    TASWIniResult SetValue(std::string_view section, std::string_view key, std::string_view value);

    // Reads the value of key 'key' of section 'section' into 'value', converted with TASWIniConverter<T> and the value
    // options (GetValueOptions(), or 'options'). Fails with NotFound when the key doesn't exist, or with the
    // converter's code (InvalidValue, ValueOutOfRange) and a message that names the section and key; then 'value'
    // stays as it was. Not reported to the log callback: the caller decides what a failure means.
    template <typename T>
    TASWIniResult TryGetValue(std::string_view section, std::string_view key, T& value) const
    {
        return ReadValue(section, key, value, nullptr);
    }

    template <typename T>
    TASWIniResult TryGetValue(std::string_view section, std::string_view key, T& value,
        TASWIniValueOptions const& options) const
    {
        return ReadValue(section, key, value, &options);
    }
};

} // namespace ASWIni

//---------------------------------------------------------------------------
#endif // #ifndef ASWIni_InterfaceH
