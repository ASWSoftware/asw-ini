/* **************************************************************************
ASWIni_Settings.h
Author: Anthony S. West - ASW Software

An INI file reader/writer library.

The settings binding: TASWIniSection, a section of an application's
settings, and TASWIniSetting<T>, one setting declared once with its section,
key, type, default value, comment and options (TASWIniSettingOptions<T>: a
range, a validator, its own value options). A setting reads and writes its
value in the document of the INI object that owns its section
(TASWBaseIni), so it is thread-safe as that object is.

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

#ifndef ASWIni_SettingsH
#define ASWIni_SettingsH
//---------------------------------------------------------------------------
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>
//---------------------------------------------------------------------------
#include "ASWIni_Base.h"
#include "ASWIni_Converters.h"
#include "ASWIni_Document.h"
#include "ASWIni_Options.h"
#include "ASWIni_Types.h"
//---------------------------------------------------------------------------

namespace ASWIni
{

class TASWIniSection;

//---------------------------------------------------------------------------

// Helpers of the settings' templates
namespace Detail
{

// Whether two values of T can be compared with operator< (see TASWIniSettingOptions::SetRange()); IsLessComparable
// says it
template <typename T, typename Enable = void>
struct TIsLessComparable : std::false_type
{
};

template <typename T>
struct TIsLessComparable<T, std::void_t<decltype(std::declval<T const&>() < std::declval<T const&>())> > :
    std::true_type
{
};

template <typename T>
constexpr bool IsLessComparable = TIsLessComparable<T>::value;

// The ValueOutOfRange result of a value whose text is 'text', outside a setting's range: from 'minText' to 'maxText',
// the texts of its ends (nullptr for an end the range doesn't have)
TASWIniResult OutOfSettingRange(std::string_view text, std::string const* minText, std::string const* maxText);

// The InvalidValue result of a value whose text is 'text', which a setting's validator refused with 'problem'
TASWIniResult RefusedValue(std::string_view text, std::string_view problem);


/////////////////////////////////////////////////////////////////////////////
// TSettingBase
//
// The part of TASWIniSetting<T> that doesn't depend on T: the owner, the section's name, the key and the comment, and
// what the setting asks of its owner. The owner calls Apply() for each setting bound to it (see
// TASWBaseIni::ApplySettings()). Neither copyable nor movable.
/////////////////////////////////////////////////////////////////////////////
class TSettingBase
{
private:
    friend class ASWIni::TASWBaseIni;

private:
    TASWBaseIni* m_Owner;
    std::string m_SectionName;
    std::string m_Key;
    std::string m_Comment;

private:
    // Applies the setting to 'document', which Load() or LoadText() read (see TASWIniSetting), adding what it
    // reports to 'messages'
    virtual void Apply(TASWIniDocument& document, std::vector<TASWIniMessage>& messages) const = 0;

protected:
    // A setting of key 'key' in section 'section', with comment 'comment'. The derived class registers it with its
    // owner (Register()) once it is complete, and unregisters it (Unregister()) in its destructor.
    TSettingBase(TASWIniSection const& section, std::string_view key, std::string_view comment);
    virtual ~TSettingBase() = default;

    // Adds to 'messages' the Error that the default value can't be applied, because it failed with 'result' (it
    // can't be written, or the range or the validator refuses it), with a message that names the section and key
    void AddInvalidDefault(TASWIniResult const& result, std::vector<TASWIniMessage>& messages) const;

    // Sets the comment of the key in 'document' to the setting's, as the options' Writing.CommentPolicy says (not when
    // the setting has none); a failure is added to 'messages' as an Error
    void ApplyComment(TASWIniDocument& document, std::vector<TASWIniMessage>& messages) const;

    // Sets the value of the key in 'document' to 'defaultText', the default value's text, and then its comment
    // (ApplyComment()). With 'problem' (the value's text isn't valid), adds the Warning of DefaultUsedMessage() to
    // 'messages'; without it (the key is missing), the Info that the key was added (EIniCode::KeyAdded). A failure is
    // added as an Error.
    void ApplyDefault(TASWIniDocument& document, std::string const& defaultText, TASWIniResult const* problem,
        std::vector<TASWIniMessage>& messages) const;

    // The UnexpectedException result of Set() for the exception being handled (call it in a catch block), with a
    // message that names the section and key; without a message when it can't be made
    TASWIniResult CaughtSetFailure() const noexcept;

    // The Warning that the default value is used, because the value's text failed with 'result' (its converter's,
    // range's or validator's failure): its code, its text followed by the default value's ('defaultText', or nullptr
    // when it can't be written), and the section and key
    TASWIniMessage DefaultUsedMessage(TASWIniResult const& result, std::string const* defaultText) const;

    TASWBaseIni& GetOwner() const noexcept;

    // Adds the setting at the end of the ones bound to its owner
    void Register() const;

    // Reports DefaultUsedMessage() (Get() gives the default value). A message that can't be made (std::bad_alloc) is
    // dropped.
    void ReportDefaultUsed(TASWIniResult const& result, std::string const* defaultText) const noexcept;

    // Reports 'result', a failure of Set(), as an Error message (see IASWIni::ReportFailure())
    void ReportFailure(TASWIniResult const& result) const noexcept;

    // The failure of Set() with the code of 'result' and a message that names the section and key, then gives the
    // message of 'result'
    TASWIniResult SetFailure(TASWIniResult const& result) const;

    // Sets the value of the key to 'text' in the owner's document, as IASWIni::SetText() does (reporting a failure),
    // and when that adds the key, also its comment, in the same edit: a failure of either changes nothing
    TASWIniResult SetText(std::string_view text);

    // Whether the key exists in the owner's document; if it does, 'text' is set to its value's text, as
    // IASWIni::TryGetText() does
    bool TryGetText(std::string& text) const;

    // Removes the setting from the ones bound to its owner, waiting for a Load() that is applying it
    void Unregister() const;

public:
    TSettingBase(TSettingBase const&) = delete;
    TSettingBase& operator=(TSettingBase const&) = delete;

    // The comment written above the key when the setting adds it (see TASWIniSetting::Set())
    std::string const& GetComment() const noexcept;

    std::string const& GetKey() const noexcept;

    // The name of the section the setting is in
    std::string const& GetSectionName() const noexcept;

    // Whether the key exists in the owner's document
    bool IsPresent() const;
};

} // namespace Detail


/////////////////////////////////////////////////////////////////////////////
// TASWIniSection
//
// A section of an application's settings: the INI object it belongs to (its owner), its name and its comment. Its
// settings (TASWIniSetting) take it in their constructors, so it is declared before them, and it must not outlive its
// owner; both hold when the section and its settings are members of the application's settings class, which derives
// from TASWBaseIni. It is bound to its owner from its constructor to its destructor, in the order sections are
// declared: after the owner's Load() or LoadText() has applied the settings, it applies its comment, if it has one,
// as the options' Writing.CommentPolicy says, when the section is in the document (it adds no section itself). The
// comment of the section "" (the keys before the first header) is the file comment.
//
//     class TAppSettings : public ASWIni::TASWBaseIni
//     {
//     public:
//         ASWIni::TASWIniSection General{ *this, "General" };
//         ASWIni::TASWIniSetting<int> LogLevel{ General, "LogLevel", 2, "From 0 (errors only) to 4 (everything).",
//             ASWIni::TASWIniSettingOptions<int>().SetRange(0, 4) };
//     };
//
// The name "" is the keys before the first header. Neither copyable nor movable.
/////////////////////////////////////////////////////////////////////////////
class TASWIniSection
{
private:
    friend class TASWBaseIni;

private:
    TASWBaseIni* m_Owner;
    std::string m_Name;
    std::string m_Comment;

private:
    // Sets the section's comment in 'document', which Load() or LoadText() read, as the class describes; a failure is
    // added to 'messages' as an Error
    void Apply(TASWIniDocument& document, std::vector<TASWIniMessage>& messages) const;

public:
    // Section 'name' of 'owner', with comment 'comment' ("" for none), bound to 'owner'
    TASWIniSection(TASWBaseIni& owner, std::string_view name, std::string_view comment = std::string_view());
    TASWIniSection(TASWIniSection const&) = delete;
    TASWIniSection& operator=(TASWIniSection const&) = delete;
    ~TASWIniSection();

    std::string const& GetComment() const noexcept;
    std::string const& GetName() const noexcept;
    TASWBaseIni& GetOwner() const noexcept;
};


/////////////////////////////////////////////////////////////////////////////
// TASWIniSettingOptions
//
// The options of a TASWIniSetting<T>: the range its values must be in, a function that checks them, and the value
// options it converts them with. Each setter returns the options, so they are given in one expression:
//
//     ASWIni::TASWIniSettingOptions<int>().SetRange(0, 4).SetValues(oneZero)
//
// The range's ends are kept by pointer, not in a std::optional<T>: bcc32c's (RAD Studio Win32) std::optional can't be
// copied for bool, nor made for a T without a default constructor.
/////////////////////////////////////////////////////////////////////////////
template <typename T>
class TASWIniSettingOptions
{
public:
    // A function that checks a value: it returns "" when the value is valid, else what is wrong with it, as the end of
    // a sentence (e.g. "it can't be empty"), which the message about the value ends with. It is given only the value,
    // and must not call the INI object.
    typedef std::function<std::string (T const& value)> TValidator;

private:
    std::shared_ptr<T const> m_Max; // Replaced, never changed, so copies of the options may share it
    std::shared_ptr<T const> m_Min;
    bool m_Optional = false;
    TValidator m_Validator;
    std::optional<TASWIniValueOptions> m_Values;

public:
    // The largest value the setting takes; nullptr (the default) when there is none
    T const* GetMax() const noexcept
    {
        return m_Max.get();
    }

    // The smallest value the setting takes; nullptr (the default) when there is none
    T const* GetMin() const noexcept
    {
        return m_Min.get();
    }

    // The function that checks the setting's values (none by default)
    TValidator const& GetValidator() const noexcept
    {
        return m_Validator;
    }

    // The value options the setting converts its values with, instead of its owner's; nullptr (the default) when it
    // uses its owner's
    TASWIniValueOptions const* GetValues() const noexcept
    {
        return m_Values.has_value() ? &*m_Values : nullptr;
    }

    // Whether a missing key is left missing when a document is loaded, instead of being added with the default value
    // (false by default)
    bool IsOptional() const noexcept
    {
        return m_Optional;
    }

    // Sets the largest value the setting takes: one that is greater (by operator<) is out of range. Only for a T that
    // operator< compares (numbers, enums, strings, ...).
    TASWIniSettingOptions& SetMax(T const& max)
    {
        static_assert(Detail::IsLessComparable<T>, "a setting's range needs a type that operator< compares");
        m_Max = std::make_shared<T const>(max);

        return *this;
    }

    // Sets the smallest value the setting takes: one that is less (by operator<) is out of range. Only for a T that
    // operator< compares.
    TASWIniSettingOptions& SetMin(T const& min)
    {
        static_assert(Detail::IsLessComparable<T>, "a setting's range needs a type that operator< compares");
        m_Min = std::make_shared<T const>(min);

        return *this;
    }

    // Sets whether the setting is optional: its key isn't added when a loaded document doesn't have it (e.g. a hidden
    // setting a user may add), only when Set() writes it
    TASWIniSettingOptions& SetOptional(bool optional = true)
    {
        m_Optional = optional;

        return *this;
    }

    // Sets the smallest and the largest value the setting takes (see SetMin() and SetMax()), e.g. SetRange(0, 4)
    TASWIniSettingOptions& SetRange(T const& min, T const& max)
    {
        return SetMin(min).SetMax(max);
    }

    // Sets the function that checks the setting's values (see TValidator); an empty one checks nothing
    TASWIniSettingOptions& SetValidator(TValidator validator)
    {
        m_Validator = std::move(validator);

        return *this;
    }

    // Sets the value options the setting converts its values with, instead of its owner's (e.g. a bool written as 1
    // and 0); valid ones (see TASWIniValueOptions::Validate())
    TASWIniSettingOptions& SetValues(TASWIniValueOptions const& values)
    {
        m_Values = values;

        return *this;
    }
};


/////////////////////////////////////////////////////////////////////////////
// TASWIniSetting
//
// One setting of an application: a key of a section (TASWIniSection), with its type T (any type TASWIniConverter
// converts), its default value, its comment and its options (TASWIniSettingOptions). It holds no value of its own:
// Get() reads the value from the owner's document and Set() writes it there, so a setting always agrees with the
// document (with Load(), and with a raw SetText() of the same key), and any thread may call them, as the owner's
// operations. A value is valid when it converts, is in the range and the validator accepts it; Get() gives the
// default value for one that isn't.
// A setting is bound to its owner from its constructor to its destructor, in the order settings are declared. When
// the owner's Load() or LoadText() has read a document, before anyone sees it, each setting bound to it is applied
// to it, in that order: a missing key is added with the default value (unless the setting is optional: see
// TASWIniSettingOptions::SetOptional()) and the setting's comment, reported as an Info message (EIniCode::KeyAdded);
// a value that isn't valid is replaced by the default value, reported as a Warning (as Get() reports it); and the
// key's comment is set to the setting's (if it has one) as the options' Writing.CommentPolicy says. So the document
// then holds every setting with a valid value, and the owner's IsModified() says whether that changed it (whether it
// needs saving). A default value that can't be written, or that the range or the validator refuses, is reported as
// an Error, and the key is left as it is. The converter and the validator are called with the owner's file lock
// held, so they must not call its Load(), LoadText(), Save(), SaveAs() or SetOptions(). SetOptions() doesn't apply
// the settings. A setting must not outlive its section's owner (see TASWIniSection). Neither copyable nor movable.
/////////////////////////////////////////////////////////////////////////////
template <typename T>
class TASWIniSetting : public Detail::TSettingBase
{
private:
    T m_Default;
    TASWIniSettingOptions<T> m_Options;

private:
    void Apply(TASWIniDocument& document, std::vector<TASWIniMessage>& messages) const override
    {
        TASWIniValueOptions const* const values = m_Options.GetValues();
        TASWIniValueOptions const options = values != nullptr ? *values : document.GetOptions().Values;
        std::string defaultText;
        TASWIniResult result = TASWIniConverter<T>::ToText(m_Default, defaultText, options);
        if (result.IsOk())
            result = Check(m_Default, defaultText, options);

        if (!result.IsOk())
        {
            AddInvalidDefault(result, messages);
            return;
        }

        TASWIniDocumentLine const* const line = document.FindKey(GetSectionName(), GetKey());
        if (line == nullptr)
        {
            if (!m_Options.IsOptional())
                ApplyDefault(document, defaultText, nullptr, messages);

            return;
        }

        std::string const text(line->GetValue());
        T value = m_Default;
        result = TASWIniConverter<T>::FromText(text, value, options);
        if (result.IsOk())
            result = Check(value, text, options);

        if (result.IsOk())
            ApplyComment(document, messages);
        else
            ApplyDefault(document, defaultText, &result, messages);
    }

    // Ok when 'value', whose text is 'text', is in the range and the validator accepts it, else the failure
    // (ValueOutOfRange or InvalidValue), with a message that quotes 'text'
    TASWIniResult Check(T const& value, std::string_view text, TASWIniValueOptions const& options) const
    {
        if constexpr (Detail::IsLessComparable<T>)
        {
            T const* const min = m_Options.GetMin();
            T const* const max = m_Options.GetMax();
            if ((min != nullptr && value < *min) || (max != nullptr && *max < value))
            {
                // The ends' texts; one that can't be written (an enum value with no name) is empty
                std::string minText;
                std::string maxText;
                if (min != nullptr)
                    static_cast<void>(TASWIniConverter<T>::ToText(*min, minText, options));

                if (max != nullptr)
                    static_cast<void>(TASWIniConverter<T>::ToText(*max, maxText, options));

                return Detail::OutOfSettingRange(text, min != nullptr ? &minText : nullptr,
                    max != nullptr ? &maxText : nullptr);
            }
        }

        if (m_Options.GetValidator())
        {
            std::string const problem = m_Options.GetValidator()(value);
            if (!problem.empty())
                return Detail::RefusedValue(text, problem);
        }

        return TASWIniResult();
    }

    // The value options the setting converts with: its own, or else its owner's
    TASWIniValueOptions ValueOptions() const
    {
        TASWIniValueOptions const* const values = m_Options.GetValues();

        return values != nullptr ? *values : GetOwner().GetValueOptions();
    }

public:
    // Setting 'key' of section 'section', whose value is 'defaultValue' while the key is missing or its value isn't
    // valid, and that is written with comment 'comment' ("" for none), bound to the section's owner. The default value
    // is expected to be valid.
    TASWIniSetting(TASWIniSection const& section, std::string_view key, T const& defaultValue,
        std::string_view comment = std::string_view(), TASWIniSettingOptions<T> const& options =
        TASWIniSettingOptions<T>())
        : TSettingBase(section, key, comment),
          m_Default(defaultValue),
          m_Options(options)
    {
        Register();
    }

    ~TASWIniSetting() override
    {
        Unregister();
    }

    // The value of the key in the owner's document, converted with TASWIniConverter<T> and the value options (the
    // setting's own, or else the owner's), or the default value when the key doesn't exist or its value isn't valid
    // (it doesn't convert, is out of range, or the validator refuses it). A value that isn't valid is reported as a
    // Warning message (see TSettingBase::ReportDefaultUsed()). May throw what the converter, the validator or the
    // owner's operations throw (e.g. std::bad_alloc).
    T Get() const
    {
        std::string text;
        if (!TryGetText(text))
            return m_Default;

        TASWIniValueOptions const options = ValueOptions();
        T value = m_Default;
        TASWIniResult result = TASWIniConverter<T>::FromText(text, value, options);
        if (result.IsOk())
            result = Check(value, text, options);

        if (result.IsOk())
            return value;

        std::string defaultText;
        bool const written = TASWIniConverter<T>::ToText(m_Default, defaultText, options).IsOk();
        ReportDefaultUsed(result, written ? &defaultText : nullptr);

        return m_Default;
    }

    T const& GetDefault() const noexcept
    {
        return m_Default;
    }

    TASWIniSettingOptions<T> const& GetOptions() const noexcept
    {
        return m_Options;
    }

    // Set(GetDefault())
    TASWIniResult Reset()
    {
        return Set(m_Default);
    }

    // Sets the value of the key in the owner's document to 'value', converted with TASWIniConverter<T> and the value
    // options, as IASWIni::SetText() does: a missing key is added (with the setting's comment above it), and a
    // missing section too. Fails, and then nothing changes, with ValueOutOfRange for a value out of the range,
    // InvalidValue for one that the validator refuses or that can't be converted (e.g. an enum value with no name),
    // each with a message that names the section and key, as SetText() does, or with UnexpectedException for an
    // exception (e.g. one the validator throws). A failure is reported as an Error message.
    TASWIniResult Set(T const& value)
    {
        TASWIniResult result;
        try
        {
            TASWIniValueOptions const options = ValueOptions();
            std::string text;
            result = TASWIniConverter<T>::ToText(value, text, options);
            if (result.IsOk())
                result = Check(value, text, options);

            // SetText() reports its own failure
            if (result.IsOk())
                return SetText(text);

            result = SetFailure(result);
        }
        catch (...)
        {
            result = CaughtSetFailure();
        }

        ReportFailure(result);

        return result;
    }
};

} // namespace ASWIni

//---------------------------------------------------------------------------
#endif // #ifndef ASWIni_SettingsH
