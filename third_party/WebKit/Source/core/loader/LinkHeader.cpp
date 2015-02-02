// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/loader/LinkHeader.h"

#include "platform/ParsingUtilities.h"

namespace blink {

// LWSP definition in https://www.ietf.org/rfc/rfc0822.txt
template <typename CharType>
static bool isWhitespace(CharType chr)
{
    return (chr == ' ') || (chr == '\t');
}

template <typename CharType>
static bool isValidURLChar(CharType chr)
{
    return !isWhitespace(chr) && chr != '>';
}

template <typename CharType>
static bool isValidParameterNameChar(CharType chr)
{
    return !isWhitespace(chr) && chr != '=';
}

template <typename CharType>
static bool isValidParameterValueChar(CharType chr)
{
    return !isWhitespace(chr) && chr != ';';
}

// Before:
//
// <cat.jpg>; rel=preload
// ^                     ^
// position              end
//
// After (if successful: otherwise the method returns false)
//
// <cat.jpg>; rel=preload
//          ^            ^
//          position     end
template <typename CharType>
static bool parseURL(CharType*& position, CharType* end, String& url)
{
    skipWhile<CharType, isWhitespace>(position, end);
    if (!skipExactly<CharType>(position, end, '<'))
        return false;
    skipWhile<CharType, isWhitespace>(position, end);

    CharType* urlStart = position;
    skipWhile<CharType, isValidURLChar>(position, end);
    CharType* urlEnd = position;
    skipUntil<CharType>(position, end, '>');
    if (!skipExactly<CharType>(position, end, '>'))
        return false;

    url = String(urlStart, urlEnd - urlStart);
    return true;
}

// Before:
//
// <cat.jpg>; rel=preload
//          ^            ^
//          position     end
//
// After (if successful: otherwise the method returns false, and modifies the isValid boolean accordingly)
//
// <cat.jpg>; rel=preload
//            ^          ^
//            position  end
template <typename CharType>
static bool parseParameterDelimiter(CharType*& position, CharType* end, bool& isValidDelimiter)
{
    isValidDelimiter = true;
    skipWhile<CharType, isWhitespace>(position, end);
    if (!skipExactly<CharType>(position, end, ';') && (position < end)) {
        isValidDelimiter = false;
        return false;
    }
    skipWhile<CharType, isWhitespace>(position, end);
    if (position == end)
        return false;
    return true;
}

static LinkHeader::LinkParameterName paramterNameFromString(String name)
{
    // FIXME: Add support for more header parameters as neccessary.
    if (name == "rel")
        return LinkHeader::LinkParameterRel;
    return LinkHeader::LinkParameterUnknown;
}

// Before:
//
// <cat.jpg>; rel=preload
//            ^          ^
//            position   end
//
// After (if successful: otherwise the method returns false)
//
// <cat.jpg>; rel=preload
//                ^      ^
//            position  end
template <typename CharType>
static bool parseParameterName(CharType*& position, CharType* end, LinkHeader::LinkParameterName& name)
{
    CharType* nameStart = position;
    skipWhile<CharType, isValidParameterNameChar>(position, end);
    CharType* nameEnd = position;
    skipWhile<CharType, isWhitespace>(position, end);
    if (!skipExactly<CharType>(position, end, '='))
        return false;
    skipWhile<CharType, isWhitespace>(position, end);
    name = paramterNameFromString(String(nameStart, nameEnd - nameStart));
    return true;
}

// Before:
//
// <cat.jpg>; rel=preload; foo=bar
//                ^               ^
//            position            end
//
// After (if successful: otherwise the method returns false)
//
// <cat.jpg>; rel=preload; foo=bar
//                       ^        ^
//                   position     end
template <typename CharType>
static bool parseParameterValue(CharType*& position, CharType* end, String& value)
{
    CharType* valueStart = position;
    skipWhile<CharType, isValidParameterValueChar>(position, end);
    CharType* valueEnd = position;
    skipWhile<CharType, isWhitespace>(position, end);
    if ((valueEnd == valueStart) || (*position != ';' && position != end))
        return false;
    value = String(valueStart, valueEnd - valueStart);
    return true;
}

void LinkHeader::setValue(LinkParameterName name, String value)
{
    // FIXME: Add support for more header parameters as neccessary.
    if (name == LinkParameterRel)
        m_rel = value;
}

template <typename CharType>
bool LinkHeader::init(CharType* headerValue, unsigned len)
{
    CharType* position = headerValue;
    CharType* end = headerValue + len;

    if (!parseURL(position, end, m_url))
        return false;

    while (position < end) {
        bool isValidDelimiter;
        if (!parseParameterDelimiter(position, end, isValidDelimiter))
            return isValidDelimiter;

        LinkParameterName parameterName;
        if (!parseParameterName(position, end, parameterName))
            return false;

        String parameterValue;
        if (!parseParameterValue(position, end, parameterValue))
            return false;

        setValue(parameterName, parameterValue);
    }

    return true;
}

LinkHeader::LinkHeader(const String& header)
{
    if (header.isNull()) {
        m_isValid = false;
        return;
    }

    if (header.is8Bit())
        m_isValid = init(header.characters8(), header.length());
    else
        m_isValid = init(header.characters16(), header.length());
}

}
