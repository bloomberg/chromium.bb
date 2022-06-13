// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_COMMON_PREF_NAMES_H_
#define COMPONENTS_FEED_CORE_COMMON_PREF_NAMES_H_

#include "build/build_config.h"

class PrefRegistrySimple;
class PrefService;

namespace feed {

namespace prefs {

// The pref name for the feed host override.
extern const char kHostOverrideHost[];
// The pref name for the feed host override auth token.
extern const char kHostOverrideBlessNonce[];

// The pref name for the bit that determines whether the conditions are reached
// to enable the upload of click and view actions in the feed with the notice
// card when using the feature kInterestFeedConditionalClickAndViewActionUpload.
// This is for when the privacy notice card is at the second position in the
// feed.
extern const char kHasReachedClickAndViewActionsUploadConditions[];

// The pref name for the bit that determines whether the notice card was present
// in the feed in the last fetch of content. The notice card is considered as
// present by default to make sure that the upload of click and view actions
// doesn't take place when the notice card is present but has not yet been
// detected.
extern const char kLastFetchHadNoticeCard[];

// The pref name for the bit that determines whether logging is enabled for the
// feed in the last fetch of content. iOS only.
#if defined(OS_IOS)
extern const char kLastFetchHadLoggingEnabled[];
#endif  // defined(OS_IOS)

// The pref name for the counter for the number of views on the privacy notice
// card.
extern const char kNoticeCardViewsCount[];

// The pref name for the counter for the number of clicks on the privacy notice
// card.
extern const char kNoticeCardClicksCount[];

// The following prefs are used only by v2.

// The pref name for the request throttler counts.
extern const char kThrottlerRequestCountListPrefName[];
// The pref name for the request throttler's last request time.
extern const char kThrottlerLastRequestTime[];
// The pref name for storing |DebugStreamData|.
extern const char kDebugStreamData[];
// The pref names for storing the request schedules.
extern const char kRequestSchedule[];
extern const char kWebFeedsRequestSchedule[];
// The pref name for storing the persistent metrics data.
extern const char kMetricsData[];
// The pref name for storing client instance id.
extern const char kClientInstanceId[];
// The pref name for the Discover API endpoint override.
extern const char kDiscoverAPIEndpointOverride[];
// The pref name for storing the server experiments the client is in.
extern const char kExperiments[];
// If set to true, the WebFeed follow intro bypasses some gates and only checks
// for recommended and scroll status.
extern const char kEnableWebFeedFollowIntroDebug[];
// Random bytes used in generating reliability logging ID.
extern const char kReliabilityLoggingIdSalt[];
// Whether the Feed may have data stored, which should be deleted if the Feed
// is ever turned off.
extern const char kHasStoredData[];
// `feed::ContentOrder` of the Web feed.
extern const char kWebFeedContentOrder[];
// The last feed type that the user was viewing.
extern const char kLastSeenFeedType[];
// The pref name for the keys of the notices.
extern const char kNoticeStates[];

}  // namespace prefs

void RegisterProfilePrefs(PrefRegistrySimple* registry);
void MigrateObsoleteProfilePrefsFeb_2021(PrefService* prefs);
void MigrateObsoleteProfilePrefsJune_2021(PrefService* prefs);

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_COMMON_PREF_NAMES_H_
