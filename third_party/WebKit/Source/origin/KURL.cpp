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

#include "wtf/MemoryInstrumentation.h"
#include "wtf/MemoryInstrumentationString.h"
#include "wtf/StdLibExtras.h"
#include "wtf/text/CString.h"
#include "wtf/text/StringHash.h"
#include "wtf/text/TextEncoding.h"
#include <googleurl/src/url_util.h>
#include <stdio.h>

namespace WebCore {

static const unsigned maximumValidPortNumber = 0xFFFE;
static const unsigned invalidPortNumber = 0xFFFF;

static void assertProtocolIsGood(const char* protocol)
{
#ifndef NDEBUG
    const char* p = protocol;
    while (*p) {
        ASSERT(*p > ' ' && *p < 0x7F && !(*p >= 'A' && *p <= 'Z'));
        ++p;
    }
#endif
}

// Returns the characters for the given string, or a pointer to a static empty
// string if the input string is null. This will always ensure we have a non-
// null character pointer since ReplaceComponents has special meaning for null.
static const url_parse::UTF16Char* charactersOrEmpty(const String& str)
{
    static const url_parse::UTF16Char zero = 0;
    return str.characters() ? reinterpret_cast<const url_parse::UTF16Char*>(str.characters()) : &zero;
}

// FIXME: This function should move to WTF.
bool lowerCaseEqualsASCII(const char* begin, const char* end, const char* str)
{
    while (begin != end && *str) {
        ASSERT(toASCIILower(*str) == *str);
        if (toASCIILower(*begin++) != *str++)
            return false;
    }

    // Both strings are equal (ignoring case) if and only if all of the characters were equal,
    // and the end of both has been reached.
    return begin == end && !*str;
}

static bool isSchemeFirstChar(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static bool isSchemeChar(char c)
{
    return isSchemeFirstChar(c) || (c >= '0' && c <= '9') || c == '.' || c == '-' || c == '+';
}

bool isValidProtocol(const String& protocol)
{
    // RFC3986: ALPHA *( ALPHA / DIGIT / "+" / "-" / "." )
    if (protocol.isEmpty())
        return false;
    if (!isSchemeFirstChar(protocol[0]))
        return false;
    unsigned protocolLength = protocol.length();
    for (unsigned i = 1; i < protocolLength; i++) {
        if (!isSchemeChar(protocol[i]))
            return false;
    }
    return true;
}

String KURL::strippedForUseAsReferrer() const
{
    KURL referrer(*this);
    referrer.setUser(String());
    referrer.setPass(String());
    referrer.removeFragmentIdentifier();
    return referrer.string();
}

bool KURL::isLocalFile() const
{
    // Including feed here might be a bad idea since drag and drop uses this check
    // and including feed would allow feeds to potentially let someone's blog
    // read the contents of the clipboard on a drag, even without a drop.
    // Likewise with using the FrameLoader::shouldTreatURLAsLocal() function.
    return protocolIs("file");
}

bool protocolIsJavaScript(const String& url)
{
    return protocolIs(url, "javascript");
}

const KURL& blankURL()
{
    DEFINE_STATIC_LOCAL(KURL, staticBlankURL, (ParsedURLString, "about:blank"));
    return staticBlankURL;
}

bool KURL::isBlankURL() const
{
    return protocolIs("about");
}

void KURL::reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
{
    WTF::MemoryClassInfo info(memoryObjectInfo, this);
    info.addMember(m_url, "url");
}

bool KURL::isSafeToSendToAnotherThread() const
{
    return m_url.isSafeToSendToAnotherThread();
}

String KURL::elidedString() const
{
    if (string().length() <= 1024)
        return string();

    return string().left(511) + "..." + string().right(510);
}

// Initializes with a string representing an absolute URL. No encoding
// information is specified. This generally happens when a KURL is converted
// to a string and then converted back. In this case, the URL is already
// canonical and in proper escaped form so needs no encoding. We treat it as
// UTF-8 just in case.
KURL::KURL(ParsedURLStringTag, const String& url)
{
    if (!url.isNull())
        m_url.init(KURL(), url, 0);
    else {
        // WebCore expects us to preserve the nullness of strings when this
        // constructor is used. In all other cases, it expects a non-null
        // empty string, which is what init() will create.
        m_url.m_isValid = false;
        m_url.m_protocolIsInHTTPFamily = false;
    }
}

// Constructs a new URL given a base URL and a possibly relative input URL.
// This assumes UTF-8 encoding.
KURL::KURL(const KURL& base, const String& relative)
{
    m_url.init(base, relative, 0);
}

// Constructs a new URL given a base URL and a possibly relative input URL.
// Any query portion of the relative URL will be encoded in the given encoding.
KURL::KURL(const KURL& base, const String& relative, const WTF::TextEncoding& encoding)
{
    m_url.init(base, relative, &encoding.encodingForFormSubmission());
}

KURL::KURL(const CString& canonicalSpec, const url_parse::Parsed& parsed, bool isValid)
    : m_url(parsed, isValid)
{
    // We know the reference fragment is the only part that can be UTF-8, so
    // we know it's ASCII when there is no ref.
    if (parsed.ref.is_nonempty())
        m_url.setUTF8(canonicalSpec);
    else
        m_url.setASCII(canonicalSpec);
}

KURL KURL::copy() const
{
    KURL result = *this;
    m_url.copyTo(&result.m_url);
    return result;
}

bool KURL::isNull() const
{
    return m_url.utf8String().isNull();
}

bool KURL::isEmpty() const
{
    return !m_url.utf8String().length();
}

bool KURL::isValid() const
{
    return m_url.m_isValid;
}

bool KURL::hasPort() const
{
    return hostEnd() < pathStart();
}

bool KURL::protocolIsInHTTPFamily() const
{
    return m_url.m_protocolIsInHTTPFamily;
}

bool KURL::hasPath() const
{
    // Note that http://www.google.com/" has a path, the path is "/". This can
    // return false only for invalid or nonstandard URLs.
    return m_url.m_parsed.path.len >= 0;
}

// We handle "parameters" separated by a semicolon, while KURL.cpp does not,
// which can lead to different results in some cases.
String KURL::lastPathComponent() const
{
    // When the output ends in a slash, WebCore has different expectations than
    // the GoogleURL library. For "/foo/bar/" the library will return the empty
    // string, but WebCore wants "bar".
    url_parse::Component path = m_url.m_parsed.path;
    if (path.len > 0 && m_url.utf8String().data()[path.end() - 1] == '/')
        path.len--;

    url_parse::Component file;
    url_parse::ExtractFileName(m_url.utf8String().data(), path, &file);

    // Bug: https://bugs.webkit.org/show_bug.cgi?id=21015 this function returns
    // a null string when the path is empty, which we duplicate here.
    if (!file.is_nonempty())
        return String();
    return m_url.componentString(file);
}

String KURL::protocol() const
{
    return m_url.componentString(m_url.m_parsed.scheme);
}

String KURL::host() const
{
    // Note: KURL.cpp unescapes here.
    return m_url.componentString(m_url.m_parsed.host);
}

// Returns 0 when there is no port.
//
// We treat URL's with out-of-range port numbers as invalid URLs, and they will
// be rejected by the canonicalizer. KURL.cpp will allow them in parsing, but
// return invalidPortNumber from this port() function, so we mirror that behavior here.
unsigned short KURL::port() const
{
    if (!m_url.m_isValid || m_url.m_parsed.port.len <= 0)
        return 0;
    int port = url_parse::ParsePort(m_url.utf8String().data(), m_url.m_parsed.port);
    ASSERT(port != url_parse::PORT_UNSPECIFIED); // Checked port.len <= 0 before.

    if (port == url_parse::PORT_INVALID || port > maximumValidPortNumber) // Mimic KURL::port()
        port = invalidPortNumber;

    return static_cast<unsigned short>(port);
}

// Returns the empty string if there is no password.
String KURL::pass() const
{
    // Bug: https://bugs.webkit.org/show_bug.cgi?id=21015 this function returns
    // a null string when the password is empty, which we duplicate here.
    if (!m_url.m_parsed.password.is_nonempty())
        return String();

    // Note: KURL.cpp unescapes here.
    return m_url.componentString(m_url.m_parsed.password);
}

// Returns the empty string if there is no username.
String KURL::user() const
{
    // Note: KURL.cpp unescapes here.
    return m_url.componentString(m_url.m_parsed.username);
}

String KURL::fragmentIdentifier() const
{
    // Empty but present refs ("foo.com/bar#") should result in the empty
    // string, which m_url.componentString will produce. Nonexistent refs
    // should be the null string.
    if (!m_url.m_parsed.ref.is_valid())
        return String();

    // Note: KURL.cpp unescapes here.
    return m_url.componentString(m_url.m_parsed.ref);
}

bool KURL::hasFragmentIdentifier() const
{
    // Note: KURL.cpp unescapes here.
    // FIXME determine if KURL.cpp agrees about an empty ref
    return m_url.m_parsed.ref.len >= 0;
}

String KURL::baseAsString() const
{
    // FIXME: There is probably a more efficient way to do this?
    return string().left(pathAfterLastSlash());
}

String KURL::query() const
{
    if (m_url.m_parsed.query.len >= 0)
        return m_url.componentString(m_url.m_parsed.query);

    // Bug: https://bugs.webkit.org/show_bug.cgi?id=21015 this function returns
    // an empty string when the query is empty rather than a null (not sure
    // which is right).
    // Returns a null if the query is not specified, instead of empty.
    if (m_url.m_parsed.query.is_valid())
        return String("", 0);
    return String();
}

String KURL::path() const
{
    return m_url.componentString(m_url.m_parsed.path);
}

bool KURL::setProtocol(const String& protocol)
{
    // Firefox and IE remove everything after the first ':'.
    int separatorPosition = protocol.find(':');
    String newProtocol = protocol.substring(0, separatorPosition);

    // If KURL is given an invalid scheme, it returns failure without modifying
    // the URL at all. This is in contrast to most other setters which modify
    // the URL and set "m_isValid."
    url_canon::RawCanonOutputT<char> canonProtocol;
    url_parse::Component protocolComponent;
    if (!url_canon::CanonicalizeScheme(newProtocol.characters(), url_parse::Component(0, newProtocol.length()), &canonProtocol, &protocolComponent)
        || !protocolComponent.is_nonempty())
        return false;

    KURLPrivate::Replacements replacements;
    replacements.SetScheme(charactersOrEmpty(newProtocol), url_parse::Component(0, newProtocol.length()));
    m_url.replaceComponents(replacements);

    // isValid could be false but we still return true here. This is because
    // WebCore or JS scripts can build up a URL by setting individual
    // components, and a JS exception is based on the return value of this
    // function. We want to throw the exception and stop the script only when
    // its trying to set a bad protocol, and not when it maybe just hasn't
    // finished building up its final scheme.
    return true;
}

void KURL::setHost(const String& host)
{
    KURLPrivate::Replacements replacements;
    replacements.SetHost(charactersOrEmpty(host), url_parse::Component(0, host.length()));
    m_url.replaceComponents(replacements);
}

void KURL::setHostAndPort(const String& s)
{
    String host = s;
    String port;
    int hostEnd = s.find(":");
    if (hostEnd != -1) {
        host = s.left(hostEnd);
        port = s.substring(hostEnd + 1);
    }

    KURLPrivate::Replacements replacements;
    // Host can't be removed, so we always set.
    replacements.SetHost(charactersOrEmpty(host), url_parse::Component(0, host.length()));

    if (port.isEmpty()) // Port may be removed, so we support clearing.
        replacements.ClearPort();
    else
        replacements.SetPort(charactersOrEmpty(port), url_parse::Component(0, port.length()));
    m_url.replaceComponents(replacements);
}

void KURL::removePort()
{
    if (!hasPort())
        return;
    String urlWithoutPort = m_url.string().left(hostEnd()) + m_url.string().substring(pathStart());
    m_url.setUTF8(urlWithoutPort.utf8());
}

void KURL::setPort(unsigned short i)
{
    KURLPrivate::Replacements replacements;
    String portString;

    portString = String::number(i);
    replacements.SetPort(reinterpret_cast<const url_parse::UTF16Char*>(portString.characters()), url_parse::Component(0, portString.length()));

    m_url.replaceComponents(replacements);
}

void KURL::setUser(const String& user)
{
    // This function is commonly called to clear the username, which we
    // normally don't have, so we optimize this case.
    if (user.isEmpty() && !m_url.m_parsed.username.is_valid())
        return;

    // The canonicalizer will clear any usernames that are empty, so we
    // don't have to explicitly call ClearUsername() here.
    KURLPrivate::Replacements replacements;
    replacements.SetUsername(charactersOrEmpty(user), url_parse::Component(0, user.length()));
    m_url.replaceComponents(replacements);
}

void KURL::setPass(const String& pass)
{
    // This function is commonly called to clear the password, which we
    // normally don't have, so we optimize this case.
    if (pass.isEmpty() && !m_url.m_parsed.password.is_valid())
        return;

    // The canonicalizer will clear any passwords that are empty, so we
    // don't have to explicitly call ClearUsername() here.
    KURLPrivate::Replacements replacements;
    replacements.SetPassword(charactersOrEmpty(pass), url_parse::Component(0, pass.length()));
    m_url.replaceComponents(replacements);
}

void KURL::setFragmentIdentifier(const String& s)
{
    // This function is commonly called to clear the ref, which we
    // normally don't have, so we optimize this case.
    if (s.isNull() && !m_url.m_parsed.ref.is_valid())
        return;

    KURLPrivate::Replacements replacements;
    if (s.isNull())
        replacements.ClearRef();
    else
        replacements.SetRef(charactersOrEmpty(s), url_parse::Component(0, s.length()));
    m_url.replaceComponents(replacements);
}

void KURL::removeFragmentIdentifier()
{
    KURLPrivate::Replacements replacements;
    replacements.ClearRef();
    m_url.replaceComponents(replacements);
}

void KURL::setQuery(const String& query)
{
    KURLPrivate::Replacements replacements;
    if (query.isNull()) {
        // KURL.cpp sets to null to clear any query.
        replacements.ClearQuery();
    } else if (query.length() > 0 && query[0] == '?') {
        // WebCore expects the query string to begin with a question mark, but
        // GoogleURL doesn't. So we trim off the question mark when setting.
        replacements.SetQuery(charactersOrEmpty(query), url_parse::Component(1, query.length() - 1));
    } else {
        // When set with the empty string or something that doesn't begin with
        // a question mark, KURL.cpp will add a question mark for you. The only
        // way this isn't compatible is if you call this function with an empty
        // string. KURL.cpp will leave a '?' with nothing following it in the
        // URL, whereas we'll clear it.
        // FIXME We should eliminate this difference.
        replacements.SetQuery(charactersOrEmpty(query), url_parse::Component(0, query.length()));
    }
    m_url.replaceComponents(replacements);
}

void KURL::setPath(const String& path)
{
    // Empty paths will be canonicalized to "/", so we don't have to worry
    // about calling ClearPath().
    KURLPrivate::Replacements replacements;
    replacements.SetPath(charactersOrEmpty(path), url_parse::Component(0, path.length()));
    m_url.replaceComponents(replacements);
}

String decodeURLEscapeSequences(const String& string)
{
    return decodeURLEscapeSequences(string, UTF8Encoding());
}

// In KURL.cpp's implementation, this is called by every component getter.
// It will unescape every character, including '\0'. This is scary, and may
// cause security holes. We never call this function for components, and
// just return the ASCII versions instead.
//
// This function is also used to decode javascript: URLs and as a general
// purpose unescaping function.
//
// FIXME These should be merged to the KURL.cpp implementation.
String decodeURLEscapeSequences(const String& string, const WTF::TextEncoding& encoding)
{
    // FIXME We can probably use KURL.cpp's version of this function
    // without modification. However, I'm concerned about
    // https://bugs.webkit.org/show_bug.cgi?id=20559 so am keeping this old
    // custom code for now. Using their version will also fix the bug that
    // we ignore the encoding.
    //
    // FIXME b/1350291: This does not get called very often. We just convert
    // first to 8-bit UTF-8, then unescape, then back to 16-bit. This kind of
    // sucks, and we don't use the encoding properly, which will make some
    // obscure anchor navigations fail.
    CString cstring = string.utf8();
    url_canon::RawCanonOutputT<url_parse::UTF16Char> unescaped;
    url_util::DecodeURLEscapeSequences(cstring.data(), cstring.length(), &unescaped);
    return String(reinterpret_cast<UChar*>(unescaped.data()), unescaped.length());
}

bool KURL::protocolIs(const char* protocol) const
{
    assertProtocolIsGood(protocol);

    // JavaScript URLs are "valid" and should be executed even if KURL decides they are invalid.
    // The free function protocolIsJavaScript() should be used instead.
    // FIXME: Chromium code needs to be fixed for this assert to be enabled. ASSERT(strcmp(protocol, "javascript"));

    if (m_url.m_parsed.scheme.len <= 0)
        return !protocol;
    return lowerCaseEqualsASCII(
        m_url.utf8String().data() + m_url.m_parsed.scheme.begin,
        m_url.utf8String().data() + m_url.m_parsed.scheme.end(),
        protocol);
}

String encodeWithURLEscapeSequences(const String& notEncodedString)
{
    CString utf8 = UTF8Encoding().encode(
        reinterpret_cast<const UChar*>(notEncodedString.characters()),
        notEncodedString.length(),
        WTF::URLEncodedEntitiesForUnencodables);

    url_canon::RawCanonOutputT<char> buffer;
    int inputLength = utf8.length();
    if (buffer.length() < inputLength * 3)
        buffer.Resize(inputLength * 3);

    url_util::EncodeURIComponent(utf8.data(), inputLength, &buffer);
    String escaped(buffer.data(), buffer.length());
    // Unescape '/'; it's safe and much prettier.
    escaped.replace("%2F", "/");
    return escaped;
}

bool KURL::isHierarchical() const
{
    if (!m_url.m_parsed.scheme.is_nonempty())
        return false;
    return url_util::IsStandard(&m_url.utf8String().data()[m_url.m_parsed.scheme.begin], m_url.m_parsed.scheme);
}

#ifndef NDEBUG
void KURL::print() const
{
    printf("%s\n", m_url.utf8String().data());
}
#endif

void KURL::invalidate()
{
    // This is only called from the constructor so resetting the (automatically
    // initialized) string and parsed structure would be a waste of time.
    m_url.m_isValid = false;
    m_url.m_protocolIsInHTTPFamily = false;
}

bool equalIgnoringFragmentIdentifier(const KURL& a, const KURL& b)
{
    // Compute the length of each URL without its ref. Note that the reference
    // begin (if it exists) points to the character *after* the '#', so we need
    // to subtract one.
    int aLength = a.m_url.utf8String().length();
    if (a.m_url.m_parsed.ref.len >= 0)
        aLength = a.m_url.m_parsed.ref.begin - 1;

    int bLength = b.m_url.utf8String().length();
    if (b.m_url.m_parsed.ref.len >= 0)
        bLength = b.m_url.m_parsed.ref.begin - 1;

    return aLength == bLength && !strncmp(a.m_url.utf8String().data(), b.m_url.utf8String().data(), aLength);
}

unsigned KURL::hostStart() const
{
    return m_url.m_parsed.CountCharactersBefore(url_parse::Parsed::HOST, false);
}

unsigned KURL::hostEnd() const
{
    return m_url.m_parsed.CountCharactersBefore(url_parse::Parsed::PORT, true);
}

unsigned KURL::pathStart() const
{
    return m_url.m_parsed.CountCharactersBefore(url_parse::Parsed::PATH, false);
}

unsigned KURL::pathEnd() const
{
    return m_url.m_parsed.CountCharactersBefore(url_parse::Parsed::QUERY, true);
}

unsigned KURL::pathAfterLastSlash() const
{
    // When there's no path, ask for what would be the beginning of it.
    if (!m_url.m_parsed.path.is_valid())
        return m_url.m_parsed.CountCharactersBefore(url_parse::Parsed::PATH, false);

    url_parse::Component filename;
    url_parse::ExtractFileName(m_url.utf8String().data(), m_url.m_parsed.path, &filename);
    return filename.begin;
}

bool protocolIs(const String& url, const char* protocol)
{
    assertProtocolIsGood(protocol);
    return url_util::FindAndCompareScheme(url.characters(), url.length(), protocol, 0);
}

inline bool KURL::protocolIs(const String& string, const char* protocol)
{
    return WebCore::protocolIs(string, protocol);
}

} // namespace WebCore
