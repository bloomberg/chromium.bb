// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_FEED_FEATURE_LIST_H_
#define COMPONENTS_FEED_FEED_FEATURE_LIST_H_

#include <string>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace feed {

extern const base::Feature kInterestFeedContentSuggestions;

extern const base::FeatureParam<std::string> kDisableTriggerTypes;
extern const base::FeatureParam<int> kSuppressRefreshDurationMinutes;
extern const base::FeatureParam<int> kTimeoutDurationSeconds;
extern const base::FeatureParam<bool> kThrottleBackgroundFetches;
extern const base::FeatureParam<bool> kOnlySetLastRefreshAttemptOnSuccess;

extern const base::Feature kInterestFeedNotifications;

extern const base::Feature kInterestFeedFeedback;

// Indicates if user card clicks and views in Chrome's feed should be reported
// for personalization. Also enables the feed header menu to manage the feed.
extern const base::Feature kReportFeedUserActions;

extern const base::Feature kInterestFeedV2;

}  // namespace feed

#endif  // COMPONENTS_FEED_FEED_FEATURE_LIST_H_
