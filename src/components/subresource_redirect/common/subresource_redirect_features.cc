// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_redirect/common/subresource_redirect_features.h"

#include "base/metrics/field_trial_params.h"
#include "base/system/sys_info.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

namespace subresource_redirect {

namespace {

// This default low memory threshold is chosen as the default threshold for
// Android in
// SiteIsolationPolicy::ShouldDisableSiteIsolationDueToMemoryThreshold(). This
// is the threshold for enabling the site-isolation preloaded list and
// populating the password entered sites. So, the subresource redirect feature
// should not compress the devices below this threshold.
constexpr int kDefaultLowMemoryThresholdMb = 1900;

// The default origin for the LitePages.
constexpr char kDefaultLitePageOrigin[] = "https://litepages.googlezip.net/";

bool IsSubresourceRedirectEnabled() {
  return base::FeatureList::IsEnabled(blink::features::kSubresourceRedirect);
}

}  // namespace

bool ShouldEnablePublicImageHintsBasedCompression() {
  bool is_enabled = IsSubresourceRedirectEnabled() &&
                    base::GetFieldTrialParamByFeatureAsBool(
                        blink::features::kSubresourceRedirect,
                        "enable_public_image_hints_based_compression", true);
  // Only one of the public image hints or login and robots based image
  // compression should be active.
  DCHECK(!is_enabled || !ShouldEnableLoginRobotsCheckedImageCompression());
  return is_enabled;
}

bool ShouldEnableLoginRobotsCheckedImageCompression() {
  bool is_enabled = IsSubresourceRedirectEnabled() &&
                    base::GetFieldTrialParamByFeatureAsBool(
                        blink::features::kSubresourceRedirect,
                        "enable_login_robots_based_compression", false);
  // Only one of the public image hints or login and robots based image
  // compression should be active.
  DCHECK(!is_enabled || !ShouldEnablePublicImageHintsBasedCompression());
  if (is_enabled && !base::GetFieldTrialParamByFeatureAsBool(
                        blink::features::kSubresourceRedirect,
                        "enable_login_robots_for_low_memory", false)) {
    return base::SysInfo::AmountOfPhysicalMemoryMB() >
           base::GetFieldTrialParamByFeatureAsInt(
               blink::features::kSubresourceRedirect,
               "login_robots_low_memory_threshold_mb",
               kDefaultLowMemoryThresholdMb);
  }
  return is_enabled;
}

bool ShouldRecordLoginRobotsCheckedSrcVideoMetrics() {
  return base::FeatureList::IsEnabled(
      blink::features::kSubresourceRedirectSrcVideo);
}

// Should the subresource be redirected to its compressed version. This returns
// false if only coverage metrics need to be recorded and actual redirection
// should not happen.
bool ShouldCompressRedirectSubresource() {
  return base::FeatureList::IsEnabled(blink::features::kSubresourceRedirect) &&
         base::GetFieldTrialParamByFeatureAsBool(
             blink::features::kSubresourceRedirect,
             "enable_subresource_server_redirect", true);
}

bool ShouldEnableRobotsRulesFetching() {
  return ShouldEnableLoginRobotsCheckedImageCompression() ||
         ShouldRecordLoginRobotsCheckedSrcVideoMetrics();
}

url::Origin GetSubresourceRedirectOrigin() {
  auto lite_page_subresource_origin = base::GetFieldTrialParamValueByFeature(
      blink::features::kSubresourceRedirect, "lite_page_subresource_origin");
  if (lite_page_subresource_origin.empty())
    return url::Origin::Create(GURL(kDefaultLitePageOrigin));
  return url::Origin::Create(GURL(lite_page_subresource_origin));
}

}  // namespace subresource_redirect
