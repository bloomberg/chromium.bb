// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/navigation/navigation_policy.h"

#include "base/command_line.h"
#include "base/metrics/histogram_macros.h"
#include "base/system/sys_info.h"
#include "services/network/public/cpp/features.h"
#include "third_party/blink/public/common/features.h"

namespace blink {

namespace {

void LogPerPolicyApplied(NavigationDownloadType type) {
  UMA_HISTOGRAM_ENUMERATION("Navigation.DownloadPolicy.LogPerPolicyApplied",
                            type);
}

void LogArbitraryPolicyPerDownload(NavigationDownloadType type) {
  UMA_HISTOGRAM_ENUMERATION(
      "Navigation.DownloadPolicy.LogArbitraryPolicyPerDownload", type);
}
}  // namespace

NavigationDownloadPolicy::NavigationDownloadPolicy() = default;
NavigationDownloadPolicy::~NavigationDownloadPolicy() = default;
NavigationDownloadPolicy::NavigationDownloadPolicy(
    const NavigationDownloadPolicy&) = default;

void NavigationDownloadPolicy::SetAllowed(NavigationDownloadType type) {
  DCHECK(type != NavigationDownloadType::kDefaultAllow);
  observed_types.set(static_cast<size_t>(type));
}

void NavigationDownloadPolicy::SetDisallowed(NavigationDownloadType type) {
  DCHECK(type != NavigationDownloadType::kDefaultAllow);
  observed_types.set(static_cast<size_t>(type));
  disallowed_types.set(static_cast<size_t>(type));
}

bool NavigationDownloadPolicy::IsType(NavigationDownloadType type) const {
  DCHECK(type != NavigationDownloadType::kDefaultAllow);
  return observed_types.test(static_cast<size_t>(type));
}

ResourceInterceptPolicy NavigationDownloadPolicy::GetResourceInterceptPolicy()
    const {
  if (disallowed_types.test(
          static_cast<size_t>(NavigationDownloadType::kSandbox)) ||
      disallowed_types.test(
          static_cast<size_t>(NavigationDownloadType::kOpenerCrossOrigin)) ||
      disallowed_types.test(
          static_cast<size_t>(NavigationDownloadType::kAdFrame)) ||
      disallowed_types.test(
          static_cast<size_t>(NavigationDownloadType::kAdFrameNoGesture))) {
    return ResourceInterceptPolicy::kAllowPluginOnly;
  }
  return disallowed_types.any() ? ResourceInterceptPolicy::kAllowNone
                                : ResourceInterceptPolicy::kAllowAll;
}

bool NavigationDownloadPolicy::IsDownloadAllowed() const {
  return disallowed_types.none();
}

void NavigationDownloadPolicy::RecordHistogram() const {
  if (observed_types.none()) {
    LogPerPolicyApplied(NavigationDownloadType::kDefaultAllow);
    LogArbitraryPolicyPerDownload(NavigationDownloadType::kDefaultAllow);
    return;
  }

  bool first_type_seen = false;
  for (size_t i = 0; i < observed_types.size(); ++i) {
    if (observed_types.test(i)) {
      NavigationDownloadType policy = static_cast<NavigationDownloadType>(i);
      DCHECK(policy != NavigationDownloadType::kDefaultAllow);
      LogPerPolicyApplied(policy);
      if (!first_type_seen) {
        LogArbitraryPolicyPerDownload(policy);
        first_type_seen = true;
      }
    }
  }
  DCHECK(first_type_seen);
}

void NavigationDownloadPolicy::ApplyDownloadFramePolicy(
    bool is_opener_navigation,
    bool has_gesture,
    bool can_access_current_origin,
    bool has_download_sandbox_flag,
    bool is_blocking_downloads_in_sandbox_enabled,
    bool from_ad) {
  if (!has_gesture)
    SetAllowed(NavigationDownloadType::kNoGesture);

  // Disallow downloads on an opener if the requestor is cross origin.
  // See crbug.com/632514.
  if (is_opener_navigation && !can_access_current_origin) {
    SetDisallowed(NavigationDownloadType::kOpenerCrossOrigin);
  }

  if (has_download_sandbox_flag) {
    if (is_blocking_downloads_in_sandbox_enabled) {
      SetDisallowed(NavigationDownloadType::kSandbox);
    } else {
      SetAllowed(NavigationDownloadType::kSandbox);
    }
  }

  if (from_ad) {
    SetAllowed(NavigationDownloadType::kAdFrame);
    if (!has_gesture) {
      if (base::FeatureList::IsEnabled(
              features::kBlockingDownloadsInAdFrameWithoutUserActivation)) {
        SetDisallowed(NavigationDownloadType::kAdFrameNoGesture);
      } else {
        SetAllowed(NavigationDownloadType::kAdFrameNoGesture);
      }
    }
  }

  blocking_downloads_in_sandbox_enabled =
      is_blocking_downloads_in_sandbox_enabled;
}

}  // namespace blink
