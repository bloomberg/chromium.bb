// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/csp/CSPSource.h"

#include "core/frame/UseCounter.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/KnownPorts.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "wtf/text/WTFString.h"

namespace blink {

CSPSource::CSPSource(ContentSecurityPolicy* policy,
                     const String& scheme,
                     const String& host,
                     int port,
                     const String& path,
                     WildcardDisposition hostWildcard,
                     WildcardDisposition portWildcard)
    : m_policy(policy),
      m_scheme(scheme.lower()),
      m_host(host),
      m_port(port),
      m_path(path),
      m_hostWildcard(hostWildcard),
      m_portWildcard(portWildcard) {}

bool CSPSource::matches(const KURL& url,
                        ResourceRequest::RedirectStatus redirectStatus) const {
  SchemeMatchingResult schemesMatch = schemeMatches(url.protocol());
  if (schemesMatch == SchemeMatchingResult::NotMatching)
    return false;
  if (isSchemeOnly())
    return true;
  bool pathsMatch = (redirectStatus == RedirectStatus::FollowedRedirect) ||
                    pathMatches(url.path());
  PortMatchingResult portsMatch = portMatches(url.port(), url.protocol());

  // if either the scheme or the port would require an upgrade (e.g. from http
  // to https) then check that both of them can upgrade to ensure that we don't
  // run into situations where we only upgrade the port but not the scheme or
  // viceversa
  if ((requiresUpgrade(schemesMatch) || (requiresUpgrade(portsMatch))) &&
      (!canUpgrade(schemesMatch) || !canUpgrade(portsMatch))) {
    return false;
  }

  return hostMatches(url.host()) && portsMatch != PortMatchingResult::NotMatching && pathsMatch;
}

CSPSource::SchemeMatchingResult CSPSource::schemeMatches(
    const String& protocol) const {
  DCHECK_EQ(protocol, protocol.lower());
  const String& scheme =
      (m_scheme.isEmpty() ? m_policy->getSelfProtocol() : m_scheme);

  if (scheme == protocol)
    return SchemeMatchingResult::MatchingExact;

  if ((scheme == "http" && protocol == "https") ||
      (scheme == "http" && protocol == "https-so") ||
      (scheme == "ws" && protocol == "wss")) {
    return SchemeMatchingResult::MatchingUpgrade;
  }

  if ((scheme == "http" && protocol == "http-so") ||
      (scheme == "https" && protocol == "https-so")) {
    return SchemeMatchingResult::MatchingExact;
  }

  return SchemeMatchingResult::NotMatching;
}

bool CSPSource::hostMatches(const String& host) const {
  Document* document = m_policy->document();
  bool match;

  bool equalHosts = m_host == host;
  if (m_hostWildcard == HasWildcard) {
    if (m_host.isEmpty()) {
      // host-part = "*"
      match = true;
    } else {
      // host-part = "*." 1*host-char *( "." 1*host-char )
      match = host.endsWith(String("." + m_host), TextCaseUnicodeInsensitive);
    }

    // Chrome used to, incorrectly, match *.x.y to x.y. This was fixed, but
    // the following count measures when a match fails that would have
    // passed the old, incorrect style, in case a lot of sites were
    // relying on that behavior.
    if (document && equalHosts)
      UseCounter::count(*document,
                        UseCounter::CSPSourceWildcardWouldMatchExactHost);
  } else {
    // host-part = 1*host-char *( "." 1*host-char )
    match = equalHosts;
  }

  return match;
}

bool CSPSource::pathMatches(const String& urlPath) const {
  if (m_path.isEmpty() || (m_path == "/" && urlPath.isEmpty()))
    return true;

  String path = decodeURLEscapeSequences(urlPath);

  if (m_path.endsWith("/"))
    return path.startsWith(m_path);

  return path == m_path;
}

CSPSource::PortMatchingResult CSPSource::portMatches(
    int port,
    const String& protocol) const {
  if (m_portWildcard == HasWildcard)
    return PortMatchingResult::MatchingWildcard;

  if (port == m_port) {
    if (port == 0)
      return PortMatchingResult::MatchingWildcard;
    return PortMatchingResult::MatchingExact;
  }

  bool isSchemeHttp;  // needed for detecting an upgrade when the port is 0
  isSchemeHttp = m_scheme.isEmpty() ? m_policy->protocolEqualsSelf("http")
                                    : equalIgnoringCase("http", m_scheme);

  if ((m_port == 80 || (m_port == 0 && isSchemeHttp)) &&
      (port == 443 || (port == 0 && defaultPortForProtocol(protocol) == 443)))
    return PortMatchingResult::MatchingUpgrade;

  if (!port) {
    if (isDefaultPortForProtocol(m_port, protocol))
      return PortMatchingResult::MatchingExact;

    return PortMatchingResult::NotMatching;
  }

  if (!m_port) {
    if (isDefaultPortForProtocol(port, protocol))
      return PortMatchingResult::MatchingExact;

    return PortMatchingResult::NotMatching;
  }

  return PortMatchingResult::NotMatching;
}

bool CSPSource::subsumes(CSPSource* other) const {
  if (schemeMatches(other->m_scheme) == SchemeMatchingResult::NotMatching)
    return false;

  if (other->isSchemeOnly() || isSchemeOnly())
    return isSchemeOnly();

  if ((m_hostWildcard == NoWildcard && other->m_hostWildcard == HasWildcard) ||
      (m_portWildcard == NoWildcard && other->m_portWildcard == HasWildcard)) {
    return false;
  }

  bool hostSubsumes = (m_host == other->m_host || hostMatches(other->m_host));
  bool portSubsumes = (m_portWildcard == HasWildcard) ||
      portMatches(other->m_port, other->m_scheme) != PortMatchingResult::NotMatching;
  bool pathSubsumes = pathMatches(other->m_path);
  return hostSubsumes && portSubsumes && pathSubsumes;
}

bool CSPSource::isSimilar(CSPSource* other) const {
  bool schemesMatch =
      schemeMatches(other->m_scheme) != SchemeMatchingResult::NotMatching
      || other->schemeMatches(m_scheme) != SchemeMatchingResult::NotMatching;
  if (!schemesMatch || isSchemeOnly() || other->isSchemeOnly())
    return schemesMatch;
  bool hostsMatch = (m_host == other->m_host) || hostMatches(other->m_host) ||
                    other->hostMatches(m_host);
  bool portsMatch = (other->m_portWildcard == HasWildcard) ||
      portMatches(other->m_port, other->m_scheme) != PortMatchingResult::NotMatching ||
      other->portMatches(m_port, m_scheme) != PortMatchingResult::NotMatching;
  bool pathsMatch = pathMatches(other->m_path) || other->pathMatches(m_path);
  if (hostsMatch && portsMatch && pathsMatch)
    return true;

  return false;
}

CSPSource* CSPSource::intersect(CSPSource* other) const {
  if (!isSimilar(other))
    return nullptr;

  String scheme = other->schemeMatches(m_scheme) != SchemeMatchingResult::NotMatching ? m_scheme : other->m_scheme;
  if (isSchemeOnly() || other->isSchemeOnly()) {
    const CSPSource* stricter = isSchemeOnly() ? other : this;
    return new CSPSource(m_policy, scheme, stricter->m_host, stricter->m_port,
                         stricter->m_path, stricter->m_hostWildcard,
                         stricter->m_portWildcard);
  }

  String host = m_hostWildcard == NoWildcard ? m_host : other->m_host;
  // Since sources are similar and paths match, pick the longer one.
  String path =
      m_path.length() > other->m_path.length() ? m_path : other->m_path;
  // Choose this port if the other port is empty, has wildcard or is a port for
  // a less secure scheme such as "http" whereas scheme of this is "https", in
  // which case the lengths would differ.
  int port = (other->m_portWildcard == HasWildcard || !other->m_port ||
              m_scheme.length() > other->m_scheme.length())
                 ? m_port
                 : other->m_port;
  WildcardDisposition hostWildcard =
      (m_hostWildcard == HasWildcard) ? other->m_hostWildcard : m_hostWildcard;
  WildcardDisposition portWildcard =
      (m_portWildcard == HasWildcard) ? other->m_portWildcard : m_portWildcard;
  return new CSPSource(m_policy, scheme, host, port, path, hostWildcard,
                       portWildcard);
}

bool CSPSource::isSchemeOnly() const {
  return m_host.isEmpty() && (m_hostWildcard == NoWildcard);
}

bool CSPSource::firstSubsumesSecond(
    const HeapVector<Member<CSPSource>>& listA,
    const HeapVector<Member<CSPSource>>& listB) {
  // Empty vector of CSPSources has an effect of 'none'.
  if (!listA.size() || !listB.size())
    return !listB.size();

  // Walk through all the items in |listB|, ensuring that each is subsumed by at
  // least one item in |listA|. If any item in |listB| is not subsumed, return
  // false.
  for (const auto& sourceB : listB) {
    bool foundMatch = false;
    for (const auto& sourceA : listA) {
      if ((foundMatch = sourceA->subsumes(sourceB)))
        break;
    }
    if (!foundMatch)
      return false;
  }
  return true;
}

WebContentSecurityPolicySourceExpression
CSPSource::exposeForNavigationalChecks() const {
  WebContentSecurityPolicySourceExpression sourceExpression;
  sourceExpression.scheme = m_scheme;
  sourceExpression.host = m_host;
  sourceExpression.isHostWildcard =
      static_cast<WebWildcardDisposition>(m_hostWildcard);
  sourceExpression.port = m_port;
  sourceExpression.isPortWildcard =
      static_cast<WebWildcardDisposition>(m_portWildcard);
  sourceExpression.path = m_path;
  return sourceExpression;
}

DEFINE_TRACE(CSPSource) {
  visitor->trace(m_policy);
}

}  // namespace blink
