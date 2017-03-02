// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/csp/SourceListDirective.h"

#include "core/frame/csp/CSPSource.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "platform/network/ContentSecurityPolicyParsers.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "wtf/HashSet.h"
#include "wtf/text/Base64.h"
#include "wtf/text/ParsingUtilities.h"
#include "wtf/text/StringToNumber.h"
#include "wtf/text/WTFString.h"

namespace blink {

SourceListDirective::SourceListDirective(const String& name,
                                         const String& value,
                                         ContentSecurityPolicy* policy)
    : CSPDirective(name, value, policy),
      m_policy(policy),
      m_directiveName(name),
      m_allowSelf(false),
      m_allowStar(false),
      m_allowInline(false),
      m_allowEval(false),
      m_allowDynamic(false),
      m_allowHashedAttributes(false),
      m_reportSample(false),
      m_hashAlgorithmsUsed(0) {
  Vector<UChar> characters;
  value.appendTo(characters);
  parse(characters.data(), characters.data() + characters.size());
}

static bool isSourceListNone(const UChar* begin, const UChar* end) {
  skipWhile<UChar, isASCIISpace>(begin, end);

  const UChar* position = begin;
  skipWhile<UChar, isSourceCharacter>(position, end);
  if (!equalIgnoringCase("'none'", StringView(begin, position - begin)))
    return false;

  skipWhile<UChar, isASCIISpace>(position, end);
  if (position != end)
    return false;

  return true;
}

bool SourceListDirective::allows(
    const KURL& url,
    ResourceRequest::RedirectStatus redirectStatus) const {
  // Wildcards match network schemes ('http', 'https', 'ftp', 'ws', 'wss'), and
  // the scheme of the protected resource:
  // https://w3c.github.io/webappsec-csp/#match-url-to-source-expression. Other
  // schemes, including custom schemes, must be explicitly listed in a source
  // list.
  if (m_allowStar) {
    if (url.protocolIsInHTTPFamily() || url.protocolIs("ftp") ||
        url.protocolIs("ws") || url.protocolIs("wss") ||
        m_policy->protocolMatchesSelf(url))
      return true;

    return hasSourceMatchInList(url, redirectStatus);
  }

  KURL effectiveURL =
      m_policy->selfMatchesInnerURL() && SecurityOrigin::shouldUseInnerURL(url)
          ? SecurityOrigin::extractInnerURL(url)
          : url;

  if (m_allowSelf && m_policy->urlMatchesSelf(effectiveURL))
    return true;

  return hasSourceMatchInList(effectiveURL, redirectStatus);
}

bool SourceListDirective::allowInline() const {
  return m_allowInline;
}

bool SourceListDirective::allowEval() const {
  return m_allowEval;
}

bool SourceListDirective::allowDynamic() const {
  return m_allowDynamic;
}

bool SourceListDirective::allowNonce(const String& nonce) const {
  String nonceStripped = nonce.stripWhiteSpace();
  return !nonceStripped.isNull() && m_nonces.contains(nonceStripped);
}

bool SourceListDirective::allowHash(const CSPHashValue& hashValue) const {
  return m_hashes.contains(hashValue);
}

bool SourceListDirective::allowHashedAttributes() const {
  return m_allowHashedAttributes;
}

bool SourceListDirective::allowReportSample() const {
  if (!m_policy->experimentalFeaturesEnabled())
    return false;
  return m_reportSample;
}

bool SourceListDirective::isNone() const {
  return !m_list.size() && !m_allowSelf && !m_allowStar && !m_allowInline &&
         !m_allowHashedAttributes && !m_allowEval && !m_allowDynamic &&
         !m_nonces.size() && !m_hashes.size();
}

uint8_t SourceListDirective::hashAlgorithmsUsed() const {
  return m_hashAlgorithmsUsed;
}

bool SourceListDirective::isHashOrNoncePresent() const {
  return !m_nonces.isEmpty() ||
         m_hashAlgorithmsUsed != ContentSecurityPolicyHashAlgorithmNone;
}

// source-list       = *WSP [ source *( 1*WSP source ) *WSP ]
//                   / *WSP "'none'" *WSP
//
void SourceListDirective::parse(const UChar* begin, const UChar* end) {
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
    CSPSource::WildcardDisposition hostWildcard = CSPSource::NoWildcard;
    CSPSource::WildcardDisposition portWildcard = CSPSource::NoWildcard;

    if (parseSource(beginSource, position, scheme, host, port, path,
                    hostWildcard, portWildcard)) {
      // Wildcard hosts and keyword sources ('self', 'unsafe-inline',
      // etc.) aren't stored in m_list, but as attributes on the source
      // list itself.
      if (scheme.isEmpty() && host.isEmpty())
        continue;
      if (ContentSecurityPolicy::getDirectiveType(host) !=
          ContentSecurityPolicy::DirectiveType::Undefined)
        m_policy->reportDirectiveAsSourceExpression(m_directiveName, host);
      m_list.push_back(new CSPSource(m_policy, scheme, host, port, path,
                                     hostWildcard, portWildcard));
    } else {
      m_policy->reportInvalidSourceExpression(
          m_directiveName, String(beginSource, position - beginSource));
    }

    DCHECK(position == end || isASCIISpace(*position));
  }
}

// source            = scheme ":"
//                   / ( [ scheme "://" ] host [ port ] [ path ] )
//                   / "'self'"
bool SourceListDirective::parseSource(
    const UChar* begin,
    const UChar* end,
    String& scheme,
    String& host,
    int& port,
    String& path,
    CSPSource::WildcardDisposition& hostWildcard,
    CSPSource::WildcardDisposition& portWildcard) {
  if (begin == end)
    return false;

  StringView token(begin, end - begin);

  if (equalIgnoringCase("'none'", token))
    return false;

  if (end - begin == 1 && *begin == '*') {
    addSourceStar();
    return true;
  }

  if (equalIgnoringCase("'self'", token)) {
    addSourceSelf();
    return true;
  }

  if (equalIgnoringCase("'unsafe-inline'", token)) {
    addSourceUnsafeInline();
    return true;
  }

  if (equalIgnoringCase("'unsafe-eval'", token)) {
    addSourceUnsafeEval();
    return true;
  }

  if (equalIgnoringCase("'strict-dynamic'", token)) {
    addSourceStrictDynamic();
    return true;
  }

  if (equalIgnoringCase("'unsafe-hashed-attributes'", token)) {
    addSourceUnsafeHashedAttributes();
    return true;
  }

  if (equalIgnoringCase("'report-sample'", token)) {
    addReportSample();
    return true;
  }

  String nonce;
  if (!parseNonce(begin, end, nonce))
    return false;

  if (!nonce.isNull()) {
    addSourceNonce(nonce);
    return true;
  }

  DigestValue hash;
  ContentSecurityPolicyHashAlgorithm algorithm =
      ContentSecurityPolicyHashAlgorithmNone;
  if (!parseHash(begin, end, hash, algorithm))
    return false;

  if (hash.size() > 0) {
    addSourceHash(algorithm, hash);
    return true;
  }

  const UChar* position = begin;
  const UChar* beginHost = begin;
  const UChar* beginPath = end;
  const UChar* beginPort = 0;

  skipWhile<UChar, isNotColonOrSlash>(position, end);

  if (position == end) {
    // host
    //     ^
    return parseHost(beginHost, position, host, hostWildcard);
  }

  if (position < end && *position == '/') {
    // host/path || host/ || /
    //     ^            ^    ^
    return parseHost(beginHost, position, host, hostWildcard) &&
           parsePath(position, end, path);
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
      if (!parseScheme(begin, position, scheme) ||
          !skipExactly<UChar>(position, end, ':') ||
          !skipExactly<UChar>(position, end, '/') ||
          !skipExactly<UChar>(position, end, '/'))
        return false;
      if (position == end)
        return false;
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

  if (!parseHost(beginHost, beginPort ? beginPort : beginPath, host,
                 hostWildcard))
    return false;

  if (beginPort) {
    if (!parsePort(beginPort, beginPath, port, portWildcard))
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
bool SourceListDirective::parseNonce(const UChar* begin,
                                     const UChar* end,
                                     String& nonce) {
  size_t nonceLength = end - begin;
  StringView prefix("'nonce-");

  // TODO(esprehn): Should be StringView(begin, nonceLength).startsWith(prefix).
  if (nonceLength <= prefix.length() ||
      !equalIgnoringCase(prefix, StringView(begin, prefix.length())))
    return true;

  const UChar* position = begin + prefix.length();
  const UChar* nonceBegin = position;

  DCHECK(position < end);
  skipWhile<UChar, isNonceCharacter>(position, end);
  DCHECK(nonceBegin <= position);

  if (position + 1 != end || *position != '\'' || position == nonceBegin)
    return false;

  nonce = String(nonceBegin, position - nonceBegin);
  return true;
}

// hash-source       = "'" hash-algorithm "-" hash-value "'"
// hash-algorithm    = "sha1" / "sha256" / "sha384" / "sha512"
// hash-value        = 1*( ALPHA / DIGIT / "+" / "/" / "=" )
//
bool SourceListDirective::parseHash(
    const UChar* begin,
    const UChar* end,
    DigestValue& hash,
    ContentSecurityPolicyHashAlgorithm& hashAlgorithm) {
  // Any additions or subtractions from this struct should also modify the
  // respective entries in the kAlgorithmMap array in checkDigest().
  static const struct {
    const char* prefix;
    ContentSecurityPolicyHashAlgorithm type;
  } kSupportedPrefixes[] = {
      // FIXME: Drop support for SHA-1. It's not in the spec.
      {"'sha1-", ContentSecurityPolicyHashAlgorithmSha1},
      {"'sha256-", ContentSecurityPolicyHashAlgorithmSha256},
      {"'sha384-", ContentSecurityPolicyHashAlgorithmSha384},
      {"'sha512-", ContentSecurityPolicyHashAlgorithmSha512},
      {"'sha-256-", ContentSecurityPolicyHashAlgorithmSha256},
      {"'sha-384-", ContentSecurityPolicyHashAlgorithmSha384},
      {"'sha-512-", ContentSecurityPolicyHashAlgorithmSha512}};

  StringView prefix;
  hashAlgorithm = ContentSecurityPolicyHashAlgorithmNone;
  size_t hashLength = end - begin;

  for (const auto& algorithm : kSupportedPrefixes) {
    prefix = algorithm.prefix;
    // TODO(esprehn): Should be StringView(begin, end -
    // begin).startsWith(prefix).
    if (hashLength > prefix.length() &&
        equalIgnoringCase(prefix, StringView(begin, prefix.length()))) {
      hashAlgorithm = algorithm.type;
      break;
    }
  }

  if (hashAlgorithm == ContentSecurityPolicyHashAlgorithmNone)
    return true;

  const UChar* position = begin + prefix.length();
  const UChar* hashBegin = position;

  DCHECK(position < end);
  skipWhile<UChar, isBase64EncodedCharacter>(position, end);
  DCHECK(hashBegin <= position);

  // Base64 encodings may end with exactly one or two '=' characters
  if (position < end)
    skipExactly<UChar>(position, position + 1, '=');
  if (position < end)
    skipExactly<UChar>(position, position + 1, '=');

  if (position + 1 != end || *position != '\'' || position == hashBegin)
    return false;

  Vector<char> hashVector;
  // We accept base64url-encoded data here by normalizing it to base64.
  base64Decode(normalizeToBase64(String(hashBegin, position - hashBegin)),
               hashVector);
  if (hashVector.size() > kMaxDigestSize)
    return false;
  hash.append(reinterpret_cast<uint8_t*>(hashVector.data()), hashVector.size());
  return true;
}

//                     ; <scheme> production from RFC 3986
// scheme      = ALPHA *( ALPHA / DIGIT / "+" / "-" / "." )
//
bool SourceListDirective::parseScheme(const UChar* begin,
                                      const UChar* end,
                                      String& scheme) {
  DCHECK(begin <= end);
  DCHECK(scheme.isEmpty());

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
// static
bool SourceListDirective::parseHost(
    const UChar* begin,
    const UChar* end,
    String& host,
    CSPSource::WildcardDisposition& hostWildcard) {
  DCHECK(begin <= end);
  DCHECK(host.isEmpty());
  DCHECK(hostWildcard == CSPSource::NoWildcard);

  if (begin == end)
    return false;

  const UChar* position = begin;

  // Parse "*" or [ "*." ].
  if (skipExactly<UChar>(position, end, '*')) {
    hostWildcard = CSPSource::HasWildcard;

    if (position == end) {
      // "*"
      return true;
    }

    if (!skipExactly<UChar>(position, end, '.'))
      return false;
  }
  const UChar* hostBegin = position;

  // Parse 1*host-hcar.
  if (!skipExactly<UChar, isHostCharacter>(position, end))
    return false;
  skipWhile<UChar, isHostCharacter>(position, end);

  // Parse *( "." 1*host-char ).
  while (position < end) {
    if (!skipExactly<UChar>(position, end, '.'))
      return false;
    if (!skipExactly<UChar, isHostCharacter>(position, end))
      return false;
    skipWhile<UChar, isHostCharacter>(position, end);
  }

  host = String(hostBegin, end - hostBegin);
  return true;
}

bool SourceListDirective::parsePath(const UChar* begin,
                                    const UChar* end,
                                    String& path) {
  DCHECK(begin <= end);
  DCHECK(path.isEmpty());

  const UChar* position = begin;
  skipWhile<UChar, isPathComponentCharacter>(position, end);
  // path/to/file.js?query=string || path/to/file.js#anchor
  //                ^                               ^
  if (position < end) {
    m_policy->reportInvalidPathCharacter(m_directiveName,
                                         String(begin, end - begin), *position);
  }

  path = decodeURLEscapeSequences(String(begin, position - begin));

  DCHECK(position <= end);
  DCHECK(position == end || (*position == '#' || *position == '?'));
  return true;
}

// port              = ":" ( 1*DIGIT / "*" )
//
bool SourceListDirective::parsePort(
    const UChar* begin,
    const UChar* end,
    int& port,
    CSPSource::WildcardDisposition& portWildcard) {
  DCHECK(begin <= end);
  DCHECK(!port);
  DCHECK(portWildcard == CSPSource::NoWildcard);

  if (!skipExactly<UChar>(begin, end, ':'))
    NOTREACHED();

  if (begin == end)
    return false;

  if (end - begin == 1 && *begin == '*') {
    port = 0;
    portWildcard = CSPSource::HasWildcard;
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

void SourceListDirective::addSourceSelf() {
  m_allowSelf = true;
}

void SourceListDirective::addSourceStar() {
  m_allowStar = true;
}

void SourceListDirective::addSourceUnsafeInline() {
  m_allowInline = true;
}

void SourceListDirective::addSourceUnsafeEval() {
  m_allowEval = true;
}

void SourceListDirective::addSourceStrictDynamic() {
  m_allowDynamic = true;
}

void SourceListDirective::addSourceUnsafeHashedAttributes() {
  m_allowHashedAttributes = true;
}

void SourceListDirective::addReportSample() {
  m_reportSample = true;
}

void SourceListDirective::addSourceNonce(const String& nonce) {
  m_nonces.insert(nonce);
}

void SourceListDirective::addSourceHash(
    const ContentSecurityPolicyHashAlgorithm& algorithm,
    const DigestValue& hash) {
  m_hashes.insert(CSPHashValue(algorithm, hash));
  m_hashAlgorithmsUsed |= algorithm;
}

void SourceListDirective::addSourceToMap(
    HeapHashMap<String, Member<CSPSource>>& hashMap,
    CSPSource* source) {
  hashMap.insert(source->getScheme(), source);
  if (source->getScheme() == "http")
    hashMap.insert("https", source);
  else if (source->getScheme() == "ws")
    hashMap.insert("wss", source);
}

bool SourceListDirective::hasSourceMatchInList(
    const KURL& url,
    ResourceRequest::RedirectStatus redirectStatus) const {
  for (size_t i = 0; i < m_list.size(); ++i) {
    if (m_list[i]->matches(url, redirectStatus))
      return true;
  }

  return false;
}

bool SourceListDirective::allowAllInline() const {
  const ContentSecurityPolicy::DirectiveType& type =
      ContentSecurityPolicy::getDirectiveType(m_directiveName);
  if (type != ContentSecurityPolicy::DirectiveType::DefaultSrc &&
      type != ContentSecurityPolicy::DirectiveType::StyleSrc &&
      type != ContentSecurityPolicy::DirectiveType::ScriptSrc) {
    return false;
  }
  return m_allowInline && !isHashOrNoncePresent() &&
         (type != ContentSecurityPolicy::DirectiveType::ScriptSrc ||
          !m_allowDynamic);
}

HeapVector<Member<CSPSource>> SourceListDirective::getSources(
    Member<CSPSource> self) const {
  HeapVector<Member<CSPSource>> sources = m_list;
  if (m_allowStar) {
    sources.push_back(new CSPSource(m_policy, "ftp", String(), 0, String(),
                                    CSPSource::NoWildcard,
                                    CSPSource::NoWildcard));
    sources.push_back(new CSPSource(m_policy, "ws", String(), 0, String(),
                                    CSPSource::NoWildcard,
                                    CSPSource::NoWildcard));
    sources.push_back(new CSPSource(m_policy, "http", String(), 0, String(),
                                    CSPSource::NoWildcard,
                                    CSPSource::NoWildcard));
    if (self) {
      sources.push_back(new CSPSource(m_policy, self->getScheme(), String(), 0,
                                      String(), CSPSource::NoWildcard,
                                      CSPSource::NoWildcard));
    }
  } else if (m_allowSelf && self) {
    sources.push_back(self);
  }

  return sources;
}

bool SourceListDirective::subsumes(
    const HeapVector<Member<SourceListDirective>>& other) const {
  if (!other.size() || other[0]->isNone())
    return other.size();

  bool allowInlineOther = other[0]->m_allowInline;
  bool allowEvalOther = other[0]->m_allowEval;
  bool allowDynamicOther = other[0]->m_allowDynamic;
  bool allowHashedAttributesOther = other[0]->m_allowHashedAttributes;
  bool isHashOrNoncePresentOther = other[0]->isHashOrNoncePresent();
  HashSet<String> noncesB = other[0]->m_nonces;
  HashSet<CSPHashValue> hashesB = other[0]->m_hashes;

  HeapVector<Member<CSPSource>> normalizedB =
      other[0]->getSources(other[0]->m_policy->getSelfSource());
  for (size_t i = 1; i < other.size(); i++) {
    allowInlineOther = allowInlineOther && other[i]->m_allowInline;
    allowEvalOther = allowEvalOther && other[i]->m_allowEval;
    allowDynamicOther = allowDynamicOther && other[i]->m_allowDynamic;
    allowHashedAttributesOther =
        allowHashedAttributesOther && other[i]->m_allowHashedAttributes;
    isHashOrNoncePresentOther =
        isHashOrNoncePresentOther && other[i]->isHashOrNoncePresent();
    noncesB = other[i]->getIntersectNonces(noncesB);
    hashesB = other[i]->getIntersectHashes(hashesB);
    normalizedB = other[i]->getIntersectCSPSources(normalizedB);
  }

  if (!subsumesNoncesAndHashes(noncesB, hashesB))
    return false;

  const ContentSecurityPolicy::DirectiveType type =
      ContentSecurityPolicy::getDirectiveType(m_directiveName);
  if (type == ContentSecurityPolicy::DirectiveType::ScriptSrc ||
      type == ContentSecurityPolicy::DirectiveType::StyleSrc) {
    if (!m_allowEval && allowEvalOther)
      return false;
    if (!m_allowHashedAttributes && allowHashedAttributesOther)
      return false;
    bool allowAllInlineOther =
        allowInlineOther && !isHashOrNoncePresentOther &&
        (type != ContentSecurityPolicy::DirectiveType::ScriptSrc ||
         !allowDynamicOther);
    if (!allowAllInline() && allowAllInlineOther)
      return false;
  }

  if (type == ContentSecurityPolicy::DirectiveType::ScriptSrc &&
      (m_allowDynamic || allowDynamicOther)) {
    // If `this` does not allow `strict-dynamic`, then it must be that `other`
    // does allow, so the result is `false`.
    if (!m_allowDynamic)
      return false;
    // All keyword source expressions have been considered so only CSPSource
    // subsumption is left. However, `strict-dynamic` ignores all CSPSources so
    // for subsumption to be true either `other` must allow `strict-dynamic` or
    // have no allowed CSPSources.
    return allowDynamicOther || !normalizedB.size();
  }

  // If embedding CSP specifies `self`, `self` refers to the embedee's origin.
  HeapVector<Member<CSPSource>> normalizedA =
      getSources(other[0]->m_policy->getSelfSource());
  return CSPSource::firstSubsumesSecond(normalizedA, normalizedB);
}

WebContentSecurityPolicySourceList
SourceListDirective::exposeForNavigationalChecks() const {
  WebContentSecurityPolicySourceList sourceList;
  sourceList.allowSelf = m_allowSelf;
  sourceList.allowStar = m_allowStar;
  WebVector<WebContentSecurityPolicySourceExpression> list(m_list.size());
  for (size_t i = 0; i < m_list.size(); ++i)
    list[i] = m_list[i]->exposeForNavigationalChecks();
  sourceList.sources.swap(list);
  return sourceList;
}

bool SourceListDirective::subsumesNoncesAndHashes(
    const HashSet<String>& nonces,
    const HashSet<CSPHashValue> hashes) const {
  for (const auto& nonce : nonces) {
    if (!m_nonces.contains(nonce))
      return false;
  }
  for (const auto& hash : hashes) {
    if (!m_hashes.contains(hash))
      return false;
  }

  return true;
}

HashSet<String> SourceListDirective::getIntersectNonces(
    const HashSet<String>& other) const {
  if (!m_nonces.size() || !other.size())
    return !m_nonces.size() ? m_nonces : other;

  HashSet<String> normalized;
  for (const auto& nonce : m_nonces) {
    if (other.contains(nonce))
      normalized.insert(nonce);
  }

  return normalized;
}

HashSet<CSPHashValue> SourceListDirective::getIntersectHashes(
    const HashSet<CSPHashValue>& other) const {
  if (!m_hashes.size() || !other.size())
    return !m_hashes.size() ? m_hashes : other;

  HashSet<CSPHashValue> normalized;
  for (const auto& hash : m_hashes) {
    if (other.contains(hash))
      normalized.insert(hash);
  }

  return normalized;
}

HeapHashMap<String, Member<CSPSource>>
SourceListDirective::getIntersectSchemesOnly(
    const HeapVector<Member<CSPSource>>& other) const {
  HeapHashMap<String, Member<CSPSource>> schemesA;
  for (const auto& sourceA : m_list) {
    if (sourceA->isSchemeOnly())
      addSourceToMap(schemesA, sourceA);
  }
  // Add schemes only sources if they are present in both `this` and `other`,
  // allowing upgrading `http` to `https` and `ws` to `wss`.
  HeapHashMap<String, Member<CSPSource>> intersect;
  for (const auto& sourceB : other) {
    if (sourceB->isSchemeOnly()) {
      if (schemesA.contains(sourceB->getScheme()))
        addSourceToMap(intersect, sourceB);
      else if (sourceB->getScheme() == "http" && schemesA.contains("https"))
        intersect.insert("https", schemesA.at("https"));
      else if (sourceB->getScheme() == "ws" && schemesA.contains("wss"))
        intersect.insert("wss", schemesA.at("wss"));
    }
  }

  return intersect;
}

HeapVector<Member<CSPSource>> SourceListDirective::getIntersectCSPSources(
    const HeapVector<Member<CSPSource>>& other) const {
  auto schemesMap = getIntersectSchemesOnly(other);
  HeapVector<Member<CSPSource>> normalized;
  // Add all normalized scheme source expressions.
  for (const auto& it : schemesMap) {
    // We do not add secure versions if insecure schemes are present.
    if ((it.key != "https" || !schemesMap.contains("http")) &&
        (it.key != "wss" || !schemesMap.contains("ws"))) {
      normalized.push_back(it.value);
    }
  }

  HeapVector<Member<CSPSource>> thisVector =
      getSources(m_policy->getSelfSource());
  for (const auto& sourceA : thisVector) {
    if (schemesMap.contains(sourceA->getScheme()))
      continue;

    CSPSource* match(nullptr);
    for (const auto& sourceB : other) {
      // No need to add a host source expression if it is subsumed by the
      // matching scheme source expression.
      if (schemesMap.contains(sourceB->getScheme()))
        continue;
      // If sourceA is scheme only but there was no intersection for it in the
      // `other` list, we add all the sourceB with that scheme.
      if (sourceA->isSchemeOnly()) {
        if (CSPSource* localMatch = sourceB->intersect(sourceA))
          normalized.push_back(localMatch);
        continue;
      }
      if (sourceB->subsumes(sourceA)) {
        match = sourceA;
        break;
      }
      if (CSPSource* localMatch = sourceB->intersect(sourceA))
        match = localMatch;
    }
    if (match)
      normalized.push_back(match);
  }
  return normalized;
}

DEFINE_TRACE(SourceListDirective) {
  visitor->trace(m_policy);
  visitor->trace(m_list);
  CSPDirective::trace(visitor);
}

}  // namespace blink
