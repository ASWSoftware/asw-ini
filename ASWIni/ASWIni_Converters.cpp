/* **************************************************************************
ASWIni_Converters.cpp
Author: Anthony S. West - ASW Software

See header for info.

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

//---------------------------------------------------------------------------
// Module header
#include "ASWIni_Converters.h"
//---------------------------------------------------------------------------
#include <algorithm>
#include <clocale>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>
#if ASWINI_STD_FLOAT_CHARCONV
#  include <charconv>
#  include <system_error>
#endif
#if defined(_WIN32)
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#  ifndef LOAD_LIBRARY_SEARCH_SYSTEM32
#    define LOAD_LIBRARY_SEARCH_SYSTEM32 0x00000800
#  endif
#endif
//---------------------------------------------------------------------------
#include "ASWIni_Utils.h"
//---------------------------------------------------------------------------

namespace ASWIni
{

namespace
{

// The texts a bool is read from, with the ASCII letters' case ignored
constexpr std::string_view TrueTexts[] = { "true", "yes", "on", "1" };
constexpr std::string_view FalseTexts[] = { "false", "no", "off", "0" };

// What reading a number's text found
enum class ENumberRead : int
{
    Ok = 0, // A number the type can hold
    Invalid = 1, // Not a number
    TooLarge = 2, // A number whose magnitude is too large for the type
};

#if defined(_WIN32)
/////////////////////////////////////////////////////////////////////////////
// TUcrt
//
// What the UniversalCrt float conversion reads with: the Windows Universal C Runtime's (ucrtbase.dll) exact
// _strtod_l() and _strtof_l(), and a "C" locale for them (see Ucrt())
/////////////////////////////////////////////////////////////////////////////
struct TUcrt
{
    typedef double (__cdecl* TStrtodL)(char const* text, char** end, void* locale);
    typedef float (__cdecl* TStrtofL)(char const* text, char** end, void* locale);

    TStrtodL StrtodL = nullptr;
    TStrtofL StrtofL = nullptr;
    void* Locale = nullptr; // A _locale_t
};
#endif

//---------------------------------------------------------------------------

template <typename T>
ENumberRead CRuntimeRead(std::string const& text, T& value, Detail::EFloatConversion conversion,
    bool* halfway = nullptr);
template <typename T>
std::string CRuntimeText(T magnitude, Detail::EFloatConversion conversion);
std::string DecimalText(std::string const& digits, int exponent);
std::string_view DecimalMark() noexcept;
int DigitValue(char c, int base) noexcept;
template <typename T>
std::string FloatText(T value, Detail::EFloatConversion conversion);
TASWIniResult InvalidFloat(std::string_view text);
TASWIniResult InvalidInteger(std::string_view text);
TASWIniResult InvalidList(std::string_view text, std::size_t number, std::string_view problem);
#if ASWINI_STD_FLOAT_CHARCONV
bool IsBelowOne(std::string_view decimal) noexcept;
#endif
bool IsDecimalNumber(std::string_view text) noexcept;
#if defined(_WIN32)
TUcrt LoadUcrt() noexcept;
#endif
bool NeedsListQuotes(std::string_view item, char delimiter, bool onlyItem) noexcept;
TASWIniResult OutOfRange(std::string_view text, std::string const& min, std::string const& max);
#if defined(_WIN32)
template <typename T>
T ProcAddress(HMODULE module, char const* name) noexcept;
#endif
std::string QuotedListItem(std::string_view item);
template <typename T>
ENumberRead ReadDecimal(std::string_view decimal, T& value, Detail::EFloatConversion conversion);
template <typename T>
TASWIniResult ReadFloat(std::string_view text, T& value, Detail::EFloatConversion conversion);
ENumberRead ReadMagnitude(std::string_view text, bool& negative, unsigned long long& magnitude) noexcept;
bool ReadQuotedItem(std::string_view text, std::size_t& at, std::string& item);
std::string ScientificText(char const* printed);
std::size_t SkipBlanks(std::string_view text, std::size_t from) noexcept;
#if defined(_WIN32)
TUcrt const* Ucrt() noexcept;
#endif
template <typename T>
std::string WholeNumberText(T magnitude);

//---------------------------------------------------------------------------
/*
    CRuntimeRead

    Reads 'text', a decimal number without a sign (see IsDecimalNumber()), with the C runtime as 'conversion' says
    (see Detail::EFloatConversion), into 'value' (set only when Ok); TooLarge when it is too large for T. When
    'halfway' isn't nullptr, it says whether the value may be 1 unit in the last place off: strtold() gave a number
    exactly halfway between two Ts (never with the Universal C Runtime, which is exact).
*/
template <typename T>
ENumberRead CRuntimeRead(std::string const& text, T& value, Detail::EFloatConversion conversion, bool* halfway)
{
    char* end = nullptr;
    if (halfway != nullptr)
        *halfway = false;

#if defined(_WIN32)
    TUcrt const* const ucrt = conversion == Detail::EFloatConversion::UniversalCrt ? Ucrt() : nullptr;
    if (ucrt != nullptr)
    {
        T read = 0;
        if constexpr (std::is_same_v<T, double>)
            read = ucrt->StrtodL(text.c_str(), &end, ucrt->Locale);
        else
            read = ucrt->StrtofL(text.c_str(), &end, ucrt->Locale);

        if (end != text.c_str() + text.size())
            return ENumberRead::Invalid;

        if (std::isinf(read))
            return ENumberRead::TooLarge;

        value = read;

        return ENumberRead::Ok;
    }
#else
    static_cast<void>(conversion);
#endif

    // strtold() reads the C locale's decimal mark, which may not be '.'
    std::string local = text;
    std::string_view const mark = DecimalMark();
    std::size_t const point = local.find('.');
    if (point != std::string::npos && mark != ".")
        local.replace(point, 1, mark.data(), mark.size());

    long double const read = std::strtold(local.c_str(), &end);
    if (end != local.c_str() + local.size())
        return ENumberRead::Invalid;

    // Narrowed by hand near the largest T, where converting a long double may give infinity (the choice between it and
    // the largest T is the implementation's): a number up to half the gap below the largest T above it rounds to it,
    // and from there on is too large. (With a long double no wider than T, that bound is infinity, which only an
    // infinite 'read' reaches.)
    long double const largest = std::numeric_limits<T>::max();
    long double const belowLargest = std::nextafter(std::numeric_limits<T>::max(), T(0));
    if (read >= largest + (largest - belowLargest) / 2)
        return ENumberRead::TooLarge;

    T const narrowed = read > largest ? std::numeric_limits<T>::max() : static_cast<T>(read);

    // A 'read' exactly halfway between two Ts may come from a text a little to one side of halfway, which the
    // narrowing can round to the wrong side
    if (halfway != nullptr)
    {
        T const infinity = std::numeric_limits<T>::infinity();
        T const other = std::nextafter(narrowed, read > narrowed ? infinity : -infinity);
        *halfway = static_cast<long double>(narrowed) != read &&
            read - static_cast<long double>(narrowed) == static_cast<long double>(other) - read;
    }

    value = narrowed;

    return ENumberRead::Ok;
}
//---------------------------------------------------------------------------
/*
    CRuntimeText

    'magnitude' (finite, not negative) as the shortest text that CRuntimeRead() reads back as it: the fewest
    significant digits from snprintf() that do, written as DecimalText() does.
*/
template <typename T>
std::string CRuntimeText(T magnitude, Detail::EFloatConversion conversion)
{
    std::string text;
    for (int digits = 1; digits <= std::numeric_limits<T>::max_digits10; ++digits)
    {
        // A float is exact as a double, and every C runtime prints a double, while "%Le" needs the compiler and the C
        // runtime to agree on a long double's size (Microsoft's C runtime has a 64-bit one, GCC an 80-bit one)
        char printed[64];
        std::snprintf(printed, sizeof(printed), "%.*e", digits - 1, static_cast<double>(magnitude));
        text = ScientificText(printed);

        // A text that strtold() reads as halfway between two Ts isn't proven to read back with an exact reader
        T readBack = 0;
        bool halfway = false;
        if (CRuntimeRead(text, readBack, conversion, &halfway) == ENumberRead::Ok && readBack == magnitude && !halfway)
            break;
    }

    // A whole number in fixed notation is written with all its digits, which are as many as the shortest ones padded
    // with zeros and closer to it, as std::to_chars() does ("57457352" rather than "57457350" for a float)
    if (text.find_first_of(".e") == std::string::npos)
        text = WholeNumberText(magnitude);

    return text;
}
//---------------------------------------------------------------------------
/*
    DecimalText

    The number with the significant digits 'digits' (no leading or trailing zeros, except "0" for 0) and the decimal
    exponent 'exponent' of its first digit, as std::to_chars() writes it: in fixed notation ("1500", "0.001") or in
    scientific notation ("1.5e+21", "1e-07"), whichever is shorter, and fixed when both are as long.
*/
std::string DecimalText(std::string const& digits, int exponent)
{
    std::string scientific(1, digits[0]);
    if (digits.size() > 1)
        scientific += "." + digits.substr(1);
    scientific += exponent < 0 ? "e-" : "e+";

    std::string const exponentDigits = std::to_string(exponent < 0 ? -exponent : exponent);
    if (exponentDigits.size() < 2)
        scientific += '0';
    scientific += exponentDigits;

    std::string fixed;
    std::size_t const size = digits.size();
    if (exponent < 0)
        fixed = "0." + std::string(static_cast<std::size_t>(-exponent - 1), '0') + digits;
    else if (size <= static_cast<std::size_t>(exponent) + 1)
        fixed = digits + std::string(static_cast<std::size_t>(exponent) + 1 - size, '0');
    else
    {
        fixed = digits.substr(0, static_cast<std::size_t>(exponent) + 1) + "." +
            digits.substr(static_cast<std::size_t>(exponent) + 1);
    }

    return fixed.size() <= scientific.size() ? fixed : scientific;
}
//---------------------------------------------------------------------------
/*
    DecimalMark

    The C locale's decimal mark, which strtold() reads ("." in the "C" locale).
*/
std::string_view DecimalMark() noexcept
{
    std::lconv const* const conventions = std::localeconv();
    if (conventions == nullptr || conventions->decimal_point == nullptr || *conventions->decimal_point == '\0')
        return ".";

    return conventions->decimal_point;
}
//---------------------------------------------------------------------------
/*
    DigitValue

    The value of 'c' as a digit of base 'base' (10, or 16 with the hex letters in either case); -1 when it isn't one.
*/
int DigitValue(char c, int base) noexcept
{
    if (c >= '0' && c <= '9')
        return c - '0';

    if (base != 16)
        return -1;

    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;

    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;

    return -1;
}
//---------------------------------------------------------------------------
/*
    FloatText

    'value' as the float and double converters write it, converted with 'conversion'.
*/
template <typename T>
std::string FloatText(T value, Detail::EFloatConversion conversion)
{
    // inf and nan by our own code, since the libraries write them differently (e.g. "-nan(ind)")
    if (std::isnan(value))
        return "nan";

    if (std::isinf(value))
        return value < 0 ? "-inf" : "inf";

#if ASWINI_STD_FLOAT_CHARCONV
    if (conversion == Detail::EFloatConversion::Standard)
    {
        // The shortest text of a double has at most 24 characters
        char buffer[64];
        std::to_chars_result const result = std::to_chars(buffer, buffer + sizeof(buffer), value);

        return std::string(buffer, result.ptr);
    }
#endif

    std::string text = CRuntimeText(std::fabs(value), conversion);
    if (std::signbit(value))
        text.insert(0, 1, '-');

    return text;
}
//---------------------------------------------------------------------------
/*
    InvalidFloat

    The InvalidValue result of 'text', which isn't a floating-point number.
*/
TASWIniResult InvalidFloat(std::string_view text)
{
    return Detail::ValueFailure(EIniCode::InvalidValue, text, "isn't a number: it must be digits with an optional '.' "
        "and exponent, after an optional sign (e.g. 1.5, -2e-3), or inf or nan.");
}
//---------------------------------------------------------------------------
/*
    InvalidInteger

    The InvalidValue result of 'text', which isn't an integer.
*/
TASWIniResult InvalidInteger(std::string_view text)
{
    return Detail::ValueFailure(EIniCode::InvalidValue, text, "isn't a whole number: it must be decimal digits, or 0x "
        "and hex digits, after an optional sign.");
}
//---------------------------------------------------------------------------
/*
    InvalidList

    The InvalidValue result of 'text', which isn't a list because its item 'number' (1 for the first) has the
    problem 'problem' (e.g. "starts with a '\"' that isn't closed").
*/
TASWIniResult InvalidList(std::string_view text, std::size_t number, std::string_view problem)
{
    std::string message = "isn't a list: its item " + Detail::IntegerText(static_cast<unsigned long long>(number));
    message += ' ';
    message += problem;
    message += '.';

    return Detail::ValueFailure(EIniCode::InvalidValue, text, message);
}
//---------------------------------------------------------------------------
#if ASWINI_STD_FLOAT_CHARCONV
/*
    IsBelowOne

    Whether 'decimal', a decimal number (see IsDecimalNumber()) that isn't 0, is below 1.
*/
bool IsBelowOne(std::string_view decimal) noexcept
{
    std::size_t const exponentBegin = decimal.find_first_of("eE");
    std::string_view const mantissa = decimal.substr(0, exponentBegin);
    std::size_t const point = std::min(mantissa.find('.'), mantissa.size());
    std::size_t const first = mantissa.find_first_of("123456789");
    if (first == std::string_view::npos)
        return false;

    // The power of ten of the first significant digit, before the exponent; then the exponent, whose digits stop
    // counting once it is far beyond any floating-point type's range
    long long power = static_cast<long long>(point) - static_cast<long long>(first);
    if (first < point)
        --power;

    if (exponentBegin != std::string_view::npos)
    {
        std::string_view digits = decimal.substr(exponentBegin + 1);
        bool const negative = !digits.empty() && digits[0] == '-';
        if (!digits.empty() && (digits[0] == '-' || digits[0] == '+'))
            digits.remove_prefix(1);

        long long exponent = 0;
        for (char const c : digits)
        {
            if (exponent < 1000000000)
                exponent = exponent * 10 + (c - '0');
        }

        power += negative ? -exponent : exponent;
    }

    return power < 0;
}
//---------------------------------------------------------------------------
#endif
/*
    IsDecimalNumber

    Whether 'text' is a decimal number without a sign: digits with an optional '.' (with at least one digit on either
    side of it) and an optional exponent ('e' or 'E', an optional sign and digits).
*/
bool IsDecimalNumber(std::string_view text) noexcept
{
    std::size_t i = 0;
    bool hasDigits = false;
    while (i < text.size() && text[i] >= '0' && text[i] <= '9')
    {
        hasDigits = true;
        ++i;
    }

    if (i < text.size() && text[i] == '.')
    {
        ++i;
        while (i < text.size() && text[i] >= '0' && text[i] <= '9')
        {
            hasDigits = true;
            ++i;
        }
    }

    if (!hasDigits)
        return false;

    if (i < text.size() && (text[i] == 'e' || text[i] == 'E'))
    {
        ++i;
        if (i < text.size() && (text[i] == '+' || text[i] == '-'))
            ++i;

        std::size_t const exponentBegin = i;
        while (i < text.size() && text[i] >= '0' && text[i] <= '9')
            ++i;

        if (i == exponentBegin)
            return false;
    }

    return i == text.size();
}
//---------------------------------------------------------------------------
#if defined(_WIN32)
/*
    LoadUcrt

    The Universal C Runtime's _strtod_l() and _strtof_l() with a "C" locale; an empty TUcrt (no Locale) when Windows
    doesn't have them.
*/
TUcrt LoadUcrt() noexcept
{
    // Only from the system folder. LOAD_LIBRARY_SEARCH_SYSTEM32 needs Windows 8, or 7 with KB2533623; without it the
    // load fails, and strtold() is used.
    HMODULE const module = LoadLibraryExW(L"ucrtbase.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (module == nullptr)
        return TUcrt();

    typedef void* (__cdecl* TCreateLocale)(int category, char const* locale);
    TCreateLocale const createLocale = ProcAddress<TCreateLocale>(module, "_create_locale");
    TUcrt ucrt;
    ucrt.StrtodL = ProcAddress<TUcrt::TStrtodL>(module, "_strtod_l");
    ucrt.StrtofL = ProcAddress<TUcrt::TStrtofL>(module, "_strtof_l");
    if (createLocale == nullptr || ucrt.StrtodL == nullptr || ucrt.StrtofL == nullptr)
        return TUcrt();

    // The Universal C Runtime's LC_ALL is 0, which the C runtime this is built with may number differently
    ucrt.Locale = createLocale(0, "C");

    return ucrt;
}
//---------------------------------------------------------------------------
#endif
/*
    NeedsListQuotes

    Whether the list item 'item' must be quoted to read back as itself (see Detail::JoinListItems()): it holds
    'delimiter' or a '"', has a space or a tab at either end, or is empty and the list's only item ('onlyItem').
*/
bool NeedsListQuotes(std::string_view item, char delimiter, bool onlyItem) noexcept
{
    if (item.empty())
        return onlyItem;

    return item.find(delimiter) != std::string_view::npos || item.find('"') != std::string_view::npos ||
        Detail::IsBlank(item.front()) || Detail::IsBlank(item.back());
}
//---------------------------------------------------------------------------
/*
    OutOfRange

    The ValueOutOfRange result of 'text', a number outside the range 'min' to 'max' (as text).
*/
TASWIniResult OutOfRange(std::string_view text, std::string const& min, std::string const& max)
{
    return Detail::ValueFailure(EIniCode::ValueOutOfRange, text, "is out of range: it must be " + min + " to " + max +
        ".");
}
//---------------------------------------------------------------------------
#if defined(_WIN32)
/*
    ProcAddress

    The function 'name' of 'module', as a T (a pointer to a function); nullptr when it has none.
*/
template <typename T>
T ProcAddress(HMODULE module, char const* name) noexcept
{
    // Through a pointer to a function without parameters, a cast GCC doesn't warn about
    return reinterpret_cast<T>(reinterpret_cast<void (*)()>(GetProcAddress(module, name)));
}
//---------------------------------------------------------------------------
#endif
/*
    QuotedListItem

    The list item 'item' in double quotes, with each '"' in it as "".
*/
std::string QuotedListItem(std::string_view item)
{
    std::string quoted = "\"";
    for (char const c : item)
    {
        quoted += c;
        if (c == '"')
            quoted += '"';
    }

    quoted += '"';

    return quoted;
}
//---------------------------------------------------------------------------
/*
    ReadDecimal

    Reads 'decimal', a decimal number without a sign (see IsDecimalNumber()), converted with 'conversion', into
    'value' (set only when Ok); a number too small for T reads as 0, and one too large is TooLarge.
*/
template <typename T>
ENumberRead ReadDecimal(std::string_view decimal, T& value, Detail::EFloatConversion conversion)
{
#if ASWINI_STD_FLOAT_CHARCONV
    if (conversion == Detail::EFloatConversion::Standard)
    {
        // Read into a local, since libc++ changes the value when the number is out of range. std::from_chars()
        // reports a number too small for T as out of range too.
        T read = 0;
        char const* const end = decimal.data() + decimal.size();
        std::from_chars_result const result = std::from_chars(decimal.data(), end, read);
        if (result.ptr != end)
            return ENumberRead::Invalid;

        if (result.ec == std::errc::result_out_of_range)
        {
            if (!IsBelowOne(decimal))
                return ENumberRead::TooLarge;

            // Whatever libc++ put there
            read = 0;
        }
        else if (result.ec != std::errc())
        {
            return ENumberRead::Invalid;
        }

        value = read;

        return ENumberRead::Ok;
    }
#endif

    return CRuntimeRead(std::string(decimal), value, conversion);
}
//---------------------------------------------------------------------------
/*
    ReadFloat

    Reads 'text' as the float and double converters read it, converted with 'conversion', into 'value' (set only on
    success).
*/
template <typename T>
TASWIniResult ReadFloat(std::string_view text, T& value, Detail::EFloatConversion conversion)
{
    std::string_view number = Detail::Trim(text);
    bool negative = false;
    if (!number.empty() && (number[0] == '+' || number[0] == '-'))
    {
        negative = number[0] == '-';
        number.remove_prefix(1);
    }

    // inf and nan by our own code, since the libraries read them differently; a NaN's sign isn't kept
    if (Detail::EqualsNoCase(number, "nan"))
    {
        value = std::numeric_limits<T>::quiet_NaN();
        return TASWIniResult();
    }

    T read = std::numeric_limits<T>::infinity();
    if (!Detail::EqualsNoCase(number, "inf") && !Detail::EqualsNoCase(number, "infinity"))
    {
        // The form is checked here, so every conversion takes the same texts (strtold() would take hex and blanks)
        if (!IsDecimalNumber(number))
            return InvalidFloat(text);

        ENumberRead const result = ReadDecimal(number, read, conversion);
        if (result == ENumberRead::TooLarge)
        {
            return OutOfRange(text, FloatText(std::numeric_limits<T>::lowest(), conversion),
                FloatText(std::numeric_limits<T>::max(), conversion));
        }

        if (result != ENumberRead::Ok)
            return InvalidFloat(text);
    }

    value = negative ? -read : read;

    return TASWIniResult();
}
//---------------------------------------------------------------------------
/*
    ReadMagnitude

    Reads 'text' (the spaces and tabs around it ignored) as an optional sign, then decimal digits or 0x (or 0X) and
    hex digits: whether it is negative, and its magnitude (set only when Ok).
*/
ENumberRead ReadMagnitude(std::string_view text, bool& negative, unsigned long long& magnitude) noexcept
{
    std::string_view digits = Detail::Trim(text);
    negative = false;
    if (!digits.empty() && (digits[0] == '+' || digits[0] == '-'))
    {
        negative = digits[0] == '-';
        digits.remove_prefix(1);
    }

    int base = 10;
    if (digits.size() >= 2 && digits[0] == '0' && (digits[1] == 'x' || digits[1] == 'X'))
    {
        base = 16;
        digits.remove_prefix(2);
    }

    // Our own loop rather than std::from_chars(): bcc32c's takes a sign for an unsigned type ("--1" would read) and
    // doesn't report every overflow. Every character must be a digit, so a second sign or a blank fails.
    if (digits.empty())
        return ENumberRead::Invalid;

    unsigned long long const largest = std::numeric_limits<unsigned long long>::max();
    unsigned long long read = 0;
    bool tooLarge = false;
    for (char const c : digits)
    {
        int const digit = DigitValue(c, base);
        if (digit < 0)
            return ENumberRead::Invalid;

        // read * base + digit must not pass 'largest'; after a digit that would, the rest must still be digits
        unsigned long long const digitValue = static_cast<unsigned long long>(digit);
        if (read > (largest - digitValue) / static_cast<unsigned long long>(base))
            tooLarge = true;
        else
            read = read * static_cast<unsigned long long>(base) + digitValue;
    }

    if (tooLarge)
        return ENumberRead::TooLarge;

    magnitude = read;

    return ENumberRead::Ok;
}
//---------------------------------------------------------------------------
/*
    ReadQuotedItem

    Reads the quoted list item whose opening '"' is at 'at' in 'text' into 'item' (appended to it), without its quotes
    and with each "" in it as one '"', and moves 'at' past its closing '"'; false when the quote isn't closed.
*/
bool ReadQuotedItem(std::string_view text, std::size_t& at, std::string& item)
{
    std::size_t i = at + 1;
    while (i < text.size())
    {
        if (text[i] == '"')
        {
            if (i + 1 == text.size() || text[i + 1] != '"')
            {
                at = i + 1;
                return true;
            }

            // The first '"' of a ""
            ++i;
        }

        item += text[i];
        ++i;
    }

    return false;
}
//---------------------------------------------------------------------------
/*
    ScientificText

    The text DecimalText() makes of 'printed', a number as snprintf("%.*e") writes it ("1.2500e+03", with the C
    locale's decimal mark): its digits without trailing zeros, and its exponent.
*/
std::string ScientificText(char const* printed)
{
    std::string digits;
    char const* c = printed;
    for (; *c != '\0' && *c != 'e' && *c != 'E'; ++c)
    {
        if (*c >= '0' && *c <= '9')
            digits += *c;
    }

    int exponent = 0;
    if (*c != '\0')
    {
        ++c;
        bool const negative = *c == '-';
        if (*c == '-' || *c == '+')
            ++c;

        for (; *c >= '0' && *c <= '9'; ++c)
            exponent = exponent * 10 + (*c - '0');

        if (negative)
            exponent = -exponent;
    }

    std::size_t const last = digits.find_last_not_of('0');
    digits.erase(last == std::string::npos ? 1 : last + 1);

    return DecimalText(digits, exponent);
}
//---------------------------------------------------------------------------
/*
    SkipBlanks

    The position of the first character of 'text' from 'from' on that isn't a space or a tab (its size when there is
    none).
*/
std::size_t SkipBlanks(std::string_view text, std::size_t from) noexcept
{
    while (from < text.size() && Detail::IsBlank(text[from]))
        ++from;

    return from;
}
//---------------------------------------------------------------------------
#if defined(_WIN32)
/*
    Ucrt

    The Universal C Runtime's functions (see TUcrt), or nullptr when Windows doesn't have them. They are loaded at
    the first call and kept for the life of the process (ucrtbase.dll isn't unloaded).
*/
TUcrt const* Ucrt() noexcept
{
    static TUcrt const ucrt = LoadUcrt();

    return ucrt.Locale != nullptr ? &ucrt : nullptr;
}
//---------------------------------------------------------------------------
#endif
/*
    WholeNumberText

    'magnitude', a whole number (finite, not negative), as all its decimal digits.
*/
template <typename T>
std::string WholeNumberText(T magnitude)
{
    // magnitude = mantissa * 2^shift, with a mantissa of T's digits
    int exponent = 0;
    T const fraction = std::frexp(magnitude, &exponent);
    int const shift = exponent - std::numeric_limits<T>::digits;
    unsigned long long const mantissa = static_cast<unsigned long long>(
        std::ldexp(fraction, std::numeric_limits<T>::digits));
    if (shift <= 0)
        return Detail::IntegerText(static_cast<unsigned long long>(magnitude));

    // The mantissa doubled 'shift' times, in base 10^9 digits from the lowest
    unsigned long const base = 1000000000;
    std::vector<unsigned long> digits = { static_cast<unsigned long>(mantissa % base),
                                          static_cast<unsigned long>(mantissa / base % base),
                                          static_cast<unsigned long>(mantissa / base / base) };
    for (int i = 0; i < shift; ++i)
    {
        unsigned long carry = 0;
        for (unsigned long& digit : digits)
        {
            digit = digit * 2 + carry;
            carry = digit / base;
            digit %= base;
        }

        if (carry > 0)
            digits.push_back(carry);
    }

    while (digits.size() > 1 && digits.back() == 0)
        digits.pop_back();

    std::string text = Detail::IntegerText(static_cast<unsigned long long>(digits.back()));
    for (std::size_t i = digits.size() - 1; i > 0; --i)
    {
        std::string const part = Detail::IntegerText(static_cast<unsigned long long>(digits[i - 1]));
        text += std::string(9 - part.size(), '0') + part;
    }

    return text;
}
//---------------------------------------------------------------------------

} // namespace

//---------------------------------------------------------------------------

namespace Detail
{

//---------------------------------------------------------------------------
std::string FloatingPointText(double value, EFloatConversion conversion)
{
    return FloatText(value, conversion);
}
//---------------------------------------------------------------------------
std::string FloatingPointText(float value, EFloatConversion conversion)
{
    return FloatText(value, conversion);
}
//---------------------------------------------------------------------------
std::string IntegerText(long long value)
{
    // The magnitude as an unsigned long long, which holds that of the smallest long long too (std::to_chars() isn't
    // used: bcc32c's writes the smallest long long wrong)
    unsigned long long magnitude = static_cast<unsigned long long>(value);
    if (value < 0)
        magnitude = 0 - magnitude;

    std::string text = IntegerText(magnitude);
    if (value < 0)
        text.insert(0, 1, '-');

    return text;
}
//---------------------------------------------------------------------------
std::string IntegerText(unsigned long long value)
{
    // The digits from the last one back; a 64-bit unsigned long long has at most 20
    char digits[24];
    std::size_t begin = sizeof(digits);
    do
    {
        digits[--begin] = static_cast<char>('0' + value % 10);
        value /= 10;
    } while (value > 0);

    return std::string(digits + begin, sizeof(digits) - begin);
}
//---------------------------------------------------------------------------
TASWIniResult InvalidEnumName(std::string_view text, std::vector<char const*> const& names)
{
    if (names.empty())
        return ValueFailure(EIniCode::InvalidValue, text, "isn't a name of the enum: it has no names.");

    std::string problem = "isn't a name of the enum: it must be ";
    for (std::size_t i = 0; i < names.size(); ++i)
    {
        if (i > 0)
            problem += i + 1 < names.size() ? ", " : " or ";

        problem += names[i];
    }

    problem += '.';

    return ValueFailure(EIniCode::InvalidValue, text, problem);
}
//---------------------------------------------------------------------------
bool IsEnumName(std::string_view text, char const* name) noexcept
{
    return name != nullptr && EqualsNoCase(Trim(text), name);
}
//---------------------------------------------------------------------------
std::string JoinListItems(std::vector<std::string> const& items, char delimiter)
{
    std::string text;
    for (std::size_t i = 0; i < items.size(); ++i)
    {
        std::string const& item = items[i];
        if (i > 0)
            text += delimiter;

        // No space before an empty item, so the text never ends with one
        if (i > 0 && !item.empty())
            text += ' ';

        if (NeedsListQuotes(item, delimiter, items.size() == 1))
            text += QuotedListItem(item);
        else
            text += item;
    }

    return text;
}
//---------------------------------------------------------------------------
TASWIniResult ListItemReadFailure(std::string_view text, std::size_t number, TASWIniResult const& itemResult)
{
    std::string problem = "can't be read: item " + IntegerText(static_cast<unsigned long long>(number)) + ": ";
    problem += itemResult.Message;

    return ValueFailure(itemResult.Code, text, problem);
}
//---------------------------------------------------------------------------
TASWIniResult ListItemWriteFailure(std::size_t number, TASWIniResult const& itemResult)
{
    TASWIniResult result;
    result.Code = itemResult.Code;
    result.Message = "The list can't be written: item " + IntegerText(static_cast<unsigned long long>(number)) + ": ";
    result.Message += itemResult.Message;

    return result;
}
//---------------------------------------------------------------------------
std::string MessageText(std::string_view text)
{
    // A long text is cut before a byte that starts a UTF-8 sequence (or is ASCII), so no sequence is split
    std::size_t size = text.size();
    if (size > MaxMessageTextSize)
    {
        size = MaxMessageTextSize;
        while (size > 0 && (static_cast<unsigned char>(text[size]) & 0xC0) == 0x80)
            --size;
    }

    std::string quoted = "\"";
    for (char const c : text.substr(0, size))
    {
        unsigned char const byte = static_cast<unsigned char>(c);
        quoted += (byte < 0x20 || byte == 0x7F) ? '?' : c;
    }

    quoted += '"';
    if (size < text.size())
        quoted += "...";

    return quoted;
}
//---------------------------------------------------------------------------
TASWIniResult ReadFloatingPoint(std::string_view text, double& value, EFloatConversion conversion)
{
    return ReadFloat(text, value, conversion);
}
//---------------------------------------------------------------------------
TASWIniResult ReadFloatingPoint(std::string_view text, float& value, EFloatConversion conversion)
{
    return ReadFloat(text, value, conversion);
}
//---------------------------------------------------------------------------
TASWIniResult ReadSignedInteger(std::string_view text, long long min, long long max, long long& value)
{
    bool negative = false;
    unsigned long long magnitude = 0;
    ENumberRead const read = ReadMagnitude(text, negative, magnitude);
    if (read == ENumberRead::Invalid)
        return InvalidInteger(text);

    // The largest magnitude on the number's side of 0. A signed type's 'min' is negative, and its magnitude is one more
    // than that of -(min + 1), which can't overflow.
    unsigned long long limit = static_cast<unsigned long long>(max);
    if (negative)
        limit = static_cast<unsigned long long>(-(min + 1)) + 1;

    if (read == ENumberRead::TooLarge || magnitude > limit)
        return OutOfRange(text, IntegerText(min), IntegerText(max));

    if (!negative)
        value = static_cast<long long>(magnitude);
    else if (magnitude == limit)
        value = min;
    else
        value = -static_cast<long long>(magnitude);

    return TASWIniResult();
}
//---------------------------------------------------------------------------
TASWIniResult ReadUnsignedInteger(std::string_view text, unsigned long long max, unsigned long long& value)
{
    bool negative = false;
    unsigned long long magnitude = 0;
    ENumberRead const read = ReadMagnitude(text, negative, magnitude);
    if (read == ENumberRead::Invalid)
        return InvalidInteger(text);

    // "-0" is 0; any other negative number is out of range
    if (read == ENumberRead::TooLarge || magnitude > max || (negative && magnitude > 0))
        return OutOfRange(text, "0", IntegerText(max));

    value = magnitude;

    return TASWIniResult();
}
//---------------------------------------------------------------------------
TASWIniResult SplitListItems(std::string_view text, char delimiter, std::vector<std::string>& items)
{
    std::vector<std::string> split;
    if (!Trim(text).empty())
    {
        std::size_t at = 0;
        while (true)
        {
            // An item: blanks, then a quoted item, or the text up to the next delimiter
            at = SkipBlanks(text, at);
            if (at < text.size() && text[at] == '"')
            {
                std::string item;
                if (!ReadQuotedItem(text, at, item))
                    return InvalidList(text, split.size() + 1, "starts with a '\"' that isn't closed");

                at = SkipBlanks(text, at);
                if (at < text.size() && text[at] != delimiter)
                    return InvalidList(text, split.size() + 1, "has more than blanks after its closing '\"'");

                split.push_back(std::move(item));
            }
            else
            {
                std::size_t const end = std::min(text.find(delimiter, at), text.size());
                split.emplace_back(TrimRight(text.substr(at, end - at)));
                at = end;
            }

            if (at == text.size())
                break;

            // Past the delimiter
            ++at;
        }
    }

    items.swap(split);

    return TASWIniResult();
}
//---------------------------------------------------------------------------
TASWIniResult UnnamedEnumValue(std::string_view number)
{
    return ValueFailure(EIniCode::InvalidValue, number,
        "has no name in the enum's ASWIniEnumNames(), so it can't be written.");
}
//---------------------------------------------------------------------------
TASWIniResult ValueFailure(EIniCode code, std::string_view text, std::string_view problem)
{
    TASWIniResult result;
    result.Code = code;
    result.Message = "The value " + MessageText(text) + " ";
    result.Message += problem;

    return result;
}
//---------------------------------------------------------------------------

} // namespace Detail

//---------------------------------------------------------------------------

/////////////////////////////////////////////////////////////////////////////
// TASWIniConverter<bool>
/////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------
TASWIniResult TASWIniConverter<bool>::FromText(std::string_view text, bool& value,
    TASWIniValueOptions const& /*options*/)
{
    std::string_view const trimmed = Detail::Trim(text);
    for (std::string_view const trueText : TrueTexts)
    {
        if (Detail::EqualsNoCase(trimmed, trueText))
        {
            value = true;
            return TASWIniResult();
        }
    }

    for (std::string_view const falseText : FalseTexts)
    {
        if (Detail::EqualsNoCase(trimmed, falseText))
        {
            value = false;
            return TASWIniResult();
        }
    }

    return Detail::ValueFailure(EIniCode::InvalidValue, text,
        "isn't a bool: it must be true, false, yes, no, on, off, 1 or 0.");
}
//---------------------------------------------------------------------------
TASWIniResult TASWIniConverter<bool>::ToText(bool value, std::string& text, TASWIniValueOptions const& options)
{
    // A BoolFormat that isn't an EBoolFormat writes the default form
    char const* written = value ? "true" : "false";
    switch (options.BoolFormat)
    {
        case EBoolFormat::TrueFalse:
            break;

        case EBoolFormat::YesNo:
            written = value ? "yes" : "no";
            break;

        case EBoolFormat::OnOff:
            written = value ? "on" : "off";
            break;

        case EBoolFormat::OneZero:
            written = value ? "1" : "0";
            break;
    }

    text = written;

    return TASWIniResult();
}
//---------------------------------------------------------------------------


/////////////////////////////////////////////////////////////////////////////
// TASWIniConverter<std::string>
/////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------
TASWIniResult TASWIniConverter<std::string>::FromText(std::string_view text, std::string& value,
    TASWIniValueOptions const& /*options*/)
{
    value.assign(text.data(), text.size());

    return TASWIniResult();
}
//---------------------------------------------------------------------------
TASWIniResult TASWIniConverter<std::string>::ToText(std::string const& value, std::string& text,
    TASWIniValueOptions const& /*options*/)
{
    text = value;

    return TASWIniResult();
}
//---------------------------------------------------------------------------


/////////////////////////////////////////////////////////////////////////////
// TASWIniConverter<double>
/////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------
TASWIniResult TASWIniConverter<double>::FromText(std::string_view text, double& value,
    TASWIniValueOptions const& /*options*/)
{
    return Detail::ReadFloatingPoint(text, value, Detail::DefaultFloatConversion);
}
//---------------------------------------------------------------------------
TASWIniResult TASWIniConverter<double>::ToText(double value, std::string& text, TASWIniValueOptions const& /*options*/)
{
    text = Detail::FloatingPointText(value, Detail::DefaultFloatConversion);

    return TASWIniResult();
}
//---------------------------------------------------------------------------


/////////////////////////////////////////////////////////////////////////////
// TASWIniConverter<float>
/////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------
TASWIniResult TASWIniConverter<float>::FromText(std::string_view text, float& value,
    TASWIniValueOptions const& /*options*/)
{
    return Detail::ReadFloatingPoint(text, value, Detail::DefaultFloatConversion);
}
//---------------------------------------------------------------------------
TASWIniResult TASWIniConverter<float>::ToText(float value, std::string& text, TASWIniValueOptions const& /*options*/)
{
    text = Detail::FloatingPointText(value, Detail::DefaultFloatConversion);

    return TASWIniResult();
}
//---------------------------------------------------------------------------

} // namespace ASWIni
