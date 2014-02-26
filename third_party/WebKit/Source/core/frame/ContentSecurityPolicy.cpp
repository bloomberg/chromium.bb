/*
 * Copyright (C) 2011 Google, Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
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
#include "core/frame/ContentSecurityPolicy.h"

#include "RuntimeEnabledFeatures.h"
#include "bindings/v8/ScriptCallStackFactory.h"
#include "bindings/v8/ScriptController.h"
#include "core/dom/DOMStringList.h"
#include "core/dom/Document.h"
#include "core/events/SecurityPolicyViolationEvent.h"
#include "core/frame/ContentSecurityPolicyResponseHeaders.h"
#include "core/frame/DOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/UseCounter.h"
#include "core/frame/csp/CSPSource.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/inspector/ScriptCallStack.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/PingLoader.h"
#include "platform/JSONValues.h"
#include "platform/NotImplemented.h"
#include "platform/ParsingUtilities.h"
#include "platform/network/ContentSecurityPolicyParsers.h"
#include "platform/network/FormData.h"
#include "platform/network/ResourceResponse.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/KnownPorts.h"
#include "platform/weborigin/SchemeRegistry.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "wtf/SHA1.h"
#include "wtf/StringHasher.h"
#include "wtf/text/Base64.h"
#include "wtf/text/StringBuilder.h"

namespace WTF {

struct VectorIntHash {
    static unsigned hash(const Vector<uint8_t>& v) { return StringHasher::computeHash(v.data(), v.size()); }
    static bool equal(const Vector<uint8_t>& a, const Vector<uint8_t>& b) { return a == b; };
    static const bool safeToCompareToEmptyOrDeleted = true;
};
template<> struct DefaultHash<Vector<uint8_t> > {
    typedef VectorIntHash Hash;
};

} // namespace WTF

namespace WebCore {

// CSP 1.0 Directives
static const char connectSrc[] = "connect-src";
static const char defaultSrc[] = "default-src";
static const char fontSrc[] = "font-src";
static const char frameSrc[] = "frame-src";
static const char imgSrc[] = "img-src";
static const char mediaSrc[] = "media-src";
static const char objectSrc[] = "object-src";
static const char reportURI[] = "report-uri";
static const char sandbox[] = "sandbox";
static const char scriptSrc[] = "script-src";
static const char styleSrc[] = "style-src";

// CSP 1.1 Directives
static const char baseURI[] = "base-uri";
static const char childSrc[] = "child-src";
static const char formAction[] = "form-action";
static const char frameAncestors[] = "frame-ancestors";
static const char pluginTypes[] = "plugin-types";
static const char reflectedXSS[] = "reflected-xss";
static const char referrer[] = "referrer";

static bool isDirectiveName(const String& name)
{
    return (equalIgnoringCase(name, connectSrc)
        || equalIgnoringCase(name, defaultSrc)
        || equalIgnoringCase(name, fontSrc)
        || equalIgnoringCase(name, frameSrc)
        || equalIgnoringCase(name, imgSrc)
        || equalIgnoringCase(name, mediaSrc)
        || equalIgnoringCase(name, objectSrc)
        || equalIgnoringCase(name, reportURI)
        || equalIgnoringCase(name, sandbox)
        || equalIgnoringCase(name, scriptSrc)
        || equalIgnoringCase(name, styleSrc)
        || equalIgnoringCase(name, baseURI)
        || equalIgnoringCase(name, childSrc)
        || equalIgnoringCase(name, formAction)
        || equalIgnoringCase(name, frameAncestors)
        || equalIgnoringCase(name, pluginTypes)
        || equalIgnoringCase(name, reflectedXSS)
        || equalIgnoringCase(name, referrer)
    );
}

static UseCounter::Feature getUseCounterType(ContentSecurityPolicy::HeaderType type)
{
    switch (type) {
    case ContentSecurityPolicy::Enforce:
        return UseCounter::ContentSecurityPolicy;
    case ContentSecurityPolicy::Report:
        return UseCounter::ContentSecurityPolicyReportOnly;
    }
    ASSERT_NOT_REACHED();
    return UseCounter::NumberOfFeatures;
}

static ReferrerPolicy mergeReferrerPolicies(ReferrerPolicy a, ReferrerPolicy b)
{
    if (a != b)
        return ReferrerPolicyNever;
    return a;
}

static bool isSourceListNone(const UChar* begin, const UChar* end)
{
    skipWhile<UChar, isASCIISpace>(begin, end);

    const UChar* position = begin;
    skipWhile<UChar, isSourceCharacter>(position, end);
    if (!equalIgnoringCase("'none'", begin, position - begin))
        return false;

    skipWhile<UChar, isASCIISpace>(position, end);
    if (position != end)
        return false;

    return true;
}

class CSPSourceList {
public:
    CSPSourceList(ContentSecurityPolicy*, const String& directiveName);

    void parse(const UChar* begin, const UChar* end);

    bool matches(const KURL&);
    bool allowInline() const { return m_allowInline; }
    bool allowEval() const { return m_allowEval; }
    bool allowNonce(const String& nonce) const { return !nonce.isNull() && m_nonces.contains(nonce); }
    bool allowHash(const CSPHashValue& hashValue) const { return m_hashes.contains(hashValue); }
    uint8_t hashAlgorithmsUsed() const { return m_hashAlgorithmsUsed; }

    bool isHashOrNoncePresent() const { return !m_nonces.isEmpty() || m_hashAlgorithmsUsed != ContentSecurityPolicy::HashAlgorithmsNone; }

private:
    bool parseSource(const UChar* begin, const UChar* end, String& scheme, String& host, int& port, String& path, bool& hostHasWildcard, bool& portHasWildcard);
    bool parseScheme(const UChar* begin, const UChar* end, String& scheme);
    bool parseHost(const UChar* begin, const UChar* end, String& host, bool& hostHasWildcard);
    bool parsePort(const UChar* begin, const UChar* end, int& port, bool& portHasWildcard);
    bool parsePath(const UChar* begin, const UChar* end, String& path);
    bool parseNonce(const UChar* begin, const UChar* end, String& nonce);
    bool parseHash(const UChar* begin, const UChar* end, Vector<uint8_t>& hash, ContentSecurityPolicy::HashAlgorithms&);

    void addSourceSelf();
    void addSourceStar();
    void addSourceUnsafeInline();
    void addSourceUnsafeEval();
    void addSourceNonce(const String& nonce);
    void addSourceHash(const ContentSecurityPolicy::HashAlgorithms&, const Vector<uint8_t>& hash);

    ContentSecurityPolicy* m_policy;
    Vector<CSPSource> m_list;
    String m_directiveName;
    bool m_allowStar;
    bool m_allowInline;
    bool m_allowEval;
    HashSet<String> m_nonces;
    HashSet<CSPHashValue> m_hashes;
    uint8_t m_hashAlgorithmsUsed;
};

CSPSourceList::CSPSourceList(ContentSecurityPolicy* policy, const String& directiveName)
    : m_policy(policy)
    , m_directiveName(directiveName)
    , m_allowStar(false)
    , m_allowInline(false)
    , m_allowEval(false)
    , m_hashAlgorithmsUsed(0)
{
}

bool CSPSourceList::matches(const KURL& url)
{
    if (m_allowStar)
        return true;

    KURL effectiveURL = SecurityOrigin::shouldUseInnerURL(url) ? SecurityOrigin::extractInnerURL(url) : url;

    for (size_t i = 0; i < m_list.size(); ++i) {
        if (m_list[i].matches(effectiveURL))
            return true;
    }

    return false;
}

// source-list       = *WSP [ source *( 1*WSP source ) *WSP ]
//                   / *WSP "'none'" *WSP
//
void CSPSourceList::parse(const UChar* begin, const UChar* end)
{
    // We represent 'none' as an empty m_list.
    if (isSourceListNone(begin, end))
        return;

    const UChar* position = begin;
    while (position < end) {
        skipWhile<UChar, isASCIISpace>(position, end);
        if (position == end)
            return;

        const UChar* beginSource = position;
        skipWhile<UChar, isSourceCharacter>(position, end);

        String scheme, host, path;
        int port = 0;
        bool hostHasWildcard = false;
        bool portHasWildcard = false;

        if (parseSource(beginSource, position, scheme, host, port, path, hostHasWildcard, portHasWildcard)) {
            // Wildcard hosts and keyword sources ('self', 'unsafe-inline',
            // etc.) aren't stored in m_list, but as attributes on the source
            // list itself.
            if (scheme.isEmpty() && host.isEmpty())
                continue;
            if (isDirectiveName(host))
                m_policy->reportDirectiveAsSourceExpression(m_directiveName, host);
            m_list.append(CSPSource(m_policy, scheme, host, port, path, hostHasWildcard, portHasWildcard));
        } else {
            m_policy->reportInvalidSourceExpression(m_directiveName, String(beginSource, position - beginSource));
        }

        ASSERT(position == end || isASCIISpace(*position));
    }
}

// source            = scheme ":"
//                   / ( [ scheme "://" ] host [ port ] [ path ] )
//                   / "'self'"
bool CSPSourceList::parseSource(const UChar* begin, const UChar* end, String& scheme, String& host, int& port, String& path, bool& hostHasWildcard, bool& portHasWildcard)
{
    if (begin == end)
        return false;

    if (equalIgnoringCase("'none'", begin, end - begin))
        return false;

    if (end - begin == 1 && *begin == '*') {
        addSourceStar();
        return true;
    }

    if (equalIgnoringCase("'self'", begin, end - begin)) {
        addSourceSelf();
        return true;
    }

    if (equalIgnoringCase("'unsafe-inline'", begin, end - begin)) {
        addSourceUnsafeInline();
        return true;
    }

    if (equalIgnoringCase("'unsafe-eval'", begin, end - begin)) {
        addSourceUnsafeEval();
        return true;
    }

    if (m_policy->experimentalFeaturesEnabled()) {
        String nonce;
        if (!parseNonce(begin, end, nonce))
            return false;

        if (!nonce.isNull()) {
            addSourceNonce(nonce);
            return true;
        }

        Vector<uint8_t> hash;
        ContentSecurityPolicy::HashAlgorithms algorithm = ContentSecurityPolicy::HashAlgorithmsNone;
        if (!parseHash(begin, end, hash, algorithm))
            return false;

        if (hash.size() > 0) {
            addSourceHash(algorithm, hash);
            return true;
        }
    }

    const UChar* position = begin;
    const UChar* beginHost = begin;
    const UChar* beginPath = end;
    const UChar* beginPort = 0;

    skipWhile<UChar, isNotColonOrSlash>(position, end);

    if (position == end) {
        // host
        //     ^
        return parseHost(beginHost, position, host, hostHasWildcard);
    }

    if (position < end && *position == '/') {
        // host/path || host/ || /
        //     ^            ^    ^
        return parseHost(beginHost, position, host, hostHasWildcard) && parsePath(position, end, path);
    }

    if (position < end && *position == ':') {
        if (end - position == 1) {
            // scheme:
            //       ^
            return parseScheme(begin, position, scheme);
        }

        if (position[1] == '/') {
            // scheme://host || scheme://
            //       ^                ^
            if (!parseScheme(begin, position, scheme)
                || !skipExactly<UChar>(position, end, ':')
                || !skipExactly<UChar>(position, end, '/')
                || !skipExactly<UChar>(position, end, '/'))
                return false;
            if (position == end)
                return true;
            beginHost = position;
            skipWhile<UChar, isNotColonOrSlash>(position, end);
        }

        if (position < end && *position == ':') {
            // host:port || scheme://host:port
            //     ^                     ^
            beginPort = position;
            skipUntil<UChar>(position, end, '/');
        }
    }

    if (position < end && *position == '/') {
        // scheme://host/path || scheme://host:port/path
        //              ^                          ^
        if (position == beginHost)
            return false;
        beginPath = position;
    }

    if (!parseHost(beginHost, beginPort ? beginPort : beginPath, host, hostHasWildcard))
        return false;

    if (beginPort) {
        if (!parsePort(beginPort, beginPath, port, portHasWildcard))
            return false;
    } else {
        port = 0;
    }

    if (beginPath != end) {
        if (!parsePath(beginPath, end, path))
            return false;
    }

    return true;
}

// nonce-source      = "'nonce-" nonce-value "'"
// nonce-value        = 1*( ALPHA / DIGIT / "+" / "/" / "=" )
//
bool CSPSourceList::parseNonce(const UChar* begin, const UChar* end, String& nonce)
{
    DEFINE_STATIC_LOCAL(const String, noncePrefix, ("'nonce-"));

    if (!equalIgnoringCase(noncePrefix.characters8(), begin, noncePrefix.length()))
        return true;

    const UChar* position = begin + noncePrefix.length();
    const UChar* nonceBegin = position;

    skipWhile<UChar, isNonceCharacter>(position, end);
    ASSERT(nonceBegin <= position);

    if ((position + 1) != end  || *position != '\'' || !(position - nonceBegin))
        return false;

    nonce = String(nonceBegin, position - nonceBegin);
    return true;
}

// hash-source       = "'" hash-algorithm "-" hash-value "'"
// hash-algorithm    = "sha1" / "sha256"
// hash-value        = 1*( ALPHA / DIGIT / "+" / "/" / "=" )
//
bool CSPSourceList::parseHash(const UChar* begin, const UChar* end, Vector<uint8_t>& hash, ContentSecurityPolicy::HashAlgorithms& hashAlgorithm)
{
    DEFINE_STATIC_LOCAL(const String, sha1Prefix, ("'sha1-"));
    DEFINE_STATIC_LOCAL(const String, sha256Prefix, ("'sha256-"));

    String prefix;
    if (equalIgnoringCase(sha1Prefix.characters8(), begin, sha1Prefix.length())) {
        prefix = sha1Prefix;
        hashAlgorithm = ContentSecurityPolicy::HashAlgorithmsSha1;
    } else if (equalIgnoringCase(sha256Prefix.characters8(), begin, sha256Prefix.length())) {
        notImplemented();
    } else {
        return true;
    }

    const UChar* position = begin + prefix.length();
    const UChar* hashBegin = position;

    skipWhile<UChar, isBase64EncodedCharacter>(position, end);
    ASSERT(hashBegin <= position);

    // Base64 encodings may end with exactly one or two '=' characters
    skipExactly<UChar>(position, position + 1, '=');
    skipExactly<UChar>(position, position + 1, '=');

    if ((position + 1) != end  || *position != '\'' || !(position - hashBegin))
        return false;

    Vector<char> hashVector;
    base64Decode(hashBegin, position - hashBegin, hashVector);
    hash.append(reinterpret_cast<uint8_t*>(hashVector.data()), hashVector.size());
    return true;
}

//                     ; <scheme> production from RFC 3986
// scheme      = ALPHA *( ALPHA / DIGIT / "+" / "-" / "." )
//
bool CSPSourceList::parseScheme(const UChar* begin, const UChar* end, String& scheme)
{
    ASSERT(begin <= end);
    ASSERT(scheme.isEmpty());

    if (begin == end)
        return false;

    const UChar* position = begin;

    if (!skipExactly<UChar, isASCIIAlpha>(position, end))
        return false;

    skipWhile<UChar, isSchemeContinuationCharacter>(position, end);

    if (position != end)
        return false;

    scheme = String(begin, end - begin);
    return true;
}

// host              = [ "*." ] 1*host-char *( "." 1*host-char )
//                   / "*"
// host-char         = ALPHA / DIGIT / "-"
//
bool CSPSourceList::parseHost(const UChar* begin, const UChar* end, String& host, bool& hostHasWildcard)
{
    ASSERT(begin <= end);
    ASSERT(host.isEmpty());
    ASSERT(!hostHasWildcard);

    if (begin == end)
        return false;

    const UChar* position = begin;

    if (skipExactly<UChar>(position, end, '*')) {
        hostHasWildcard = true;

        if (position == end)
            return true;

        if (!skipExactly<UChar>(position, end, '.'))
            return false;
    }

    const UChar* hostBegin = position;

    while (position < end) {
        if (!skipExactly<UChar, isHostCharacter>(position, end))
            return false;

        skipWhile<UChar, isHostCharacter>(position, end);

        if (position < end && !skipExactly<UChar>(position, end, '.'))
            return false;
    }

    ASSERT(position == end);
    host = String(hostBegin, end - hostBegin);
    return true;
}

bool CSPSourceList::parsePath(const UChar* begin, const UChar* end, String& path)
{
    ASSERT(begin <= end);
    ASSERT(path.isEmpty());

    const UChar* position = begin;
    skipWhile<UChar, isPathComponentCharacter>(position, end);
    // path/to/file.js?query=string || path/to/file.js#anchor
    //                ^                               ^
    if (position < end)
        m_policy->reportInvalidPathCharacter(m_directiveName, String(begin, end - begin), *position);

    path = decodeURLEscapeSequences(String(begin, position - begin));

    ASSERT(position <= end);
    ASSERT(position == end || (*position == '#' || *position == '?'));
    return true;
}

// port              = ":" ( 1*DIGIT / "*" )
//
bool CSPSourceList::parsePort(const UChar* begin, const UChar* end, int& port, bool& portHasWildcard)
{
    ASSERT(begin <= end);
    ASSERT(!port);
    ASSERT(!portHasWildcard);

    if (!skipExactly<UChar>(begin, end, ':'))
        ASSERT_NOT_REACHED();

    if (begin == end)
        return false;

    if (end - begin == 1 && *begin == '*') {
        port = 0;
        portHasWildcard = true;
        return true;
    }

    const UChar* position = begin;
    skipWhile<UChar, isASCIIDigit>(position, end);

    if (position != end)
        return false;

    bool ok;
    port = charactersToIntStrict(begin, end - begin, &ok);
    return ok;
}

void CSPSourceList::addSourceSelf()
{
    m_list.append(CSPSource(m_policy, m_policy->securityOrigin()->protocol(), m_policy->securityOrigin()->host(), m_policy->securityOrigin()->port(), String(), false, false));
}

void CSPSourceList::addSourceStar()
{
    m_allowStar = true;
}

void CSPSourceList::addSourceUnsafeInline()
{
    m_allowInline = true;
}

void CSPSourceList::addSourceUnsafeEval()
{
    m_allowEval = true;
}

void CSPSourceList::addSourceNonce(const String& nonce)
{
    m_nonces.add(nonce);
}

void CSPSourceList::addSourceHash(const ContentSecurityPolicy::HashAlgorithms& algorithm, const Vector<uint8_t>& hash)
{
    m_hashes.add(CSPHashValue(algorithm, hash));
    m_hashAlgorithmsUsed |= algorithm;
}

class CSPDirective {
public:
    CSPDirective(const String& name, const String& value, ContentSecurityPolicy* policy)
        : m_name(name)
        , m_text(name + ' ' + value)
        , m_policy(policy)
    {
    }

    const String& text() const { return m_text; }

protected:
    const ContentSecurityPolicy* policy() const { return m_policy; }

private:
    String m_name;
    String m_text;
    ContentSecurityPolicy* m_policy;
};

class MediaListDirective : public CSPDirective {
public:
    MediaListDirective(const String& name, const String& value, ContentSecurityPolicy* policy)
        : CSPDirective(name, value, policy)
    {
        Vector<UChar> characters;
        value.appendTo(characters);
        parse(characters.data(), characters.data() + characters.size());
    }

    bool allows(const String& type)
    {
        return m_pluginTypes.contains(type);
    }

private:
    void parse(const UChar* begin, const UChar* end)
    {
        const UChar* position = begin;

        // 'plugin-types ____;' OR 'plugin-types;'
        if (position == end) {
            policy()->reportInvalidPluginTypes(String());
            return;
        }

        while (position < end) {
            // _____ OR _____mime1/mime1
            // ^        ^
            skipWhile<UChar, isASCIISpace>(position, end);
            if (position == end)
                return;

            // mime1/mime1 mime2/mime2
            // ^
            begin = position;
            if (!skipExactly<UChar, isMediaTypeCharacter>(position, end)) {
                skipWhile<UChar, isNotASCIISpace>(position, end);
                policy()->reportInvalidPluginTypes(String(begin, position - begin));
                continue;
            }
            skipWhile<UChar, isMediaTypeCharacter>(position, end);

            // mime1/mime1 mime2/mime2
            //      ^
            if (!skipExactly<UChar>(position, end, '/')) {
                skipWhile<UChar, isNotASCIISpace>(position, end);
                policy()->reportInvalidPluginTypes(String(begin, position - begin));
                continue;
            }

            // mime1/mime1 mime2/mime2
            //       ^
            if (!skipExactly<UChar, isMediaTypeCharacter>(position, end)) {
                skipWhile<UChar, isNotASCIISpace>(position, end);
                policy()->reportInvalidPluginTypes(String(begin, position - begin));
                continue;
            }
            skipWhile<UChar, isMediaTypeCharacter>(position, end);

            // mime1/mime1 mime2/mime2 OR mime1/mime1  OR mime1/mime1/error
            //            ^                          ^               ^
            if (position < end && isNotASCIISpace(*position)) {
                skipWhile<UChar, isNotASCIISpace>(position, end);
                policy()->reportInvalidPluginTypes(String(begin, position - begin));
                continue;
            }
            m_pluginTypes.add(String(begin, position - begin));

            ASSERT(position == end || isASCIISpace(*position));
        }
    }

    HashSet<String> m_pluginTypes;
};

class SourceListDirective : public CSPDirective {
public:
    SourceListDirective(const String& name, const String& value, ContentSecurityPolicy* policy)
        : CSPDirective(name, value, policy)
        , m_sourceList(policy, name)
    {
        Vector<UChar> characters;
        value.appendTo(characters);

        m_sourceList.parse(characters.data(), characters.data() + characters.size());
    }

    bool allows(const KURL& url)
    {
        return m_sourceList.matches(url.isEmpty() ? policy()->url() : url);
    }

    bool allowInline() const { return m_sourceList.allowInline(); }
    bool allowEval() const { return m_sourceList.allowEval(); }
    bool allowNonce(const String& nonce) const { return m_sourceList.allowNonce(nonce.stripWhiteSpace()); }
    bool allowHash(const CSPHashValue& hashValue) const { return m_sourceList.allowHash(hashValue); }
    bool isHashOrNoncePresent() const { return m_sourceList.isHashOrNoncePresent(); }

    uint8_t hashAlgorithmsUsed() const { return m_sourceList.hashAlgorithmsUsed(); }

private:
    CSPSourceList m_sourceList;
};

class CSPDirectiveList {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static PassOwnPtr<CSPDirectiveList> create(ContentSecurityPolicy*, const UChar* begin, const UChar* end, ContentSecurityPolicy::HeaderType, ContentSecurityPolicy::HeaderSource);

    void parse(const UChar* begin, const UChar* end);

    const String& header() const { return m_header; }
    ContentSecurityPolicy::HeaderType headerType() const { return m_headerType; }
    ContentSecurityPolicy::HeaderSource headerSource() const { return m_headerSource; }

    bool allowJavaScriptURLs(const String& contextURL, const WTF::OrdinalNumber& contextLine, ContentSecurityPolicy::ReportingStatus) const;
    bool allowInlineEventHandlers(const String& contextURL, const WTF::OrdinalNumber& contextLine, ContentSecurityPolicy::ReportingStatus) const;
    bool allowInlineScript(const String& contextURL, const WTF::OrdinalNumber& contextLine, ContentSecurityPolicy::ReportingStatus) const;
    bool allowInlineStyle(const String& contextURL, const WTF::OrdinalNumber& contextLine, ContentSecurityPolicy::ReportingStatus) const;
    bool allowEval(ScriptState*, ContentSecurityPolicy::ReportingStatus) const;
    bool allowPluginType(const String& type, const String& typeAttribute, const KURL&, ContentSecurityPolicy::ReportingStatus) const;

    bool allowScriptFromSource(const KURL&, ContentSecurityPolicy::ReportingStatus) const;
    bool allowObjectFromSource(const KURL&, ContentSecurityPolicy::ReportingStatus) const;
    bool allowChildFrameFromSource(const KURL&, ContentSecurityPolicy::ReportingStatus) const;
    bool allowImageFromSource(const KURL&, ContentSecurityPolicy::ReportingStatus) const;
    bool allowStyleFromSource(const KURL&, ContentSecurityPolicy::ReportingStatus) const;
    bool allowFontFromSource(const KURL&, ContentSecurityPolicy::ReportingStatus) const;
    bool allowMediaFromSource(const KURL&, ContentSecurityPolicy::ReportingStatus) const;
    bool allowConnectToSource(const KURL&, ContentSecurityPolicy::ReportingStatus) const;
    bool allowFormAction(const KURL&, ContentSecurityPolicy::ReportingStatus) const;
    bool allowBaseURI(const KURL&, ContentSecurityPolicy::ReportingStatus) const;
    bool allowAncestors(LocalFrame*, ContentSecurityPolicy::ReportingStatus) const;
    bool allowChildContextFromSource(const KURL&, ContentSecurityPolicy::ReportingStatus) const;
    bool allowScriptNonce(const String&) const;
    bool allowStyleNonce(const String&) const;
    bool allowScriptHash(const CSPHashValue&) const;
    bool allowStyleHash(const CSPHashValue&) const;

    const String& evalDisabledErrorMessage() const { return m_evalDisabledErrorMessage; }
    ReflectedXSSDisposition reflectedXSSDisposition() const { return m_reflectedXSSDisposition; }
    ReferrerPolicy referrerPolicy() const { return m_referrerPolicy; }
    bool didSetReferrerPolicy() const { return m_didSetReferrerPolicy; }
    bool isReportOnly() const { return m_reportOnly; }
    const Vector<KURL>& reportURIs() const { return m_reportURIs; }

private:
    CSPDirectiveList(ContentSecurityPolicy*, ContentSecurityPolicy::HeaderType, ContentSecurityPolicy::HeaderSource);

    bool parseDirective(const UChar* begin, const UChar* end, String& name, String& value);
    void parseReportURI(const String& name, const String& value);
    void parsePluginTypes(const String& name, const String& value);
    void parseReflectedXSS(const String& name, const String& value);
    void parseReferrer(const String& name, const String& value);
    void addDirective(const String& name, const String& value);
    void applySandboxPolicy(const String& name, const String& sandboxPolicy);

    template <class CSPDirectiveType>
    void setCSPDirective(const String& name, const String& value, OwnPtr<CSPDirectiveType>&);

    SourceListDirective* operativeDirective(SourceListDirective*) const;
    SourceListDirective* operativeDirective(SourceListDirective*, SourceListDirective* override) const;
    void reportViolation(const String& directiveText, const String& effectiveDirective, const String& consoleMessage, const KURL& blockedURL) const;
    void reportViolationWithLocation(const String& directiveText, const String& effectiveDirective, const String& consoleMessage, const KURL& blockedURL, const String& contextURL, const WTF::OrdinalNumber& contextLine) const;
    void reportViolationWithState(const String& directiveText, const String& effectiveDirective, const String& consoleMessage, const KURL& blockedURL, ScriptState*) const;

    bool checkEval(SourceListDirective*) const;
    bool checkInline(SourceListDirective*) const;
    bool checkNonce(SourceListDirective*, const String&) const;
    bool checkHash(SourceListDirective*, const CSPHashValue&) const;
    bool checkSource(SourceListDirective*, const KURL&) const;
    bool checkMediaType(MediaListDirective*, const String& type, const String& typeAttribute) const;
    bool checkAncestors(SourceListDirective*, LocalFrame*) const;

    void setEvalDisabledErrorMessage(const String& errorMessage) { m_evalDisabledErrorMessage = errorMessage; }

    bool checkEvalAndReportViolation(SourceListDirective*, const String& consoleMessage, ScriptState*) const;
    bool checkInlineAndReportViolation(SourceListDirective*, const String& consoleMessage, const String& contextURL, const WTF::OrdinalNumber& contextLine, bool isScript) const;

    bool checkSourceAndReportViolation(SourceListDirective*, const KURL&, const String& effectiveDirective) const;
    bool checkMediaTypeAndReportViolation(MediaListDirective*, const String& type, const String& typeAttribute, const String& consoleMessage) const;
    bool checkAncestorsAndReportViolation(SourceListDirective*, LocalFrame*) const;

    bool denyIfEnforcingPolicy() const { return m_reportOnly; }

    ContentSecurityPolicy* m_policy;

    String m_header;
    ContentSecurityPolicy::HeaderType m_headerType;
    ContentSecurityPolicy::HeaderSource m_headerSource;

    bool m_reportOnly;
    bool m_haveSandboxPolicy;
    ReflectedXSSDisposition m_reflectedXSSDisposition;

    bool m_didSetReferrerPolicy;
    ReferrerPolicy m_referrerPolicy;

    OwnPtr<MediaListDirective> m_pluginTypes;
    OwnPtr<SourceListDirective> m_baseURI;
    OwnPtr<SourceListDirective> m_childSrc;
    OwnPtr<SourceListDirective> m_connectSrc;
    OwnPtr<SourceListDirective> m_defaultSrc;
    OwnPtr<SourceListDirective> m_fontSrc;
    OwnPtr<SourceListDirective> m_formAction;
    OwnPtr<SourceListDirective> m_frameAncestors;
    OwnPtr<SourceListDirective> m_frameSrc;
    OwnPtr<SourceListDirective> m_imgSrc;
    OwnPtr<SourceListDirective> m_mediaSrc;
    OwnPtr<SourceListDirective> m_objectSrc;
    OwnPtr<SourceListDirective> m_scriptSrc;
    OwnPtr<SourceListDirective> m_styleSrc;

    Vector<KURL> m_reportURIs;

    String m_evalDisabledErrorMessage;
};

CSPDirectiveList::CSPDirectiveList(ContentSecurityPolicy* policy, ContentSecurityPolicy::HeaderType type, ContentSecurityPolicy::HeaderSource source)
    : m_policy(policy)
    , m_headerType(type)
    , m_headerSource(source)
    , m_reportOnly(false)
    , m_haveSandboxPolicy(false)
    , m_reflectedXSSDisposition(ReflectedXSSUnset)
    , m_didSetReferrerPolicy(false)
    , m_referrerPolicy(ReferrerPolicyDefault)
{
    m_reportOnly = type == ContentSecurityPolicy::Report;
}

PassOwnPtr<CSPDirectiveList> CSPDirectiveList::create(ContentSecurityPolicy* policy, const UChar* begin, const UChar* end, ContentSecurityPolicy::HeaderType type, ContentSecurityPolicy::HeaderSource source)
{
    OwnPtr<CSPDirectiveList> directives = adoptPtr(new CSPDirectiveList(policy, type, source));
    directives->parse(begin, end);

    if (!directives->checkEval(directives->operativeDirective(directives->m_scriptSrc.get()))) {
        String message = "Refused to evaluate a string as JavaScript because 'unsafe-eval' is not an allowed source of script in the following Content Security Policy directive: \"" + directives->operativeDirective(directives->m_scriptSrc.get())->text() + "\".\n";
        directives->setEvalDisabledErrorMessage(message);
    }

    if (directives->isReportOnly() && directives->reportURIs().isEmpty())
        policy->reportMissingReportURI(String(begin, end - begin));

    return directives.release();
}

void CSPDirectiveList::reportViolation(const String& directiveText, const String& effectiveDirective, const String& consoleMessage, const KURL& blockedURL) const
{
    String message = m_reportOnly ? "[Report Only] " + consoleMessage : consoleMessage;
    m_policy->client()->addConsoleMessage(SecurityMessageSource, ErrorMessageLevel, message);
    m_policy->reportViolation(directiveText, effectiveDirective, message, blockedURL, m_reportURIs, m_header);
}

void CSPDirectiveList::reportViolationWithLocation(const String& directiveText, const String& effectiveDirective, const String& consoleMessage, const KURL& blockedURL, const String& contextURL, const WTF::OrdinalNumber& contextLine) const
{
    String message = m_reportOnly ? "[Report Only] " + consoleMessage : consoleMessage;
    m_policy->client()->addConsoleMessage(SecurityMessageSource, ErrorMessageLevel, message, contextURL, contextLine.oneBasedInt());
    m_policy->reportViolation(directiveText, effectiveDirective, message, blockedURL, m_reportURIs, m_header);
}

void CSPDirectiveList::reportViolationWithState(const String& directiveText, const String& effectiveDirective, const String& consoleMessage, const KURL& blockedURL, ScriptState* state) const
{
    String message = m_reportOnly ? "[Report Only] " + consoleMessage : consoleMessage;
    m_policy->client()->addConsoleMessage(SecurityMessageSource, ErrorMessageLevel, message, state);
    m_policy->reportViolation(directiveText, effectiveDirective, message, blockedURL, m_reportURIs, m_header);
}

bool CSPDirectiveList::checkEval(SourceListDirective* directive) const
{
    return !directive || directive->allowEval();
}

bool CSPDirectiveList::checkInline(SourceListDirective* directive) const
{
    return !directive || (directive->allowInline() && !directive->isHashOrNoncePresent());
}

bool CSPDirectiveList::checkNonce(SourceListDirective* directive, const String& nonce) const
{
    return !directive || directive->allowNonce(nonce);
}

bool CSPDirectiveList::checkHash(SourceListDirective* directive, const CSPHashValue& hashValue) const
{
    return !directive || directive->allowHash(hashValue);
}

bool CSPDirectiveList::checkSource(SourceListDirective* directive, const KURL& url) const
{
    return !directive || directive->allows(url);
}

bool CSPDirectiveList::checkAncestors(SourceListDirective* directive, LocalFrame* frame) const
{
    if (!frame || !directive)
        return true;

    for (LocalFrame* current = frame->tree().parent(); current; current = current->tree().parent()) {
        if (!directive->allows(current->document()->url()))
            return false;
    }
    return true;
}

bool CSPDirectiveList::checkMediaType(MediaListDirective* directive, const String& type, const String& typeAttribute) const
{
    if (!directive)
        return true;
    if (typeAttribute.isEmpty() || typeAttribute.stripWhiteSpace() != type)
        return false;
    return directive->allows(type);
}

SourceListDirective* CSPDirectiveList::operativeDirective(SourceListDirective* directive) const
{
    return directive ? directive : m_defaultSrc.get();
}

SourceListDirective* CSPDirectiveList::operativeDirective(SourceListDirective* directive, SourceListDirective* override) const
{
    return directive ? directive : override;
}

bool CSPDirectiveList::checkEvalAndReportViolation(SourceListDirective* directive, const String& consoleMessage, ScriptState* state) const
{
    if (checkEval(directive))
        return true;

    String suffix = String();
    if (directive == m_defaultSrc)
        suffix = " Note that 'script-src' was not explicitly set, so 'default-src' is used as a fallback.";

    reportViolationWithState(directive->text(), scriptSrc, consoleMessage + "\"" + directive->text() + "\"." + suffix + "\n", KURL(), state);
    if (!m_reportOnly) {
        m_policy->reportBlockedScriptExecutionToInspector(directive->text());
        return false;
    }
    return true;
}

bool CSPDirectiveList::checkMediaTypeAndReportViolation(MediaListDirective* directive, const String& type, const String& typeAttribute, const String& consoleMessage) const
{
    if (checkMediaType(directive, type, typeAttribute))
        return true;

    String message = consoleMessage + "\'" + directive->text() + "\'.";
    if (typeAttribute.isEmpty())
        message = message + " When enforcing the 'plugin-types' directive, the plugin's media type must be explicitly declared with a 'type' attribute on the containing element (e.g. '<object type=\"[TYPE GOES HERE]\" ...>').";

    reportViolation(directive->text(), pluginTypes, message + "\n", KURL());
    return denyIfEnforcingPolicy();
}

bool CSPDirectiveList::checkInlineAndReportViolation(SourceListDirective* directive, const String& consoleMessage, const String& contextURL, const WTF::OrdinalNumber& contextLine, bool isScript) const
{
    if (checkInline(directive))
        return true;

    String suffix = String();
    if (directive->allowInline() && directive->isHashOrNoncePresent()) {
        // If inline is allowed, but a hash or nonce is present, we ignore 'unsafe-inline'. Throw a reasonable error.
        suffix = " Note that 'unsafe-inline' is ignored if either a hash or nonce value is present in the source list.";
    } else {
        suffix = " Either the 'unsafe-inline' keyword, a hash ('sha256-...'), or a nonce ('nonce-...') is required to enable inline execution.";
        if (directive == m_defaultSrc)
            suffix = suffix + " Note also that '" + String(isScript ? "script" : "style") + "-src' was not explicitly set, so 'default-src' is used as a fallback.";
    }

    reportViolationWithLocation(directive->text(), isScript ? scriptSrc : styleSrc, consoleMessage + "\"" + directive->text() + "\"." + suffix + "\n", KURL(), contextURL, contextLine);

    if (!m_reportOnly) {
        if (isScript)
            m_policy->reportBlockedScriptExecutionToInspector(directive->text());
        return false;
    }
    return true;
}

bool CSPDirectiveList::checkSourceAndReportViolation(SourceListDirective* directive, const KURL& url, const String& effectiveDirective) const
{
    if (checkSource(directive, url))
        return true;

    String prefix;
    if (baseURI == effectiveDirective)
        prefix = "Refused to set the document's base URI to '";
    else if (childSrc == effectiveDirective)
        prefix = "Refused to create a child context containing '";
    else if (connectSrc == effectiveDirective)
        prefix = "Refused to connect to '";
    else if (fontSrc == effectiveDirective)
        prefix = "Refused to load the font '";
    else if (formAction == effectiveDirective)
        prefix = "Refused to send form data to '";
    else if (frameSrc == effectiveDirective)
        prefix = "Refused to frame '";
    else if (imgSrc == effectiveDirective)
        prefix = "Refused to load the image '";
    else if (mediaSrc == effectiveDirective)
        prefix = "Refused to load media from '";
    else if (objectSrc == effectiveDirective)
        prefix = "Refused to load plugin data from '";
    else if (scriptSrc == effectiveDirective)
        prefix = "Refused to load the script '";
    else if (styleSrc == effectiveDirective)
        prefix = "Refused to load the stylesheet '";

    String suffix = String();
    if (directive == m_defaultSrc)
        suffix = " Note that '" + effectiveDirective + "' was not explicitly set, so 'default-src' is used as a fallback.";

    reportViolation(directive->text(), effectiveDirective, prefix + url.elidedString() + "' because it violates the following Content Security Policy directive: \"" + directive->text() + "\"." + suffix + "\n", url);
    return denyIfEnforcingPolicy();
}

bool CSPDirectiveList::checkAncestorsAndReportViolation(SourceListDirective* directive, LocalFrame* frame) const
{
    if (checkAncestors(directive, frame))
        return true;

    reportViolation(directive->text(), "frame-ancestors", "Refused to display '" + frame->document()->url().elidedString() + " in a frame because an ancestor violates the following Content Security Policy directive: \"" + directive->text() + "\".", frame->document()->url());
    return denyIfEnforcingPolicy();
}

bool CSPDirectiveList::allowJavaScriptURLs(const String& contextURL, const WTF::OrdinalNumber& contextLine, ContentSecurityPolicy::ReportingStatus reportingStatus) const
{
    DEFINE_STATIC_LOCAL(String, consoleMessage, ("Refused to execute JavaScript URL because it violates the following Content Security Policy directive: "));
    if (reportingStatus == ContentSecurityPolicy::SendReport)
        return checkInlineAndReportViolation(operativeDirective(m_scriptSrc.get()), consoleMessage, contextURL, contextLine, true);

    return checkInline(operativeDirective(m_scriptSrc.get()));
}

bool CSPDirectiveList::allowInlineEventHandlers(const String& contextURL, const WTF::OrdinalNumber& contextLine, ContentSecurityPolicy::ReportingStatus reportingStatus) const
{
    DEFINE_STATIC_LOCAL(String, consoleMessage, ("Refused to execute inline event handler because it violates the following Content Security Policy directive: "));
    if (reportingStatus == ContentSecurityPolicy::SendReport)
        return checkInlineAndReportViolation(operativeDirective(m_scriptSrc.get()), consoleMessage, contextURL, contextLine, true);
    return checkInline(operativeDirective(m_scriptSrc.get()));
}

bool CSPDirectiveList::allowInlineScript(const String& contextURL, const WTF::OrdinalNumber& contextLine, ContentSecurityPolicy::ReportingStatus reportingStatus) const
{
    DEFINE_STATIC_LOCAL(String, consoleMessage, ("Refused to execute inline script because it violates the following Content Security Policy directive: "));
    return reportingStatus == ContentSecurityPolicy::SendReport ?
        checkInlineAndReportViolation(operativeDirective(m_scriptSrc.get()), consoleMessage, contextURL, contextLine, true) :
        checkInline(operativeDirective(m_scriptSrc.get()));
}

bool CSPDirectiveList::allowInlineStyle(const String& contextURL, const WTF::OrdinalNumber& contextLine, ContentSecurityPolicy::ReportingStatus reportingStatus) const
{
    DEFINE_STATIC_LOCAL(String, consoleMessage, ("Refused to apply inline style because it violates the following Content Security Policy directive: "));
    return reportingStatus == ContentSecurityPolicy::SendReport ?
        checkInlineAndReportViolation(operativeDirective(m_styleSrc.get()), consoleMessage, contextURL, contextLine, false) :
        checkInline(operativeDirective(m_styleSrc.get()));
}

bool CSPDirectiveList::allowEval(ScriptState* state, ContentSecurityPolicy::ReportingStatus reportingStatus) const
{
    DEFINE_STATIC_LOCAL(String, consoleMessage, ("Refused to evaluate a string as JavaScript because 'unsafe-eval' is not an allowed source of script in the following Content Security Policy directive: "));

    return reportingStatus == ContentSecurityPolicy::SendReport ?
        checkEvalAndReportViolation(operativeDirective(m_scriptSrc.get()), consoleMessage, state) :
        checkEval(operativeDirective(m_scriptSrc.get()));
}

bool CSPDirectiveList::allowPluginType(const String& type, const String& typeAttribute, const KURL& url, ContentSecurityPolicy::ReportingStatus reportingStatus) const
{
    return reportingStatus == ContentSecurityPolicy::SendReport ?
        checkMediaTypeAndReportViolation(m_pluginTypes.get(), type, typeAttribute, "Refused to load '" + url.elidedString() + "' (MIME type '" + typeAttribute + "') because it violates the following Content Security Policy Directive: ") :
        checkMediaType(m_pluginTypes.get(), type, typeAttribute);
}

bool CSPDirectiveList::allowScriptFromSource(const KURL& url, ContentSecurityPolicy::ReportingStatus reportingStatus) const
{
    return reportingStatus == ContentSecurityPolicy::SendReport ?
        checkSourceAndReportViolation(operativeDirective(m_scriptSrc.get()), url, scriptSrc) :
        checkSource(operativeDirective(m_scriptSrc.get()), url);
}

bool CSPDirectiveList::allowObjectFromSource(const KURL& url, ContentSecurityPolicy::ReportingStatus reportingStatus) const
{
    if (url.isBlankURL())
        return true;
    return reportingStatus == ContentSecurityPolicy::SendReport ?
        checkSourceAndReportViolation(operativeDirective(m_objectSrc.get()), url, objectSrc) :
        checkSource(operativeDirective(m_objectSrc.get()), url);
}

bool CSPDirectiveList::allowChildFrameFromSource(const KURL& url, ContentSecurityPolicy::ReportingStatus reportingStatus) const
{
    if (url.isBlankURL())
        return true;

    // 'frame-src' is the only directive which overrides something other than the default sources.
    // It overrides 'child-src', which overrides the default sources. So, we do this nested set
    // of calls to 'operativeDirective()' to grab 'frame-src' if it exists, 'child-src' if it
    // doesn't, and 'defaut-src' if neither are available.
    //
    // All of this only applies, of course, if we're in CSP 1.1. In CSP 1.0, 'frame-src'
    // overrides 'default-src' directly.
    SourceListDirective* whichDirective = m_policy->experimentalFeaturesEnabled() ?
        operativeDirective(m_frameSrc.get(), operativeDirective(m_childSrc.get())) :
        operativeDirective(m_frameSrc.get());

    return reportingStatus == ContentSecurityPolicy::SendReport ?
        checkSourceAndReportViolation(whichDirective, url, frameSrc) :
        checkSource(whichDirective, url);
}

bool CSPDirectiveList::allowImageFromSource(const KURL& url, ContentSecurityPolicy::ReportingStatus reportingStatus) const
{
    return reportingStatus == ContentSecurityPolicy::SendReport ?
        checkSourceAndReportViolation(operativeDirective(m_imgSrc.get()), url, imgSrc) :
        checkSource(operativeDirective(m_imgSrc.get()), url);
}

bool CSPDirectiveList::allowStyleFromSource(const KURL& url, ContentSecurityPolicy::ReportingStatus reportingStatus) const
{
    return reportingStatus == ContentSecurityPolicy::SendReport ?
        checkSourceAndReportViolation(operativeDirective(m_styleSrc.get()), url, styleSrc) :
        checkSource(operativeDirective(m_styleSrc.get()), url);
}

bool CSPDirectiveList::allowFontFromSource(const KURL& url, ContentSecurityPolicy::ReportingStatus reportingStatus) const
{
    return reportingStatus == ContentSecurityPolicy::SendReport ?
        checkSourceAndReportViolation(operativeDirective(m_fontSrc.get()), url, fontSrc) :
        checkSource(operativeDirective(m_fontSrc.get()), url);
}

bool CSPDirectiveList::allowMediaFromSource(const KURL& url, ContentSecurityPolicy::ReportingStatus reportingStatus) const
{
    return reportingStatus == ContentSecurityPolicy::SendReport ?
        checkSourceAndReportViolation(operativeDirective(m_mediaSrc.get()), url, mediaSrc) :
        checkSource(operativeDirective(m_mediaSrc.get()), url);
}

bool CSPDirectiveList::allowConnectToSource(const KURL& url, ContentSecurityPolicy::ReportingStatus reportingStatus) const
{
    return reportingStatus == ContentSecurityPolicy::SendReport ?
        checkSourceAndReportViolation(operativeDirective(m_connectSrc.get()), url, connectSrc) :
        checkSource(operativeDirective(m_connectSrc.get()), url);
}

bool CSPDirectiveList::allowFormAction(const KURL& url, ContentSecurityPolicy::ReportingStatus reportingStatus) const
{
    return reportingStatus == ContentSecurityPolicy::SendReport ?
        checkSourceAndReportViolation(m_formAction.get(), url, formAction) :
        checkSource(m_formAction.get(), url);
}

bool CSPDirectiveList::allowBaseURI(const KURL& url, ContentSecurityPolicy::ReportingStatus reportingStatus) const
{
    return reportingStatus == ContentSecurityPolicy::SendReport ?
        checkSourceAndReportViolation(m_baseURI.get(), url, baseURI) :
        checkSource(m_baseURI.get(), url);
}

bool CSPDirectiveList::allowAncestors(LocalFrame* frame, ContentSecurityPolicy::ReportingStatus reportingStatus) const
{
    return reportingStatus == ContentSecurityPolicy::SendReport ?
        checkAncestorsAndReportViolation(m_frameAncestors.get(), frame) :
        checkAncestors(m_frameAncestors.get(), frame);
}

bool CSPDirectiveList::allowChildContextFromSource(const KURL& url, ContentSecurityPolicy::ReportingStatus reportingStatus) const
{
    return reportingStatus == ContentSecurityPolicy::SendReport ?
        checkSourceAndReportViolation(operativeDirective(m_childSrc.get()), url, childSrc) :
        checkSource(operativeDirective(m_childSrc.get()), url);
}

bool CSPDirectiveList::allowScriptNonce(const String& nonce) const
{
    return checkNonce(operativeDirective(m_scriptSrc.get()), nonce);
}

bool CSPDirectiveList::allowStyleNonce(const String& nonce) const
{
    return checkNonce(operativeDirective(m_styleSrc.get()), nonce);
}

bool CSPDirectiveList::allowScriptHash(const CSPHashValue& hashValue) const
{
    return checkHash(operativeDirective(m_scriptSrc.get()), hashValue);
}

bool CSPDirectiveList::allowStyleHash(const CSPHashValue& hashValue) const
{
    return checkHash(operativeDirective(m_styleSrc.get()), hashValue);
}

// policy            = directive-list
// directive-list    = [ directive *( ";" [ directive ] ) ]
//
void CSPDirectiveList::parse(const UChar* begin, const UChar* end)
{
    m_header = String(begin, end - begin);

    if (begin == end)
        return;

    const UChar* position = begin;
    while (position < end) {
        const UChar* directiveBegin = position;
        skipUntil<UChar>(position, end, ';');

        String name, value;
        if (parseDirective(directiveBegin, position, name, value)) {
            ASSERT(!name.isEmpty());
            addDirective(name, value);
        }

        ASSERT(position == end || *position == ';');
        skipExactly<UChar>(position, end, ';');
    }
}

// directive         = *WSP [ directive-name [ WSP directive-value ] ]
// directive-name    = 1*( ALPHA / DIGIT / "-" )
// directive-value   = *( WSP / <VCHAR except ";"> )
//
bool CSPDirectiveList::parseDirective(const UChar* begin, const UChar* end, String& name, String& value)
{
    ASSERT(name.isEmpty());
    ASSERT(value.isEmpty());

    const UChar* position = begin;
    skipWhile<UChar, isASCIISpace>(position, end);

    // Empty directive (e.g. ";;;"). Exit early.
    if (position == end)
        return false;

    const UChar* nameBegin = position;
    skipWhile<UChar, isCSPDirectiveNameCharacter>(position, end);

    // The directive-name must be non-empty.
    if (nameBegin == position) {
        skipWhile<UChar, isNotASCIISpace>(position, end);
        m_policy->reportUnsupportedDirective(String(nameBegin, position - nameBegin));
        return false;
    }

    name = String(nameBegin, position - nameBegin);

    if (position == end)
        return true;

    if (!skipExactly<UChar, isASCIISpace>(position, end)) {
        skipWhile<UChar, isNotASCIISpace>(position, end);
        m_policy->reportUnsupportedDirective(String(nameBegin, position - nameBegin));
        return false;
    }

    skipWhile<UChar, isASCIISpace>(position, end);

    const UChar* valueBegin = position;
    skipWhile<UChar, isCSPDirectiveValueCharacter>(position, end);

    if (position != end) {
        m_policy->reportInvalidDirectiveValueCharacter(name, String(valueBegin, end - valueBegin));
        return false;
    }

    // The directive-value may be empty.
    if (valueBegin == position)
        return true;

    value = String(valueBegin, position - valueBegin);
    return true;
}

void CSPDirectiveList::parseReportURI(const String& name, const String& value)
{
    if (!m_reportURIs.isEmpty()) {
        m_policy->reportDuplicateDirective(name);
        return;
    }

    Vector<UChar> characters;
    value.appendTo(characters);

    const UChar* position = characters.data();
    const UChar* end = position + characters.size();

    while (position < end) {
        skipWhile<UChar, isASCIISpace>(position, end);

        const UChar* urlBegin = position;
        skipWhile<UChar, isNotASCIISpace>(position, end);

        if (urlBegin < position) {
            String url = String(urlBegin, position - urlBegin);
            m_reportURIs.append(m_policy->completeURL(url));
        }
    }
}


template<class CSPDirectiveType>
void CSPDirectiveList::setCSPDirective(const String& name, const String& value, OwnPtr<CSPDirectiveType>& directive)
{
    if (directive) {
        m_policy->reportDuplicateDirective(name);
        return;
    }
    directive = adoptPtr(new CSPDirectiveType(name, value, m_policy));
}

void CSPDirectiveList::applySandboxPolicy(const String& name, const String& sandboxPolicy)
{
    if (m_reportOnly) {
        m_policy->reportInvalidInReportOnly(name);
        return;
    }
    if (m_haveSandboxPolicy) {
        m_policy->reportDuplicateDirective(name);
        return;
    }
    m_haveSandboxPolicy = true;
    String invalidTokens;
    m_policy->enforceSandboxFlags(parseSandboxPolicy(sandboxPolicy, invalidTokens));
    if (!invalidTokens.isNull())
        m_policy->reportInvalidSandboxFlags(invalidTokens);
}

void CSPDirectiveList::parseReflectedXSS(const String& name, const String& value)
{
    if (m_reflectedXSSDisposition != ReflectedXSSUnset) {
        m_policy->reportDuplicateDirective(name);
        m_reflectedXSSDisposition = ReflectedXSSInvalid;
        return;
    }

    if (value.isEmpty()) {
        m_reflectedXSSDisposition = ReflectedXSSInvalid;
        m_policy->reportInvalidReflectedXSS(value);
        return;
    }

    Vector<UChar> characters;
    value.appendTo(characters);

    const UChar* position = characters.data();
    const UChar* end = position + characters.size();

    skipWhile<UChar, isASCIISpace>(position, end);
    const UChar* begin = position;
    skipWhile<UChar, isNotASCIISpace>(position, end);

    // value1
    //       ^
    if (equalIgnoringCase("allow", begin, position - begin)) {
        m_reflectedXSSDisposition = AllowReflectedXSS;
    } else if (equalIgnoringCase("filter", begin, position - begin)) {
        m_reflectedXSSDisposition = FilterReflectedXSS;
    } else if (equalIgnoringCase("block", begin, position - begin)) {
        m_reflectedXSSDisposition = BlockReflectedXSS;
    } else {
        m_reflectedXSSDisposition = ReflectedXSSInvalid;
        m_policy->reportInvalidReflectedXSS(value);
        return;
    }

    skipWhile<UChar, isASCIISpace>(position, end);
    if (position == end && m_reflectedXSSDisposition != ReflectedXSSUnset)
        return;

    // value1 value2
    //        ^
    m_reflectedXSSDisposition = ReflectedXSSInvalid;
    m_policy->reportInvalidReflectedXSS(value);
}

void CSPDirectiveList::parseReferrer(const String& name, const String& value)
{
    if (m_didSetReferrerPolicy) {
        m_policy->reportDuplicateDirective(name);
        m_referrerPolicy = ReferrerPolicyNever;
        return;
    }

    m_didSetReferrerPolicy = true;

    if (value.isEmpty()) {
        m_policy->reportInvalidReferrer(value);
        m_referrerPolicy = ReferrerPolicyNever;
        return;
    }

    Vector<UChar> characters;
    value.appendTo(characters);

    const UChar* position = characters.data();
    const UChar* end = position + characters.size();

    skipWhile<UChar, isASCIISpace>(position, end);
    const UChar* begin = position;
    skipWhile<UChar, isNotASCIISpace>(position, end);

    // value1
    //       ^
    if (equalIgnoringCase("always", begin, position - begin)) {
        m_referrerPolicy = ReferrerPolicyAlways;
    } else if (equalIgnoringCase("default", begin, position - begin)) {
        m_referrerPolicy = ReferrerPolicyDefault;
    } else if (equalIgnoringCase("never", begin, position - begin)) {
        m_referrerPolicy = ReferrerPolicyNever;
    } else if (equalIgnoringCase("origin", begin, position - begin)) {
        m_referrerPolicy = ReferrerPolicyOrigin;
    } else {
        m_referrerPolicy = ReferrerPolicyNever;
        m_policy->reportInvalidReferrer(value);
        return;
    }

    skipWhile<UChar, isASCIISpace>(position, end);
    if (position == end)
        return;

    // value1 value2
    //        ^
    m_referrerPolicy = ReferrerPolicyNever;
    m_policy->reportInvalidReferrer(value);

}

void CSPDirectiveList::addDirective(const String& name, const String& value)
{
    ASSERT(!name.isEmpty());

    if (equalIgnoringCase(name, defaultSrc)) {
        setCSPDirective<SourceListDirective>(name, value, m_defaultSrc);
    } else if (equalIgnoringCase(name, scriptSrc)) {
        setCSPDirective<SourceListDirective>(name, value, m_scriptSrc);
        m_policy->usesScriptHashAlgorithms(m_scriptSrc->hashAlgorithmsUsed());
    } else if (equalIgnoringCase(name, objectSrc)) {
        setCSPDirective<SourceListDirective>(name, value, m_objectSrc);
    } else if (equalIgnoringCase(name, frameSrc)) {
        setCSPDirective<SourceListDirective>(name, value, m_frameSrc);
    } else if (equalIgnoringCase(name, imgSrc)) {
        setCSPDirective<SourceListDirective>(name, value, m_imgSrc);
    } else if (equalIgnoringCase(name, styleSrc)) {
        setCSPDirective<SourceListDirective>(name, value, m_styleSrc);
        m_policy->usesStyleHashAlgorithms(m_styleSrc->hashAlgorithmsUsed());
    } else if (equalIgnoringCase(name, fontSrc)) {
        setCSPDirective<SourceListDirective>(name, value, m_fontSrc);
    } else if (equalIgnoringCase(name, mediaSrc)) {
        setCSPDirective<SourceListDirective>(name, value, m_mediaSrc);
    } else if (equalIgnoringCase(name, connectSrc)) {
        setCSPDirective<SourceListDirective>(name, value, m_connectSrc);
    } else if (equalIgnoringCase(name, sandbox)) {
        applySandboxPolicy(name, value);
    } else if (equalIgnoringCase(name, reportURI)) {
        parseReportURI(name, value);
    } else if (m_policy->experimentalFeaturesEnabled()) {
        if (equalIgnoringCase(name, baseURI))
            setCSPDirective<SourceListDirective>(name, value, m_baseURI);
        else if (equalIgnoringCase(name, childSrc))
            setCSPDirective<SourceListDirective>(name, value, m_childSrc);
        else if (equalIgnoringCase(name, formAction))
            setCSPDirective<SourceListDirective>(name, value, m_formAction);
        else if (equalIgnoringCase(name, frameAncestors))
            setCSPDirective<SourceListDirective>(name, value, m_frameAncestors);
        else if (equalIgnoringCase(name, pluginTypes))
            setCSPDirective<MediaListDirective>(name, value, m_pluginTypes);
        else if (equalIgnoringCase(name, reflectedXSS))
            parseReflectedXSS(name, value);
        else if (equalIgnoringCase(name, referrer))
            parseReferrer(name, value);
        else
            m_policy->reportUnsupportedDirective(name);
    } else {
        m_policy->reportUnsupportedDirective(name);
    }
}

ContentSecurityPolicy::ContentSecurityPolicy(ExecutionContextClient* client)
    : m_client(client)
    , m_overrideInlineStyleAllowed(false)
    , m_scriptHashAlgorithmsUsed(HashAlgorithmsNone)
    , m_styleHashAlgorithmsUsed(HashAlgorithmsNone)
{
}

ContentSecurityPolicy::~ContentSecurityPolicy()
{
}

void ContentSecurityPolicy::copyStateFrom(const ContentSecurityPolicy* other)
{
    ASSERT(m_policies.isEmpty());
    for (CSPDirectiveListVector::const_iterator iter = other->m_policies.begin(); iter != other->m_policies.end(); ++iter)
        addPolicyFromHeaderValue((*iter)->header(), (*iter)->headerType(), (*iter)->headerSource());
}

void ContentSecurityPolicy::didReceiveHeaders(const ContentSecurityPolicyResponseHeaders& headers)
{
    if (!headers.contentSecurityPolicy().isEmpty())
        didReceiveHeader(headers.contentSecurityPolicy(), ContentSecurityPolicy::Enforce, ContentSecurityPolicy::HeaderSourceHTTP);
    if (!headers.contentSecurityPolicyReportOnly().isEmpty())
        didReceiveHeader(headers.contentSecurityPolicyReportOnly(), ContentSecurityPolicy::Report, ContentSecurityPolicy::HeaderSourceHTTP);
}

void ContentSecurityPolicy::didReceiveHeader(const String& header, HeaderType type, HeaderSource source)
{
    addPolicyFromHeaderValue(header, type, source);
}

void ContentSecurityPolicy::addPolicyFromHeaderValue(const String& header, HeaderType type, HeaderSource source)
{
    Document* document = this->document();
    if (document) {
        UseCounter::count(*document, getUseCounterType(type));

        // CSP 1.1 defines report-only in a <meta> element as invalid. Measure for now, disable in experimental mode.
        if (source == ContentSecurityPolicy::HeaderSourceMeta && type == ContentSecurityPolicy::Report) {
            UseCounter::count(*document, UseCounter::ContentSecurityPolicyReportOnlyInMeta);
            if (experimentalFeaturesEnabled()) {
                reportReportOnlyInMeta(header);
                return;
            }
        }
    }


    Vector<UChar> characters;
    header.appendTo(characters);

    const UChar* begin = characters.data();
    const UChar* end = begin + characters.size();

    // RFC2616, section 4.2 specifies that headers appearing multiple times can
    // be combined with a comma. Walk the header string, and parse each comma
    // separated chunk as a separate header.
    const UChar* position = begin;
    while (position < end) {
        skipUntil<UChar>(position, end, ',');

        // header1,header2 OR header1
        //        ^                  ^
        OwnPtr<CSPDirectiveList> policy = CSPDirectiveList::create(this, begin, position, type, source);

        // We disable 'eval()' even in the case of report-only policies, and rely on the check in the V8Initializer::codeGenerationCheckCallbackInMainThread callback to determine whether the call should execute or not.
        if (!policy->allowEval(0, SuppressReport))
            m_client->disableEval(policy->evalDisabledErrorMessage());

        m_policies.append(policy.release());

        // Skip the comma, and begin the next header from the current position.
        ASSERT(position == end || *position == ',');
        skipExactly<UChar>(position, end, ',');
        begin = position;
    }

    if (document && type != Report && didSetReferrerPolicy())
        document->setReferrerPolicy(referrerPolicy());
}

void ContentSecurityPolicy::setOverrideAllowInlineStyle(bool value)
{
    m_overrideInlineStyleAllowed = value;
}

const String& ContentSecurityPolicy::deprecatedHeader() const
{
    return m_policies.isEmpty() ? emptyString() : m_policies[0]->header();
}

ContentSecurityPolicy::HeaderType ContentSecurityPolicy::deprecatedHeaderType() const
{
    return m_policies.isEmpty() ? Enforce : m_policies[0]->headerType();
}

template<bool (CSPDirectiveList::*allowed)(ContentSecurityPolicy::ReportingStatus) const>
bool isAllowedByAll(const CSPDirectiveListVector& policies, ContentSecurityPolicy::ReportingStatus reportingStatus)
{
    for (size_t i = 0; i < policies.size(); ++i) {
        if (!(policies[i].get()->*allowed)(reportingStatus))
            return false;
    }
    return true;
}

template<bool (CSPDirectiveList::*allowed)(ScriptState* state, ContentSecurityPolicy::ReportingStatus) const>
bool isAllowedByAllWithState(const CSPDirectiveListVector& policies, ScriptState* state, ContentSecurityPolicy::ReportingStatus reportingStatus)
{
    for (size_t i = 0; i < policies.size(); ++i) {
        if (!(policies[i].get()->*allowed)(state, reportingStatus))
            return false;
    }
    return true;
}

template<bool (CSPDirectiveList::*allowed)(const String&, const WTF::OrdinalNumber&, ContentSecurityPolicy::ReportingStatus) const>
bool isAllowedByAllWithContext(const CSPDirectiveListVector& policies, const String& contextURL, const WTF::OrdinalNumber& contextLine, ContentSecurityPolicy::ReportingStatus reportingStatus)
{
    for (size_t i = 0; i < policies.size(); ++i) {
        if (!(policies[i].get()->*allowed)(contextURL, contextLine, reportingStatus))
            return false;
    }
    return true;
}

template<bool (CSPDirectiveList::*allowed)(const String&) const>
bool isAllowedByAllWithNonce(const CSPDirectiveListVector& policies, const String& nonce)
{
    for (size_t i = 0; i < policies.size(); ++i) {
        if (!(policies[i].get()->*allowed)(nonce))
            return false;
    }
    return true;
}

template<bool (CSPDirectiveList::*allowed)(const CSPHashValue&) const>
bool isAllowedByAllWithHash(const CSPDirectiveListVector& policies, const CSPHashValue& hashValue)
{
    for (size_t i = 0; i < policies.size(); ++i) {
        if (!(policies[i].get()->*allowed)(hashValue))
            return false;
    }
    return true;
}

template<bool (CSPDirectiveList::*allowFromURL)(const KURL&, ContentSecurityPolicy::ReportingStatus) const>
bool isAllowedByAllWithURL(const CSPDirectiveListVector& policies, const KURL& url, ContentSecurityPolicy::ReportingStatus reportingStatus)
{
    if (SchemeRegistry::schemeShouldBypassContentSecurityPolicy(url.protocol()))
        return true;

    for (size_t i = 0; i < policies.size(); ++i) {
        if (!(policies[i].get()->*allowFromURL)(url, reportingStatus))
            return false;
    }
    return true;
}

template<bool (CSPDirectiveList::*allowed)(LocalFrame*, ContentSecurityPolicy::ReportingStatus) const>
bool isAllowedByAllWithFrame(const CSPDirectiveListVector& policies, LocalFrame* frame, ContentSecurityPolicy::ReportingStatus reportingStatus)
{
    for (size_t i = 0; i < policies.size(); ++i) {
        if (!(policies[i].get()->*allowed)(frame, reportingStatus))
            return false;
    }
    return true;
}

bool ContentSecurityPolicy::allowJavaScriptURLs(const String& contextURL, const WTF::OrdinalNumber& contextLine, ContentSecurityPolicy::ReportingStatus reportingStatus) const
{
    return isAllowedByAllWithContext<&CSPDirectiveList::allowJavaScriptURLs>(m_policies, contextURL, contextLine, reportingStatus);
}

bool ContentSecurityPolicy::allowInlineEventHandlers(const String& contextURL, const WTF::OrdinalNumber& contextLine, ContentSecurityPolicy::ReportingStatus reportingStatus) const
{
    return isAllowedByAllWithContext<&CSPDirectiveList::allowInlineEventHandlers>(m_policies, contextURL, contextLine, reportingStatus);
}

bool ContentSecurityPolicy::allowInlineScript(const String& contextURL, const WTF::OrdinalNumber& contextLine, ContentSecurityPolicy::ReportingStatus reportingStatus) const
{
    return isAllowedByAllWithContext<&CSPDirectiveList::allowInlineScript>(m_policies, contextURL, contextLine, reportingStatus);
}

bool ContentSecurityPolicy::allowInlineStyle(const String& contextURL, const WTF::OrdinalNumber& contextLine, ContentSecurityPolicy::ReportingStatus reportingStatus) const
{
    if (m_overrideInlineStyleAllowed)
        return true;
    return isAllowedByAllWithContext<&CSPDirectiveList::allowInlineStyle>(m_policies, contextURL, contextLine, reportingStatus);
}

bool ContentSecurityPolicy::allowEval(ScriptState* state, ContentSecurityPolicy::ReportingStatus reportingStatus) const
{
    return isAllowedByAllWithState<&CSPDirectiveList::allowEval>(m_policies, state, reportingStatus);
}

String ContentSecurityPolicy::evalDisabledErrorMessage() const
{
    for (size_t i = 0; i < m_policies.size(); ++i) {
        if (!m_policies[i]->allowEval(0, SuppressReport))
            return m_policies[i]->evalDisabledErrorMessage();
    }
    return String();
}

bool ContentSecurityPolicy::allowPluginType(const String& type, const String& typeAttribute, const KURL& url, ContentSecurityPolicy::ReportingStatus reportingStatus) const
{
    for (size_t i = 0; i < m_policies.size(); ++i) {
        if (!m_policies[i]->allowPluginType(type, typeAttribute, url, reportingStatus))
            return false;
    }
    return true;
}

bool ContentSecurityPolicy::allowScriptFromSource(const KURL& url, ContentSecurityPolicy::ReportingStatus reportingStatus) const
{
    return isAllowedByAllWithURL<&CSPDirectiveList::allowScriptFromSource>(m_policies, url, reportingStatus);
}

bool ContentSecurityPolicy::allowScriptNonce(const String& nonce) const
{
    return isAllowedByAllWithNonce<&CSPDirectiveList::allowScriptNonce>(m_policies, nonce);
}

bool ContentSecurityPolicy::allowStyleNonce(const String& nonce) const
{
    return isAllowedByAllWithNonce<&CSPDirectiveList::allowStyleNonce>(m_policies, nonce);
}

// TODO(jww) We don't currently have a WTF SHA256 implementation. Once we
// have that, we should implement a proper check for sha256 hash values in
// both allowScriptHash and allowStyleHash.
bool ContentSecurityPolicy::allowScriptHash(const String& source) const
{
    if (HashAlgorithmsSha1 & m_scriptHashAlgorithmsUsed) {
        Vector<uint8_t, 20> digest;
        SHA1 sourceSha1;
        sourceSha1.addBytes(UTF8Encoding().normalizeAndEncode(source, WTF::EntitiesForUnencodables));
        sourceSha1.computeHash(digest);

        if (isAllowedByAllWithHash<&CSPDirectiveList::allowScriptHash>(m_policies, CSPHashValue(HashAlgorithmsSha1, Vector<uint8_t>(digest))))
            return true;
    }

    return false;
}

bool ContentSecurityPolicy::allowStyleHash(const String& source) const
{
    if (HashAlgorithmsSha1 & m_styleHashAlgorithmsUsed) {
        Vector<uint8_t, 20> digest;
        SHA1 sourceSha1;
        sourceSha1.addBytes(UTF8Encoding().normalizeAndEncode(source, WTF::EntitiesForUnencodables));
        sourceSha1.computeHash(digest);

        if (isAllowedByAllWithHash<&CSPDirectiveList::allowStyleHash>(m_policies, CSPHashValue(HashAlgorithmsSha1, Vector<uint8_t>(digest))))
            return true;
    }

    return false;
}

void ContentSecurityPolicy::usesScriptHashAlgorithms(uint8_t algorithms)
{
    m_scriptHashAlgorithmsUsed |= algorithms;
}

void ContentSecurityPolicy::usesStyleHashAlgorithms(uint8_t algorithms)
{
    m_styleHashAlgorithmsUsed |= algorithms;
}

bool ContentSecurityPolicy::allowObjectFromSource(const KURL& url, ContentSecurityPolicy::ReportingStatus reportingStatus) const
{
    return isAllowedByAllWithURL<&CSPDirectiveList::allowObjectFromSource>(m_policies, url, reportingStatus);
}

bool ContentSecurityPolicy::allowChildFrameFromSource(const KURL& url, ContentSecurityPolicy::ReportingStatus reportingStatus) const
{
    return isAllowedByAllWithURL<&CSPDirectiveList::allowChildFrameFromSource>(m_policies, url, reportingStatus);
}

bool ContentSecurityPolicy::allowImageFromSource(const KURL& url, ContentSecurityPolicy::ReportingStatus reportingStatus) const
{
    return isAllowedByAllWithURL<&CSPDirectiveList::allowImageFromSource>(m_policies, url, reportingStatus);
}

bool ContentSecurityPolicy::allowStyleFromSource(const KURL& url, ContentSecurityPolicy::ReportingStatus reportingStatus) const
{
    return isAllowedByAllWithURL<&CSPDirectiveList::allowStyleFromSource>(m_policies, url, reportingStatus);
}

bool ContentSecurityPolicy::allowFontFromSource(const KURL& url, ContentSecurityPolicy::ReportingStatus reportingStatus) const
{
    return isAllowedByAllWithURL<&CSPDirectiveList::allowFontFromSource>(m_policies, url, reportingStatus);
}

bool ContentSecurityPolicy::allowMediaFromSource(const KURL& url, ContentSecurityPolicy::ReportingStatus reportingStatus) const
{
    return isAllowedByAllWithURL<&CSPDirectiveList::allowMediaFromSource>(m_policies, url, reportingStatus);
}

bool ContentSecurityPolicy::allowConnectToSource(const KURL& url, ContentSecurityPolicy::ReportingStatus reportingStatus) const
{
    return isAllowedByAllWithURL<&CSPDirectiveList::allowConnectToSource>(m_policies, url, reportingStatus);
}

bool ContentSecurityPolicy::allowFormAction(const KURL& url, ContentSecurityPolicy::ReportingStatus reportingStatus) const
{
    return isAllowedByAllWithURL<&CSPDirectiveList::allowFormAction>(m_policies, url, reportingStatus);
}

bool ContentSecurityPolicy::allowBaseURI(const KURL& url, ContentSecurityPolicy::ReportingStatus reportingStatus) const
{
    return isAllowedByAllWithURL<&CSPDirectiveList::allowBaseURI>(m_policies, url, reportingStatus);
}

bool ContentSecurityPolicy::allowAncestors(LocalFrame* frame, ContentSecurityPolicy::ReportingStatus reportingStatus) const
{
    return isAllowedByAllWithFrame<&CSPDirectiveList::allowAncestors>(m_policies, frame, reportingStatus);
}

bool ContentSecurityPolicy::allowChildContextFromSource(const KURL& url, ContentSecurityPolicy::ReportingStatus reportingStatus) const
{
    return isAllowedByAllWithURL<&CSPDirectiveList::allowChildContextFromSource>(m_policies, url, reportingStatus);
}

bool ContentSecurityPolicy::allowWorkerContextFromSource(const KURL& url, ContentSecurityPolicy::ReportingStatus reportingStatus) const
{
    // CSP 1.1 moves workers from 'script-src' to the new 'child-src'. Measure the impact of this backwards-incompatible change.
    if (m_client->isDocument()) {
        Document* document = static_cast<Document*>(m_client);
        UseCounter::count(*document, UseCounter::WorkerSubjectToCSP);
        if (isAllowedByAllWithURL<&CSPDirectiveList::allowChildContextFromSource>(m_policies, url, SuppressReport) && !isAllowedByAllWithURL<&CSPDirectiveList::allowScriptFromSource>(m_policies, url, SuppressReport))
            UseCounter::count(*document, UseCounter::WorkerAllowedByChildBlockedByScript);
    }

    return experimentalFeaturesEnabled() ?
        isAllowedByAllWithURL<&CSPDirectiveList::allowChildContextFromSource>(m_policies, url, reportingStatus) :
        isAllowedByAllWithURL<&CSPDirectiveList::allowScriptFromSource>(m_policies, url, reportingStatus);
}

bool ContentSecurityPolicy::isActive() const
{
    return !m_policies.isEmpty();
}

ReflectedXSSDisposition ContentSecurityPolicy::reflectedXSSDisposition() const
{
    ReflectedXSSDisposition disposition = ReflectedXSSUnset;
    for (size_t i = 0; i < m_policies.size(); ++i) {
        if (m_policies[i]->reflectedXSSDisposition() > disposition)
            disposition = std::max(disposition, m_policies[i]->reflectedXSSDisposition());
    }
    return disposition;
}

ReferrerPolicy ContentSecurityPolicy::referrerPolicy() const
{
    ReferrerPolicy policy = ReferrerPolicyDefault;
    bool first = true;
    for (size_t i = 0; i < m_policies.size(); ++i) {
        if (m_policies[i]->didSetReferrerPolicy()) {
            if (first)
                policy = m_policies[i]->referrerPolicy();
            else
                policy = mergeReferrerPolicies(policy, m_policies[i]->referrerPolicy());
        }
    }
    return policy;
}

bool ContentSecurityPolicy::didSetReferrerPolicy() const
{
    for (size_t i = 0; i < m_policies.size(); ++i) {
        if (m_policies[i]->didSetReferrerPolicy())
            return true;
    }
    return false;
}

SecurityOrigin* ContentSecurityPolicy::securityOrigin() const
{
    return m_client->securityContext().securityOrigin();
}

const KURL ContentSecurityPolicy::url() const
{
    return m_client->contextURL();
}

KURL ContentSecurityPolicy::completeURL(const String& url) const
{
    return m_client->contextCompleteURL(url);
}

void ContentSecurityPolicy::enforceSandboxFlags(SandboxFlags mask) const
{
    if (Document* document = this->document())
        document->enforceSandboxFlags(mask);
}

static String stripURLForUseInReport(Document* document, const KURL& url)
{
    if (!url.isValid())
        return String();
    if (!url.isHierarchical() || url.protocolIs("file"))
        return url.protocol();
    return document->securityOrigin()->canRequest(url) ? url.strippedForUseAsReferrer() : SecurityOrigin::create(url)->toString();
}

static void gatherSecurityPolicyViolationEventData(SecurityPolicyViolationEventInit& init, Document* document, const String& directiveText, const String& effectiveDirective, const KURL& blockedURL, const String& header)
{
    init.documentURI = document->url().string();
    init.referrer = document->referrer();
    init.blockedURI = stripURLForUseInReport(document, blockedURL);
    init.violatedDirective = directiveText;
    init.effectiveDirective = effectiveDirective;
    init.originalPolicy = header;
    init.sourceFile = String();
    init.lineNumber = 0;
    init.columnNumber = 0;
    init.statusCode = 0;

    if (!SecurityOrigin::isSecure(document->url()) && document->loader())
        init.statusCode = document->loader()->response().httpStatusCode();

    RefPtr<ScriptCallStack> stack = createScriptCallStack(1, false);
    if (!stack)
        return;

    const ScriptCallFrame& callFrame = stack->at(0);

    if (callFrame.lineNumber()) {
        KURL source = KURL(ParsedURLString, callFrame.sourceURL());
        init.sourceFile = stripURLForUseInReport(document, source);
        init.lineNumber = callFrame.lineNumber();
        init.columnNumber = callFrame.columnNumber();
    }
}

void ContentSecurityPolicy::reportViolation(const String& directiveText, const String& effectiveDirective, const String& consoleMessage, const KURL& blockedURL, const Vector<KURL>& reportURIs, const String& header)
{
    // FIXME: Support sending reports from worker.
    if (!m_client->isDocument())
        return;

    Document* document = this->document();
    LocalFrame* frame = document->frame();
    if (!frame)
        return;

    SecurityPolicyViolationEventInit violationData;
    gatherSecurityPolicyViolationEventData(violationData, document, directiveText, effectiveDirective, blockedURL, header);

    if (experimentalFeaturesEnabled())
        frame->domWindow()->enqueueDocumentEvent(SecurityPolicyViolationEvent::create(EventTypeNames::securitypolicyviolation, violationData));

    if (reportURIs.isEmpty())
        return;

    // We need to be careful here when deciding what information to send to the
    // report-uri. Currently, we send only the current document's URL and the
    // directive that was violated. The document's URL is safe to send because
    // it's the document itself that's requesting that it be sent. You could
    // make an argument that we shouldn't send HTTPS document URLs to HTTP
    // report-uris (for the same reasons that we supress the Referer in that
    // case), but the Referer is sent implicitly whereas this request is only
    // sent explicitly. As for which directive was violated, that's pretty
    // harmless information.

    RefPtr<JSONObject> cspReport = JSONObject::create();
    cspReport->setString("document-uri", violationData.documentURI);
    cspReport->setString("referrer", violationData.referrer);
    cspReport->setString("violated-directive", violationData.violatedDirective);
    if (experimentalFeaturesEnabled())
        cspReport->setString("effective-directive", violationData.effectiveDirective);
    cspReport->setString("original-policy", violationData.originalPolicy);
    cspReport->setString("blocked-uri", violationData.blockedURI);
    if (!violationData.sourceFile.isEmpty() && violationData.lineNumber) {
        cspReport->setString("source-file", violationData.sourceFile);
        cspReport->setNumber("line-number", violationData.lineNumber);
        cspReport->setNumber("column-number", violationData.columnNumber);
    }
    cspReport->setNumber("status-code", violationData.statusCode);

    RefPtr<JSONObject> reportObject = JSONObject::create();
    reportObject->setObject("csp-report", cspReport.release());
    String stringifiedReport = reportObject->toJSONString();

    if (!shouldSendViolationReport(stringifiedReport))
        return;

    RefPtr<FormData> report = FormData::create(stringifiedReport.utf8());

    for (size_t i = 0; i < reportURIs.size(); ++i)
        PingLoader::sendViolationReport(frame, reportURIs[i], report, PingLoader::ContentSecurityPolicyViolationReport);

    didSendViolationReport(stringifiedReport);
}

void ContentSecurityPolicy::reportInvalidReferrer(const String& invalidValue) const
{
    logToConsole("The 'referrer' Content Security Policy directive has the invalid value \"" + invalidValue + "\". Valid values are \"always\", \"default\", \"never\", and \"origin\".");
}

void ContentSecurityPolicy::reportReportOnlyInMeta(const String& header) const
{
    logToConsole("The report-only Content Security Policy '" + header + "' was delivered via a <meta> element, which is disallowed. The policy has been ignored.");
}

void ContentSecurityPolicy::reportMetaOutsideHead(const String& header) const
{
    logToConsole("The Content Security Policy '" + header + "' was delivered via a <meta> element outside the document's <head>, which is disallowed. The policy has been ignored.");
}

void ContentSecurityPolicy::reportInvalidInReportOnly(const String& name) const
{
    logToConsole("The Content Security Policy directive '" + name + "' is ignored when delivered in a report-only policy.");
}

void ContentSecurityPolicy::reportUnsupportedDirective(const String& name) const
{
    DEFINE_STATIC_LOCAL(String, allow, ("allow"));
    DEFINE_STATIC_LOCAL(String, options, ("options"));
    DEFINE_STATIC_LOCAL(String, policyURI, ("policy-uri"));
    DEFINE_STATIC_LOCAL(String, allowMessage, ("The 'allow' directive has been replaced with 'default-src'. Please use that directive instead, as 'allow' has no effect."));
    DEFINE_STATIC_LOCAL(String, optionsMessage, ("The 'options' directive has been replaced with 'unsafe-inline' and 'unsafe-eval' source expressions for the 'script-src' and 'style-src' directives. Please use those directives instead, as 'options' has no effect."));
    DEFINE_STATIC_LOCAL(String, policyURIMessage, ("The 'policy-uri' directive has been removed from the specification. Please specify a complete policy via the Content-Security-Policy header."));

    String message = "Unrecognized Content-Security-Policy directive '" + name + "'.\n";
    if (equalIgnoringCase(name, allow))
        message = allowMessage;
    else if (equalIgnoringCase(name, options))
        message = optionsMessage;
    else if (equalIgnoringCase(name, policyURI))
        message = policyURIMessage;

    logToConsole(message);
}

void ContentSecurityPolicy::reportDirectiveAsSourceExpression(const String& directiveName, const String& sourceExpression) const
{
    String message = "The Content Security Policy directive '" + directiveName + "' contains '" + sourceExpression + "' as a source expression. Did you mean '" + directiveName + " ...; " + sourceExpression + "...' (note the semicolon)?";
    logToConsole(message);
}

void ContentSecurityPolicy::reportDuplicateDirective(const String& name) const
{
    String message = "Ignoring duplicate Content-Security-Policy directive '" + name + "'.\n";
    logToConsole(message);
}

void ContentSecurityPolicy::reportInvalidPluginTypes(const String& pluginType) const
{
    String message;
    if (pluginType.isNull())
        message = "'plugin-types' Content Security Policy directive is empty; all plugins will be blocked.\n";
    else
        message = "Invalid plugin type in 'plugin-types' Content Security Policy directive: '" + pluginType + "'.\n";
    logToConsole(message);
}

void ContentSecurityPolicy::reportInvalidSandboxFlags(const String& invalidFlags) const
{
    logToConsole("Error while parsing the 'sandbox' Content Security Policy directive: " + invalidFlags);
}

void ContentSecurityPolicy::reportInvalidReflectedXSS(const String& invalidValue) const
{
    logToConsole("The 'reflected-xss' Content Security Policy directive has the invalid value \"" + invalidValue + "\". Valid values are \"allow\", \"filter\", and \"block\".");
}

void ContentSecurityPolicy::reportInvalidDirectiveValueCharacter(const String& directiveName, const String& value) const
{
    String message = "The value for Content Security Policy directive '" + directiveName + "' contains an invalid character: '" + value + "'. Non-whitespace characters outside ASCII 0x21-0x7E must be percent-encoded, as described in RFC 3986, section 2.1: http://tools.ietf.org/html/rfc3986#section-2.1.";
    logToConsole(message);
}

void ContentSecurityPolicy::reportInvalidPathCharacter(const String& directiveName, const String& value, const char invalidChar) const
{
    ASSERT(invalidChar == '#' || invalidChar == '?');

    String ignoring = "The fragment identifier, including the '#', will be ignored.";
    if (invalidChar == '?')
        ignoring = "The query component, including the '?', will be ignored.";
    String message = "The source list for Content Security Policy directive '" + directiveName + "' contains a source with an invalid path: '" + value + "'. " + ignoring;
    logToConsole(message);
}

void ContentSecurityPolicy::reportInvalidSourceExpression(const String& directiveName, const String& source) const
{
    String message = "The source list for Content Security Policy directive '" + directiveName + "' contains an invalid source: '" + source + "'. It will be ignored.";
    if (equalIgnoringCase(source, "'none'"))
        message = message + " Note that 'none' has no effect unless it is the only expression in the source list.";
    logToConsole(message);
}

void ContentSecurityPolicy::reportMissingReportURI(const String& policy) const
{
    logToConsole("The Content Security Policy '" + policy + "' was delivered in report-only mode, but does not specify a 'report-uri'; the policy will have no effect. Please either add a 'report-uri' directive, or deliver the policy via the 'Content-Security-Policy' header.");
}

void ContentSecurityPolicy::logToConsole(const String& message) const
{
    m_client->addConsoleMessage(SecurityMessageSource, ErrorMessageLevel, message);
}

void ContentSecurityPolicy::reportBlockedScriptExecutionToInspector(const String& directiveText) const
{
    m_client->reportBlockedScriptExecutionToInspector(directiveText);
}

bool ContentSecurityPolicy::experimentalFeaturesEnabled() const
{
    return RuntimeEnabledFeatures::experimentalContentSecurityPolicyFeaturesEnabled();
}

bool ContentSecurityPolicy::shouldBypassMainWorld(ExecutionContext* context)
{
    if (context && context->isDocument()) {
        Document* document = toDocument(context);
        if (document->frame())
            return document->frame()->script().shouldBypassMainWorldContentSecurityPolicy();
    }
    return false;
}

bool ContentSecurityPolicy::shouldSendViolationReport(const String& report) const
{
    // Collisions have no security impact, so we can save space by storing only the string's hash rather than the whole report.
    return !m_violationReportsSent.contains(report.impl()->hash());
}

void ContentSecurityPolicy::didSendViolationReport(const String& report)
{
    m_violationReportsSent.add(report.impl()->hash());
}

} // namespace WebCore
