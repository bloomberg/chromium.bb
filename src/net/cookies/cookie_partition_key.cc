// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cookies/cookie_partition_key.h"

#include <ostream>
#include <tuple>

#include "base/feature_list.h"
#include "base/stl_util.h"
#include "net/base/features.h"
#include "net/cookies/cookie_constants.h"

namespace net {

CookiePartitionKey::CookiePartitionKey() = default;

CookiePartitionKey::CookiePartitionKey(
    const SchemefulSite& site,
    absl::optional<base::UnguessableToken> nonce)
    : site_(site), nonce_(nonce) {}

CookiePartitionKey::CookiePartitionKey(const GURL& url)
    : site_(SchemefulSite(url)) {}

CookiePartitionKey::CookiePartitionKey(bool from_script)
    : from_script_(from_script) {}

CookiePartitionKey::CookiePartitionKey(const CookiePartitionKey& other) =
    default;

CookiePartitionKey::CookiePartitionKey(CookiePartitionKey&& other) = default;

CookiePartitionKey& CookiePartitionKey::operator=(
    const CookiePartitionKey& other) = default;

CookiePartitionKey& CookiePartitionKey::operator=(CookiePartitionKey&& other) =
    default;

CookiePartitionKey::~CookiePartitionKey() = default;

bool CookiePartitionKey::operator==(const CookiePartitionKey& other) const {
  return site_ == other.site_ && nonce_ == other.nonce_;
}

bool CookiePartitionKey::operator!=(const CookiePartitionKey& other) const {
  return site_ != other.site_ || nonce_ != other.nonce_;
}

bool CookiePartitionKey::operator<(const CookiePartitionKey& other) const {
  return std::tie(site_, nonce_) < std::tie(other.site_, other.nonce_);
}

// static
bool CookiePartitionKey::Serialize(const absl::optional<CookiePartitionKey>& in,
                                   std::string& out) {
  if (!in) {
    out = kEmptyCookiePartitionKey;
    return true;
  }
  if (!in->IsSerializeable())
    return false;
  out = in->site_.GetURL().SchemeIsFile()
            ? in->site_.SerializeFileSiteWithHost()
            : in->site_.Serialize();
  return true;
}

// static
bool CookiePartitionKey::Deserialize(const std::string& in,
                                     absl::optional<CookiePartitionKey>& out) {
  if (in == kEmptyCookiePartitionKey) {
    out = absl::nullopt;
    return true;
  }
  if (!base::FeatureList::IsEnabled(features::kPartitionedCookies))
    return false;
  auto schemeful_site = SchemefulSite::Deserialize(in);
  // SchemfulSite is opaque if the input is invalid.
  if (schemeful_site.opaque())
    return false;
  out = absl::make_optional(CookiePartitionKey(schemeful_site, absl::nullopt));
  return true;
}

absl::optional<CookiePartitionKey> CookiePartitionKey::FromNetworkIsolationKey(
    const NetworkIsolationKey& network_isolation_key,
    const SchemefulSite* first_party_set_owner_site) {
  if (!base::FeatureList::IsEnabled(features::kPartitionedCookies))
    return absl::nullopt;

  // TODO(crbug.com/1225444): Check if the top frame site is in a First-Party
  // Set or if it is an extension URL.
  const SchemefulSite* partition_key_site =
      first_party_set_owner_site
          ? first_party_set_owner_site
          : base::OptionalOrNullptr(network_isolation_key.GetTopFrameSite());
  if (!partition_key_site)
    return absl::nullopt;

  absl::optional<base::UnguessableToken> nonce =
      network_isolation_key.GetNonce();
  return absl::make_optional(
      net::CookiePartitionKey(*partition_key_site, nonce));
}

bool CookiePartitionKey::IsSerializeable() const {
  if (!base::FeatureList::IsEnabled(features::kPartitionedCookies))
    return false;
  // We should not try to serialize a partition key created by a renderer.
  DCHECK(!from_script_);
  return !site_.opaque() && !nonce_.has_value();
}

std::ostream& operator<<(std::ostream& os, const CookiePartitionKey& cpk) {
  os << cpk.site();
  return os;
}

}  // namespace net
