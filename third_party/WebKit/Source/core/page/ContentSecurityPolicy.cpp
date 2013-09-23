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
#include "core/page/ContentSecurityPolicy.h"

#include "RuntimeEnabledFeatures.h"
#include "bindings/v8/ScriptCallStackFactory.h"
#include "bindings/v8/ScriptController.h"
#include "bindings/v8/ScriptState.h"
#include "core/dom/DOMStringList.h"
#include "core/dom/Document.h"
#include "core/events/SecurityPolicyViolationEvent.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/inspector/ScriptCallStack.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/PingLoader.h"
#include "core/page/ContentSecurityPolicyResponseHeaders.h"
#include "core/page/Frame.h"
#include "core/page/UseCounter.h"
#include "core/platform/JSONValues.h"
#include "core/platform/ParsingUtilities.h"
#include "core/platform/network/FormData.h"
#include "core/platform/network/ResourceResponse.h"
#include "weborigin/KURL.h"
#include "weborigin/KnownPorts.h"
#include "weborigin/SchemeRegistry.h"
#include "weborigin/SecurityOrigin.h"
#include "wtf/HashSet.h"
#include "wtf/text/TextPosition.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

// Normally WebKit uses "static" for internal linkage, but using "static" for
// these functions causes a compile error because these functions are used as
// template parameters.
namespace {

bool isDirectiveNameCharacter(UChar c)
{
    return isASCIIAlphanumeric(c) || c == '-';
}

bool isDirectiveValueCharacter(UChar c)
{
    return isASCIISpace(c) || (c >= 0x21 && c <= 0x7e); // Whitespace + VCHAR
}

bool isNonceCharacter(UChar c)
{
    return isASCIIAlphanumeric(c) || c == '+' || c == '/';
}

bool isSourceCharacter(UChar c)
{
    return !isASCIISpace(c);
}

bool isPathComponentCharacter(UChar c)
{
    return c != '?' && c != '#';
}

bool isHostCharacter(UChar c)
{
    return isASCIIAlphanumeric(c) || c == '-';
}

bool isSchemeContinuationCharacter(UChar c)
{
    return isASCIIAlphanumeric(c) || c == '+' || c == '-' || c == '.';
}

bool isNotASCIISpace(UChar c)
{
    return !isASCIISpace(c);
}

bool isNotColonOrSlash(UChar c)
{
    return c != ':' && c != '/';
}

bool isMediaTypeCharacter(UChar c)
{
    return !isASCIISpace(c) && c != '/';
}

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
static const char formAction[] = "form-action";
static const char pluginTypes[] = "plugin-types";
static const char reflectedXSS[] = "reflected-xss";

bool isDirectiveName(const String& name)
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
        || equalIgnoringCase(name, formAction)
        || equalIgnoringCase(name, pluginTypes)
        || equalIgnoringCase(name, reflectedXSS)
    );
}

UseCounter::Feature getUseCounterType(ContentSecurityPolicy::HeaderType type)
{
    switch (type) {
    case ContentSecurityPolicy::PrefixedEnforce:
        return UseCounter::PrefixedContentSecurityPolicy;
    case ContentSecurityPolicy::Enforce:
        return UseCounter::ContentSecurityPolicy;
    case ContentSecurityPolicy::PrefixedReport:
        return UseCounter::PrefixedContentSecurityPolicyReportOnly;
    case ContentSecurityPolicy::Report:
        return UseCounter::ContentSecurityPolicyReportOnly;
    }
    ASSERT_NOT_REACHED();
    return UseCounter::NumberOfFeatures;
}

} // namespace

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

class CSPSource {
public:
    CSPSource(ContentSecurityPolicy* policy, const String& scheme, const String& host, int port, const String& path, bool hostHasWildcard, bool portHasWildcard)
        : m_policy(policy)
        , m_scheme(scheme)
        , m_host(host)
        , m_port(port)
        , m_path(path)
        , m_hostHasWildcard(hostHasWildcard)
        , m_portHasWildcard(portHasWildcard)
    {
    }

    bool matches(const KURL& url) const
    {
        if (!schemeMatches(url))
            return false;
        if (isSchemeOnly())
            return true;
        return hostMatches(url) && portMatches(url) && pathMatches(url);
    }

private:
    bool schemeMatches(const KURL& url) const
    {
        if (m_scheme.isEmpty()) {
            String protectedResourceScheme(m_policy->securityOrigin()->protocol());
            if (equalIgnoringCase("http", protectedResourceScheme))
                return url.protocolIs("http") || url.protocolIs("https");
            return equalIgnoringCase(url.protocol(), protectedResourceScheme);
        }
        return equalIgnoringCase(url.protocol(), m_scheme);
    }

    bool hostMatches(const KURL& url) const
    {
        const String& host = url.host();
        if (equalIgnoringCase(host, m_host))
            return true;
        return m_hostHasWildcard && host.endsWith("." + m_host, false);

    }

    bool pathMatches(const KURL& url) const
    {
        if (m_path.isEmpty())
            return true;

        String path = decodeURLEscapeSequences(url.path());

        if (m_path.endsWith("/"))
            return path.startsWith(m_path, false);

        return path == m_path;
    }

    bool portMatches(const KURL& url) const
    {
        if (m_portHasWildcard)
            return true;

        int port = url.port();

        if (port == m_port)
            return true;

        if (!port)
            return isDefaultPortForProtocol(m_port, url.protocol());

        if (!m_port)
            return isDefaultPortForProtocol(port, url.protocol());

        return false;
    }

    bool isSchemeOnly() const { return m_host.isEmpty(); }

    ContentSecurityPolicy* m_policy;
    String m_scheme;
    String m_host;
    int m_port;
    String m_path;

    bool m_hostHasWildcard;
    bool m_portHasWildcard;
};

class CSPSourceList {
public:
    CSPSourceList(ContentSecurityPolicy*, const String& directiveName);

    void parse(const UChar* begin, const UChar* end);

    bool matches(const KURL&);
    bool allowInline() const { return m_allowInline; }
    bool allowEval() const { return m_allowEval; }
    bool allowNonce(const String& nonce) const { return !nonce.isNull() && m_nonces.contains(nonce); }

private:
    bool parseSource(const UChar* begin, const UChar* end, String& scheme, String& host, int& port, String& path, bool& hostHasWildcard, bool& portHasWildcard);
    bool parseScheme(const UChar* begin, const UChar* end, String& scheme);
    bool parseHost(const UChar* begin, const UChar* end, String& host, bool& hostHasWildcard);
    bool parsePort(const UChar* begin, const UChar* end, int& port, bool& portHasWildcard);
    bool parsePath(const UChar* begin, const UChar* end, String& path);
    bool parseNonce(const UChar* begin, const UChar* end, String& nonce);

    void addSourceSelf();
    void addSourceStar();
    void addSourceUnsafeInline();
    void addSourceUnsafeEval();
    void addSourceNonce(const String& nonce);

    ContentSecurityPolicy* m_policy;
    Vector<CSPSource> m_list;
    String m_directiveName;
    bool m_allowStar;
    bool m_allowInline;
    bool m_allowEval;
    HashSet<String> m_nonces;
};

CSPSourceList::CSPSourceList(ContentSecurityPolicy* policy, const String& directiveName)
    : m_policy(policy)
    , m_directiveName(directiveName)
    , m_allowStar(false)
    , m_allowInline(false)
    , m_allowEval(false)
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
        } else
            m_policy->reportInvalidSourceExpression(m_directiveName, String(beginSource, position - beginSource));

        ASSERT(position == end || isASCIISpace(*position));
     }
}

// source            = scheme ":"
//                   / ( [ scheme "://" ] host [ port ] [ path ] )
//                   / "'self'"
//
bool CSPSourceList::parseSource(const UChar* begin, const UChar* end,
                                String& scheme, String& host, int& port, String& path,
                                bool& hostHasWildcard, bool& portHasWildcard)
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
// nonce-value       = 1*( ALPHA / DIGIT / "+" / "/" )
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

    if (((position + 1) != end  && *position != '\'') || !(position - nonceBegin))
        return false;

    nonce = String(nonceBegin, position - nonceBegin);
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

private:
    CSPSourceList m_sourceList;
};

class CSPDirectiveList {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static PassOwnPtr<CSPDirectiveList> create(ContentSecurityPolicy*, const UChar* begin, const UChar* end, ContentSecurityPolicy::HeaderType);

    void parse(const UChar* begin, const UChar* end);

    const String& header() const { return m_header; }
    ContentSecurityPolicy::HeaderType headerType() const { return m_headerType; }

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
    bool allowScriptNonce(const String&) const;

    void gatherReportURIs(DOMStringList&) const;
    const String& evalDisabledErrorMessage() const { return m_evalDisabledErrorMessage; }
    ContentSecurityPolicy::ReflectedXSSDisposition reflectedXSSDisposition() const { return m_reflectedXSSDisposition; }
    bool isReportOnly() const { return m_reportOnly; }
    const Vector<KURL>& reportURIs() const { return m_reportURIs; }

private:
    CSPDirectiveList(ContentSecurityPolicy*, ContentSecurityPolicy::HeaderType);

    bool parseDirective(const UChar* begin, const UChar* end, String& name, String& value);
    void parseReportURI(const String& name, const String& value);
    void parsePluginTypes(const String& name, const String& value);
    void parseReflectedXSS(const String& name, const String& value);
    void addDirective(const String& name, const String& value);
    void applySandboxPolicy(const String& name, const String& sandboxPolicy);

    template <class CSPDirectiveType>
    void setCSPDirective(const String& name, const String& value, OwnPtr<CSPDirectiveType>&);

    SourceListDirective* operativeDirective(SourceListDirective*) const;
    void reportViolation(const String& directiveText, const String& effectiveDirective, const String& consoleMessage, const KURL& blockedURL) const;
    void reportViolationWithLocation(const String& directiveText, const String& effectiveDirective, const String& consoleMessage, const KURL& blockedURL, const String& contextURL, const WTF::OrdinalNumber& contextLine) const;
    void reportViolationWithState(const String& directiveText, const String& effectiveDirective, const String& consoleMessage, const KURL& blockedURL, ScriptState*) const;

    bool checkEval(SourceListDirective*) const;
    bool checkInline(SourceListDirective*) const;
    bool checkNonce(SourceListDirective*, const String&) const;
    bool checkSource(SourceListDirective*, const KURL&) const;
    bool checkMediaType(MediaListDirective*, const String& type, const String& typeAttribute) const;

    void setEvalDisabledErrorMessage(const String& errorMessage) { m_evalDisabledErrorMessage = errorMessage; }

    bool checkEvalAndReportViolation(SourceListDirective*, const String& consoleMessage, ScriptState*) const;
    bool checkInlineAndReportViolation(SourceListDirective*, const String& consoleMessage, const String& contextURL, const WTF::OrdinalNumber& contextLine, bool isScript) const;

    bool checkSourceAndReportViolation(SourceListDirective*, const KURL&, const String& effectiveDirective) const;
    bool checkMediaTypeAndReportViolation(MediaListDirective*, const String& type, const String& typeAttribute, const String& consoleMessage) const;

    bool denyIfEnforcingPolicy() const { return m_reportOnly; }

    ContentSecurityPolicy* m_policy;

    String m_header;
    ContentSecurityPolicy::HeaderType m_headerType;

    bool m_reportOnly;
    bool m_haveSandboxPolicy;
    ContentSecurityPolicy::ReflectedXSSDisposition m_reflectedXSSDisposition;

    OwnPtr<MediaListDirective> m_pluginTypes;
    OwnPtr<SourceListDirective> m_baseURI;
    OwnPtr<SourceListDirective> m_connectSrc;
    OwnPtr<SourceListDirective> m_defaultSrc;
    OwnPtr<SourceListDirective> m_fontSrc;
    OwnPtr<SourceListDirective> m_formAction;
    OwnPtr<SourceListDirective> m_frameSrc;
    OwnPtr<SourceListDirective> m_imgSrc;
    OwnPtr<SourceListDirective> m_mediaSrc;
    OwnPtr<SourceListDirective> m_objectSrc;
    OwnPtr<SourceListDirective> m_scriptSrc;
    OwnPtr<SourceListDirective> m_styleSrc;

    Vector<KURL> m_reportURIs;

    String m_evalDisabledErrorMessage;
};

CSPDirectiveList::CSPDirectiveList(ContentSecurityPolicy* policy, ContentSecurityPolicy::HeaderType type)
    : m_policy(policy)
    , m_headerType(type)
    , m_reportOnly(false)
    , m_haveSandboxPolicy(false)
    , m_reflectedXSSDisposition(ContentSecurityPolicy::ReflectedXSSUnset)
{
    m_reportOnly = (type == ContentSecurityPolicy::Report || type == ContentSecurityPolicy::PrefixedReport);
}

PassOwnPtr<CSPDirectiveList> CSPDirectiveList::create(ContentSecurityPolicy* policy, const UChar* begin, const UChar* end, ContentSecurityPolicy::HeaderType type)
{
    OwnPtr<CSPDirectiveList> directives = adoptPtr(new CSPDirectiveList(policy, type));
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
    m_policy->scriptExecutionContext()->addConsoleMessage(SecurityMessageSource, ErrorMessageLevel, message);
    m_policy->reportViolation(directiveText, effectiveDirective, message, blockedURL, m_reportURIs, m_header);
}

void CSPDirectiveList::reportViolationWithLocation(const String& directiveText, const String& effectiveDirective, const String& consoleMessage, const KURL& blockedURL, const String& contextURL, const WTF::OrdinalNumber& contextLine) const
{
    String message = m_reportOnly ? "[Report Only] " + consoleMessage : consoleMessage;
    m_policy->scriptExecutionContext()->addConsoleMessage(SecurityMessageSource, ErrorMessageLevel, message, contextURL, contextLine.oneBasedInt());
    m_policy->reportViolation(directiveText, effectiveDirective, message, blockedURL, m_reportURIs, m_header);
}

void CSPDirectiveList::reportViolationWithState(const String& directiveText, const String& effectiveDirective, const String& consoleMessage, const KURL& blockedURL, ScriptState* state) const
{
    String message = m_reportOnly ? "[Report Only] " + consoleMessage : consoleMessage;
    m_policy->scriptExecutionContext()->addConsoleMessage(SecurityMessageSource, ErrorMessageLevel, message, state);
    m_policy->reportViolation(directiveText, effectiveDirective, message, blockedURL, m_reportURIs, m_header);
}

bool CSPDirectiveList::checkEval(SourceListDirective* directive) const
{
    return !directive || directive->allowEval();
}

bool CSPDirectiveList::checkInline(SourceListDirective* directive) const
{
    return !directive || directive->allowInline();
}

bool CSPDirectiveList::checkNonce(SourceListDirective* directive, const String& nonce) const
{
    return !directive || directive->allowNonce(nonce);
}

bool CSPDirectiveList::checkSource(SourceListDirective* directive, const KURL& url) const
{
    return !directive || directive->allows(url);
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
    if (directive == m_defaultSrc)
        suffix = " Note that '" + String(isScript ? "script" : "style") + "-src' was not explicitly set, so 'default-src' is used as a fallback.";

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

bool CSPDirectiveList::allowJavaScriptURLs(const String& contextURL, const WTF::OrdinalNumber& contextLine, ContentSecurityPolicy::ReportingStatus reportingStatus) const
{
    DEFINE_STATIC_LOCAL(String, consoleMessage, ("Refused to execute JavaScript URL because it violates the following Content Security Policy directive: "));
    if (reportingStatus == ContentSecurityPolicy::SendReport) {
        return checkInlineAndReportViolation(operativeDirective(m_scriptSrc.get()), consoleMessage, contextURL, contextLine, true);
    } else {
        return checkInline(operativeDirective(m_scriptSrc.get()));
    }
}

bool CSPDirectiveList::allowInlineEventHandlers(const String& contextURL, const WTF::OrdinalNumber& contextLine, ContentSecurityPolicy::ReportingStatus reportingStatus) const
{
    DEFINE_STATIC_LOCAL(String, consoleMessage, ("Refused to execute inline event handler because it violates the following Content Security Policy directive: "));
    if (reportingStatus == ContentSecurityPolicy::SendReport) {
        return checkInlineAndReportViolation(operativeDirective(m_scriptSrc.get()), consoleMessage, contextURL, contextLine, true);
    } else {
        return checkInline(operativeDirective(m_scriptSrc.get()));
    }
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
    return reportingStatus == ContentSecurityPolicy::SendReport ?
        checkSourceAndReportViolation(operativeDirective(m_frameSrc.get()), url, frameSrc) :
        checkSource(operativeDirective(m_frameSrc.get()), url);
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

void CSPDirectiveList::gatherReportURIs(DOMStringList& list) const
{
    for (size_t i = 0; i < m_reportURIs.size(); ++i)
        list.append(m_reportURIs[i].string());
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

bool CSPDirectiveList::allowScriptNonce(const String& nonce) const
{
    return checkNonce(operativeDirective(m_scriptSrc.get()), nonce);
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
    skipWhile<UChar, isDirectiveNameCharacter>(position, end);

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
    skipWhile<UChar, isDirectiveValueCharacter>(position, end);

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
    if (m_haveSandboxPolicy) {
        m_policy->reportDuplicateDirective(name);
        return;
    }
    m_haveSandboxPolicy = true;
    String invalidTokens;
    m_policy->enforceSandboxFlags(SecurityContext::parseSandboxPolicy(sandboxPolicy, invalidTokens));
    if (!invalidTokens.isNull())
        m_policy->reportInvalidSandboxFlags(invalidTokens);
}

void CSPDirectiveList::parseReflectedXSS(const String& name, const String& value)
{
    if (m_reflectedXSSDisposition != ContentSecurityPolicy::ReflectedXSSUnset) {
        m_policy->reportDuplicateDirective(name);
        m_reflectedXSSDisposition = ContentSecurityPolicy::ReflectedXSSInvalid;
        return;
    }

    if (value.isEmpty()) {
        m_reflectedXSSDisposition = ContentSecurityPolicy::ReflectedXSSInvalid;
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
    if (equalIgnoringCase("allow", begin, position - begin))
        m_reflectedXSSDisposition = ContentSecurityPolicy::AllowReflectedXSS;
    else if (equalIgnoringCase("filter", begin, position - begin))
        m_reflectedXSSDisposition = ContentSecurityPolicy::FilterReflectedXSS;
    else if (equalIgnoringCase("block", begin, position - begin))
        m_reflectedXSSDisposition = ContentSecurityPolicy::BlockReflectedXSS;
    else {
        m_reflectedXSSDisposition = ContentSecurityPolicy::ReflectedXSSInvalid;
        m_policy->reportInvalidReflectedXSS(value);
        return;
    }

    skipWhile<UChar, isASCIISpace>(position, end);
    if (position == end && m_reflectedXSSDisposition != ContentSecurityPolicy::ReflectedXSSUnset)
        return;

    // value1 value2
    //        ^
    m_reflectedXSSDisposition = ContentSecurityPolicy::ReflectedXSSInvalid;
    m_policy->reportInvalidReflectedXSS(value);
}

void CSPDirectiveList::addDirective(const String& name, const String& value)
{
    ASSERT(!name.isEmpty());

    if (equalIgnoringCase(name, defaultSrc))
        setCSPDirective<SourceListDirective>(name, value, m_defaultSrc);
    else if (equalIgnoringCase(name, scriptSrc))
        setCSPDirective<SourceListDirective>(name, value, m_scriptSrc);
    else if (equalIgnoringCase(name, objectSrc))
        setCSPDirective<SourceListDirective>(name, value, m_objectSrc);
    else if (equalIgnoringCase(name, frameSrc))
        setCSPDirective<SourceListDirective>(name, value, m_frameSrc);
    else if (equalIgnoringCase(name, imgSrc))
        setCSPDirective<SourceListDirective>(name, value, m_imgSrc);
    else if (equalIgnoringCase(name, styleSrc))
        setCSPDirective<SourceListDirective>(name, value, m_styleSrc);
    else if (equalIgnoringCase(name, fontSrc))
        setCSPDirective<SourceListDirective>(name, value, m_fontSrc);
    else if (equalIgnoringCase(name, mediaSrc))
        setCSPDirective<SourceListDirective>(name, value, m_mediaSrc);
    else if (equalIgnoringCase(name, connectSrc))
        setCSPDirective<SourceListDirective>(name, value, m_connectSrc);
    else if (equalIgnoringCase(name, sandbox))
        applySandboxPolicy(name, value);
    else if (equalIgnoringCase(name, reportURI))
        parseReportURI(name, value);
    else if (m_policy->experimentalFeaturesEnabled()) {
        if (equalIgnoringCase(name, baseURI))
            setCSPDirective<SourceListDirective>(name, value, m_baseURI);
        else if (equalIgnoringCase(name, formAction))
            setCSPDirective<SourceListDirective>(name, value, m_formAction);
        else if (equalIgnoringCase(name, pluginTypes))
            setCSPDirective<MediaListDirective>(name, value, m_pluginTypes);
        else if (equalIgnoringCase(name, reflectedXSS))
            parseReflectedXSS(name, value);
        else
            m_policy->reportUnsupportedDirective(name);
    }
    else
        m_policy->reportUnsupportedDirective(name);
}

ContentSecurityPolicy::ContentSecurityPolicy(ScriptExecutionContext* scriptExecutionContext)
    : m_scriptExecutionContext(scriptExecutionContext)
    , m_overrideInlineStyleAllowed(false)
{
}

ContentSecurityPolicy::~ContentSecurityPolicy()
{
}

void ContentSecurityPolicy::copyStateFrom(const ContentSecurityPolicy* other)
{
    ASSERT(m_policies.isEmpty());
    for (CSPDirectiveListVector::const_iterator iter = other->m_policies.begin(); iter != other->m_policies.end(); ++iter)
        addPolicyFromHeaderValue((*iter)->header(), (*iter)->headerType());
}

void ContentSecurityPolicy::didReceiveHeaders(const ContentSecurityPolicyResponseHeaders& headers)
{
    if (!headers.contentSecurityPolicy().isEmpty())
        didReceiveHeader(headers.contentSecurityPolicy(), ContentSecurityPolicy::Enforce);
    if (!headers.contentSecurityPolicyReportOnly().isEmpty())
        didReceiveHeader(headers.contentSecurityPolicyReportOnly(), ContentSecurityPolicy::Report);
    if (!headers.xWebKitCSP().isEmpty())
        didReceiveHeader(headers.xWebKitCSP(), ContentSecurityPolicy::PrefixedEnforce);
    if (!headers.xWebKitCSPReportOnly().isEmpty())
        didReceiveHeader(headers.xWebKitCSPReportOnly(), ContentSecurityPolicy::PrefixedReport);
}

void ContentSecurityPolicy::didReceiveHeader(const String& header, HeaderType type)
{
    addPolicyFromHeaderValue(header, type);
}

void ContentSecurityPolicy::addPolicyFromHeaderValue(const String& header, HeaderType type)
{
    if (m_scriptExecutionContext->isDocument()) {
        Document* document = toDocument(m_scriptExecutionContext);
        if (type == PrefixedReport || type == PrefixedEnforce)
            UseCounter::countDeprecation(document, getUseCounterType(type));
        else
            UseCounter::count(document, getUseCounterType(type));
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
        OwnPtr<CSPDirectiveList> policy = CSPDirectiveList::create(this, begin, position, type);

        // We disable 'eval()' even in the case of report-only policies, and rely on the check in the V8Initializer::codeGenerationCheckCallbackInMainThread callback to determine whether the call should execute or not.
        if (!policy->allowEval(0, SuppressReport))
            m_scriptExecutionContext->disableEval(policy->evalDisabledErrorMessage());

        m_policies.append(policy.release());

        // Skip the comma, and begin the next header from the current position.
        ASSERT(position == end || *position == ',');
        skipExactly<UChar>(position, end, ',');
        begin = position;
    }
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

bool ContentSecurityPolicy::isActive() const
{
    return !m_policies.isEmpty();
}

ContentSecurityPolicy::ReflectedXSSDisposition ContentSecurityPolicy::reflectedXSSDisposition() const
{
    ReflectedXSSDisposition disposition = ReflectedXSSUnset;
    for (size_t i = 0; i < m_policies.size(); ++i) {
        if (m_policies[i]->reflectedXSSDisposition() > disposition)
            disposition = std::max(disposition, m_policies[i]->reflectedXSSDisposition());
    }
    return disposition;
}

void ContentSecurityPolicy::gatherReportURIs(DOMStringList& list) const
{
    for (size_t i = 0; i < m_policies.size(); ++i)
        m_policies[i]->gatherReportURIs(list);
}

SecurityOrigin* ContentSecurityPolicy::securityOrigin() const
{
    return m_scriptExecutionContext->securityOrigin();
}

const KURL& ContentSecurityPolicy::url() const
{
    return m_scriptExecutionContext->url();
}

KURL ContentSecurityPolicy::completeURL(const String& url) const
{
    return m_scriptExecutionContext->completeURL(url);
}

void ContentSecurityPolicy::enforceSandboxFlags(SandboxFlags mask) const
{
    m_scriptExecutionContext->enforceSandboxFlags(mask);
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
    if (!m_scriptExecutionContext->isDocument())
        return;

    Document* document = toDocument(m_scriptExecutionContext);
    Frame* frame = document->frame();
    if (!frame)
        return;

    SecurityPolicyViolationEventInit violationData;
    gatherSecurityPolicyViolationEventData(violationData, document, directiveText, effectiveDirective, blockedURL, header);

    if (experimentalFeaturesEnabled())
        document->enqueueDocumentEvent(SecurityPolicyViolationEvent::create(eventNames().securitypolicyviolationEvent, violationData));

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

void ContentSecurityPolicy::reportInvalidNonce(const String& nonce) const
{
    String message = "Ignoring invalid Content Security Policy script nonce: '" + nonce + "'.\n";
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
    m_scriptExecutionContext->addConsoleMessage(SecurityMessageSource, ErrorMessageLevel, message);
}

void ContentSecurityPolicy::reportBlockedScriptExecutionToInspector(const String& directiveText) const
{
    InspectorInstrumentation::scriptExecutionBlockedByCSP(m_scriptExecutionContext, directiveText);
}

bool ContentSecurityPolicy::experimentalFeaturesEnabled() const
{
    return RuntimeEnabledFeatures::experimentalContentSecurityPolicyFeaturesEnabled();
}

bool ContentSecurityPolicy::shouldBypassMainWorld(ScriptExecutionContext* context)
{
    if (context && context->isDocument()) {
        Document* document = toDocument(context);
        if (document->frame())
            return document->frame()->script()->shouldBypassMainWorldContentSecurityPolicy();
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
