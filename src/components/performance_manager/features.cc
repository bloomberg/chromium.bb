// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This header contains field trial and variations definitions for policies,
// mechanisms and features in the performance_manager component.

#include "components/performance_manager/public/features.h"

#include "base/metrics/field_trial_params.h"

namespace performance_manager {
namespace features {

const base::Feature kTabLoadingFrameNavigationThrottles{
    "TabLoadingFrameNavigationThrottles", base::FEATURE_DISABLED_BY_DEFAULT};

// Parameters associated with the "TabLoadingFrameNavigationThrottles"
// feature.
const base::FeatureParam<int> kMinimumThrottleTimeoutMilliseconds = {
    &kTabLoadingFrameNavigationThrottles, "MinimumThrottleTimeoutMilliseconds",
    1000};
// This defaults to the 99th %ile of LargestContentfulPaint (LCP).
const base::FeatureParam<int> kMaximumThrottleTimeoutMilliseconds = {
    &kTabLoadingFrameNavigationThrottles, "MaximumThrottleTimeoutMilliseconds",
    40000};

TabLoadingFrameNavigationThrottlesParams::
    TabLoadingFrameNavigationThrottlesParams() = default;

TabLoadingFrameNavigationThrottlesParams::
    ~TabLoadingFrameNavigationThrottlesParams() = default;

// static
TabLoadingFrameNavigationThrottlesParams
TabLoadingFrameNavigationThrottlesParams::GetParams() {
  TabLoadingFrameNavigationThrottlesParams params;
  params.minimum_throttle_timeout = base::TimeDelta::FromMilliseconds(
      kMinimumThrottleTimeoutMilliseconds.Get());
  params.maximum_throttle_timeout = base::TimeDelta::FromMilliseconds(
      kMaximumThrottleTimeoutMilliseconds.Get());
  return params;
}

}  // namespace features
}  // namespace performance_manager
