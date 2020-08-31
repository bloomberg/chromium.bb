// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/feed/core/v2/metrics_reporter.h"

#include <cmath>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"

namespace feed {
namespace {
using feed::internal::FeedEngagementType;
using feed::internal::FeedUserActionType;
const int kMaxSuggestionsTotal = 50;
// Maximum time to wait before declaring a load operation failed.
// For both ContentSuggestions.Feed.UserJourney.OpenFeed
// and ContentSuggestions.Feed.UserJourney.GetMore.
constexpr base::TimeDelta kLoadTimeout = base::TimeDelta::FromSeconds(15);
// Maximum time to wait before declaring opening a card a failure.
// For ContentSuggestions.Feed.UserJourney.OpenCard.
constexpr base::TimeDelta kOpenTimeout = base::TimeDelta::FromSeconds(20);

void ReportEngagementTypeHistogram(FeedEngagementType engagement_type) {
  base::UmaHistogramEnumeration("ContentSuggestions.Feed.EngagementType",
                                engagement_type);
}

void ReportContentSuggestionsOpened(int index_in_stream) {
  base::UmaHistogramExactLinear("NewTabPage.ContentSuggestions.Opened",
                                index_in_stream, kMaxSuggestionsTotal);
}

void ReportUserActionHistogram(FeedUserActionType action_type) {
  base::UmaHistogramEnumeration("ContentSuggestions.Feed.UserAction",
                                action_type);
}

}  // namespace

MetricsReporter::MetricsReporter(const base::TickClock* clock)
    : clock_(clock) {}

MetricsReporter::~MetricsReporter() = default;

void MetricsReporter::OnEnterBackground() {
  FinalizeMetrics();
}

// Engagement Tracking.

void MetricsReporter::RecordInteraction() {
  RecordEngagement(/*scroll_distance_dp=*/0, /*interacted=*/true);
  ReportEngagementTypeHistogram(FeedEngagementType::kFeedInteracted);
}

void MetricsReporter::RecordEngagement(int scroll_distance_dp,
                                       bool interacted) {
  scroll_distance_dp = std::abs(scroll_distance_dp);
  // Determine if this interaction is part of a new 'session'.
  auto now = clock_->NowTicks();
  const base::TimeDelta kVisitTimeout = base::TimeDelta::FromMinutes(5);
  if (now - visit_start_time_ > kVisitTimeout) {
    engaged_reported_ = false;
    engaged_simple_reported_ = false;
  }
  // Reset the last active time for session measurement.
  visit_start_time_ = now;

  // Report the user as engaged-simple if they have scrolled any amount or
  // interacted with the card, and we have not already reported it for this
  // chrome run.
  if (!engaged_simple_reported_ && (scroll_distance_dp > 0 || interacted)) {
    ReportEngagementTypeHistogram(FeedEngagementType::kFeedEngagedSimple);
    engaged_simple_reported_ = true;
  }

  // Report the user as engaged if they have scrolled more than the threshold or
  // interacted with the card, and we have not already reported it this chrome
  // run.
  const int kMinScrollThresholdDp = 160;  // 1 inch.
  if (!engaged_reported_ &&
      (scroll_distance_dp > kMinScrollThresholdDp || interacted)) {
    ReportEngagementTypeHistogram(FeedEngagementType::kFeedEngaged);
    engaged_reported_ = true;
  }
}

void MetricsReporter::StreamScrolled(int distance_dp) {
  RecordEngagement(distance_dp, /*interacted=*/false);

  if (!scrolled_reported_) {
    ReportEngagementTypeHistogram(FeedEngagementType::kFeedScrolled);
    scrolled_reported_ = true;
  }
}

void MetricsReporter::ContentSliceViewed(SurfaceId surface_id,
                                         int index_in_stream) {
  base::UmaHistogramExactLinear("NewTabPage.ContentSuggestions.Shown",
                                index_in_stream, kMaxSuggestionsTotal);

  ReportOpenFeedIfNeeded(surface_id, true);
}

void MetricsReporter::OpenAction(int index_in_stream) {
  CardOpenBegin();
  ReportUserActionHistogram(FeedUserActionType::kTappedOnCard);
  base::RecordAction(
      base::UserMetricsAction("ContentSuggestions.Feed.CardAction.Open"));
  ReportContentSuggestionsOpened(index_in_stream);
  RecordInteraction();
}

void MetricsReporter::OpenInNewTabAction(int index_in_stream) {
  CardOpenBegin();
  ReportUserActionHistogram(FeedUserActionType::kTappedOpenInNewTab);
  base::RecordAction(base::UserMetricsAction(
      "ContentSuggestions.Feed.CardAction.OpenInNewTab"));
  ReportContentSuggestionsOpened(index_in_stream);
  RecordInteraction();
}

void MetricsReporter::OpenInNewIncognitoTabAction() {
  ReportUserActionHistogram(FeedUserActionType::kTappedOpenInNewIncognitoTab);
  base::RecordAction(base::UserMetricsAction(
      "ContentSuggestions.Feed.CardAction.OpenInNewIncognitoTab"));
  RecordInteraction();
}

void MetricsReporter::SendFeedbackAction() {
  ReportUserActionHistogram(FeedUserActionType::kTappedSendFeedback);
  base::RecordAction(base::UserMetricsAction(
      "ContentSuggestions.Feed.CardAction.SendFeedback"));
  RecordInteraction();
}

void MetricsReporter::DownloadAction() {
  ReportUserActionHistogram(FeedUserActionType::kTappedDownload);
  base::RecordAction(
      base::UserMetricsAction("ContentSuggestions.Feed.CardAction.Download"));
  RecordInteraction();
}

void MetricsReporter::LearnMoreAction() {
  ReportUserActionHistogram(FeedUserActionType::kTappedLearnMore);
  base::RecordAction(
      base::UserMetricsAction("ContentSuggestions.Feed.CardAction.LearnMore"));
  RecordInteraction();
}

void MetricsReporter::NavigationStarted() {
  // TODO(harringtond): Use this or remove it.
}

void MetricsReporter::PageLoaded() {
  ReportCardOpenEndIfNeeded(true);
}

void MetricsReporter::RemoveAction() {
  ReportUserActionHistogram(FeedUserActionType::kTappedHideStory);
  base::RecordAction(
      base::UserMetricsAction("ContentSuggestions.Feed.CardAction.HideStory"));
  RecordInteraction();
}

void MetricsReporter::NotInterestedInAction() {
  ReportUserActionHistogram(FeedUserActionType::kTappedNotInterestedIn);
  base::RecordAction(base::UserMetricsAction(
      "ContentSuggestions.Feed.CardAction.NotInterestedIn"));
  RecordInteraction();
}

void MetricsReporter::ManageInterestsAction() {
  ReportUserActionHistogram(FeedUserActionType::kTappedManageInterests);
  base::RecordAction(base::UserMetricsAction(
      "ContentSuggestions.Feed.CardAction.ManageInterests"));
  RecordInteraction();
}

void MetricsReporter::ContextMenuOpened() {
  ReportUserActionHistogram(FeedUserActionType::kOpenedContextMenu);
  base::RecordAction(base::UserMetricsAction(
      "ContentSuggestions.Feed.CardAction.ContextMenu"));
}

void MetricsReporter::SurfaceOpened(SurfaceId surface_id) {
  surfaces_waiting_for_content_.emplace(surface_id, clock_->NowTicks());
  ReportUserActionHistogram(FeedUserActionType::kOpenedFeedSurface);
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&MetricsReporter::ReportOpenFeedIfNeeded, GetWeakPtr(),
                     surface_id, false),
      kLoadTimeout);
}

void MetricsReporter::SurfaceClosed(SurfaceId surface_id) {
  ReportOpenFeedIfNeeded(surface_id, false);
  ReportGetMoreIfNeeded(surface_id, false);
}

void MetricsReporter::FinalizeMetrics() {
  ReportCardOpenEndIfNeeded(false);
  for (auto iter = surfaces_waiting_for_content_.begin();
       iter != surfaces_waiting_for_content_.end();) {
    ReportOpenFeedIfNeeded((iter++)->first, false);
  }
  for (auto iter = surfaces_waiting_for_more_content_.begin();
       iter != surfaces_waiting_for_more_content_.end();) {
    ReportGetMoreIfNeeded((iter++)->first, false);
  }
}

void MetricsReporter::ReportOpenFeedIfNeeded(SurfaceId surface_id,
                                             bool success) {
  auto iter = surfaces_waiting_for_content_.find(surface_id);
  if (iter == surfaces_waiting_for_content_.end())
    return;
  base::TimeDelta latency = clock_->NowTicks() - iter->second;
  surfaces_waiting_for_content_.erase(iter);
  if (success) {
    base::UmaHistogramCustomTimes(
        "ContentSuggestions.Feed.UserJourney.OpenFeed.SuccessDuration", latency,
        base::TimeDelta::FromMilliseconds(50), kLoadTimeout, 50);
  } else {
    base::UmaHistogramCustomTimes(
        "ContentSuggestions.Feed.UserJourney.OpenFeed.FailureDuration", latency,
        base::TimeDelta::FromMilliseconds(50), kLoadTimeout, 50);
  }
}

void MetricsReporter::ReportGetMoreIfNeeded(SurfaceId surface_id,
                                            bool success) {
  auto iter = surfaces_waiting_for_more_content_.find(surface_id);
  if (iter == surfaces_waiting_for_more_content_.end())
    return;
  base::TimeDelta latency = clock_->NowTicks() - iter->second;
  surfaces_waiting_for_more_content_.erase(iter);
  if (success) {
    base::UmaHistogramCustomTimes(
        "ContentSuggestions.Feed.UserJourney.GetMore.SuccessDuration", latency,
        base::TimeDelta::FromMilliseconds(50), kLoadTimeout, 50);
  } else {
    base::UmaHistogramCustomTimes(
        "ContentSuggestions.Feed.UserJourney.GetMore.FailureDuration", latency,
        base::TimeDelta::FromMilliseconds(50), kLoadTimeout, 50);
  }
}

void MetricsReporter::CardOpenBegin() {
  ReportCardOpenEndIfNeeded(false);
  pending_open_ = clock_->NowTicks();
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&MetricsReporter::CardOpenTimeout, GetWeakPtr(),
                     *pending_open_),
      kOpenTimeout);
}

void MetricsReporter::CardOpenTimeout(base::TimeTicks start_ticks) {
  if (pending_open_ && start_ticks == *pending_open_)
    ReportCardOpenEndIfNeeded(false);
}

void MetricsReporter::ReportCardOpenEndIfNeeded(bool success) {
  if (!pending_open_)
    return;
  base::TimeDelta latency = clock_->NowTicks() - *pending_open_;
  pending_open_.reset();
  if (success) {
    base::UmaHistogramCustomTimes(
        "ContentSuggestions.Feed.UserJourney.OpenCard.SuccessDuration", latency,
        base::TimeDelta::FromMilliseconds(100), kOpenTimeout, 50);
  } else {
    base::UmaHistogramBoolean(
        "ContentSuggestions.Feed.UserJourney.OpenCard.Failure", true);
  }
}

void MetricsReporter::NetworkRequestComplete(NetworkRequestType type,
                                             int http_status_code) {
  switch (type) {
    case NetworkRequestType::kFeedQuery:
      base::UmaHistogramSparse(
          "ContentSuggestions.Feed.Network.ResponseStatus.FeedQuery",
          http_status_code);
      return;
    case NetworkRequestType::kUploadActions:
      base::UmaHistogramSparse(
          "ContentSuggestions.Feed.Network.ResponseStatus.UploadActions",
          http_status_code);
      return;
  }
}

void MetricsReporter::OnLoadStream(LoadStreamStatus load_from_store_status,
                                   LoadStreamStatus final_status) {
  DVLOG(1) << "OnLoadStream load_from_store_status=" << load_from_store_status
           << " final_status=" << final_status;
  base::UmaHistogramEnumeration(
      "ContentSuggestions.Feed.LoadStreamStatus.Initial", final_status);
  if (load_from_store_status != LoadStreamStatus::kNoStatus) {
    base::UmaHistogramEnumeration(
        "ContentSuggestions.Feed.LoadStreamStatus.InitialFromStore",
        load_from_store_status);
  }
}

void MetricsReporter::OnBackgroundRefresh(LoadStreamStatus final_status) {
  base::UmaHistogramEnumeration(
      "ContentSuggestions.Feed.LoadStreamStatus.BackgroundRefresh",
      final_status);
}

void MetricsReporter::OnLoadMoreBegin(SurfaceId surface_id) {
  ReportGetMoreIfNeeded(surface_id, false);
  surfaces_waiting_for_more_content_.emplace(surface_id, clock_->NowTicks());

  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&MetricsReporter::ReportGetMoreIfNeeded, GetWeakPtr(),
                     surface_id, false),
      kLoadTimeout);
}

void MetricsReporter::OnLoadMore(LoadStreamStatus status) {
  DVLOG(1) << "OnLoadMore status=" << status;
  base::UmaHistogramEnumeration(
      "ContentSuggestions.Feed.LoadStreamStatus.LoadMore", status);
}

void MetricsReporter::SurfaceReceivedContent(SurfaceId surface_id) {
  ReportGetMoreIfNeeded(surface_id, true);
}

void MetricsReporter::OnClearAll(base::TimeDelta time_since_last_clear) {
  base::UmaHistogramCustomTimes(
      "ContentSuggestions.Feed.Scheduler.TimeSinceLastFetchOnClear",
      time_since_last_clear, base::TimeDelta::FromSeconds(1),
      base::TimeDelta::FromDays(7),
      /*bucket_count=*/50);
}

}  // namespace feed
