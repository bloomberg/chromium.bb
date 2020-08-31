// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_METRICS_REPORTER_H_
#define COMPONENTS_FEED_CORE_V2_METRICS_REPORTER_H_

#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "components/feed/core/v2/enums.h"
#include "components/feed/core/v2/feed_stream.h"

namespace base {
class TickClock;
}  // namespace base
namespace feed {
namespace internal {
// This enum is used for a UMA histogram. Keep in sync with FeedEngagementType
// in enums.xml.
enum class FeedEngagementType {
  kFeedEngaged = 0,
  kFeedEngagedSimple = 1,
  kFeedInteracted = 2,
  kFeedScrolled = 3,
  kMaxValue = FeedEngagementType::kFeedScrolled,
};

// This enum must match FeedUserActionType in enums.xml.
// Note that most of these have a corresponding UserMetricsAction reported here.
// Exceptions are described below.
enum class FeedUserActionType {
  kTappedOnCard = 0,
  // This is not an actual user action, so there will be no UserMetricsAction
  // reported for this.
  kShownCard = 1,
  kTappedSendFeedback = 2,
  kTappedLearnMore = 3,
  kTappedHideStory = 4,
  kTappedNotInterestedIn = 5,
  kTappedManageInterests = 6,
  kTappedDownload = 7,
  kTappedOpenInNewTab = 8,
  kOpenedContextMenu = 9,
  // User action not reported here. See Suggestions.SurfaceVisible.
  kOpenedFeedSurface = 10,
  kTappedOpenInNewIncognitoTab = 11,
  kMaxValue = kTappedOpenInNewIncognitoTab,
};

}  // namespace internal

// Reports UMA metrics for feed.
// Note this is inherited only for testing.
class MetricsReporter {
 public:
  explicit MetricsReporter(const base::TickClock* clock);
  virtual ~MetricsReporter();
  MetricsReporter(const MetricsReporter&) = delete;
  MetricsReporter& operator=(const MetricsReporter&) = delete;

  // User interactions. See |FeedStreamApi| for definitions.

  virtual void ContentSliceViewed(SurfaceId surface_id, int index_in_stream);
  void OpenAction(int index_in_stream);
  void OpenInNewTabAction(int index_in_stream);
  void OpenInNewIncognitoTabAction();
  void SendFeedbackAction();
  void LearnMoreAction();
  void DownloadAction();
  void NavigationStarted();
  void PageLoaded();
  void RemoveAction();
  void NotInterestedInAction();
  void ManageInterestsAction();
  void ContextMenuOpened();
  // Indicates the user scrolled the feed by |distance_dp| and then stopped
  // scrolling.
  void StreamScrolled(int distance_dp);

  // Called when the Feed surface is opened and closed.
  void SurfaceOpened(SurfaceId surface_id);
  void SurfaceClosed(SurfaceId surface_id);

  // Network metrics.

  static void NetworkRequestComplete(NetworkRequestType type,
                                     int http_status_code);

  // Stream events.

  virtual void OnLoadStream(LoadStreamStatus load_from_store_status,
                            LoadStreamStatus final_status);
  virtual void OnBackgroundRefresh(LoadStreamStatus final_status);
  void OnLoadMoreBegin(SurfaceId surface_id);
  virtual void OnLoadMore(LoadStreamStatus final_status);
  virtual void OnClearAll(base::TimeDelta time_since_last_clear);
  // Called each time the surface receives new content.
  void SurfaceReceivedContent(SurfaceId surface_id);
  // Called when Chrome is entering the background.
  void OnEnterBackground();

 private:
  base::WeakPtr<MetricsReporter> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }
  void CardOpenBegin();
  void CardOpenTimeout(base::TimeTicks start_ticks);
  void ReportCardOpenEndIfNeeded(bool success);
  void RecordEngagement(int scroll_distance_dp, bool interacted);
  void RecordInteraction();
  void ReportOpenFeedIfNeeded(SurfaceId surface_id, bool success);
  void ReportGetMoreIfNeeded(SurfaceId surface_id, bool success);
  void FinalizeMetrics();

  const base::TickClock* clock_;

  base::TimeTicks visit_start_time_;
  bool engaged_simple_reported_ = false;
  bool engaged_reported_ = false;
  bool scrolled_reported_ = false;
  // The time a surface was opened, for surfaces still waiting for content.
  std::map<SurfaceId, base::TimeTicks> surfaces_waiting_for_content_;
  // The time a surface requested more content, for surfaces still waiting for
  // more content.
  std::map<SurfaceId, base::TimeTicks> surfaces_waiting_for_more_content_;

  // Tracking ContentSuggestions.Feed.UserJourney.OpenCard.*:
  // We assume at most one card is opened at a time. The time the card was
  // tapped is stored here. Upon timeout, another open attempt, or
  // |ChromeStopping()|, the open is considered failed. Otherwise, if the
  // loading the page succeeds, the open is considered successful.
  base::Optional<base::TimeTicks> pending_open_;

  base::WeakPtrFactory<MetricsReporter> weak_ptr_factory_{this};
};
}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_METRICS_REPORTER_H_
