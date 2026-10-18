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
#include <limits>
#include <string>
#include <string_view>
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

// What ReadMagnitude() found in a text
enum class EMagnitude : int
{
    Ok = 0, // An integer whose magnitude fits an unsigned long long
    Invalid = 1, // Not an integer
    TooLarge = 2, // An integer whose magnitude doesn't fit an unsigned long long
};

//---------------------------------------------------------------------------

int DigitValue(char c, int base) noexcept;
TASWIniResult InvalidInteger(std::string_view text);
TASWIniResult OutOfRange(std::string_view text, std::string const& min, std::string const& max);
EMagnitude ReadMagnitude(std::string_view text, bool& negative, unsigned long long& magnitude) noexcept;

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
    OutOfRange

    The ValueOutOfRange result of 'text', an integer outside the range 'min' to 'max' (as text).
*/
TASWIniResult OutOfRange(std::string_view text, std::string const& min, std::string const& max)
{
    return Detail::ValueFailure(EIniCode::ValueOutOfRange, text, "is out of range: it must be " + min + " to " + max +
        ".");
}
//---------------------------------------------------------------------------
/*
    ReadMagnitude

    Reads 'text' (the spaces and tabs around it ignored) as an optional sign, then decimal digits or 0x (or 0X) and
    hex digits: whether it is negative, and its magnitude (set only when Ok).
*/
EMagnitude ReadMagnitude(std::string_view text, bool& negative, unsigned long long& magnitude) noexcept
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
        return EMagnitude::Invalid;

    unsigned long long const largest = std::numeric_limits<unsigned long long>::max();
    unsigned long long read = 0;
    bool tooLarge = false;
    for (char const c : digits)
    {
        int const digit = DigitValue(c, base);
        if (digit < 0)
            return EMagnitude::Invalid;

        // read * base + digit must not pass 'largest'; after a digit that would, the rest must still be digits
        unsigned long long const digitValue = static_cast<unsigned long long>(digit);
        if (read > (largest - digitValue) / static_cast<unsigned long long>(base))
            tooLarge = true;
        else
            read = read * static_cast<unsigned long long>(base) + digitValue;
    }

    if (tooLarge)
        return EMagnitude::TooLarge;

    magnitude = read;

    return EMagnitude::Ok;
}
//---------------------------------------------------------------------------

} // namespace

//---------------------------------------------------------------------------

namespace Detail
{

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
TASWIniResult ReadSignedInteger(std::string_view text, long long min, long long max, long long& value)
{
    bool negative = false;
    unsigned long long magnitude = 0;
    EMagnitude const read = ReadMagnitude(text, negative, magnitude);
    if (read == EMagnitude::Invalid)
        return InvalidInteger(text);

    // The largest magnitude on the number's side of 0. A signed type's 'min' is negative, and its magnitude is one more
    // than that of -(min + 1), which can't overflow.
    unsigned long long limit = static_cast<unsigned long long>(max);
    if (negative)
        limit = static_cast<unsigned long long>(-(min + 1)) + 1;

    if (read == EMagnitude::TooLarge || magnitude > limit)
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
    EMagnitude const read = ReadMagnitude(text, negative, magnitude);
    if (read == EMagnitude::Invalid)
        return InvalidInteger(text);

    // "-0" is 0; any other negative number is out of range
    if (read == EMagnitude::TooLarge || magnitude > max || (negative && magnitude > 0))
        return OutOfRange(text, "0", IntegerText(max));

    value = magnitude;

    return TASWIniResult();
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

} // namespace ASWIni
