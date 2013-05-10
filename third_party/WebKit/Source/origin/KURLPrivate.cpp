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
#include "origin/KURL.h"

#include "wtf/HashMap.h"
#include "wtf/MemoryInstrumentation.h"
#include "wtf/MemoryInstrumentationString.h"
#include "wtf/text/CString.h"
#include "wtf/text/StringHash.h"
#include "wtf/text/TextEncoding.h"
#include <algorithm>
#include <googleurl/src/url_util.h>

namespace WebCore {

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

KURLPrivate::KURLPrivate()
    : m_isValid(false)
    , m_protocolIsInHTTPFamily(false)
    , m_utf8IsASCII(true)
    , m_stringIsValid(false)
{
}

KURLPrivate::KURLPrivate(const url_parse::Parsed& parsed, bool isValid)
    : m_isValid(isValid)
    , m_protocolIsInHTTPFamily(false)
    , m_parsed(parsed)
    , m_utf8IsASCII(true)
    , m_stringIsValid(false)
{
}

KURLPrivate::KURLPrivate(WTF::HashTableDeletedValueType)
    : m_string(WTF::HashTableDeletedValue)
{
}

KURLPrivate::KURLPrivate(const KURLPrivate& o)
    : m_isValid(o.m_isValid)
    , m_protocolIsInHTTPFamily(o.m_protocolIsInHTTPFamily)
    , m_parsed(o.m_parsed)
    , m_utf8(o.m_utf8)
    , m_utf8IsASCII(o.m_utf8IsASCII)
    , m_stringIsValid(o.m_stringIsValid)
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
    m_utf8 = o.m_utf8;
    m_utf8IsASCII = o.m_utf8IsASCII;
    m_stringIsValid = o.m_stringIsValid;
    m_string = o.m_string;
    if (o.m_innerURL.get())
        m_innerURL = adoptPtr(new KURL(o.m_innerURL->copy()));
    else
        m_innerURL.clear();
    return *this;
}

// Setters for the data. Using the ASCII version when you know the
// data is ASCII will be slightly more efficient. The UTF-8 version
// will always be correct if the caller is unsure.
void KURLPrivate::setUTF8(const CString& string)
{
    const char* data = string.data();
    unsigned dataLength = string.length();

    // The m_utf8IsASCII must always be correct since the DeprecatedString
    // getter must create it with the proper constructor. This test can be
    // removed when DeprecatedString is gone, but it still might be a
    // performance win.
    m_utf8IsASCII = true;
    for (unsigned i = 0; i < dataLength; i++) {
        if (static_cast<unsigned char>(data[i]) >= 0x80) {
            m_utf8IsASCII = false;
            break;
        }
    }

    m_utf8 = string;
    m_stringIsValid = false;
    initProtocolIsInHTTPFamily();
    initInnerURL();
}

void KURLPrivate::setASCII(const CString& string)
{
    m_utf8 = string;
    m_utf8IsASCII = true;
    m_stringIsValid = false;
    initProtocolIsInHTTPFamily();
    initInnerURL();
}

void KURLPrivate::init(const KURL& base, const String& relative, const WTF::TextEncoding* queryEncoding)
{
    init(base, relative.characters(), relative.length(), queryEncoding);
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

    url_canon::RawCanonOutputT<char> output;
    const CString& baseStr = base.m_url.utf8String();
    m_isValid = url_util::ResolveRelative(baseStr.data(), baseStr.length(), base.m_url.m_parsed, relative, relativeLength, charsetConverter, &output, &m_parsed);

    // See FIXME in KURLPrivate in the header. If canonicalization has not
    // changed the string, we can avoid an extra allocation by using assignment.
    //
    // When KURL encounters an error such that the URL is invalid and empty
    // (for example, resolving a relative URL on a non-hierarchical base), it
    // will produce an isNull URL, and calling setUTF8 will produce an empty
    // non-null URL. This is unlikely to affect anything, but we preserve this
    // just in case.
    if (m_isValid || output.length()) {
        // Without ref, the whole url is guaranteed to be ASCII-only.
        if (m_parsed.ref.is_nonempty())
            setUTF8(CString(output.data(), output.length()));
        else
            setASCII(CString(output.data(), output.length()));
    } else {
        // WebCore expects resolved URLs to be empty rather than null.
        setUTF8(CString("", 0));
    }
}

void KURLPrivate::initInnerURL()
{
    if (!m_isValid) {
        m_innerURL.clear();
        return;
    }
    url_parse::Parsed* innerParsed = m_parsed.inner_parsed();
    if (innerParsed)
        m_innerURL = adoptPtr(new KURL(ParsedURLString, String(m_utf8.data() + innerParsed->scheme.begin, innerParsed->Length() - innerParsed->scheme.begin)));
    else
        m_innerURL.clear();
}

void KURLPrivate::initProtocolIsInHTTPFamily()
{
    if (!m_isValid) {
        m_protocolIsInHTTPFamily = false;
        return;
    }

    const char* scheme = m_utf8.data() + m_parsed.scheme.begin;
    if (m_parsed.scheme.len == 4)
        m_protocolIsInHTTPFamily = lowerCaseEqualsASCII(scheme, scheme + 4, "http");
    else if (m_parsed.scheme.len == 5)
        m_protocolIsInHTTPFamily = lowerCaseEqualsASCII(scheme, scheme + 5, "https");
    else
        m_protocolIsInHTTPFamily = false;
}

void KURLPrivate::copyTo(KURLPrivate* dest) const
{
    dest->m_isValid = m_isValid;
    dest->m_protocolIsInHTTPFamily = m_protocolIsInHTTPFamily;
    dest->m_parsed = m_parsed;

    // Don't copy the 16-bit string since that will be regenerated as needed.
    dest->m_utf8 = CString(m_utf8.data(), m_utf8.length());
    dest->m_utf8IsASCII = m_utf8IsASCII;
    dest->m_stringIsValid = false;
    dest->m_string = String(); // Clear the invalid string to avoid cross thread ref counting.
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
        if (utf8String().isNull())
            return String();
        return String("", 0);
    }
    // begin and len are in terms of bytes which do not match
    // if string() is UTF-16 and input contains non-ASCII characters.
    // However, the only part in urlString that can contain non-ASCII
    // characters is 'ref' at the end of the string. In that case,
    // begin will always match the actual value and len (in terms of
    // byte) will be longer than what's needed by 'mid'. However, mid
    // truncates len to avoid go past the end of a string so that we can
    // get away withtout doing anything here.
    return string().substring(component.begin, component.len);
}

void KURLPrivate::replaceComponents(const Replacements& replacements)
{
    url_canon::RawCanonOutputT<char> output;
    url_parse::Parsed newParsed;

    m_isValid = url_util::ReplaceComponents(utf8String().data(), utf8String().length(), m_parsed, replacements, 0, &output, &newParsed);

    m_parsed = newParsed;
    if (m_parsed.ref.is_nonempty())
        setUTF8(CString(output.data(), output.length()));
    else
        setASCII(CString(output.data(), output.length()));
}

const String& KURLPrivate::string() const
{
    if (!m_stringIsValid) {
        // Handle the null case separately. Otherwise, constructing
        // the string like we do below would generate the empty string,
        // not the null string.
        if (m_utf8.isNull())
            m_string = String();
        else if (m_utf8IsASCII)
            m_string = String(m_utf8.data(), m_utf8.length());
        else
            m_string = String::fromUTF8(m_utf8.data(), m_utf8.length());
        m_stringIsValid = true;
    }
    return m_string;
}

void KURLPrivate::reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
{
    WTF::MemoryClassInfo info(memoryObjectInfo, this);
    info.addMember(m_utf8, "utf8");
    info.addMember(m_string, "string");
    info.addMember(m_innerURL, "innerURL");
    info.addMember(m_parsed, "parsed");
}

bool KURLPrivate::isSafeToSendToAnotherThread() const
{
    return m_string.isSafeToSendToAnotherThread()
        && m_utf8.isSafeToSendToAnotherThread()
        && (!m_innerURL || m_innerURL->isSafeToSendToAnotherThread());
}

} // namespace WebCore
