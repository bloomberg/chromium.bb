// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/csp/CSPSource.h"

#include "core/frame/UseCounter.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/KnownPorts.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

CSPSource::CSPSource(ContentSecurityPolicy* policy,
                     const String& scheme,
                     const String& host,
                     int port,
                     const String& path,
                     WildcardDisposition host_wildcard,
                     WildcardDisposition port_wildcard)
    : policy_(policy),
      scheme_(scheme.DeprecatedLower()),
      host_(host),
      port_(port),
      path_(path),
      host_wildcard_(host_wildcard),
      port_wildcard_(port_wildcard) {}

bool CSPSource::Matches(const KURL& url,
                        ResourceRequest::RedirectStatus redirect_status) const {
  SchemeMatchingResult schemes_match = SchemeMatches(url.Protocol());
  if (schemes_match == SchemeMatchingResult::kNotMatching)
    return false;
  if (IsSchemeOnly())
    return true;
  bool paths_match = (redirect_status == RedirectStatus::kFollowedRedirect) ||
                     PathMatches(url.GetPath());
  PortMatchingResult ports_match = PortMatches(url.Port(), url.Protocol());

  // if either the scheme or the port would require an upgrade (e.g. from http
  // to https) then check that both of them can upgrade to ensure that we don't
  // run into situations where we only upgrade the port but not the scheme or
  // viceversa
  if ((RequiresUpgrade(schemes_match) || (RequiresUpgrade(ports_match))) &&
      (!CanUpgrade(schemes_match) || !CanUpgrade(ports_match))) {
    return false;
  }

  return HostMatches(url.Host()) &&
         ports_match != PortMatchingResult::kNotMatching && paths_match;
}

CSPSource::SchemeMatchingResult CSPSource::SchemeMatches(
    const String& protocol) const {
  DCHECK_EQ(protocol, protocol.DeprecatedLower());
  const String& scheme =
      (scheme_.IsEmpty() ? policy_->GetSelfProtocol() : scheme_);

  if (scheme == protocol)
    return SchemeMatchingResult::kMatchingExact;

  if ((scheme == "http" && protocol == "https") ||
      (scheme == "http" && protocol == "https-so") ||
      (scheme == "ws" && protocol == "wss")) {
    return SchemeMatchingResult::kMatchingUpgrade;
  }

  if ((scheme == "http" && protocol == "http-so") ||
      (scheme == "https" && protocol == "https-so")) {
    return SchemeMatchingResult::kMatchingExact;
  }

  return SchemeMatchingResult::kNotMatching;
}

bool CSPSource::HostMatches(const String& host) const {
  Document* document = policy_->GetDocument();
  bool match;

  bool equal_hosts = host_ == host;
  if (host_wildcard_ == kHasWildcard) {
    if (host_.IsEmpty()) {
      // host-part = "*"
      match = true;
    } else {
      // host-part = "*." 1*host-char *( "." 1*host-char )
      match = host.EndsWithIgnoringCase(String("." + host_));
    }

    // Chrome used to, incorrectly, match *.x.y to x.y. This was fixed, but
    // the following count measures when a match fails that would have
    // passed the old, incorrect style, in case a lot of sites were
    // relying on that behavior.
    if (document && equal_hosts) {
      UseCounter::Count(*document,
                        WebFeature::kCSPSourceWildcardWouldMatchExactHost);
    }
  } else {
    // host-part = 1*host-char *( "." 1*host-char )
    match = equal_hosts;
  }

  return match;
}

bool CSPSource::PathMatches(const String& url_path) const {
  if (path_.IsEmpty() || (path_ == "/" && url_path.IsEmpty()))
    return true;

  String path = DecodeURLEscapeSequences(url_path);

  if (path_.EndsWith("/"))
    return path.StartsWith(path_);

  return path == path_;
}

CSPSource::PortMatchingResult CSPSource::PortMatches(
    int port,
    const String& protocol) const {
  if (port_wildcard_ == kHasWildcard)
    return PortMatchingResult::kMatchingWildcard;

  if (port == port_) {
    if (port == 0)
      return PortMatchingResult::kMatchingWildcard;
    return PortMatchingResult::kMatchingExact;
  }

  bool is_scheme_http;  // needed for detecting an upgrade when the port is 0
  is_scheme_http = scheme_.IsEmpty() ? policy_->ProtocolEqualsSelf("http")
                                     : EqualIgnoringASCIICase("http", scheme_);

  if ((port_ == 80 || (port_ == 0 && is_scheme_http)) &&
      (port == 443 || (port == 0 && DefaultPortForProtocol(protocol) == 443)))
    return PortMatchingResult::kMatchingUpgrade;

  if (!port) {
    if (IsDefaultPortForProtocol(port_, protocol))
      return PortMatchingResult::kMatchingExact;

    return PortMatchingResult::kNotMatching;
  }

  if (!port_) {
    if (IsDefaultPortForProtocol(port, protocol))
      return PortMatchingResult::kMatchingExact;

    return PortMatchingResult::kNotMatching;
  }

  return PortMatchingResult::kNotMatching;
}

bool CSPSource::Subsumes(CSPSource* other) const {
  if (SchemeMatches(other->scheme_) == SchemeMatchingResult::kNotMatching)
    return false;

  if (other->IsSchemeOnly() || IsSchemeOnly())
    return IsSchemeOnly();

  if ((host_wildcard_ == kNoWildcard &&
       other->host_wildcard_ == kHasWildcard) ||
      (port_wildcard_ == kNoWildcard &&
       other->port_wildcard_ == kHasWildcard)) {
    return false;
  }

  bool host_subsumes = (host_ == other->host_ || HostMatches(other->host_));
  bool port_subsumes = (port_wildcard_ == kHasWildcard) ||
                       PortMatches(other->port_, other->scheme_) !=
                           PortMatchingResult::kNotMatching;
  bool path_subsumes = PathMatches(other->path_);
  return host_subsumes && port_subsumes && path_subsumes;
}

bool CSPSource::IsSimilar(CSPSource* other) const {
  bool schemes_match =
      SchemeMatches(other->scheme_) != SchemeMatchingResult::kNotMatching ||
      other->SchemeMatches(scheme_) != SchemeMatchingResult::kNotMatching;
  if (!schemes_match || IsSchemeOnly() || other->IsSchemeOnly())
    return schemes_match;
  bool hosts_match = (host_ == other->host_) || HostMatches(other->host_) ||
                     other->HostMatches(host_);
  bool ports_match =
      (other->port_wildcard_ == kHasWildcard) ||
      PortMatches(other->port_, other->scheme_) !=
          PortMatchingResult::kNotMatching ||
      other->PortMatches(port_, scheme_) != PortMatchingResult::kNotMatching;
  bool paths_match = PathMatches(other->path_) || other->PathMatches(path_);
  if (hosts_match && ports_match && paths_match)
    return true;

  return false;
}

CSPSource* CSPSource::Intersect(CSPSource* other) const {
  if (!IsSimilar(other))
    return nullptr;

  String scheme =
      other->SchemeMatches(scheme_) != SchemeMatchingResult::kNotMatching
          ? scheme_
          : other->scheme_;
  if (IsSchemeOnly() || other->IsSchemeOnly()) {
    const CSPSource* stricter = IsSchemeOnly() ? other : this;
    return new CSPSource(policy_, scheme, stricter->host_, stricter->port_,
                         stricter->path_, stricter->host_wildcard_,
                         stricter->port_wildcard_);
  }

  String host = host_wildcard_ == kNoWildcard ? host_ : other->host_;
  // Since sources are similar and paths match, pick the longer one.
  String path = path_.length() > other->path_.length() ? path_ : other->path_;
  // Choose this port if the other port is empty, has wildcard or is a port for
  // a less secure scheme such as "http" whereas scheme of this is "https", in
  // which case the lengths would differ.
  int port = (other->port_wildcard_ == kHasWildcard || !other->port_ ||
              scheme_.length() > other->scheme_.length())
                 ? port_
                 : other->port_;
  WildcardDisposition host_wildcard =
      (host_wildcard_ == kHasWildcard) ? other->host_wildcard_ : host_wildcard_;
  WildcardDisposition port_wildcard =
      (port_wildcard_ == kHasWildcard) ? other->port_wildcard_ : port_wildcard_;
  return new CSPSource(policy_, scheme, host, port, path, host_wildcard,
                       port_wildcard);
}

bool CSPSource::IsSchemeOnly() const {
  return host_.IsEmpty() && (host_wildcard_ == kNoWildcard);
}

bool CSPSource::FirstSubsumesSecond(
    const HeapVector<Member<CSPSource>>& list_a,
    const HeapVector<Member<CSPSource>>& list_b) {
  // Empty vector of CSPSources has an effect of 'none'.
  if (!list_a.size() || !list_b.size())
    return !list_b.size();

  // Walk through all the items in |listB|, ensuring that each is subsumed by at
  // least one item in |listA|. If any item in |listB| is not subsumed, return
  // false.
  for (const auto& source_b : list_b) {
    bool found_match = false;
    for (const auto& source_a : list_a) {
      if ((found_match = source_a->Subsumes(source_b)))
        break;
    }
    if (!found_match)
      return false;
  }
  return true;
}

WebContentSecurityPolicySourceExpression
CSPSource::ExposeForNavigationalChecks() const {
  WebContentSecurityPolicySourceExpression source_expression;
  source_expression.scheme = scheme_;
  source_expression.host = host_;
  source_expression.is_host_wildcard =
      static_cast<WebWildcardDisposition>(host_wildcard_);
  source_expression.port = port_;
  source_expression.is_port_wildcard =
      static_cast<WebWildcardDisposition>(port_wildcard_);
  source_expression.path = path_;
  return source_expression;
}

DEFINE_TRACE(CSPSource) {
  visitor->Trace(policy_);
}

}  // namespace blink
