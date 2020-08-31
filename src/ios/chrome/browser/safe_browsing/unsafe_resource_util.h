// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFE_BROWSING_UNSAFE_RESOURCE_UTIL_H_
#define IOS_CHROME_BROWSER_SAFE_BROWSING_UNSAFE_RESOURCE_UTIL_H_

#include "components/security_interstitials/core/base_safe_browsing_error_ui.h"
#include "components/security_interstitials/core/unsafe_resource.h"

// Runs |resource|'s callback on the appropriate thread.
void RunUnsafeResourceCallback(
    const security_interstitials::UnsafeResource& resource,
    bool proceed,
    bool showed_interstitial);

// Returns the interstitial reason for |resource|.
security_interstitials::BaseSafeBrowsingErrorUI::SBInterstitialReason
GetUnsafeResourceInterstitialReason(
    const security_interstitials::UnsafeResource& resource);

// Returns the metric prefix for error pages for |resource|.
std::string GetUnsafeResourceMetricPrefix(
    const security_interstitials::UnsafeResource& resource);

#endif  // IOS_CHROME_BROWSER_SAFE_BROWSING_UNSAFE_RESOURCE_UTIL_H_
