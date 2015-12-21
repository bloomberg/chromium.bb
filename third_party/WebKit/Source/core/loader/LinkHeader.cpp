// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
    // TODO(yoav): We need to move this function to a central location and possibly rewrite as a lookup table. https://crbug.com/527324

    // Alpha-numeric is a valid char.
    // This is likely the common case - bailing early.
    if (isASCIIAlphanumeric(chr))
        return true;
    // A separator or CTL or '%', '*' or '\'' means the char is not valid.
    // So any of: |{}[]/\:;<=>?@,()*'"%
    if (chr <= ' ' || chr > '|' || chr == '{' || chr == ']' || chr == '['
        || chr == '/' || chr == '\\' || (chr <= '@' && chr >= ':') || chr == ','
        || (chr >= '(' && chr <= '*') || chr == '\'' || chr == '"' || chr == '%') {
        return false;
    }
    return true;
}

template <typename CharType>
static bool isValidParameterValueEnd(CharType chr)
{
    return chr == ';' || chr == ',';
}

template <typename CharType>
static bool isValidParameterValueChar(CharType chr)
{
    return !isWhitespace(chr) && !isValidParameterValueEnd(chr);
}

// Verify that the parameter is a link-extension which according to spec doesn't have to have a value.
static bool isExtensionParameter(LinkHeader::LinkParameterName name)
{
    return name >= LinkHeader::LinkParameterUnknown;
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

template <typename CharType>
static bool invalidParameterDelimiter(CharType*& position, CharType* end)
{
    return (!skipExactly<CharType>(position, end, ';') && (position < end) && (*position != ','));
}

template <typename CharType>
static bool validFieldEnd(CharType*& position, CharType* end)
{
    ASSERT(position <= end);
    return (position == end || *position == ',');
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
static bool parseParameterDelimiter(CharType*& position, CharType* end, bool& isValid)
{
    isValid = true;
    skipWhile<CharType, isWhitespace>(position, end);
    if (invalidParameterDelimiter(position, end)) {
        isValid = false;
        return false;
    }
    skipWhile<CharType, isWhitespace>(position, end);
    if (validFieldEnd(position, end))
        return false;
    return true;
}

static LinkHeader::LinkParameterName paramterNameFromString(String name)
{
    if (equalIgnoringCase(name, "rel"))
        return LinkHeader::LinkParameterRel;
    if (equalIgnoringCase(name, "anchor"))
        return LinkHeader::LinkParameterAnchor;
    if (equalIgnoringCase(name, "crossorigin"))
        return LinkHeader::LinkParameterCrossOrigin;
    if (equalIgnoringCase(name, "title"))
        return LinkHeader::LinkParameterTitle;
    if (equalIgnoringCase(name, "media"))
        return LinkHeader::LinkParameterMedia;
    if (equalIgnoringCase(name, "type"))
        return LinkHeader::LinkParameterType;
    if (equalIgnoringCase(name, "rev"))
        return LinkHeader::LinkParameterRev;
    if (equalIgnoringCase(name, "hreflang"))
        return LinkHeader::LinkParameterHreflang;
    if (equalIgnoringCase(name, "as"))
        return LinkHeader::LinkParameterAs;
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
    bool hasEqual = skipExactly<CharType>(position, end, '=');
    skipWhile<CharType, isWhitespace>(position, end);
    name = paramterNameFromString(String(nameStart, nameEnd - nameStart));
    if (hasEqual)
        return true;
    bool validParameterValueEnd = (position == end) || isValidParameterValueEnd(*position);
    return validParameterValueEnd && isExtensionParameter(name);
}

// Before:
//
// <cat.jpg>; rel="preload"; type="image/jpeg";
//                ^                            ^
//            position                        end
//
// After (if the parameter starts with a quote, otherwise the method returns false)
//
// <cat.jpg>; rel="preload"; type="image/jpeg";
//                         ^                   ^
//                     position               end
template <typename CharType>
static bool skipQuotesIfNeeded(CharType*& position, CharType* end, bool& completeQuotes)
{
    ASSERT(position <= end);
    unsigned char quote;
    completeQuotes = false;
    if (skipExactly<CharType>(position, end, '\''))
        quote = '\'';
    else if (skipExactly<CharType>(position, end, '"'))
        quote = '"';
    else
        return false;

    while (!completeQuotes && position < end) {
        skipUntil(position, end, static_cast<CharType>(quote));
        if (*(position - 1) != '\\')
            completeQuotes = true;
        completeQuotes = skipExactly(position, end, static_cast<CharType>(quote)) && completeQuotes;
    }
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
    CharType* valueEnd = position;
    bool completeQuotes;
    bool hasQuotes = skipQuotesIfNeeded(position, end, completeQuotes);
    if (!hasQuotes) {
        skipWhile<CharType, isValidParameterValueChar>(position, end);
    }
    valueEnd = position;
    skipWhile<CharType, isWhitespace>(position, end);
    if ((!completeQuotes && valueStart == valueEnd) || (position != end && !isValidParameterValueEnd(*position))) {
        value = String("");
        return false;
    }
    if (hasQuotes)
        ++valueStart;
    if (completeQuotes)
        --valueEnd;
    value = String(valueStart, valueEnd - valueStart);
    return !hasQuotes || completeQuotes;
}

void LinkHeader::setValue(LinkParameterName name, String value)
{
    // FIXME: Add support for more header parameters as neccessary.
    if (name == LinkParameterRel && !m_rel)
        m_rel = value.lower();
    else if (name == LinkParameterAnchor)
        m_isValid = false;
    else if (name == LinkParameterCrossOrigin)
        m_crossOrigin = crossOriginAttributeValue(value);
    else if (name == LinkParameterAs)
        m_as = value.lower();
}

template <typename CharType>
static void findNextHeader(CharType*& position, CharType* end)
{
    skipUntil<CharType>(position, end, ',');
    skipExactly<CharType>(position, end, ',');
}

template <typename CharType>
LinkHeader::LinkHeader(CharType*& position, CharType* end)
    : m_crossOrigin(CrossOriginAttributeNotSet)
    , m_isValid(true)
{
    if (!parseURL(position, end, m_url)) {
        m_isValid = false;
        findNextHeader(position, end);
        return;
    }

    while (m_isValid && position < end) {
        if (!parseParameterDelimiter(position, end, m_isValid)) {
            findNextHeader(position, end);
            return;
        }

        LinkParameterName parameterName;
        if (!parseParameterName(position, end, parameterName)) {
            findNextHeader(position, end);
            m_isValid = false;
            return;
        }

        String parameterValue;
        if (!parseParameterValue(position, end, parameterValue) && !isExtensionParameter(parameterName)) {
            findNextHeader(position, end);
            m_isValid = false;
            return;
        }

        setValue(parameterName, parameterValue);
    }
}

LinkHeaderSet::LinkHeaderSet(const String& header)
{
    if (header.isNull())
        return;

    if (header.is8Bit())
        init(header.characters8(), header.length());
    else
        init(header.characters16(), header.length());
}

template <typename CharType>
void LinkHeaderSet::init(CharType* headerValue, unsigned len)
{
    CharType* position = headerValue;
    CharType* end = headerValue + len;
    while (position < end)
        m_headerSet.append(LinkHeader(position, end));
}

}
