// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/core/unsafe_resource.h"

#include "components/safe_browsing/core/db/util.h"

namespace security_interstitials {

UnsafeResource::UnsafeResource()
    : is_subresource(false),
      is_subframe(false),
      threat_type(safe_browsing::SB_THREAT_TYPE_SAFE),
      resource_type(safe_browsing::ResourceType::kMainFrame),
      threat_source(safe_browsing::ThreatSource::UNKNOWN),
      is_delayed_warning(false) {}

UnsafeResource::UnsafeResource(const UnsafeResource& other) = default;

UnsafeResource::~UnsafeResource() = default;

bool UnsafeResource::IsMainPageLoadBlocked() const {
  // Subresource hits cannot happen until after main page load is committed.
  if (is_subresource)
    return false;

  switch (threat_type) {
    // Client-side phishing/malware detection interstitials never block the main
    // frame load, since they happen after the page is finished loading.
    case safe_browsing::SB_THREAT_TYPE_URL_CLIENT_SIDE_PHISHING:
    case safe_browsing::SB_THREAT_TYPE_URL_CLIENT_SIDE_MALWARE:
    // Malicious ad activity reporting happens in the background.
    case safe_browsing::SB_THREAT_TYPE_BLOCKED_AD_POPUP:
    case safe_browsing::SB_THREAT_TYPE_BLOCKED_AD_REDIRECT:
    // Ad sampling happens in the background.
    case safe_browsing::SB_THREAT_TYPE_AD_SAMPLE:
    // Chrome SAVED password reuse warning happens after the page is finished
    // loading.
    case safe_browsing::SB_THREAT_TYPE_SAVED_PASSWORD_REUSE:
    // Chrome GAIA signed in and syncing password reuse warning happens after
    // the page is finished loading.
    case safe_browsing::SB_THREAT_TYPE_SIGNED_IN_SYNC_PASSWORD_REUSE:
    // Chrome GAIA signed in and non-syncing password reuse warning happens
    // after the page is finished loading.
    case safe_browsing::SB_THREAT_TYPE_SIGNED_IN_NON_SYNC_PASSWORD_REUSE:
    // Enterprise password reuse warning happens after the page is finished
    // loading.
    case safe_browsing::SB_THREAT_TYPE_ENTERPRISE_PASSWORD_REUSE:
    // Suspicious site collection happens in the background
    case safe_browsing::SB_THREAT_TYPE_SUSPICIOUS_SITE:
      return false;

    default:
      break;
  }

  return true;
}

}  // namespace security_interstitials
