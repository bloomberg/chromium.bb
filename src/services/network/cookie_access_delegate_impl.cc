// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/cookie_access_delegate_impl.h"

#include <set>

#include "base/callback_forward.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "net/base/schemeful_site.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_util.h"
#include "net/cookies/first_party_set_metadata.h"
#include "services/network/first_party_sets/first_party_sets.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace network {

CookieAccessDelegateImpl::CookieAccessDelegateImpl(
    mojom::CookieAccessDelegateType type,
    FirstPartySets* const first_party_sets,
    const CookieSettings* cookie_settings)
    : type_(type),
      cookie_settings_(cookie_settings),
      first_party_sets_(first_party_sets) {
  if (type == mojom::CookieAccessDelegateType::USE_CONTENT_SETTINGS) {
    DCHECK(cookie_settings);
  }
}

CookieAccessDelegateImpl::~CookieAccessDelegateImpl() = default;

bool CookieAccessDelegateImpl::ShouldTreatUrlAsTrustworthy(
    const GURL& url) const {
  return IsUrlPotentiallyTrustworthy(url);
}

net::CookieAccessSemantics CookieAccessDelegateImpl::GetAccessSemantics(
    const net::CanonicalCookie& cookie) const {
  switch (type_) {
    case mojom::CookieAccessDelegateType::ALWAYS_LEGACY:
      return net::CookieAccessSemantics::LEGACY;
    case mojom::CookieAccessDelegateType::ALWAYS_NONLEGACY:
      return net::CookieAccessSemantics::NONLEGACY;
    case mojom::CookieAccessDelegateType::USE_CONTENT_SETTINGS:
      return cookie_settings_->GetCookieAccessSemanticsForDomain(
          cookie.Domain());
  }
}

bool CookieAccessDelegateImpl::ShouldIgnoreSameSiteRestrictions(
    const GURL& url,
    const net::SiteForCookies& site_for_cookies) const {
  if (cookie_settings_) {
    return cookie_settings_->ShouldIgnoreSameSiteRestrictions(url,
                                                              site_for_cookies);
  }
  return false;
}

absl::optional<net::FirstPartySetMetadata>
CookieAccessDelegateImpl::ComputeFirstPartySetMetadataMaybeAsync(
    const net::SchemefulSite& site,
    const net::SchemefulSite* top_frame_site,
    const std::set<net::SchemefulSite>& party_context,
    base::OnceCallback<void(net::FirstPartySetMetadata)> callback) const {
  if (!first_party_sets_)
    return {net::FirstPartySetMetadata()};
  return first_party_sets_->ComputeMetadata(site, top_frame_site, party_context,
                                            std::move(callback));
}

absl::optional<FirstPartySets::OwnerResult>
CookieAccessDelegateImpl::FindFirstPartySetOwner(
    const net::SchemefulSite& site,
    base::OnceCallback<void(FirstPartySets::OwnerResult)> callback) const {
  if (!first_party_sets_)
    return {absl::nullopt};
  return first_party_sets_->FindOwner(site, std::move(callback));
}

absl::optional<FirstPartySets::OwnersResult>
CookieAccessDelegateImpl::FindFirstPartySetOwners(
    const base::flat_set<net::SchemefulSite>& sites,
    base::OnceCallback<void(FirstPartySets::OwnersResult)> callback) const {
  if (!first_party_sets_)
    return {{}};
  return first_party_sets_->FindOwners(sites, std::move(callback));
}

absl::optional<FirstPartySets::SetsByOwner>
CookieAccessDelegateImpl::RetrieveFirstPartySets(
    base::OnceCallback<void(FirstPartySets::SetsByOwner)> callback) const {
  if (!first_party_sets_)
    return {{}};
  return first_party_sets_->Sets(std::move(callback));
}

}  // namespace network
