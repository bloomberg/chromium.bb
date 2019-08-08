// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/cors/origin_access_entry.h"

#include "base/strings/string_util.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "services/network/public/mojom/cors_origin_pattern.mojom.h"
#include "url/origin.h"
#include "url/url_util.h"

namespace network {

namespace cors {

namespace {

bool IsSubdomainOfHost(const std::string& subdomain, const std::string& host) {
  if (subdomain.length() <= host.length())
    return false;

  if (subdomain[subdomain.length() - host.length() - 1] != '.')
    return false;

  if (!base::EndsWith(subdomain, host, base::CompareCase::SENSITIVE))
    return false;

  return true;
}

}  // namespace

OriginAccessEntry::OriginAccessEntry(
    const std::string& protocol,
    const std::string& host,
    const mojom::CorsOriginAccessMatchMode mode,
    const mojom::CorsOriginAccessMatchPriority priority)
    : protocol_(protocol),
      host_(host),
      mode_(mode),
      priority_(priority),
      host_is_ip_address_(url::HostIsIPAddress(host)),
      host_is_public_suffix_(false) {
  if (host_is_ip_address_)
    return;

  // Look for top-level domains, either with or without an additional dot.
  // Call sites in Blink passes some things that aren't technically hosts like
  // "*.foo", so use the permissive variant.
  size_t public_suffix_length =
      net::registry_controlled_domains::PermissiveGetHostRegistryLength(
          host_, net::registry_controlled_domains::INCLUDE_UNKNOWN_REGISTRIES,
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  if (public_suffix_length == 0)
    public_suffix_length = host_.length();

  if (host_.length() <= public_suffix_length + 1) {
    host_is_public_suffix_ = true;
  } else if (mode_ ==
                 mojom::CorsOriginAccessMatchMode::kAllowRegistrableDomains &&
             public_suffix_length) {
    // The "2" in the next line is 1 for the '.', plus a 1-char minimum label
    // length.
    const size_t dot =
        host_.rfind('.', host_.length() - public_suffix_length - 2);
    if (dot == std::string::npos)
      registrable_domain_ = host_;
    else
      registrable_domain_ = host_.substr(dot + 1);
  }
}

OriginAccessEntry::OriginAccessEntry(OriginAccessEntry&& from) = default;

OriginAccessEntry::MatchResult OriginAccessEntry::MatchesOrigin(
    const url::Origin& origin) const {
  if (protocol_ != origin.scheme())
    return kDoesNotMatchOrigin;

  return MatchesDomain(origin);
}

OriginAccessEntry::MatchResult OriginAccessEntry::MatchesDomain(
    const url::Origin& origin) const {
  // Special case: Include subdomains and empty host means "all hosts, including
  // ip addresses".
  if (mode_ != mojom::CorsOriginAccessMatchMode::kDisallowSubdomains &&
      host_.empty())
    return kMatchesOrigin;

  // Exact match.
  if (host_ == origin.host())
    return kMatchesOrigin;

  // Don't try to do subdomain matching on IP addresses.
  if (host_is_ip_address_)
    return kDoesNotMatchOrigin;

  // Match subdomains.
  switch (mode_) {
    case mojom::CorsOriginAccessMatchMode::kDisallowSubdomains:
      return kDoesNotMatchOrigin;

    case mojom::CorsOriginAccessMatchMode::kAllowSubdomains:
      if (!IsSubdomainOfHost(origin.host(), host_))
        return kDoesNotMatchOrigin;
      break;

    case mojom::CorsOriginAccessMatchMode::kAllowRegistrableDomains:
      // Fall back to a simple subdomain check if no registrable domain could
      // be found:
      if (registrable_domain_.empty()) {
        if (!IsSubdomainOfHost(origin.host(), host_))
          return kDoesNotMatchOrigin;
      } else if (registrable_domain_ != origin.host() &&
                 !IsSubdomainOfHost(origin.host(), registrable_domain_)) {
        return kDoesNotMatchOrigin;
      }
      break;
  };

  if (host_is_public_suffix_)
    return kMatchesOriginButIsPublicSuffix;

  return kMatchesOrigin;
}

mojo::InlinedStructPtr<mojom::CorsOriginPattern>
OriginAccessEntry::CreateCorsOriginPattern() const {
  return mojom::CorsOriginPattern::New(protocol_, host_, mode_, priority_);
}

}  // namespace cors

}  // namespace network
