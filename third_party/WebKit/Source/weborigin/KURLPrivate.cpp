/*
 * Copyright (C) 2004, 2007, 2008, 2011, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Research In Motion Limited. All rights reserved.
 * Copyright (C) 2008, 2009, 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "weborigin/KURL.h"

#include "wtf/HashMap.h"
#include "wtf/MemoryInstrumentation.h"
#include "wtf/MemoryInstrumentationString.h"
#include "wtf/text/CString.h"
#include "wtf/text/StringHash.h"
#include "wtf/text/StringUTF8Adaptor.h"
#include "wtf/text/TextEncoding.h"
#include <algorithm>
#include <googleurl/src/url_util.h>

namespace WebCore {

namespace {

static bool isUnicodeEncoding(const WTF::TextEncoding* encoding)
{
    return encoding->encodingForFormSubmission() == UTF8Encoding();
}

class KURLCharsetConverter : public url_canon::CharsetConverter {
public:
    // The encoding parameter may be 0, but in this case the object must not be called.
    explicit KURLCharsetConverter(const WTF::TextEncoding* encoding)
        : m_encoding(encoding)
    {
    }

    virtual void ConvertFromUTF16(const url_parse::UTF16Char* input, int inputLength, url_canon::CanonOutput* output)
    {
        CString encoded = m_encoding->encode(input, inputLength, WTF::URLEncodedEntitiesForUnencodables);
        output->Append(encoded.data(), static_cast<int>(encoded.length()));
    }

private:
    const WTF::TextEncoding* m_encoding;
};

} // namespace

KURLPrivate::KURLPrivate()
    : m_isValid(false)
    , m_protocolIsInHTTPFamily(false)
{
}

KURLPrivate::KURLPrivate(const CString& canonicalSpec, const url_parse::Parsed& parsed, bool isValid)
    : m_isValid(isValid)
    , m_protocolIsInHTTPFamily(false)
    , m_parsed(parsed)
    , m_string(String::fromUTF8(canonicalSpec))
{
    initProtocolIsInHTTPFamily();
    initInnerURL();
}

KURLPrivate::KURLPrivate(WTF::HashTableDeletedValueType)
    : m_string(WTF::HashTableDeletedValue)
{
}

KURLPrivate::KURLPrivate(const KURLPrivate& o)
    : m_isValid(o.m_isValid)
    , m_protocolIsInHTTPFamily(o.m_protocolIsInHTTPFamily)
    , m_parsed(o.m_parsed)
    , m_string(o.m_string)
{
    if (o.m_innerURL.get())
        m_innerURL = adoptPtr(new KURL(o.m_innerURL->copy()));
}

KURLPrivate& KURLPrivate::operator=(const KURLPrivate& o)
{
    m_isValid = o.m_isValid;
    m_protocolIsInHTTPFamily = o.m_protocolIsInHTTPFamily;
    m_parsed = o.m_parsed;
    m_string = o.m_string;
    if (o.m_innerURL.get())
        m_innerURL = adoptPtr(new KURL(o.m_innerURL->copy()));
    else
        m_innerURL.clear();
    return *this;
}

void KURLPrivate::init(const KURL& base, const String& relative, const WTF::TextEncoding* queryEncoding)
{
    if (!relative.isNull() && relative.is8Bit()) {
        StringUTF8Adaptor relativeUTF8(relative);
        init(base, relativeUTF8.data(), relativeUTF8.length(), queryEncoding);
    } else
        init(base, relative.characters16(), relative.length(), queryEncoding);
    initProtocolIsInHTTPFamily();
    initInnerURL();
}

template <typename CHAR>
void KURLPrivate::init(const KURL& base, const CHAR* relative, int relativeLength, const WTF::TextEncoding* queryEncoding)
{
    // As a performance optimization, we do not use the charset converter
    // if encoding is UTF-8 or other Unicode encodings. Note that this is
    // per HTML5 2.5.3 (resolving URL). The URL canonicalizer will be more
    // efficient with no charset converter object because it can do UTF-8
    // internally with no extra copies.

    // We feel free to make the charset converter object every time since it's
    // just a wrapper around a reference.
    KURLCharsetConverter charsetConverterObject(queryEncoding);
    KURLCharsetConverter* charsetConverter = (!queryEncoding || isUnicodeEncoding(queryEncoding)) ? 0 : &charsetConverterObject;

    StringUTF8Adaptor baseUTF8(base.m_url.string());

    url_canon::RawCanonOutputT<char> output;
    m_isValid = url_util::ResolveRelative(baseUTF8.data(), baseUTF8.length(), base.m_url.m_parsed, relative, relativeLength, charsetConverter, &output, &m_parsed);

    // See FIXME in KURLPrivate in the header. If canonicalization has not
    // changed the string, we can avoid an extra allocation by using assignment.
    m_string = String::fromUTF8(output.data(), output.length());
}

void KURLPrivate::initInnerURL()
{
    if (!m_isValid) {
        m_innerURL.clear();
        return;
    }
    url_parse::Parsed* innerParsed = m_parsed.inner_parsed();
    if (innerParsed)
        m_innerURL = adoptPtr(new KURL(ParsedURLString, m_string.substring(innerParsed->scheme.begin, innerParsed->Length() - innerParsed->scheme.begin)));
    else
        m_innerURL.clear();
}

template<typename CHAR>
bool internalProtocolIs(const url_parse::Component& scheme, const CHAR* spec, const char* protocol)
{
    const CHAR* begin = spec + scheme.begin;
    const CHAR* end = begin + scheme.len;

    while (begin != end && *protocol) {
        ASSERT(toASCIILower(*protocol) == *protocol);
        if (toASCIILower(*begin++) != *protocol++)
            return false;
    }

    // Both strings are equal (ignoring case) if and only if all of the characters were equal,
    // and the end of both has been reached.
    return begin == end && !*protocol;
}

template<typename CHAR>
bool protocolIsInHTTPFamily(const url_parse::Component& scheme, const CHAR* spec)
{
    if (scheme.len == 4)
        return internalProtocolIs(scheme, spec, "http");
    if (scheme.len == 5)
        return internalProtocolIs(scheme, spec, "https");
    return false;
}

void KURLPrivate::initProtocolIsInHTTPFamily()
{
    if (!m_isValid) {
        m_protocolIsInHTTPFamily = false;
        return;
    }

    if (!m_string.isNull() && m_string.is8Bit())
        m_protocolIsInHTTPFamily = protocolIsInHTTPFamily(m_parsed.scheme, m_string.characters8());
    else
        m_protocolIsInHTTPFamily = protocolIsInHTTPFamily(m_parsed.scheme, m_string.characters16());
}

bool KURLPrivate::protocolIs(const char* protocol) const
{
    if (!m_string.isNull() && m_string.is8Bit())
        return internalProtocolIs(m_parsed.scheme, m_string.characters8(), protocol);
    return internalProtocolIs(m_parsed.scheme, m_string.characters16(), protocol);
}

void KURLPrivate::copyTo(KURLPrivate* dest) const
{
    dest->m_isValid = m_isValid;
    dest->m_protocolIsInHTTPFamily = m_protocolIsInHTTPFamily;
    dest->m_parsed = m_parsed;
    dest->m_string = m_string.isolatedCopy();
    if (m_innerURL)
        dest->m_innerURL = adoptPtr(new KURL(m_innerURL->copy()));
    else
        dest->m_innerURL.clear();
}

String KURLPrivate::componentString(const url_parse::Component& component) const
{
    if (!m_isValid || component.len <= 0) {
        // KURL returns a null string if the URL is itself a null string, and an
        // empty string for other nonexistent entities.
        if (m_string.isNull())
            return String();
        return emptyString();
    }
    // begin and len are in terms of bytes which do not match
    // if string() is UTF-16 and input contains non-ASCII characters.
    // However, the only part in urlString that can contain non-ASCII
    // characters is 'ref' at the end of the string. In that case,
    // begin will always match the actual value and len (in terms of
    // byte) will be longer than what's needed by 'mid'. However, mid
    // truncates len to avoid go past the end of a string so that we can
    // get away without doing anything here.
    return string().substring(component.begin, component.len);
}

void KURLPrivate::replaceComponents(const Replacements& replacements)
{
    url_canon::RawCanonOutputT<char> output;
    url_parse::Parsed newParsed;

    StringUTF8Adaptor utf8(m_string);
    m_isValid = url_util::ReplaceComponents(utf8.data(), utf8.length(), m_parsed, replacements, 0, &output, &newParsed);

    m_parsed = newParsed;
    m_string = String::fromUTF8(output.data(), output.length());
}

const String& KURLPrivate::string() const
{
    return m_string;
}

void KURLPrivate::reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
{
    WTF::MemoryClassInfo info(memoryObjectInfo, this);
    info.addMember(m_string, "string");
    info.addMember(m_innerURL, "innerURL");
    info.addMember(m_parsed, "parsed");
}

bool KURLPrivate::isSafeToSendToAnotherThread() const
{
    return m_string.isSafeToSendToAnotherThread()
        && (!m_innerURL || m_innerURL->isSafeToSendToAnotherThread());
}

} // namespace WebCore
