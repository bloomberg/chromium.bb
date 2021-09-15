// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/feed_feature_list.h"
#include "components/feed/buildflags.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"

namespace feed {

const base::Feature kInterestFeedContentSuggestions{
    "InterestFeedContentSuggestions", base::FEATURE_ENABLED_BY_DEFAULT};
// InterestFeedV2 takes precedence over InterestFeedContentSuggestions.
// InterestFeedV2 is cached in ChromeCachedFlags. If the default value here is
// changed, please update the cached one's default value in CachedFeatureFlags.
const base::Feature kInterestFeedV2{"InterestFeedV2",
                                    base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kInterestFeedV2Autoplay{"InterestFeedV2Autoplay",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kInterestFeedV2Hearts{"InterestFeedV2Hearts",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kInterestFeedV2Scrolling{"InterestFeedV2Scrolling",
                                             base::FEATURE_ENABLED_BY_DEFAULT};

const base::FeatureParam<std::string> kDisableTriggerTypes{
    &kInterestFeedContentSuggestions, "disable_trigger_types", ""};
const base::FeatureParam<int> kSuppressRefreshDurationMinutes{
    &kInterestFeedContentSuggestions, "suppress_refresh_duration_minutes", 30};
const base::FeatureParam<int> kTimeoutDurationSeconds{
    &kInterestFeedContentSuggestions, "timeout_duration_seconds", 30};
const base::FeatureParam<bool> kThrottleBackgroundFetches{
    &kInterestFeedContentSuggestions, "throttle_background_fetches", true};
const base::FeatureParam<bool> kOnlySetLastRefreshAttemptOnSuccess{
    &kInterestFeedContentSuggestions,
    "only_set_last_refresh_attempt_on_success", true};

const base::Feature kInterestFeedV1ClicksAndViewsConditionalUpload{
    "InterestFeedV1ClickAndViewActionsConditionalUpload",
    base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kInterestFeedV2ClicksAndViewsConditionalUpload{
    "InterestFeedV2ClickAndViewActionsConditionalUpload",
    base::FEATURE_DISABLED_BY_DEFAULT};

#if defined(OS_IOS)
const base::Feature kInterestFeedNoticeCardAutoDismiss{
    "InterestFeedNoticeCardAutoDismiss", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

const base::Feature kInterestFeedSpinnerAlwaysAnimate{
    "InterestFeedSpinnerAlwaysAnimate", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kWebFeed{"WebFeed", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kDiscoFeedEndpoint{"DiscoFeedEndpoint",
                                       base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kXsurfaceMetricsReporting{
    "XsurfaceMetricsReporting", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kReliabilityLogging{"FeedReliabilityLogging",
                                        base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kFeedInteractiveRefresh{"FeedInteractiveRefresh",
                                            base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kFeedImageMemoryCacheSizePercentage{
    "FeedImageMemoryCacheSizePercentage", base::FEATURE_DISABLED_BY_DEFAULT};

const char kDefaultReferrerUrl[] = "https://www.google.com/";

std::string GetFeedReferrerUrl() {
  const base::Feature* feature = base::FeatureList::IsEnabled(kInterestFeedV2)
                                     ? &kInterestFeedV2
                                     : &kInterestFeedContentSuggestions;
  std::string referrer =
      base::GetFieldTrialParamValueByFeature(*feature, "referrer_url");
  return referrer.empty() ? kDefaultReferrerUrl : referrer;
}

}  // namespace feed
