// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/metrics_reporter.h"

#include <map>
#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/task_environment.h"
#include "components/feed/core/common/pref_names.h"
#include "components/feed/core/shared_prefs/pref_names.h"
#include "components/feed/core/v2/public/common_enums.h"
#include "components/feed/core/v2/public/feed_api.h"
#include "components/feed/core/v2/public/stream_type.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feed {
constexpr SurfaceId kSurfaceId = SurfaceId(5);
const base::TimeDelta kEpsilon = base::TimeDelta::FromMilliseconds(1);

class MetricsReporterTest : public testing::Test {
 protected:
  void SetUp() override {
    feed::prefs::RegisterFeedSharedProfilePrefs(profile_prefs_.registry());
    feed::RegisterProfilePrefs(profile_prefs_.registry());

    // Tests start at the beginning of a day.
    task_environment_.AdvanceClock(
        (base::Time::Now().LocalMidnight() + base::TimeDelta::FromDays(1)) -
        base::Time::Now() + base::TimeDelta::FromSeconds(1));

    RecreateMetricsReporter();
  }
  std::map<FeedEngagementType, int> ReportedEngagementType(
      const StreamType& stream_type) {
    std::map<FeedEngagementType, int> result;
    const char* histogram_name =
        stream_type.IsForYou()
            ? "ContentSuggestions.Feed.EngagementType"
            : "ContentSuggestions.Feed.WebFeed.EngagementType";
    for (const auto& bucket : histogram_.GetAllSamples(histogram_name)) {
      result[static_cast<FeedEngagementType>(bucket.min)] += bucket.count;
    }
    return result;
  }

  void RecreateMetricsReporter() {
    reporter_ = std::make_unique<MetricsReporter>(&profile_prefs_);
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingPrefServiceSimple profile_prefs_;
  std::unique_ptr<MetricsReporter> reporter_;
  base::HistogramTester histogram_;
  base::UserActionTester user_actions_;
};

TEST_F(MetricsReporterTest, SliceViewedReportsSuggestionShown) {
  reporter_->ContentSliceViewed(kForYouStream, 5, 7);
  histogram_.ExpectUniqueSample("NewTabPage.ContentSuggestions.Shown", 5, 1);
  reporter_->ContentSliceViewed(kWebFeedStream, 5, 7);
  histogram_.ExpectUniqueSample("ContentSuggestions.Feed.WebFeed.Shown", 5, 1);
  histogram_.ExpectTotalCount("ContentSuggestions.Feed.ReachedEndOfFeed", 0);
  histogram_.ExpectTotalCount(
      "ContentSuggestions.Feed.WebFeed.ReachedEndOfFeed", 0);
}

TEST_F(MetricsReporterTest, LastSliceViewedReportsReachedEndOfFeed) {
  reporter_->ContentSliceViewed(kForYouStream, 5, 6);
  histogram_.ExpectUniqueSample("ContentSuggestions.Feed.ReachedEndOfFeed", 6,
                                1);
}

TEST_F(MetricsReporterTest, WebFeed_LastSliceViewedReportsReachedEndOfFeed) {
  reporter_->ContentSliceViewed(kWebFeedStream, 5, 6);
  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.WebFeed.ReachedEndOfFeed", 6, 1);
}

TEST_F(MetricsReporterTest, ScrollingSmall) {
  reporter_->StreamScrolled(kForYouStream, 100);

  std::map<FeedEngagementType, int> want({
      {FeedEngagementType::kFeedScrolled, 1},
      {FeedEngagementType::kFeedEngagedSimple, 1},
  });
  EXPECT_EQ(want, ReportedEngagementType(kForYouStream));
}

TEST_F(MetricsReporterTest, ScrollingCanTriggerEngaged) {
  reporter_->StreamScrolled(kForYouStream, 161);

  std::map<FeedEngagementType, int> want({
      {FeedEngagementType::kFeedScrolled, 1},
      {FeedEngagementType::kFeedEngaged, 1},
      {FeedEngagementType::kFeedEngagedSimple, 1},
  });
  EXPECT_EQ(want, ReportedEngagementType(kForYouStream));
}

TEST_F(MetricsReporterTest, OpeningContentIsInteracting) {
  reporter_->OpenAction(kForYouStream, 5);

  std::map<FeedEngagementType, int> want({
      {FeedEngagementType::kFeedEngaged, 1},
      {FeedEngagementType::kFeedInteracted, 1},
      {FeedEngagementType::kFeedEngagedSimple, 1},
  });
  EXPECT_EQ(want, ReportedEngagementType(kForYouStream));
}

TEST_F(MetricsReporterTest, RemovingContentIsInteracting) {
  reporter_->OtherUserAction(kForYouStream,
                             FeedUserActionType::kTappedHideStory);

  std::map<FeedEngagementType, int> want({
      {FeedEngagementType::kFeedEngaged, 1},
      {FeedEngagementType::kFeedInteracted, 1},
      {FeedEngagementType::kFeedEngagedSimple, 1},
  });
  EXPECT_EQ(want, ReportedEngagementType(kForYouStream));
}

TEST_F(MetricsReporterTest, NotInterestedInIsInteracting) {
  reporter_->OtherUserAction(kForYouStream,
                             FeedUserActionType::kTappedNotInterestedIn);

  std::map<FeedEngagementType, int> want({
      {FeedEngagementType::kFeedEngaged, 1},
      {FeedEngagementType::kFeedInteracted, 1},
      {FeedEngagementType::kFeedEngagedSimple, 1},
  });
  EXPECT_EQ(want, ReportedEngagementType(kForYouStream));
}

TEST_F(MetricsReporterTest, ManageInterestsInIsInteracting) {
  reporter_->OtherUserAction(kForYouStream,
                             FeedUserActionType::kTappedManageInterests);

  std::map<FeedEngagementType, int> want({
      {FeedEngagementType::kFeedEngaged, 1},
      {FeedEngagementType::kFeedInteracted, 1},
      {FeedEngagementType::kFeedEngagedSimple, 1},
  });
  EXPECT_EQ(want, ReportedEngagementType(kForYouStream));
}

TEST_F(MetricsReporterTest, VisitsCanLastMoreThanFiveMinutes) {
  reporter_->StreamScrolled(kForYouStream, 1);
  task_environment_.FastForwardBy(base::TimeDelta::FromMinutes(5) - kEpsilon);
  reporter_->OpenAction(kForYouStream, 0);
  task_environment_.FastForwardBy(base::TimeDelta::FromMinutes(5) - kEpsilon);
  reporter_->StreamScrolled(kForYouStream, 1);

  std::map<FeedEngagementType, int> want({
      {FeedEngagementType::kFeedEngaged, 1},
      {FeedEngagementType::kFeedInteracted, 1},
      {FeedEngagementType::kFeedScrolled, 1},
      {FeedEngagementType::kFeedEngagedSimple, 1},
  });
  EXPECT_EQ(want, ReportedEngagementType(kForYouStream));
}

TEST_F(MetricsReporterTest, NewVisitAfterInactivity) {
  reporter_->OpenAction(kForYouStream, 0);
  reporter_->StreamScrolled(kForYouStream, 1);
  task_environment_.FastForwardBy(base::TimeDelta::FromMinutes(5) + kEpsilon);
  reporter_->OpenAction(kForYouStream, 0);
  reporter_->StreamScrolled(kForYouStream, 1);

  std::map<FeedEngagementType, int> want({
      {FeedEngagementType::kFeedEngaged, 2},
      {FeedEngagementType::kFeedInteracted, 2},
      {FeedEngagementType::kFeedEngagedSimple, 2},
      {FeedEngagementType::kFeedScrolled, 2},
  });
  EXPECT_EQ(want, ReportedEngagementType(kForYouStream));
}

TEST_F(MetricsReporterTest, ReportsLoadStreamStatus) {
  reporter_->OnLoadStream(kForYouStream, LoadStreamStatus::kDataInStoreIsStale,
                          LoadStreamStatus::kLoadedFromNetwork,
                          /*is_initial_load=*/true,
                          /*loaded_new_content_from_network=*/true,
                          /*stored_content_age=*/base::TimeDelta::FromDays(5),
                          /*content_count=*/12,
                          std::make_unique<LoadLatencyTimes>());

  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.LoadStreamStatus.Initial",
      LoadStreamStatus::kLoadedFromNetwork, 1);
  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.LoadStreamStatus.InitialFromStore",
      LoadStreamStatus::kDataInStoreIsStale, 1);
  histogram_.ExpectUniqueSample("ContentSuggestions.Feed.LoadedCardCount", 12,
                                1);
}

TEST_F(MetricsReporterTest, WebFeed_ReportsLoadStreamStatus) {
  reporter_->OnLoadStream(kWebFeedStream, LoadStreamStatus::kDataInStoreIsStale,
                          LoadStreamStatus::kLoadedFromNetwork,
                          /*is_initial_load=*/true,
                          /*loaded_new_content_from_network=*/true,
                          /*stored_content_age=*/base::TimeDelta::FromDays(5),
                          /*content_count=*/12,
                          std::make_unique<LoadLatencyTimes>());

  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.WebFeed.LoadStreamStatus.Initial",
      LoadStreamStatus::kLoadedFromNetwork, 1);
  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.WebFeed.LoadStreamStatus.InitialFromStore",
      LoadStreamStatus::kDataInStoreIsStale, 1);
  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.WebFeed.LoadedCardCount", 12, 1);
}

TEST_F(MetricsReporterTest, OnLoadStreamDoesNotReportLoadedCardCountOnFailure) {
  reporter_->OnLoadStream(kForYouStream, LoadStreamStatus::kDataInStoreIsStale,
                          LoadStreamStatus::kDataInStoreIsExpired,
                          /*is_initial_load=*/true,
                          /*loaded_new_content_from_network=*/false,
                          /*stored_content_age=*/base::TimeDelta::FromDays(5),
                          /*content_count=*/12,
                          std::make_unique<LoadLatencyTimes>());

  histogram_.ExpectTotalCount("ContentSuggestions.Feed.LoadedCardCount", 0);
}

TEST_F(MetricsReporterTest, ReportsLoadStreamStatusForManualRefresh) {
  reporter_->OnLoadStream(kForYouStream, LoadStreamStatus::kDataInStoreIsStale,
                          LoadStreamStatus::kLoadedFromNetwork,
                          /*is_initial_load=*/false,
                          /*loaded_new_content_from_network=*/true,
                          /*stored_content_age=*/base::TimeDelta::FromDays(5),
                          /*content_count=*/12,
                          std::make_unique<LoadLatencyTimes>());

  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.LoadStreamStatus.Initial",
      LoadStreamStatus::kLoadedFromNetwork, 0);
  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.LoadStreamStatus.InitialFromStore",
      LoadStreamStatus::kDataInStoreIsStale, 0);
  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.LoadStreamStatus.ManualRefresh",
      LoadStreamStatus::kLoadedFromNetwork, 1);
}

TEST_F(MetricsReporterTest, ReportsLoadStreamStatusIgnoresNoStatusFromStore) {
  reporter_->OnLoadStream(kForYouStream, LoadStreamStatus::kNoStatus,
                          LoadStreamStatus::kLoadedFromNetwork,
                          /*is_initial_load=*/true,
                          /*loaded_new_content_from_network=*/true,
                          /*stored_content_age=*/base::TimeDelta(),
                          /*content_count=*/12,
                          std::make_unique<LoadLatencyTimes>());

  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.LoadStreamStatus.Initial",
      LoadStreamStatus::kLoadedFromNetwork, 1);
  histogram_.ExpectTotalCount(
      "ContentSuggestions.Feed.LoadStreamStatus.InitialFromStore", 0);
  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.LoadStreamStatus.UserRefresh",
      LoadStreamStatus::kLoadedFromNetwork, 0);
}

TEST_F(MetricsReporterTest, ReportsContentAgeBlockingRefresh) {
  reporter_->OnLoadStream(kForYouStream, LoadStreamStatus::kDataInStoreIsStale,
                          LoadStreamStatus::kLoadedFromNetwork,
                          /*is_initial_load=*/true,
                          /*loaded_new_content_from_network=*/true,
                          /*stored_content_age=*/base::TimeDelta::FromDays(5),
                          /*content_count=*/12,
                          std::make_unique<LoadLatencyTimes>());

  histogram_.ExpectUniqueTimeSample(
      "ContentSuggestions.Feed.ContentAgeOnLoad.BlockingRefresh",
      base::TimeDelta::FromDays(5), 1);
}

TEST_F(MetricsReporterTest, ReportsContentAgeNoRefresh) {
  reporter_->OnLoadStream(kForYouStream, LoadStreamStatus::kDataInStoreIsStale,
                          LoadStreamStatus::kLoadedFromStore,
                          /*is_initial_load=*/true,
                          /*loaded_new_content_from_network=*/false,
                          /*stored_content_age=*/base::TimeDelta::FromDays(5),
                          /*content_count=*/12,
                          std::make_unique<LoadLatencyTimes>());

  histogram_.ExpectUniqueTimeSample(
      "ContentSuggestions.Feed.ContentAgeOnLoad.NotRefreshed",
      base::TimeDelta::FromDays(5), 1);
}

TEST_F(MetricsReporterTest, DoNotReportContentAgeWhenNotPositive) {
  reporter_->OnLoadStream(
      kForYouStream, LoadStreamStatus::kDataInStoreIsStale,
      LoadStreamStatus::kLoadedFromStore, /*is_initial_load=*/true,
      /*loaded_new_content_from_network=*/false,
      /*stored_content_age=*/-base::TimeDelta::FromSeconds(1),
      /*content_count=*/12, std::make_unique<LoadLatencyTimes>());
  reporter_->OnLoadStream(kForYouStream, LoadStreamStatus::kDataInStoreIsStale,
                          LoadStreamStatus::kLoadedFromStore,
                          /*is_initial_load=*/true,
                          /*loaded_new_content_from_network=*/false,
                          /*stored_content_age=*/base::TimeDelta(),
                          /*content_count=*/12,
                          std::make_unique<LoadLatencyTimes>());
  histogram_.ExpectTotalCount(
      "ContentSuggestions.Feed.ContentAgeOnLoad.NotRefreshed", 0);
  histogram_.ExpectTotalCount(
      "ContentSuggestions.Feed.ContentAgeOnLoad.BlockingRefresh", 0);
}

TEST_F(MetricsReporterTest, ReportsLoadStepLatenciesOnFirstView) {
  {
    auto latencies = std::make_unique<LoadLatencyTimes>();
    task_environment_.FastForwardBy(base::TimeDelta::FromMilliseconds(150));
    latencies->StepComplete(LoadLatencyTimes::kLoadFromStore);
    task_environment_.FastForwardBy(base::TimeDelta::FromMilliseconds(50));
    latencies->StepComplete(LoadLatencyTimes::kUploadActions);
    reporter_->OnLoadStream(kForYouStream, LoadStreamStatus::kNoStatus,
                            LoadStreamStatus::kLoadedFromNetwork,
                            /*is_initial_load=*/true,
                            /*loaded_new_content_from_network=*/true,
                            /*stored_content_age=*/base::TimeDelta(),
                            /*content_count=*/12, std::move(latencies));
  }
  task_environment_.FastForwardBy(base::TimeDelta::FromMilliseconds(300));
  reporter_->FeedViewed(kSurfaceId);
  reporter_->FeedViewed(kSurfaceId);

  histogram_.ExpectUniqueTimeSample(
      "ContentSuggestions.Feed.LoadStepLatency.LoadFromStore",
      base::TimeDelta::FromMilliseconds(150), 1);
  histogram_.ExpectUniqueTimeSample(
      "ContentSuggestions.Feed.LoadStepLatency.ActionUpload",
      base::TimeDelta::FromMilliseconds(50), 1);
  histogram_.ExpectUniqueTimeSample(
      "ContentSuggestions.Feed.LoadStepLatency.StreamView",
      base::TimeDelta::FromMilliseconds(300), 1);
}

TEST_F(MetricsReporterTest, ReportsLoadMoreStatus) {
  reporter_->OnLoadMore(LoadStreamStatus::kLoadedFromNetwork);

  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.LoadStreamStatus.LoadMore",
      LoadStreamStatus::kLoadedFromNetwork, 1);
}

TEST_F(MetricsReporterTest, ReportsBackgroundRefreshStatus) {
  reporter_->OnBackgroundRefresh(kForYouStream,
                                 LoadStreamStatus::kLoadedFromNetwork);

  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.LoadStreamStatus.BackgroundRefresh",
      LoadStreamStatus::kLoadedFromNetwork, 1);
}

TEST_F(MetricsReporterTest, WebFeed_ReportsBackgroundRefreshStatus) {
  reporter_->OnBackgroundRefresh(kWebFeedStream,
                                 LoadStreamStatus::kLoadedFromNetwork);

  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.WebFeed.LoadStreamStatus.BackgroundRefresh",
      LoadStreamStatus::kLoadedFromNetwork, 1);
}

TEST_F(MetricsReporterTest, OpenAction) {
  reporter_->OpenAction(kForYouStream, 5);

  std::map<FeedEngagementType, int> want({
      {FeedEngagementType::kFeedEngaged, 1},
      {FeedEngagementType::kFeedInteracted, 1},
      {FeedEngagementType::kFeedEngagedSimple, 1},
  });
  EXPECT_EQ(want, ReportedEngagementType(kForYouStream));
  EXPECT_EQ(1, user_actions_.GetActionCount(
                   "ContentSuggestions.Feed.CardAction.Open"));
  histogram_.ExpectUniqueSample("ContentSuggestions.Feed.UserActions",
                                FeedUserActionType::kTappedOnCard, 1);
  histogram_.ExpectUniqueSample("NewTabPage.ContentSuggestions.Opened", 5, 1);
}

TEST_F(MetricsReporterTest, OpenActionWebFeed) {
  reporter_->OpenAction(kWebFeedStream, 5);

  std::map<FeedEngagementType, int> want({
      {FeedEngagementType::kFeedEngaged, 1},
      {FeedEngagementType::kFeedInteracted, 1},
      {FeedEngagementType::kFeedEngagedSimple, 1},
  });
  EXPECT_EQ(want, ReportedEngagementType(kWebFeedStream));
  EXPECT_EQ(1, user_actions_.GetActionCount(
                   "ContentSuggestions.Feed.CardAction.Open"));
  histogram_.ExpectUniqueSample("ContentSuggestions.Feed.UserActions",
                                FeedUserActionType::kTappedOnCard, 1);
  histogram_.ExpectUniqueSample("ContentSuggestions.Feed.WebFeed.Opened", 5, 1);
}

TEST_F(MetricsReporterTest, OpenInNewTabAction) {
  reporter_->OpenInNewTabAction(kForYouStream, 5);

  std::map<FeedEngagementType, int> want({
      {FeedEngagementType::kFeedEngaged, 1},
      {FeedEngagementType::kFeedInteracted, 1},
      {FeedEngagementType::kFeedEngagedSimple, 1},
  });
  EXPECT_EQ(want, ReportedEngagementType(kForYouStream));
  EXPECT_EQ(1, user_actions_.GetActionCount(
                   "ContentSuggestions.Feed.CardAction.OpenInNewTab"));
  histogram_.ExpectUniqueSample("ContentSuggestions.Feed.UserActions",
                                FeedUserActionType::kTappedOpenInNewTab, 1);
  histogram_.ExpectUniqueSample("NewTabPage.ContentSuggestions.Opened", 5, 1);
}

TEST_F(MetricsReporterTest, OpenInNewIncognitoTabAction) {
  reporter_->OtherUserAction(kForYouStream,
                             FeedUserActionType::kTappedOpenInNewIncognitoTab);

  std::map<FeedEngagementType, int> want({
      {FeedEngagementType::kFeedEngaged, 1},
      {FeedEngagementType::kFeedInteracted, 1},
      {FeedEngagementType::kFeedEngagedSimple, 1},
  });
  EXPECT_EQ(want, ReportedEngagementType(kForYouStream));
  EXPECT_EQ(1, user_actions_.GetActionCount(
                   "ContentSuggestions.Feed.CardAction.OpenInNewIncognitoTab"));
  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.UserActions",
      FeedUserActionType::kTappedOpenInNewIncognitoTab, 1);
  histogram_.ExpectTotalCount("NewTabPage.ContentSuggestions.Opened", 0);
}

TEST_F(MetricsReporterTest, SendFeedbackAction) {
  reporter_->OtherUserAction(kForYouStream,
                             FeedUserActionType::kTappedSendFeedback);

  std::map<FeedEngagementType, int> want({
      {FeedEngagementType::kFeedEngaged, 1},
      {FeedEngagementType::kFeedInteracted, 1},
      {FeedEngagementType::kFeedEngagedSimple, 1},
  });
  EXPECT_EQ(want, ReportedEngagementType(kForYouStream));
  EXPECT_EQ(1, user_actions_.GetActionCount(
                   "ContentSuggestions.Feed.CardAction.SendFeedback"));
  histogram_.ExpectUniqueSample("ContentSuggestions.Feed.UserActions",
                                FeedUserActionType::kTappedSendFeedback, 1);
}

TEST_F(MetricsReporterTest, DownloadAction) {
  reporter_->OtherUserAction(kForYouStream,
                             FeedUserActionType::kTappedDownload);

  std::map<FeedEngagementType, int> want({
      {FeedEngagementType::kFeedEngaged, 1},
      {FeedEngagementType::kFeedInteracted, 1},
      {FeedEngagementType::kFeedEngagedSimple, 1},
  });
  EXPECT_EQ(want, ReportedEngagementType(kForYouStream));
  EXPECT_EQ(1, user_actions_.GetActionCount(
                   "ContentSuggestions.Feed.CardAction.Download"));
  histogram_.ExpectUniqueSample("ContentSuggestions.Feed.UserActions",
                                FeedUserActionType::kTappedDownload, 1);
}

TEST_F(MetricsReporterTest, LearnMoreAction) {
  reporter_->OtherUserAction(kForYouStream,
                             FeedUserActionType::kTappedLearnMore);

  std::map<FeedEngagementType, int> want({
      {FeedEngagementType::kFeedEngaged, 1},
      {FeedEngagementType::kFeedInteracted, 1},
      {FeedEngagementType::kFeedEngagedSimple, 1},
  });
  EXPECT_EQ(want, ReportedEngagementType(kForYouStream));
  EXPECT_EQ(1, user_actions_.GetActionCount(
                   "ContentSuggestions.Feed.CardAction.LearnMore"));
  histogram_.ExpectUniqueSample("ContentSuggestions.Feed.UserActions",
                                FeedUserActionType::kTappedLearnMore, 1);
}

TEST_F(MetricsReporterTest, RemoveAction) {
  reporter_->OtherUserAction(kForYouStream,
                             FeedUserActionType::kTappedHideStory);

  std::map<FeedEngagementType, int> want({
      {FeedEngagementType::kFeedEngaged, 1},
      {FeedEngagementType::kFeedInteracted, 1},
      {FeedEngagementType::kFeedEngagedSimple, 1},
  });
  EXPECT_EQ(want, ReportedEngagementType(kForYouStream));
  EXPECT_EQ(1, user_actions_.GetActionCount(
                   "ContentSuggestions.Feed.CardAction.HideStory"));
  histogram_.ExpectUniqueSample("ContentSuggestions.Feed.UserActions",
                                FeedUserActionType::kTappedHideStory, 1);
}

TEST_F(MetricsReporterTest, NotInterestedInAction) {
  reporter_->OtherUserAction(kForYouStream,
                             FeedUserActionType::kTappedNotInterestedIn);

  std::map<FeedEngagementType, int> want({
      {FeedEngagementType::kFeedEngaged, 1},
      {FeedEngagementType::kFeedInteracted, 1},
      {FeedEngagementType::kFeedEngagedSimple, 1},
  });
  EXPECT_EQ(want, ReportedEngagementType(kForYouStream));
  EXPECT_EQ(1, user_actions_.GetActionCount(
                   "ContentSuggestions.Feed.CardAction.NotInterestedIn"));
  histogram_.ExpectUniqueSample("ContentSuggestions.Feed.UserActions",
                                FeedUserActionType::kTappedNotInterestedIn, 1);
}

TEST_F(MetricsReporterTest, ManageInterestsAction) {
  reporter_->OtherUserAction(kForYouStream,
                             FeedUserActionType::kTappedManageInterests);

  std::map<FeedEngagementType, int> want({
      {FeedEngagementType::kFeedEngaged, 1},
      {FeedEngagementType::kFeedInteracted, 1},
      {FeedEngagementType::kFeedEngagedSimple, 1},
  });
  EXPECT_EQ(want, ReportedEngagementType(kForYouStream));
  EXPECT_EQ(1, user_actions_.GetActionCount(
                   "ContentSuggestions.Feed.CardAction.ManageInterests"));
  histogram_.ExpectUniqueSample("ContentSuggestions.Feed.UserActions",
                                FeedUserActionType::kTappedManageInterests, 1);
}

TEST_F(MetricsReporterTest, ContextMenuOpened) {
  reporter_->OtherUserAction(kForYouStream,
                             FeedUserActionType::kOpenedContextMenu);

  std::map<FeedEngagementType, int> want_empty;
  EXPECT_EQ(want_empty, ReportedEngagementType(kForYouStream));
  EXPECT_EQ(1, user_actions_.GetActionCount(
                   "ContentSuggestions.Feed.CardAction.ContextMenu"));
  histogram_.ExpectUniqueSample("ContentSuggestions.Feed.UserActions",
                                FeedUserActionType::kOpenedContextMenu, 1);
}

TEST_F(MetricsReporterTest, SurfaceOpened) {
  reporter_->SurfaceOpened(kForYouStream, kSurfaceId);

  std::map<FeedEngagementType, int> want_empty;
  EXPECT_EQ(want_empty, ReportedEngagementType(kForYouStream));
  histogram_.ExpectUniqueSample("ContentSuggestions.Feed.UserActions",
                                FeedUserActionType::kOpenedFeedSurface, 1);
}

TEST_F(MetricsReporterTest, OpenFeedSuccessDuration) {
  reporter_->SurfaceOpened(kForYouStream, kSurfaceId);
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(9));
  reporter_->FeedViewed(kSurfaceId);

  histogram_.ExpectUniqueTimeSample(
      "ContentSuggestions.Feed.UserJourney.OpenFeed.SuccessDuration",
      base::TimeDelta::FromSeconds(9), 1);
}

TEST_F(MetricsReporterTest, WebFeed_OpenFeedSuccessDuration) {
  reporter_->SurfaceOpened(kWebFeedStream, kSurfaceId);
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(9));
  reporter_->FeedViewed(kSurfaceId);

  histogram_.ExpectUniqueTimeSample(
      "ContentSuggestions.Feed.UserJourney.OpenFeed.WebFeed.SuccessDuration",
      base::TimeDelta::FromSeconds(9), 1);
}

TEST_F(MetricsReporterTest, OpenFeedLoadTimeout) {
  reporter_->SurfaceOpened(kForYouStream, kSurfaceId);
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(16));

  histogram_.ExpectUniqueTimeSample(
      "ContentSuggestions.Feed.UserJourney.OpenFeed.FailureDuration",
      base::TimeDelta::FromSeconds(15), 1);
  histogram_.ExpectTotalCount(
      "ContentSuggestions.Feed.UserJourney.OpenFeed.SuccessDuration", 0);
}

TEST_F(MetricsReporterTest, WebFeed_OpenFeedLoadTimeout) {
  reporter_->SurfaceOpened(kWebFeedStream, kSurfaceId);
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(16));

  histogram_.ExpectUniqueTimeSample(
      "ContentSuggestions.Feed.UserJourney.OpenFeed.WebFeed.FailureDuration",
      base::TimeDelta::FromSeconds(15), 1);
  histogram_.ExpectTotalCount(
      "ContentSuggestions.Feed.UserJourney.OpenFeed.WebFeed.SuccessDuration",
      0);
}

TEST_F(MetricsReporterTest, OpenFeedCloseBeforeLoad) {
  reporter_->SurfaceOpened(kForYouStream, kSurfaceId);
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(14));
  reporter_->SurfaceClosed(kSurfaceId);

  histogram_.ExpectUniqueTimeSample(
      "ContentSuggestions.Feed.UserJourney.OpenFeed.FailureDuration",
      base::TimeDelta::FromSeconds(14), 1);
  histogram_.ExpectTotalCount(
      "ContentSuggestions.Feed.UserJourney.OpenFeed.SuccessDuration", 0);
}

TEST_F(MetricsReporterTest, WebFeed_OpenFeedCloseBeforeLoad) {
  reporter_->SurfaceOpened(kWebFeedStream, kSurfaceId);
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(14));
  reporter_->SurfaceClosed(kSurfaceId);

  histogram_.ExpectUniqueTimeSample(
      "ContentSuggestions.Feed.UserJourney.OpenFeed.WebFeed.FailureDuration",
      base::TimeDelta::FromSeconds(14), 1);
  histogram_.ExpectTotalCount(
      "ContentSuggestions.Feed.UserJourney.OpenFeed.WebFeed.SuccessDuration",
      0);
}

TEST_F(MetricsReporterTest, OpenCardSuccessDuration) {
  reporter_->OpenAction(kForYouStream, 0);
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(19));
  reporter_->PageLoaded();

  histogram_.ExpectUniqueTimeSample(
      "ContentSuggestions.Feed.UserJourney.OpenCard.SuccessDuration",
      base::TimeDelta::FromSeconds(19), 1);
}

TEST_F(MetricsReporterTest, WebFeed_OpenCardSuccessDuration) {
  reporter_->OpenAction(kWebFeedStream, 0);
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(19));
  reporter_->PageLoaded();

  histogram_.ExpectUniqueTimeSample(
      "ContentSuggestions.Feed.UserJourney.OpenCard.WebFeed.SuccessDuration",
      base::TimeDelta::FromSeconds(19), 1);
}

TEST_F(MetricsReporterTest, OpenCardTimeout) {
  reporter_->OpenAction(kForYouStream, 0);
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(21));
  reporter_->PageLoaded();

  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.UserJourney.OpenCard.Failure", 1, 1);
  histogram_.ExpectTotalCount(
      "ContentSuggestions.Feed.UserJourney.OpenCard.SuccessDuration", 0);
}

TEST_F(MetricsReporterTest, WebFeed_OpenCardTimeout) {
  reporter_->OpenAction(kWebFeedStream, 0);
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(21));
  reporter_->PageLoaded();

  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.UserJourney.OpenCard.WebFeed.Failure", 1, 1);
  histogram_.ExpectTotalCount(
      "ContentSuggestions.Feed.UserJourney.OpenCard.WebFeed.SuccessDuration",
      0);
}

TEST_F(MetricsReporterTest, OpenCardFailureTwiceAndThenSucceed) {
  reporter_->OpenAction(kForYouStream, 0);
  reporter_->OpenAction(kForYouStream, 1);
  reporter_->OpenAction(kForYouStream, 2);
  reporter_->PageLoaded();

  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.UserJourney.OpenCard.Failure", 1, 2);
  histogram_.ExpectTotalCount(
      "ContentSuggestions.Feed.UserJourney.OpenCard.SuccessDuration", 1);
}

TEST_F(MetricsReporterTest, WebFeed_OpenCardFailureTwiceAndThenSucceed) {
  reporter_->OpenAction(kWebFeedStream, 0);
  reporter_->OpenAction(kWebFeedStream, 1);
  reporter_->OpenAction(kWebFeedStream, 2);
  reporter_->PageLoaded();

  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.UserJourney.OpenCard.WebFeed.Failure", 1, 2);
  histogram_.ExpectTotalCount(
      "ContentSuggestions.Feed.UserJourney.OpenCard.WebFeed.SuccessDuration",
      1);
}

TEST_F(MetricsReporterTest, OpenCardCloseChromeFailure) {
  reporter_->OpenAction(kForYouStream, 0);
  reporter_->OnEnterBackground();

  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.UserJourney.OpenCard.Failure", 1, 1);
  histogram_.ExpectTotalCount(
      "ContentSuggestions.Feed.UserJourney.OpenCard.SuccessDuration", 0);
}

TEST_F(MetricsReporterTest, TimeSpentInFeedCountsOnlyForegroundTime) {
  reporter_->OpenAction(kForYouStream, 0);
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));
  reporter_->OnEnterBackground();
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(2));
  reporter_->OpenAction(kForYouStream, 0);
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(3));
  reporter_->OnEnterBackground();

  // Trigger reporting the persistent metrics the next day.
  task_environment_.FastForwardBy(base::TimeDelta::FromDays(1));
  RecreateMetricsReporter();

  histogram_.ExpectUniqueTimeSample("ContentSuggestions.Feed.TimeSpentInFeed",
                                    base::TimeDelta::FromSeconds(4), 1);
}

TEST_F(MetricsReporterTest, TimeSpentInFeedLimitsIdleTime) {
  reporter_->OpenAction(kForYouStream, 0);
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(31));
  reporter_->OnEnterBackground();

  // Trigger reporting the persistent metrics the next day.
  task_environment_.FastForwardBy(base::TimeDelta::FromDays(1));
  RecreateMetricsReporter();

  histogram_.ExpectUniqueTimeSample("ContentSuggestions.Feed.TimeSpentInFeed",
                                    base::TimeDelta::FromSeconds(30), 1);
}

TEST_F(MetricsReporterTest, TimeSpentInFeedIsPerDay) {
  // One interaction every hour for 2 days. Should be reported at 30 seconds per
  // interaction due to the interaction timeout. The 49th |OpenAction()| call
  // triggers reporting the UMA for the previous day.
  for (int i = 0; i < 49; ++i) {
    reporter_->OpenAction(kForYouStream, 0);
    task_environment_.FastForwardBy(base::TimeDelta::FromHours(1));
  }

  histogram_.ExpectUniqueTimeSample("ContentSuggestions.Feed.TimeSpentInFeed",
                                    base::TimeDelta::FromSeconds(30) * 24, 2);
}

TEST_F(MetricsReporterTest, TimeSpentIsPersisted) {
  // Verify that the previous test also works when |MetricsReporter| is
  // destroyed and recreated. The 49th |OpenAction()| call triggers reporting
  // the UMA for the previous day.
  for (int i = 0; i < 49; ++i) {
    reporter_->OpenAction(kForYouStream, 0);
    task_environment_.FastForwardBy(base::TimeDelta::FromHours(1));
    reporter_->OnEnterBackground();
    RecreateMetricsReporter();
  }

  histogram_.ExpectUniqueTimeSample("ContentSuggestions.Feed.TimeSpentInFeed",
                                    base::TimeDelta::FromSeconds(30) * 24, 2);
}

TEST_F(MetricsReporterTest, TimeSpentInFeedTracksWholeScrollTime) {
  reporter_->StreamScrollStart();
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(2));
  reporter_->StreamScrolled(kForYouStream, 1);
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));
  reporter_->OnEnterBackground();

  // Trigger reporting the persistent metrics the next day.
  task_environment_.FastForwardBy(base::TimeDelta::FromDays(1));
  RecreateMetricsReporter();

  histogram_.ExpectUniqueTimeSample("ContentSuggestions.Feed.TimeSpentInFeed",
                                    base::TimeDelta::FromSeconds(3), 1);
}

TEST_F(MetricsReporterTest, TurnOnAction) {
  reporter_->OtherUserAction(kForYouStream, FeedUserActionType::kTappedTurnOn);
  histogram_.ExpectUniqueSample("ContentSuggestions.Feed.UserActions",
                                FeedUserActionType::kTappedTurnOn, 1);
}

TEST_F(MetricsReporterTest, TurnOffAction) {
  reporter_->OtherUserAction(kForYouStream, FeedUserActionType::kTappedTurnOff);
  histogram_.ExpectUniqueSample("ContentSuggestions.Feed.UserActions",
                                FeedUserActionType::kTappedTurnOff, 1);
}

TEST_F(MetricsReporterTest, NetworkRequestCompleteReportsUma) {
  MetricsReporter::NetworkRequestComplete(NetworkRequestType::kListWebFeeds,
                                          200, base::TimeDelta::FromSeconds(2));

  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.Network.ResponseStatus.ListFollowedWebFeeds",
      200, 1);
  histogram_.ExpectUniqueTimeSample(
      "ContentSuggestions.Feed.Network.Duration.ListFollowedWebFeeds",
      base::TimeDelta::FromSeconds(2), 1);
}

}  // namespace feed
