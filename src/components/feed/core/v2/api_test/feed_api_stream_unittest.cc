// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback_helpers.h"
#include "base/feature_list.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/feed/core/common/pref_names.h"
#include "components/feed/core/shared_prefs/pref_names.h"
#include "components/feed/core/v2/api_test/feed_api_test.h"
#include "components/feed/core/v2/config.h"
#include "components/feed/core/v2/enums.h"
#include "components/feed/core/v2/feed_network.h"
#include "components/feed/core/v2/feed_stream.h"
#include "components/feed/core/v2/feedstore_util.h"
#include "components/feed/core/v2/public/feed_api.h"
#include "components/feed/core/v2/public/feed_service.h"
#include "components/feed/core/v2/public/stream_type.h"
#include "components/feed/core/v2/scheduling.h"
#include "components/feed/core/v2/test/callback_receiver.h"
#include "components/feed/core/v2/test/stream_builder.h"
#include "components/feed/feed_feature_list.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

// This file is primarily for testing access to the Feed content, though it
// contains some other miscellaneous tests.

namespace feed {
namespace test {
namespace {

TEST_F(FeedApiTest, IsArticlesListVisibleByDefault) {
  EXPECT_TRUE(stream_->IsArticlesListVisible());
}

TEST_F(FeedApiTest, DoNotRefreshIfArticlesListIsHidden) {
  profile_prefs_.SetBoolean(prefs::kArticlesListVisible, false);
  stream_->ExecuteRefreshTask(RefreshTaskId::kRefreshForYouFeed);
  EXPECT_FALSE(refresh_scheduler_.scheduled_run_times.count(
      RefreshTaskId::kRefreshForYouFeed));
  EXPECT_EQ(std::set<RefreshTaskId>({RefreshTaskId::kRefreshForYouFeed}),
            refresh_scheduler_.completed_tasks);
}

TEST_F(FeedApiTest, BackgroundRefreshForYouSuccess) {
  // Trigger a background refresh.
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  stream_->ExecuteRefreshTask(RefreshTaskId::kRefreshForYouFeed);
  WaitForIdleTaskQueue();

  // Verify the refresh happened and that we can load a stream without the
  // network.
  ASSERT_TRUE(refresh_scheduler_.completed_tasks.count(
      RefreshTaskId::kRefreshForYouFeed));
  EXPECT_EQ(LoadStreamStatus::kLoadedFromNetwork,
            metrics_reporter_->background_refresh_status);
  EXPECT_TRUE(response_translator_.InjectedResponseConsumed());
  EXPECT_FALSE(stream_->GetModel(kForYouStream));
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  EXPECT_EQ("loading -> 2 slices", surface.DescribeUpdates());
}

TEST_F(FeedApiTest, WebFeedDoesNotBackgroundRefresh) {
  {
    RefreshResponseData injected_response;
    injected_response.model_update_request = MakeTypicalInitialModelState();
    RequestSchedule schedule;
    schedule.anchor_time = kTestTimeEpoch;
    schedule.refresh_offsets = {base::TimeDelta::FromSeconds(12),
                                base::TimeDelta::FromSeconds(48)};

    injected_response.request_schedule = schedule;
    response_translator_.InjectResponse(std::move(injected_response));
  }

  TestWebFeedSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  // The request schedule should be ignored.
  EXPECT_TRUE(refresh_scheduler_.scheduled_run_times.empty());
}

TEST_F(FeedApiTest, BackgroundRefreshPrefetchesImages) {
  // Trigger a background refresh.
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  stream_->ExecuteRefreshTask(RefreshTaskId::kRefreshForYouFeed);
  EXPECT_EQ(0, prefetch_image_call_count_);
  WaitForIdleTaskQueue();

  std::vector<GURL> expected_fetches(
      {GURL("http://image0/"), GURL("http://favicon0/"), GURL("http://image1/"),
       GURL("http://favicon1/")});
  // Verify that images were prefetched.
  EXPECT_EQ(4, prefetch_image_call_count_);
  EXPECT_EQ(expected_fetches, prefetched_images_);
}

TEST_F(FeedApiTest, BackgroundRefreshNotAttemptedWhenModelIsLoading) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  stream_->ExecuteRefreshTask(RefreshTaskId::kRefreshForYouFeed);
  WaitForIdleTaskQueue();

  EXPECT_EQ(metrics_reporter_->Stream(kForYouStream).background_refresh_status,
            LoadStreamStatus::kModelAlreadyLoaded);
}

TEST_F(FeedApiTest, BackgroundRefreshNotAttemptedAfterModelIsLoaded) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  stream_->ExecuteRefreshTask(RefreshTaskId::kRefreshForYouFeed);
  WaitForIdleTaskQueue();

  EXPECT_EQ(metrics_reporter_->background_refresh_status,
            LoadStreamStatus::kModelAlreadyLoaded);
}

TEST_F(FeedApiTest, SurfaceReceivesInitialContent) {
  {
    auto model = CreateStreamModel();
    model->Update(MakeTypicalInitialModelState());
    stream_->LoadModelForTesting(kForYouStream, std::move(model));
  }
  TestForYouSurface surface(stream_.get());
  ASSERT_TRUE(surface.initial_state);
  const feedui::StreamUpdate& initial_state = surface.initial_state.value();
  ASSERT_EQ(2, initial_state.updated_slices().size());
  EXPECT_NE("", initial_state.updated_slices(0).slice().slice_id());
  EXPECT_EQ("f:0", initial_state.updated_slices(0)
                       .slice()
                       .xsurface_slice()
                       .xsurface_frame());
  EXPECT_NE("", initial_state.updated_slices(1).slice().slice_id());
  EXPECT_EQ("f:1", initial_state.updated_slices(1)
                       .slice()
                       .xsurface_slice()
                       .xsurface_frame());
  ASSERT_EQ(1, initial_state.new_shared_states().size());
  EXPECT_EQ("ss:0",
            initial_state.new_shared_states()[0].xsurface_shared_state());
}

TEST_F(FeedApiTest, SurfaceReceivesInitialContentLoadedAfterAttach) {
  TestForYouSurface surface(stream_.get());
  ASSERT_FALSE(surface.initial_state);
  {
    auto model = CreateStreamModel();
    model->Update(MakeTypicalInitialModelState());
    stream_->LoadModelForTesting(kForYouStream, std::move(model));
  }

  ASSERT_EQ("loading -> 2 slices", surface.DescribeUpdates());
  const feedui::StreamUpdate& initial_state = surface.initial_state.value();

  EXPECT_NE("", initial_state.updated_slices(0).slice().slice_id());
  EXPECT_EQ("f:0", initial_state.updated_slices(0)
                       .slice()
                       .xsurface_slice()
                       .xsurface_frame());
  EXPECT_NE("", initial_state.updated_slices(1).slice().slice_id());
  EXPECT_EQ("f:1", initial_state.updated_slices(1)
                       .slice()
                       .xsurface_slice()
                       .xsurface_frame());
  ASSERT_EQ(1, initial_state.new_shared_states().size());
  EXPECT_EQ("ss:0",
            initial_state.new_shared_states()[0].xsurface_shared_state());
}

TEST_F(FeedApiTest, SurfaceReceivesUpdatedContent) {
  {
    auto model = CreateStreamModel();
    model->ExecuteOperations(MakeTypicalStreamOperations());
    stream_->LoadModelForTesting(kForYouStream, std::move(model));
  }
  TestForYouSurface surface(stream_.get());
  // Remove #1, add #2.
  stream_->ExecuteOperations(
      kForYouStream, {
                         MakeOperation(MakeRemove(MakeClusterId(1))),
                         MakeOperation(MakeCluster(2, MakeRootId())),
                         MakeOperation(MakeContentNode(2, MakeClusterId(2))),
                         MakeOperation(MakeContent(2)),
                     });
  ASSERT_TRUE(surface.update);
  const feedui::StreamUpdate& initial_state = surface.initial_state.value();
  const feedui::StreamUpdate& update = surface.update.value();

  ASSERT_EQ("2 slices -> 2 slices", surface.DescribeUpdates());
  // First slice is just an ID that matches the old 1st slice ID.
  EXPECT_EQ(initial_state.updated_slices(0).slice().slice_id(),
            update.updated_slices(0).slice_id());
  // Second slice is a new xsurface slice.
  EXPECT_NE("", update.updated_slices(1).slice().slice_id());
  EXPECT_EQ("f:2",
            update.updated_slices(1).slice().xsurface_slice().xsurface_frame());
}

TEST_F(FeedApiTest, SurfaceReceivesSecondUpdatedContent) {
  {
    auto model = CreateStreamModel();
    model->ExecuteOperations(MakeTypicalStreamOperations());
    stream_->LoadModelForTesting(kForYouStream, std::move(model));
  }
  TestForYouSurface surface(stream_.get());
  // Add #2.
  stream_->ExecuteOperations(
      kForYouStream, {
                         MakeOperation(MakeCluster(2, MakeRootId())),
                         MakeOperation(MakeContentNode(2, MakeClusterId(2))),
                         MakeOperation(MakeContent(2)),
                     });

  // Clear the last update and add #3.
  stream_->ExecuteOperations(
      kForYouStream, {
                         MakeOperation(MakeCluster(3, MakeRootId())),
                         MakeOperation(MakeContentNode(3, MakeClusterId(3))),
                         MakeOperation(MakeContent(3)),
                     });

  // The last update should have only one new piece of content.
  // This verifies the current content set is tracked properly.
  ASSERT_EQ("2 slices -> 3 slices -> 4 slices", surface.DescribeUpdates());

  ASSERT_EQ(4, surface.update->updated_slices().size());
  EXPECT_FALSE(surface.update->updated_slices(0).has_slice());
  EXPECT_FALSE(surface.update->updated_slices(1).has_slice());
  EXPECT_FALSE(surface.update->updated_slices(2).has_slice());
  EXPECT_EQ("f:3", surface.update->updated_slices(3)
                       .slice()
                       .xsurface_slice()
                       .xsurface_frame());
}

TEST_F(FeedApiTest, RemoveAllContentResultsInZeroState) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  // Remove both pieces of content.
  stream_->ExecuteOperations(kForYouStream,
                             {
                                 MakeOperation(MakeRemove(MakeClusterId(0))),
                                 MakeOperation(MakeRemove(MakeClusterId(1))),
                             });

  ASSERT_EQ("loading -> 2 slices -> no-cards", surface.DescribeUpdates());
}

TEST_F(FeedApiTest, DetachSurface) {
  {
    auto model = CreateStreamModel();
    model->ExecuteOperations(MakeTypicalStreamOperations());
    stream_->LoadModelForTesting(kForYouStream, std::move(model));
  }
  TestForYouSurface surface(stream_.get());
  EXPECT_TRUE(surface.initial_state);
  surface.Detach();
  surface.Clear();

  // Arbitrary stream change. Surface should not see the update.
  stream_->ExecuteOperations(kForYouStream,
                             {
                                 MakeOperation(MakeRemove(MakeClusterId(1))),
                             });
  EXPECT_FALSE(surface.update);
}

TEST_F(FeedApiTest, FetchImage) {
  CallbackReceiver<NetworkResponse> receiver;
  stream_->FetchImage(GURL("https://example.com"), receiver.Bind());

  EXPECT_EQ("dummyresponse", receiver.GetResult()->response_bytes);
}

TEST_P(FeedStreamTestForAllStreamTypes, LoadFromNetwork) {
  {
    WaitForIdleTaskQueue();
    auto metadata = stream_->GetMetadata();
    metadata.set_consistency_token("token");
    stream_->SetMetadata(metadata);
  }

  // Store is empty, so we should fallback to a network request.
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  ASSERT_TRUE(network_.query_request_sent);
  EXPECT_EQ(0, network_.GetApiRequestCount<QueryInteractiveFeedDiscoverApi>());
  EXPECT_EQ(
      "token",
      network_.query_request_sent->feed_request().consistency_token().token());
  EXPECT_TRUE(response_translator_.InjectedResponseConsumed());

  EXPECT_EQ("loading -> 2 slices", surface.DescribeUpdates());
  // Verify the model is filled correctly.
  EXPECT_STRINGS_EQUAL(
      ModelStateFor(MakeTypicalInitialModelState()),
      stream_->GetModel(GetStreamType())->DumpStateForTesting());
  // Verify the data was written to the store.
  EXPECT_STRINGS_EQUAL(ModelStateFor(MakeTypicalInitialModelState()),
                       ModelStateFor(GetStreamType(), store_.get()));
}

TEST_F(FeedApiTest, WebFeedLoadWithNoSubscriptions) {
  TestWebFeedSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  EXPECT_EQ("loading -> no-subscriptions", surface.DescribeUpdates());
}

// Test that we use QueryInteractiveFeedDiscoverApi and QueryNextPageDiscoverApi
// when kDiscoFeedEndpoint is enabled.
TEST_F(FeedApiTest, LoadFromNetworkDiscoFeedEnabled) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(kDiscoFeedEndpoint);

  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  response_translator_.InjectResponse(MakeTypicalNextPageState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  EXPECT_EQ(1, network_.GetApiRequestCount<QueryInteractiveFeedDiscoverApi>());

  stream_->LoadMore(surface, base::DoNothing());
  WaitForIdleTaskQueue();

  EXPECT_EQ(1, network_.GetApiRequestCount<QueryNextPageDiscoverApi>());
  EXPECT_EQ("loading -> 2 slices -> 2 slices +spinner -> 4 slices",
            surface.DescribeUpdates());
}

TEST_P(FeedNetworkEndpointTest, TestAllNetworkEndpointConfigs) {
  SetUseFeedQueryRequestsForWebFeeds(GetWebFeedUsesFeedQueryRequests());

  // Enable WebFeed and subscribe to a page, so that we can check if the WebFeed
  // is refreshed by ForceRefreshForDebugging.
  base::test::ScopedFeatureList features;
  std::vector<base::Feature> enabled_features = {kWebFeed},
                             disabled_features = {};
  if (GetDiscoFeedEnabled()) {
    enabled_features.push_back(kDiscoFeedEndpoint);
  } else {
    disabled_features.push_back(kDiscoFeedEndpoint);
  }
  features.InitWithFeatures(enabled_features, disabled_features);

  // WebFeed stream is only fetched when there's a subscription.
  network_.InjectListWebFeedsResponse({MakeWireWebFeed("cats")});

  // Force a refresh that results in a successful load of both feed types.
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  response_translator_.InjectResponse(MakeTypicalInitialModelState());

  TestForYouSurface surface(stream_.get());
  TestWebFeedSurface web_feed_surface(stream_.get());
  WaitForIdleTaskQueue();

  EXPECT_EQ("2 slices", surface.DescribeState());
  EXPECT_EQ("2 slices", web_feed_surface.DescribeState());

  // Total 2 queries (Web + For You).
  EXPECT_EQ(2, network_.send_query_call_count);
  // API request to DiscoFeed (For You) - if enabled by feature
  EXPECT_EQ(GetDiscoFeedEnabled() ? 1 : 0,
            network_.GetApiRequestCount<QueryInteractiveFeedDiscoverApi>());
  // API request to WebFeedList if FeedQuery not enabled by config.
  EXPECT_EQ(GetWebFeedUsesFeedQueryRequests() ? 0 : 1,
            network_.GetApiRequestCount<WebFeedListContentsDiscoverApi>());
}

// Perform a background refresh when DiscoFeedEndpoint is enabled. A
// QueryBackgroundFeedDiscoverApi request should be made.
TEST_F(FeedApiTest, BackgroundRefreshDiscoFeedEnabled) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(kDiscoFeedEndpoint);

  // Trigger a background refresh.
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  stream_->ExecuteRefreshTask(RefreshTaskId::kRefreshForYouFeed);
  WaitForIdleTaskQueue();

  // Verify the refresh happened and that we can load a stream without the
  // network.
  ASSERT_TRUE(refresh_scheduler_.completed_tasks.count(
      RefreshTaskId::kRefreshForYouFeed));
  EXPECT_EQ(1, network_.GetApiRequestCount<QueryBackgroundFeedDiscoverApi>());
  EXPECT_EQ(LoadStreamStatus::kLoadedFromNetwork,
            metrics_reporter_->background_refresh_status);
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  EXPECT_EQ("loading -> 2 slices", surface.DescribeUpdates());
}

TEST_F(FeedApiTest, ForceRefreshForDebugging) {
  // WebFeed stream is only fetched when there's a subscription.
  network_.InjectListWebFeedsResponse({MakeWireWebFeed("cats")});

  // Force a refresh that results in a successful load of both feed types.
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  stream_->ForceRefreshForDebugging();

  WaitForIdleTaskQueue();

  is_offline_ = true;

  TestForYouSurface surface(stream_.get());
  TestWebFeedSurface web_feed_surface(stream_.get());
  WaitForIdleTaskQueue();
  EXPECT_EQ("2 slices", surface.DescribeState());
  EXPECT_EQ("2 slices", web_feed_surface.DescribeState());
}

TEST_F(FeedApiTest, RefreshScheduleFlow) {
  // Inject a typical network response, with a server-defined request schedule.
  {
    RequestSchedule schedule;
    schedule.anchor_time = kTestTimeEpoch;
    schedule.refresh_offsets = {base::TimeDelta::FromSeconds(12),
                                base::TimeDelta::FromSeconds(48)};
    RefreshResponseData response_data;
    response_data.model_update_request = MakeTypicalInitialModelState();
    response_data.request_schedule = schedule;

    response_translator_.InjectResponse(std::move(response_data));

    // Load the stream, and then destroy the surface to allow background
    // refresh.
    TestForYouSurface surface(stream_.get());
    WaitForIdleTaskQueue();
    UnloadModel(surface.GetStreamType());
  }

  // Verify the first refresh was scheduled.
  EXPECT_EQ(base::TimeDelta::FromSeconds(12),
            refresh_scheduler_
                .scheduled_run_times[RefreshTaskId::kRefreshForYouFeed]);

  // Simulate executing the background task.
  refresh_scheduler_.Clear();
  task_environment_.AdvanceClock(base::TimeDelta::FromSeconds(12));
  stream_->ExecuteRefreshTask(RefreshTaskId::kRefreshForYouFeed);
  WaitForIdleTaskQueue();

  // Verify |RefreshTaskComplete()| was called and next refresh was scheduled.
  EXPECT_TRUE(refresh_scheduler_.completed_tasks.count(
      RefreshTaskId::kRefreshForYouFeed));
  EXPECT_EQ(base::TimeDelta::FromSeconds(48 - 12),
            refresh_scheduler_
                .scheduled_run_times[RefreshTaskId::kRefreshForYouFeed]);

  // Simulate executing the background task again.
  refresh_scheduler_.Clear();
  task_environment_.AdvanceClock(base::TimeDelta::FromSeconds(48 - 12));
  stream_->ExecuteRefreshTask(RefreshTaskId::kRefreshForYouFeed);
  WaitForIdleTaskQueue();

  // Verify |RefreshTaskComplete()| was called and next refresh was scheduled.
  EXPECT_TRUE(refresh_scheduler_.completed_tasks.count(
      RefreshTaskId::kRefreshForYouFeed));
  EXPECT_EQ(GetFeedConfig().default_background_refresh_interval,
            refresh_scheduler_
                .scheduled_run_times[RefreshTaskId::kRefreshForYouFeed]);
}

TEST_F(FeedApiTest, ForceRefreshIfMissedScheduledRefresh) {
  // Inject a typical network response, with a server-defined request schedule.
  {
    RequestSchedule schedule;
    schedule.anchor_time = kTestTimeEpoch;
    schedule.refresh_offsets = {base::TimeDelta::FromSeconds(12),
                                base::TimeDelta::FromSeconds(48)};
    RefreshResponseData response_data;
    response_data.model_update_request = MakeTypicalInitialModelState();
    response_data.request_schedule = schedule;

    response_translator_.InjectResponse(std::move(response_data));
  }
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  ASSERT_EQ(1, network_.send_query_call_count);
  surface.Detach();
  stream_->UnloadModel(surface.GetStreamType());

  // Ensure a refresh is foreced only after a scheduled refresh was missed.
  // First, load the stream after 11 seconds.
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  task_environment_.AdvanceClock(base::TimeDelta::FromSeconds(11));
  surface.Attach(stream_.get());
  WaitForIdleTaskQueue();
  ASSERT_EQ(1, network_.send_query_call_count);  // no refresh yet

  // Load the stream after 13 seconds. We missed the scheduled refresh at
  // 12 seconds.
  surface.Detach();
  stream_->UnloadModel(surface.GetStreamType());
  task_environment_.AdvanceClock(base::TimeDelta::FromSeconds(2));
  surface.Attach(stream_.get());
  WaitForIdleTaskQueue();

  ASSERT_EQ(2, network_.send_query_call_count);
  EXPECT_EQ(LoadStreamStatus::kDataInStoreStaleMissedLastRefresh,
            metrics_reporter_->load_stream_from_store_status);
}

TEST_P(FeedStreamTestForAllStreamTypes, LoadFromNetworkBecauseStoreIsStale) {
  // Fill the store with stream data that is just barely stale, and verify we
  // fetch new data over the network.
  store_->OverwriteStream(
      GetStreamType(),
      MakeTypicalInitialModelState(
          /*first_cluster_id=*/0,
          kTestTimeEpoch -
              GetFeedConfig().GetStalenessThreshold(GetStreamType()) -
              base::TimeDelta::FromMinutes(1)),
      base::DoNothing());

  // Store is stale, so we should fallback to a network request.
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  ASSERT_TRUE(network_.query_request_sent);
  EXPECT_TRUE(response_translator_.InjectedResponseConsumed());
  ASSERT_TRUE(surface.initial_state);
}

// Same as LoadFromNetworkBecauseStoreIsStale, but with expired content.
TEST_F(FeedApiTest, LoadFromNetworkBecauseStoreIsExpired) {
  base::HistogramTester histograms;
  const base::TimeDelta kContentAge =
      GetFeedConfig().content_expiration_threshold +
      base::TimeDelta::FromMinutes(1);
  store_->OverwriteStream(
      kForYouStream,
      MakeTypicalInitialModelState(
          /*first_cluster_id=*/0, kTestTimeEpoch - kContentAge),
      base::DoNothing());

  // Store is stale, so we should fallback to a network request.
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  ASSERT_TRUE(network_.query_request_sent);
  EXPECT_TRUE(response_translator_.InjectedResponseConsumed());
  ASSERT_TRUE(surface.initial_state);
  EXPECT_EQ(LoadStreamStatus::kDataInStoreIsExpired,
            metrics_reporter_->load_stream_from_store_status);
  histograms.ExpectUniqueTimeSample(
      "ContentSuggestions.Feed.ContentAgeOnLoad.BlockingRefresh", kContentAge,
      1);
}

TEST_F(FeedApiTest, LoadStaleDataBecauseNetworkRequestFails) {
  // Fill the store with stream data that is just barely stale.
  base::HistogramTester histograms;
  const base::TimeDelta kContentAge =
      GetFeedConfig().stale_content_threshold + base::TimeDelta::FromMinutes(1);
  store_->OverwriteStream(
      kForYouStream,
      MakeTypicalInitialModelState(
          /*first_cluster_id=*/0, kTestTimeEpoch - kContentAge),
      base::DoNothing());

  // Store is stale, so we should fallback to a network request. Since we didn't
  // inject a network response, the network update will fail.
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  ASSERT_TRUE(network_.query_request_sent);
  EXPECT_EQ("loading -> 2 slices", surface.DescribeUpdates());
  EXPECT_EQ(LoadStreamStatus::kDataInStoreIsStale,
            metrics_reporter_->load_stream_from_store_status);
  EXPECT_EQ(LoadStreamStatus::kLoadedStaleDataFromStoreDueToNetworkFailure,
            metrics_reporter_->load_stream_status);
  histograms.ExpectUniqueTimeSample(
      "ContentSuggestions.Feed.ContentAgeOnLoad.NotRefreshed", kContentAge, 1);
}

TEST_P(FeedStreamTestForAllStreamTypes, LoadFailsStoredDataIsExpired) {
  // Fill the store with stream data that is just barely expired.
  store_->OverwriteStream(
      GetStreamType(),
      MakeTypicalInitialModelState(
          /*first_cluster_id=*/0,
          kTestTimeEpoch - GetFeedConfig().content_expiration_threshold -
              base::TimeDelta::FromMinutes(1)),
      base::DoNothing());

  // Store contains expired content, so we should fallback to a network request.
  // Since we didn't inject a network response, the network update will fail.
  TestSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  ASSERT_TRUE(network_.query_request_sent);
  EXPECT_EQ("loading -> cant-refresh", surface.DescribeUpdates());
  EXPECT_EQ(LoadStreamStatus::kDataInStoreIsExpired,
            metrics_reporter_->load_stream_from_store_status);
  EXPECT_EQ(LoadStreamStatus::kProtoTranslationFailed,
            metrics_reporter_->load_stream_status);
}

TEST_F(FeedApiTest, LoadFromNetworkFailsDueToProtoTranslation) {
  // No data in the store, so we should fetch from the network.
  // The network will respond with an empty response, which should fail proto
  // translation.
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  EXPECT_EQ(LoadStreamStatus::kProtoTranslationFailed,
            metrics_reporter_->load_stream_status);
}
TEST_F(FeedApiTest, DoNotLoadFromNetworkWhenOffline) {
  is_offline_ = true;
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  EXPECT_EQ(LoadStreamStatus::kCannotLoadFromNetworkOffline,
            metrics_reporter_->load_stream_status);
  EXPECT_EQ("loading -> cant-refresh", surface.DescribeUpdates());
}

TEST_F(FeedApiTest, DoNotLoadStreamWhenArticleListIsHidden) {
  profile_prefs_.SetBoolean(prefs::kArticlesListVisible, false);
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  EXPECT_EQ(LoadStreamStatus::kLoadNotAllowedArticlesListHidden,
            metrics_reporter_->load_stream_status);
  EXPECT_EQ("no-cards", surface.DescribeUpdates());
}

TEST_F(FeedApiTest, DoNotLoadStreamWhenEulaIsNotAccepted) {
  is_eula_accepted_ = false;
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  EXPECT_EQ(LoadStreamStatus::kLoadNotAllowedEulaNotAccepted,
            metrics_reporter_->load_stream_status);
  EXPECT_EQ("no-cards", surface.DescribeUpdates());
}

TEST_F(FeedApiTest, LoadStreamAfterEulaIsAccepted) {
  // Connect a surface before the EULA is accepted.
  is_eula_accepted_ = false;
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  ASSERT_EQ("no-cards", surface.DescribeUpdates());

  // Accept EULA, our surface should receive data.
  is_eula_accepted_ = true;
  stream_->OnEulaAccepted();
  WaitForIdleTaskQueue();

  EXPECT_EQ("loading -> 2 slices", surface.DescribeUpdates());
}

TEST_F(FeedApiTest, ForceSignedOutRequestAfterHistoryIsDeleted) {
  stream_->OnAllHistoryDeleted();

  const std::string kSessionId = "session-id";

  // This test injects response post translation/parsing. We have to configure
  // the response data that should come out of the translator, which should
  // mark the request/response as having been made from the signed-out state.
  StreamModelUpdateRequestGenerator model_generator;
  model_generator.signed_in = false;

  // Advance the clock, but not past the end of the forced-signed-out period.
  task_environment_.FastForwardBy(kSuppressRefreshDuration -
                                  base::TimeDelta::FromSeconds(1));

  // Refresh the feed, queuing up a signed-out response.
  response_translator_.InjectResponse(model_generator.MakeFirstPage(),
                                      kSessionId);
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  // Validate that the network request was sent as signed out.
  ASSERT_EQ(1, network_.send_query_call_count);
  EXPECT_EQ("", network_.last_gaia);
  EXPECT_TRUE(network_.query_request_sent->feed_request()
                  .client_info()
                  .chrome_client_info()
                  .session_id()
                  .empty());

  // Validate the downstream consumption of the response.
  EXPECT_EQ("loading -> 2 slices", surface.DescribeUpdates());
  EXPECT_EQ(kSessionId, stream_->GetMetadata().session_id().token());
  EXPECT_FALSE(stream_->GetModel(surface.GetStreamType())->signed_in());

  // Advance the clock beyond the forced signed out period.
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(2));
  EXPECT_FALSE(stream_->GetModel(surface.GetStreamType())->signed_in());

  // Requests for subsequent pages continue the use existing session.
  // Subsequent responses may omit the session id.
  response_translator_.InjectResponse(model_generator.MakeNextPage());
  stream_->LoadMore(surface, base::DoNothing());
  WaitForIdleTaskQueue();

  // Validate that the network request was sent as signed out and
  // contained the session id.
  ASSERT_EQ(2, network_.send_query_call_count);
  EXPECT_EQ("", network_.last_gaia);
  EXPECT_EQ(kSessionId, stream_->GetMetadata().session_id().token());
  EXPECT_EQ(network_.query_request_sent->feed_request()
                .client_info()
                .chrome_client_info()
                .session_id(),
            kSessionId);

  // The model should still be in the signed-out state.
  EXPECT_FALSE(stream_->GetModel(kForYouStream)->signed_in());

  // Force a refresh of the feed by clearing the cache. The request for the
  // first page should revert back to signed-in. The response data will denote
  // a signed-in response with no session_id.
  model_generator.signed_in = true;
  response_translator_.InjectResponse(model_generator.MakeFirstPage());
  stream_->OnCacheDataCleared();
  WaitForIdleTaskQueue();

  // Validate that a signed-in request was sent.
  ASSERT_EQ(3, network_.send_query_call_count);
  EXPECT_NE("", network_.last_gaia);

  // The model should now be in the signed-in state.
  EXPECT_TRUE(stream_->GetModel(kForYouStream)->signed_in());
  EXPECT_TRUE(stream_->GetMetadata().session_id().token().empty());
}

TEST_F(FeedApiTest, WebFeedUsesSignedInRequestAfterHistoryIsDeleted) {
  // WebFeed stream is only fetched when there's a subscription.
  network_.InjectListWebFeedsResponse({MakeWireWebFeed("cats")});

  stream_->OnAllHistoryDeleted();

  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestWebFeedSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  ASSERT_EQ(1, network_.send_query_call_count);
  EXPECT_NE("", network_.last_gaia);
}

TEST_F(FeedApiTest, AllowSignedInRequestAfterHistoryIsDeletedAfterDelay) {
  stream_->OnAllHistoryDeleted();
  task_environment_.FastForwardBy(kSuppressRefreshDuration +
                                  base::TimeDelta::FromSeconds(1));
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  EXPECT_EQ("loading -> 2 slices", surface.DescribeUpdates());
  EXPECT_NE("", network_.last_gaia);
  EXPECT_TRUE(stream_->GetMetadata().session_id().token().empty());
}

TEST_F(FeedApiTest, ShouldMakeFeedQueryRequestConsumesQuota) {
  LoadStreamStatus status = LoadStreamStatus::kNoStatus;
  for (; status == LoadStreamStatus::kNoStatus;
       status = stream_
                    ->ShouldMakeFeedQueryRequest(kForYouStream,
                                                 LoadType::kInitialLoad)
                    .load_stream_status) {
  }

  ASSERT_EQ(LoadStreamStatus::kCannotLoadFromNetworkThrottled, status);
}

TEST_F(FeedApiTest, LoadStreamFromStore) {
  // Fill the store with stream data that is just barely fresh, and verify it
  // loads.
  store_->OverwriteStream(
      kForYouStream,
      MakeTypicalInitialModelState(
          /*first_cluster_id=*/0, kTestTimeEpoch -
                                      GetFeedConfig().stale_content_threshold +
                                      base::TimeDelta::FromMinutes(1)),
      base::DoNothing());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  ASSERT_EQ("loading -> 2 slices", surface.DescribeUpdates());
  EXPECT_FALSE(network_.query_request_sent);
  // Verify the model is filled correctly.
  EXPECT_STRINGS_EQUAL(ModelStateFor(MakeTypicalInitialModelState()),
                       stream_->GetModel(kForYouStream)->DumpStateForTesting());
}

TEST_F(FeedApiTest, LoadingSpinnerIsSentInitially) {
  store_->OverwriteStream(kForYouStream, MakeTypicalInitialModelState(),
                          base::DoNothing());
  TestForYouSurface surface(stream_.get());

  ASSERT_EQ("loading", surface.DescribeUpdates());
}

TEST_F(FeedApiTest, DetachSurfaceWhileLoadingModel) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  surface.Detach();
  WaitForIdleTaskQueue();

  EXPECT_EQ("loading", surface.DescribeUpdates());
  EXPECT_TRUE(network_.query_request_sent);
}

TEST_F(FeedApiTest, AttachMultipleSurfacesLoadsModelOnce) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  TestForYouSurface other_surface(stream_.get());
  WaitForIdleTaskQueue();

  ASSERT_EQ(1, network_.send_query_call_count);
  ASSERT_EQ("loading -> 2 slices", surface.DescribeUpdates());
  ASSERT_EQ("loading -> 2 slices", other_surface.DescribeUpdates());

  // After load, another surface doesn't trigger any tasks,
  // and immediately has content.
  TestForYouSurface later_surface(stream_.get());

  ASSERT_EQ("2 slices", later_surface.DescribeUpdates());
  EXPECT_TRUE(IsTaskQueueIdle());
}

TEST_P(FeedStreamTestForAllStreamTypes, ModelChangesAreSavedToStorage) {
  store_->OverwriteStream(GetStreamType(), MakeTypicalInitialModelState(),
                          base::DoNothing());
  TestSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  ASSERT_TRUE(surface.initial_state);

  // Remove #1, add #2.
  const std::vector<feedstore::DataOperation> operations = {
      MakeOperation(MakeRemove(MakeClusterId(1))),
      MakeOperation(MakeCluster(2, MakeRootId())),
      MakeOperation(MakeContentNode(2, MakeClusterId(2))),
      MakeOperation(MakeContent(2)),
  };
  stream_->ExecuteOperations(GetStreamType(), operations);

  WaitForIdleTaskQueue();

  // Verify changes are applied to storage.
  EXPECT_STRINGS_EQUAL(
      ModelStateFor(MakeTypicalInitialModelState(), operations),
      ModelStateFor(GetStreamType(), store_.get()));

  // Unload and reload the model from the store, and verify we can still apply
  // operations correctly.
  surface.Detach();
  surface.Clear();
  UnloadModel(GetStreamType());
  surface.Attach(stream_.get());
  WaitForIdleTaskQueue();
  ASSERT_TRUE(surface.initial_state);

  // Remove #2, add #3.
  const std::vector<feedstore::DataOperation> operations2 = {
      MakeOperation(MakeRemove(MakeClusterId(2))),
      MakeOperation(MakeCluster(3, MakeRootId())),
      MakeOperation(MakeContentNode(3, MakeClusterId(3))),
      MakeOperation(MakeContent(3)),
  };
  stream_->ExecuteOperations(surface.GetStreamType(), operations2);

  WaitForIdleTaskQueue();
  EXPECT_STRINGS_EQUAL(
      ModelStateFor(MakeTypicalInitialModelState(), operations, operations2),
      ModelStateFor(GetStreamType(), store_.get()));
}

TEST_F(FeedApiTest, ReportSliceViewedIdentifiesCorrectIndex) {
  store_->OverwriteStream(kForYouStream, MakeTypicalInitialModelState(),
                          base::DoNothing());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  stream_->ReportSliceViewed(
      surface.GetSurfaceId(), surface.GetStreamType(),
      surface.initial_state->updated_slices(1).slice().slice_id());
  EXPECT_EQ(1, metrics_reporter_->slice_viewed_index);
}

TEST_F(FeedApiTest, ReportOpenInNewTabAction) {
  store_->OverwriteStream(kForYouStream, MakeTypicalInitialModelState(),
                          base::DoNothing());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  base::UserActionTester user_actions;

  stream_->ReportOpenInNewTabAction(
      GURL(), surface.GetStreamType(),
      surface.initial_state->updated_slices(1).slice().slice_id());

  EXPECT_EQ(1, user_actions.GetActionCount(
                   "ContentSuggestions.Feed.CardAction.OpenInNewTab"));
}

TEST_F(FeedApiTest, HasUnreadContentAfterLoadFromNetwork) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestUnreadContentObserver observer;
  stream_->AddUnreadContentObserver(kForYouStream, &observer);
  TestForYouSurface surface(stream_.get());

  WaitForIdleTaskQueue();

  EXPECT_EQ(std::vector<bool>({false, true}), observer.calls);
}

TEST_F(FeedApiTest, HasUnreadContentInitially) {
  // Prime the feed with new content.
  {
    response_translator_.InjectResponse(MakeTypicalInitialModelState());
    TestForYouSurface surface(stream_.get());
    WaitForIdleTaskQueue();
  }

  // Reload FeedStream. Add an observer before initialization completes.
  // After initialization, the observer will be informed about unread content.
  CreateStream(/*wait_for_initialization*/ false);
  TestUnreadContentObserver observer;
  stream_->AddUnreadContentObserver(kForYouStream, &observer);
  WaitForIdleTaskQueue();

  EXPECT_EQ(std::vector<bool>({true}), observer.calls);
}

TEST_F(FeedApiTest, NetworkFetchWithNoNewContentDoesNotProvideUnreadContent) {
  TestUnreadContentObserver observer;
  stream_->AddUnreadContentObserver(kForYouStream, &observer);
  // Load content from the network, and view it.
  {
    response_translator_.InjectResponse(MakeTypicalInitialModelState());
    TestForYouSurface surface(stream_.get());
    WaitForIdleTaskQueue();

    stream_->ReportSliceViewed(
        surface.GetSurfaceId(), surface.GetStreamType(),
        surface.initial_state->updated_slices(1).slice().slice_id());
  }
  // Wait until the feed content is stale.

  task_environment_.FastForwardBy(base::TimeDelta::FromHours(100));

  // Load content from the network again. This time there is no new content.
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  EXPECT_EQ(std::vector<bool>({false, true, false}), observer.calls);
}

TEST_F(FeedApiTest, RemovedUnreadContentObserverDoesNotReceiveCalls) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestUnreadContentObserver observer;
  stream_->AddUnreadContentObserver(kForYouStream, &observer);
  stream_->RemoveUnreadContentObserver(kForYouStream, &observer);
  TestForYouSurface surface(stream_.get());

  WaitForIdleTaskQueue();

  EXPECT_EQ(std::vector<bool>({false}), observer.calls);
}

TEST_F(FeedApiTest, DeletedUnreadContentObserverDoesNotCrash) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  {
    TestUnreadContentObserver observer;
    stream_->AddUnreadContentObserver(kForYouStream, &observer);
  }
  TestForYouSurface surface(stream_.get());

  WaitForIdleTaskQueue();
}

TEST_F(FeedApiTest, HasUnreadContentAfterLoadFromStore) {
  store_->OverwriteStream(kForYouStream, MakeTypicalInitialModelState(),
                          base::DoNothing());
  TestForYouSurface surface(stream_.get());
  TestUnreadContentObserver observer;
  stream_->AddUnreadContentObserver(kForYouStream, &observer);

  WaitForIdleTaskQueue();

  EXPECT_EQ(std::vector<bool>({true}), observer.calls);
}

TEST_F(FeedApiTest, ReportSliceViewedUpdatesObservers) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  TestUnreadContentObserver observer;
  stream_->AddUnreadContentObserver(kForYouStream, &observer);
  WaitForIdleTaskQueue();

  stream_->ReportSliceViewed(
      surface.GetSurfaceId(), surface.GetStreamType(),
      surface.initial_state->updated_slices(1).slice().slice_id());
  task_environment_.RunUntilIdle();

  EXPECT_EQ(std::vector<bool>({true, false}), observer.calls);

  // Verify that the fact the stream was viewed persists.
  CreateStream();

  TestUnreadContentObserver observer2;
  stream_->AddUnreadContentObserver(kForYouStream, &observer2);
  TestForYouSurface surface2(stream_.get());
  WaitForIdleTaskQueue();

  EXPECT_EQ(std::vector<bool>({false}), observer2.calls);
}

TEST_P(FeedStreamTestForAllStreamTypes, LoadMoreAppendsContent) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  ASSERT_EQ("loading -> 2 slices", surface.DescribeUpdates());

  // Load page 2.
  response_translator_.InjectResponse(MakeTypicalNextPageState(2));
  CallbackReceiver<bool> callback;
  stream_->LoadMore(surface, callback.Bind());
  // Ensure metrics reporter was informed at the start of the operation.
  EXPECT_EQ(surface.GetSurfaceId(), metrics_reporter_->load_more_surface_id);
  WaitForIdleTaskQueue();
  ASSERT_EQ(absl::optional<bool>(true), callback.GetResult());
  EXPECT_EQ("2 slices +spinner -> 4 slices", surface.DescribeUpdates());

  // Load page 3.
  response_translator_.InjectResponse(MakeTypicalNextPageState(3));
  stream_->LoadMore(surface, callback.Bind());

  WaitForIdleTaskQueue();
  ASSERT_EQ(absl::optional<bool>(true), callback.GetResult());
  EXPECT_EQ("4 slices +spinner -> 6 slices", surface.DescribeUpdates());
}

TEST_P(FeedStreamTestForAllStreamTypes, LoadMorePersistsData) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  ASSERT_EQ("loading -> 2 slices", surface.DescribeUpdates());

  // Load page 2.
  response_translator_.InjectResponse(MakeTypicalNextPageState(2));
  CallbackReceiver<bool> callback;
  stream_->LoadMore(surface, callback.Bind());

  WaitForIdleTaskQueue();
  ASSERT_EQ(absl::optional<bool>(true), callback.GetResult());

  // Verify stored state is equivalent to in-memory model.
  EXPECT_STRINGS_EQUAL(
      stream_->GetModel(GetStreamType())->DumpStateForTesting(),
      ModelStateFor(GetStreamType(), store_.get()));
}

TEST_F(FeedApiTest, LoadMorePersistAndLoadMore) {
  // Verify we can persist a LoadMore, and then do another LoadMore after
  // reloading state.
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  ASSERT_EQ("loading -> 2 slices", surface.DescribeUpdates());

  // Load page 2.
  response_translator_.InjectResponse(MakeTypicalNextPageState(2));
  CallbackReceiver<bool> callback;
  stream_->LoadMore(surface, callback.Bind());
  WaitForIdleTaskQueue();
  ASSERT_EQ(absl::optional<bool>(true), callback.GetResult());

  surface.Detach();
  UnloadModel(kForYouStream);

  // Load page 3.
  surface.Attach(stream_.get());
  response_translator_.InjectResponse(MakeTypicalNextPageState(3));
  WaitForIdleTaskQueue();
  callback.Clear();
  surface.Clear();
  stream_->LoadMore(surface, callback.Bind());
  WaitForIdleTaskQueue();

  ASSERT_EQ(absl::optional<bool>(true), callback.GetResult());
  ASSERT_EQ("4 slices +spinner -> 6 slices", surface.DescribeUpdates());
  // Verify stored state is equivalent to in-memory model.
  EXPECT_STRINGS_EQUAL(
      stream_->GetModel(surface.GetStreamType())->DumpStateForTesting(),
      ModelStateFor(kForYouStream, store_.get()));
}

TEST_F(FeedApiTest, LoadMoreSendsTokens) {
  {
    auto metadata = stream_->GetMetadata();
    metadata.set_consistency_token("token");
    stream_->SetMetadata(metadata);
  }

  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  ASSERT_EQ("loading -> 2 slices", surface.DescribeUpdates());

  response_translator_.InjectResponse(MakeTypicalNextPageState(2));
  CallbackReceiver<bool> callback;
  stream_->LoadMore(surface, callback.Bind());

  WaitForIdleTaskQueue();
  ASSERT_EQ("2 slices +spinner -> 4 slices", surface.DescribeUpdates());

  EXPECT_EQ(
      "token",
      network_.query_request_sent->feed_request().consistency_token().token());
  EXPECT_EQ("page-2", network_.query_request_sent->feed_request()
                          .feed_query()
                          .next_page_token()
                          .next_page_token()
                          .next_page_token());

  response_translator_.InjectResponse(MakeTypicalNextPageState(3));
  stream_->LoadMore(surface, callback.Bind());

  WaitForIdleTaskQueue();
  ASSERT_EQ("4 slices +spinner -> 6 slices", surface.DescribeUpdates());

  EXPECT_EQ(
      "token",
      network_.query_request_sent->feed_request().consistency_token().token());
  EXPECT_EQ("page-3", network_.query_request_sent->feed_request()
                          .feed_query()
                          .next_page_token()
                          .next_page_token()
                          .next_page_token());
}

TEST_F(FeedApiTest, LoadMoreAbortsIfNoNextPageToken) {
  {
    std::unique_ptr<StreamModelUpdateRequest> initial_state =
        MakeTypicalInitialModelState();
    initial_state->stream_data.clear_next_page_token();
    response_translator_.InjectResponse(std::move(initial_state));
  }
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  CallbackReceiver<bool> callback;
  stream_->LoadMore(surface, callback.Bind());
  WaitForIdleTaskQueue();

  // LoadMore fails, and does not make an additional request.
  EXPECT_EQ(absl::optional<bool>(false), callback.GetResult());
  ASSERT_EQ(1, network_.send_query_call_count);
  EXPECT_EQ("loading -> 2 slices", surface.DescribeUpdates());
  EXPECT_EQ(absl::nullopt, metrics_reporter_->load_more_surface_id)
      << "metrics reporter was informed about a load more operation which "
         "didn't begin";
}

TEST_F(FeedApiTest, LoadMoreFail) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  ASSERT_EQ("loading -> 2 slices", surface.DescribeUpdates());

  // Don't inject another response, which results in a proto translation
  // failure.
  CallbackReceiver<bool> callback;
  stream_->LoadMore(surface, callback.Bind());
  WaitForIdleTaskQueue();

  EXPECT_EQ(absl::optional<bool>(false), callback.GetResult());
  EXPECT_EQ("2 slices +spinner -> 2 slices", surface.DescribeUpdates());
}

TEST_F(FeedApiTest, LoadMoreWithClearAllInResponse) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  ASSERT_EQ("loading -> 2 slices", surface.DescribeUpdates());

  // Use a different initial state (which includes a CLEAR_ALL).
  response_translator_.InjectResponse(MakeTypicalInitialModelState(5));
  CallbackReceiver<bool> callback;
  stream_->LoadMore(surface, callback.Bind());

  WaitForIdleTaskQueue();
  ASSERT_EQ(absl::optional<bool>(true), callback.GetResult());

  // Verify stored state is equivalent to in-memory model.
  EXPECT_STRINGS_EQUAL(
      stream_->GetModel(surface.GetStreamType())->DumpStateForTesting(),
      ModelStateFor(kForYouStream, store_.get()));

  // Verify the new state has been pushed to |surface|.
  ASSERT_EQ("2 slices +spinner -> 2 slices", surface.DescribeUpdates());

  const feedui::StreamUpdate& initial_state = surface.update.value();
  ASSERT_EQ(2, initial_state.updated_slices().size());
  EXPECT_NE("", initial_state.updated_slices(0).slice().slice_id());
  EXPECT_EQ("f:5", initial_state.updated_slices(0)
                       .slice()
                       .xsurface_slice()
                       .xsurface_frame());
  EXPECT_NE("", initial_state.updated_slices(1).slice().slice_id());
  EXPECT_EQ("f:6", initial_state.updated_slices(1)
                       .slice()
                       .xsurface_slice()
                       .xsurface_frame());
}

TEST_F(FeedApiTest, LoadMoreBeforeLoad) {
  CallbackReceiver<bool> callback;
  TestForYouSurface surface;
  stream_->LoadMore(surface, callback.Bind());

  EXPECT_EQ(absl::optional<bool>(false), callback.GetResult());
}

TEST_F(FeedApiTest, ReadNetworkResponse) {
  base::HistogramTester histograms;
  network_.InjectRealFeedQueryResponse();
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  ASSERT_EQ("loading -> 10 slices", surface.DescribeUpdates());

  // Verify we're processing some of the data on the request.

  // The response has a privacy_notice_fulfilled=true.
  histograms.ExpectUniqueSample(
      "ContentSuggestions.Feed.ActivityLoggingEnabled", 1, 1);

  // A request schedule with two entries was in the response. The first entry
  // should have already been scheduled/consumed, leaving only the second
  // entry still in the the refresh_offsets vector.
  RequestSchedule schedule = prefs::GetRequestSchedule(
      RefreshTaskId::kRefreshForYouFeed, profile_prefs_);
  EXPECT_EQ(std::vector<base::TimeDelta>({
                base::TimeDelta::FromSeconds(86308) +
                    base::TimeDelta::FromNanoseconds(822963644),
                base::TimeDelta::FromSeconds(120000),
            }),
            schedule.refresh_offsets);

  // The stream's user attributes are set, so activity logging is enabled.
  EXPECT_TRUE(stream_->IsActivityLoggingEnabled(kForYouStream));
  // This network response has content.
  EXPECT_TRUE(stream_->HasUnreadContent(kForYouStream));
}

TEST_F(FeedApiTest, ReadNetworkResponseWithNoContent) {
  base::HistogramTester histograms;
  network_.InjectRealFeedQueryResponseWithNoContent();
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  ASSERT_EQ("loading -> loading -> no-cards", surface.DescribeUpdates());

  // This network response has no content.
  EXPECT_FALSE(stream_->HasUnreadContent(kForYouStream));
}

TEST_F(FeedApiTest, ClearAllAfterLoadResultsInRefresh) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  stream_->OnCacheDataCleared();  // triggers ClearAll().

  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  WaitForIdleTaskQueue();

  EXPECT_EQ("loading -> 2 slices -> loading -> 2 slices",
            surface.DescribeUpdates());
}

TEST_F(FeedApiTest, ClearAllWithNoSurfacesAttachedDoesNotReload) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  surface.Detach();

  stream_->OnCacheDataCleared();  // triggers ClearAll().
  WaitForIdleTaskQueue();

  EXPECT_EQ("loading -> 2 slices", surface.DescribeUpdates());
}

TEST_F(FeedApiTest, ClearAllWhileLoadingMoreDoesNotLoadMore) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  CallbackReceiver<bool> cr;
  stream_->LoadMore(surface, cr.Bind());
  response_translator_.InjectResponse(MakeTypicalNextPageState(2));
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  stream_->OnCacheDataCleared();  // triggers ClearAll().
  WaitForIdleTaskQueue();

  EXPECT_EQ(false, cr.GetResult());
  EXPECT_EQ(
      "loading -> 2 slices -> 2 slices +spinner -> 2 slices -> loading -> 2 "
      "slices",
      surface.DescribeUpdates());
}

TEST_F(FeedApiTest, ClearAllWipesAllState) {
  // Trigger saving a consistency token, so it can be cleared later.
  network_.consistency_token = "token-11";
  stream_->UploadAction(MakeFeedAction(42ul), true, base::DoNothing());
  // Trigger saving a feed stream, so it can be cleared later.
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  // Enqueue an action, so it can be cleared later.
  stream_->UploadAction(MakeFeedAction(43ul), false, base::DoNothing());

  // Trigger ClearAll, this should erase everything.
  stream_->OnCacheDataCleared();
  WaitForIdleTaskQueue();

  ASSERT_EQ("loading -> 2 slices -> loading -> cant-refresh",
            surface.DescribeUpdates());
  EXPECT_EQ(R"("m": {
}
"recommendedIndex": {
}
"subs": {
}
)",
            DumpStoreState(true));
  EXPECT_EQ("", stream_->GetMetadata().consistency_token());
  EXPECT_FALSE(stream_->IsActivityLoggingEnabled(kForYouStream));
}

TEST_F(FeedApiTest, StorePendingAction) {
  stream_->UploadAction(MakeFeedAction(42ul), false, base::DoNothing());
  WaitForIdleTaskQueue();

  std::vector<feedstore::StoredAction> result =
      ReadStoredActions(stream_->GetStore());
  ASSERT_EQ(1ul, result.size());

  EXPECT_EQ(ToTextProto(MakeFeedAction(42ul).action_payload()),
            ToTextProto(result[0].action().action_payload()));
}

TEST_F(FeedApiTest, UploadActionWhileSignedOutIsNoOp) {
  signed_in_gaia_ = "";
  ASSERT_EQ(stream_->GetSyncSignedInGaia(), "");
  stream_->UploadAction(MakeFeedAction(42ul), false, base::DoNothing());
  WaitForIdleTaskQueue();

  EXPECT_EQ(0ul, ReadStoredActions(stream_->GetStore()).size());
}

TEST_F(FeedApiTest, SignOutWhileUploadActionDoesNotUpload) {
  stream_->UploadAction(MakeFeedAction(42ul), true, base::DoNothing());
  signed_in_gaia_ = "";

  WaitForIdleTaskQueue();

  EXPECT_EQ(UploadActionsStatus::kAbortUploadForSignedOutUser,
            metrics_reporter_->upload_action_status);
  EXPECT_EQ(0, network_.GetActionRequestCount());
}

TEST_F(FeedApiTest, ClearAllWhileUploadActionDoesNotUpload) {
  CallbackReceiver<UploadActionsTask::Result> cr;
  stream_->UploadAction(MakeFeedAction(42ul), true, cr.Bind());
  stream_->OnCacheDataCleared();  // triggers ClearAll().
  WaitForIdleTaskQueue();

  EXPECT_EQ(UploadActionsStatus::kAbortUploadActionsWithPendingClearAll,
            metrics_reporter_->upload_action_status);
  EXPECT_EQ(0, network_.GetActionRequestCount());
  ASSERT_TRUE(cr.GetResult());
  EXPECT_EQ(0ul, cr.GetResult()->upload_attempt_count);
}

TEST_F(FeedApiTest, WrongUserUploadActionDoesNotUpload) {
  CallbackReceiver<UploadActionsTask::Result> cr;
  stream_->UploadAction(MakeFeedAction(42ul), true, cr.Bind());
  // Sign in as another user.
  signed_in_gaia_ = "someothergaia";

  WaitForIdleTaskQueue();

  // Action should not upload.
  EXPECT_EQ(UploadActionsStatus::kAbortUploadForWrongUser,
            metrics_reporter_->upload_action_status);
  EXPECT_EQ(0, network_.GetActionRequestCount());
  ASSERT_TRUE(cr.GetResult());
  EXPECT_EQ(0ul, cr.GetResult()->upload_attempt_count);
}

TEST_F(FeedApiTest, StorePendingActionAndUploadNow) {
  network_.consistency_token = "token-11";

  // Call |ProcessThereAndBackAgain()|, which triggers Upload() with
  // upload_now=true.
  {
    feedwire::ThereAndBackAgainData msg;
    *msg.mutable_action_payload() = MakeFeedAction(42ul).action_payload();
    stream_->ProcessThereAndBackAgain(msg.SerializeAsString());
  }
  WaitForIdleTaskQueue();

  // Verify the action was uploaded.
  EXPECT_EQ(1, network_.GetActionRequestCount());
  std::vector<feedstore::StoredAction> result =
      ReadStoredActions(stream_->GetStore());
  ASSERT_EQ(0ul, result.size());
}

TEST_F(FeedApiTest, ProcessViewActionResultsInDelayedUpload) {
  network_.consistency_token = "token-11";

  stream_->ProcessViewAction(MakeFeedAction(42ul).SerializeAsString());
  WaitForIdleTaskQueue();
  // Verify it's not uploaded immediately.
  ASSERT_EQ(0, network_.GetActionRequestCount());

  // Trigger a network refresh.
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  // Verify the action was uploaded.
  EXPECT_EQ(1, network_.GetActionRequestCount());
}

TEST_F(FeedApiTest, ActionsUploadWithoutConditionsWhenFeatureDisabled) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());

  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  stream_->ProcessViewAction(
      feedwire::FeedAction::default_instance().SerializeAsString());
  WaitForIdleTaskQueue();
  stream_->ProcessThereAndBackAgain(
      MakeThereAndBackAgainData(42ul).SerializeAsString());
  WaitForIdleTaskQueue();

  // Verify the actions were uploaded.
  ASSERT_EQ(1, network_.GetActionRequestCount());
  EXPECT_EQ(2, network_.GetActionRequestSent()->feed_actions_size());
}

TEST_F(FeedApiTest, LoadStreamFromNetworkUploadsActions) {
  stream_->UploadAction(MakeFeedAction(99ul), false, base::DoNothing());
  WaitForIdleTaskQueue();

  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  EXPECT_EQ(1, network_.GetActionRequestCount());
  EXPECT_EQ(1, network_.GetActionRequestSent()->feed_actions_size());

  // Uploaded action should have been erased from store.
  stream_->UploadAction(MakeFeedAction(100ul), true, base::DoNothing());
  WaitForIdleTaskQueue();
  EXPECT_EQ(2, network_.GetActionRequestCount());
  EXPECT_EQ(1, network_.GetActionRequestSent()->feed_actions_size());
}

TEST_F(FeedApiTest, UploadedActionsHaveSequentialNumbers) {
  // Send 3 actions.
  stream_->UploadAction(MakeFeedAction(1ul), false, base::DoNothing());
  stream_->UploadAction(MakeFeedAction(2ul), false, base::DoNothing());
  stream_->UploadAction(MakeFeedAction(3ul), true, base::DoNothing());
  WaitForIdleTaskQueue();
  ASSERT_EQ(1, network_.GetActionRequestCount());
  feedwire::UploadActionsRequest request1 = *network_.GetActionRequestSent();

  // Send another action in a new request.
  stream_->UploadAction(MakeFeedAction(4ul), true, base::DoNothing());
  WaitForIdleTaskQueue();
  ASSERT_EQ(2, network_.GetActionRequestCount());
  feedwire::UploadActionsRequest request2 = *network_.GetActionRequestSent();

  // Verify that sent actions have sequential numbers.
  ASSERT_EQ(3, request1.feed_actions_size());
  ASSERT_EQ(1, request2.feed_actions_size());

  EXPECT_EQ(1, request1.feed_actions(0).client_data().sequence_number());
  EXPECT_EQ(2, request1.feed_actions(1).client_data().sequence_number());
  EXPECT_EQ(3, request1.feed_actions(2).client_data().sequence_number());
  EXPECT_EQ(4, request2.feed_actions(0).client_data().sequence_number());
}

TEST_F(FeedApiTest, LoadMoreUploadsActions) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  stream_->UploadAction(MakeFeedAction(99ul), false, base::DoNothing());
  WaitForIdleTaskQueue();

  network_.consistency_token = "token-12";

  stream_->LoadMore(surface, base::DoNothing());
  WaitForIdleTaskQueue();

  EXPECT_EQ(1, network_.GetActionRequestSent()->feed_actions_size());
  EXPECT_EQ("token-12", stream_->GetMetadata().consistency_token());

  // Uploaded action should have been erased from the store.
  network_.ClearTestData();
  stream_->UploadAction(MakeFeedAction(100ul), true, base::DoNothing());
  WaitForIdleTaskQueue();
  EXPECT_EQ(1, network_.GetActionRequestSent()->feed_actions_size());

  EXPECT_EQ(
      ToTextProto(MakeFeedAction(100ul).action_payload()),
      ToTextProto(
          network_.GetActionRequestSent()->feed_actions(0).action_payload()));
}

TEST_F(FeedApiTest, LoadMoreUpdatesIsActivityLoggingEnabled) {
  EXPECT_FALSE(stream_->IsActivityLoggingEnabled(kForYouStream));
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  EXPECT_TRUE(stream_->IsActivityLoggingEnabled(kForYouStream));

  int page = 2;
  for (bool signed_in : {true, false}) {
    for (bool waa_on : {true, false}) {
      for (bool privacy_notice_fulfilled : {true, false}) {
        response_translator_.InjectResponse(
            MakeTypicalNextPageState(page++, kTestTimeEpoch, signed_in, waa_on,
                                     privacy_notice_fulfilled));
        CallbackReceiver<bool> callback;
        stream_->LoadMore(surface, callback.Bind());
        WaitForIdleTaskQueue();
        EXPECT_EQ(
            stream_->IsActivityLoggingEnabled(kForYouStream),
            (signed_in && waa_on) ||
                (!signed_in && GetFeedConfig().send_signed_out_session_logs))
            << "signed_in=" << signed_in << " waa_on=" << waa_on
            << " privacy_notice_fulfilled=" << privacy_notice_fulfilled
            << " send_signed_out_session_logs="
            << GetFeedConfig().send_signed_out_session_logs;
      }
    }
  }
}

TEST_F(FeedApiTest, BackgroundingAppUploadsActions) {
  stream_->UploadAction(MakeFeedAction(1ul), false, base::DoNothing());
  stream_->OnEnterBackground();
  WaitForIdleTaskQueue();
  EXPECT_EQ(1, network_.GetActionRequestSent()->feed_actions_size());
  EXPECT_EQ(
      ToTextProto(MakeFeedAction(1ul).action_payload()),
      ToTextProto(
          network_.GetActionRequestSent()->feed_actions(0).action_payload()));
}

TEST_F(FeedApiTest, BackgroundingAppDoesNotUploadActions) {
  Config config;
  config.upload_actions_on_enter_background = false;
  SetFeedConfigForTesting(config);

  stream_->UploadAction(MakeFeedAction(1ul), false, base::DoNothing());
  stream_->OnEnterBackground();
  WaitForIdleTaskQueue();
  EXPECT_EQ(0, network_.GetActionRequestCount());
}

TEST_F(FeedApiTest, UploadedActionsAreNotSentAgain) {
  stream_->UploadAction(MakeFeedAction(1ul), false, base::DoNothing());
  stream_->OnEnterBackground();
  WaitForIdleTaskQueue();
  ASSERT_EQ(1, network_.GetActionRequestCount());

  stream_->OnEnterBackground();
  WaitForIdleTaskQueue();
  EXPECT_EQ(1, network_.GetActionRequestCount());
}

TEST_F(FeedApiTest, UploadActionsOneBatch) {
  UploadActions(
      {MakeFeedAction(97ul), MakeFeedAction(98ul), MakeFeedAction(99ul)});
  WaitForIdleTaskQueue();

  EXPECT_EQ(1, network_.GetActionRequestCount());
  EXPECT_EQ(3, network_.GetActionRequestSent()->feed_actions_size());

  stream_->UploadAction(MakeFeedAction(99ul), true, base::DoNothing());
  WaitForIdleTaskQueue();
  EXPECT_EQ(2, network_.GetActionRequestCount());
  EXPECT_EQ(1, network_.GetActionRequestSent()->feed_actions_size());
}

TEST_F(FeedApiTest, UploadActionsMultipleBatches) {
  UploadActions({
      // Batch 1: One really big action.
      MakeFeedAction(100ul, /*pad_size=*/20001ul),

      // Batch 2
      MakeFeedAction(101ul, 10000ul),
      MakeFeedAction(102ul, 9000ul),

      // Batch 3. Trigger upload.
      MakeFeedAction(103ul, 2000ul),
  });
  WaitForIdleTaskQueue();

  EXPECT_EQ(3, network_.GetActionRequestCount());

  stream_->UploadAction(MakeFeedAction(99ul), true, base::DoNothing());
  WaitForIdleTaskQueue();
  EXPECT_EQ(4, network_.GetActionRequestCount());
  EXPECT_EQ(1, network_.GetActionRequestSent()->feed_actions_size());
}

TEST_F(FeedApiTest, UploadActionsSkipsStaleActionsByTimestamp) {
  stream_->UploadAction(MakeFeedAction(2ul), false, base::DoNothing());
  WaitForIdleTaskQueue();
  task_environment_.FastForwardBy(base::TimeDelta::FromHours(25));

  // Trigger upload
  CallbackReceiver<UploadActionsTask::Result> cr;
  stream_->UploadAction(MakeFeedAction(3ul), true, cr.Bind());
  WaitForIdleTaskQueue();

  // Just one action should have been uploaded.
  EXPECT_EQ(1, network_.GetActionRequestCount());
  EXPECT_EQ(1, network_.GetActionRequestSent()->feed_actions_size());
  EXPECT_EQ(
      ToTextProto(MakeFeedAction(3ul).action_payload()),
      ToTextProto(
          network_.GetActionRequestSent()->feed_actions(0).action_payload()));

  ASSERT_TRUE(cr.GetResult());
  EXPECT_EQ(1ul, cr.GetResult()->upload_attempt_count);
  EXPECT_EQ(1ul, cr.GetResult()->stale_count);
}

TEST_F(FeedApiTest, UploadActionsErasesStaleActionsByAttempts) {
  // Three failed uploads, plus one more to cause the first action to be erased.
  network_.InjectEmptyActionRequestResult();
  stream_->UploadAction(MakeFeedAction(0ul), true, base::DoNothing());
  network_.InjectEmptyActionRequestResult();
  stream_->UploadAction(MakeFeedAction(1ul), true, base::DoNothing());
  network_.InjectEmptyActionRequestResult();
  stream_->UploadAction(MakeFeedAction(2ul), true, base::DoNothing());

  CallbackReceiver<UploadActionsTask::Result> cr;
  stream_->UploadAction(MakeFeedAction(3ul), true, cr.Bind());
  WaitForIdleTaskQueue();

  // Four requests, three pending actions in the last request.
  EXPECT_EQ(4, network_.GetActionRequestCount());
  EXPECT_EQ(3, network_.GetActionRequestSent()->feed_actions_size());

  // Action 0 should have been erased.
  ASSERT_TRUE(cr.GetResult());
  EXPECT_EQ(3ul, cr.GetResult()->upload_attempt_count);
  EXPECT_EQ(1ul, cr.GetResult()->stale_count);
}

TEST_F(FeedApiTest, MetadataLoadedWhenDatabaseInitialized) {
  const auto kExpiry = kTestTimeEpoch + base::TimeDelta::FromDays(1234);
  {
    // Write some metadata so it can be loaded when FeedStream starts up.
    feedstore::Metadata initial_metadata;
    feedstore::SetSessionId(initial_metadata, "session-id", kExpiry);
    initial_metadata.set_consistency_token("token");
    initial_metadata.set_gaia(GetSyncSignedInGaia());
    store_->WriteMetadata(initial_metadata, base::DoNothing());
  }

  // Creating a stream should load metadata.
  CreateStream();

  EXPECT_EQ("session-id", stream_->GetMetadata().session_id().token());
  EXPECT_TIME_EQ(kExpiry,
                 feedstore::GetSessionIdExpiryTime(stream_->GetMetadata()));
  EXPECT_EQ("token", stream_->GetMetadata().consistency_token());
  // Verify the schema has been updated to the current version.
  EXPECT_EQ((int)FeedStore::kCurrentStreamSchemaVersion,
            stream_->GetMetadata().stream_schema_version());
}

TEST_F(FeedApiTest, ClearAllWhenDatabaseInitializedForWrongUser) {
  {
    // Write some metadata so it can be loaded when FeedStream starts up.
    feedstore::Metadata initial_metadata;
    initial_metadata.set_consistency_token("token");
    initial_metadata.set_gaia("someotherusergaia");
    store_->WriteMetadata(initial_metadata, base::DoNothing());
  }

  // Creating a stream should init database.
  CreateStream();

  EXPECT_EQ("{\n}\n", DumpStoreState());
  EXPECT_EQ("", stream_->GetMetadata().consistency_token());
}

TEST_F(FeedApiTest, ModelUnloadsAfterTimeout) {
  Config config;
  config.model_unload_timeout = base::TimeDelta::FromSeconds(1);
  SetFeedConfigForTesting(config);

  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  surface.Detach();

  task_environment_.FastForwardBy(base::TimeDelta::FromMilliseconds(999));
  WaitForIdleTaskQueue();
  EXPECT_TRUE(stream_->GetModel(surface.GetStreamType()));

  task_environment_.FastForwardBy(base::TimeDelta::FromMilliseconds(2));
  WaitForIdleTaskQueue();
  EXPECT_FALSE(stream_->GetModel(surface.GetStreamType()));
}

TEST_F(FeedApiTest, ModelDoesNotUnloadIfSurfaceIsAttached) {
  Config config;
  config.model_unload_timeout = base::TimeDelta::FromSeconds(1);
  SetFeedConfigForTesting(config);

  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  surface.Detach();

  task_environment_.FastForwardBy(base::TimeDelta::FromMilliseconds(999));
  WaitForIdleTaskQueue();
  EXPECT_TRUE(stream_->GetModel(surface.GetStreamType()));

  surface.Attach(stream_.get());

  task_environment_.FastForwardBy(base::TimeDelta::FromMilliseconds(2));
  WaitForIdleTaskQueue();
  EXPECT_TRUE(stream_->GetModel(surface.GetStreamType()));
}

TEST_F(FeedApiTest, ModelUnloadsAfterSecondTimeout) {
  Config config;
  config.model_unload_timeout = base::TimeDelta::FromSeconds(1);
  SetFeedConfigForTesting(config);

  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  surface.Detach();

  task_environment_.FastForwardBy(base::TimeDelta::FromMilliseconds(999));
  WaitForIdleTaskQueue();
  EXPECT_TRUE(stream_->GetModel(surface.GetStreamType()));

  // Attaching another surface will prolong the unload time for another second.
  surface.Attach(stream_.get());
  surface.Detach();

  task_environment_.FastForwardBy(base::TimeDelta::FromMilliseconds(999));
  WaitForIdleTaskQueue();
  EXPECT_TRUE(stream_->GetModel(surface.GetStreamType()));

  task_environment_.FastForwardBy(base::TimeDelta::FromMilliseconds(2));
  WaitForIdleTaskQueue();
  EXPECT_FALSE(stream_->GetModel(surface.GetStreamType()));
}

TEST_F(FeedApiTest, SendsClientInstanceId) {
  // Store is empty, so we should fallback to a network request.
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  ASSERT_EQ(1, network_.send_query_call_count);
  ASSERT_TRUE(network_.query_request_sent);

  // Instance ID is a random token. Verify it is not empty.
  std::string first_instance_id = network_.query_request_sent->feed_request()
                                      .client_info()
                                      .client_instance_id();
  EXPECT_NE("", first_instance_id);

  // No signed-out session id was in the request.
  EXPECT_TRUE(network_.query_request_sent->feed_request()
                  .client_info()
                  .chrome_client_info()
                  .session_id()
                  .empty());

  // LoadMore, and verify the same token is used.
  response_translator_.InjectResponse(MakeTypicalNextPageState(2));
  stream_->LoadMore(surface, base::DoNothing());
  WaitForIdleTaskQueue();

  ASSERT_EQ(2, network_.send_query_call_count);
  EXPECT_EQ(first_instance_id, network_.query_request_sent->feed_request()
                                   .client_info()
                                   .client_instance_id());

  // No signed-out session id was in the request.
  EXPECT_TRUE(network_.query_request_sent->feed_request()
                  .client_info()
                  .chrome_client_info()
                  .session_id()
                  .empty());

  // Trigger a ClearAll to verify the instance ID changes.
  stream_->OnSignedOut();
  WaitForIdleTaskQueue();

  EXPECT_FALSE(stream_->GetModel(surface.GetStreamType()));
  const bool is_for_next_page = false;  // No model so no first page yet.
  const std::string new_instance_id =
      stream_->GetRequestMetadata(kForYouStream, is_for_next_page)
          .client_instance_id;
  ASSERT_NE("", new_instance_id);
  ASSERT_NE(first_instance_id, new_instance_id);
}

TEST_F(FeedApiTest, SignedOutSessionIdConsistency) {
  const std::string kSessionToken1("session-token-1");
  const std::string kSessionToken2("session-token-2");

  signed_in_gaia_ = "";

  StreamModelUpdateRequestGenerator model_generator;
  model_generator.signed_in = false;

  // (1) Do an initial load of the store
  //     - this should trigger a network request
  //     - the request should not include client-instance-id
  //     - the request should not include a session-id
  //     - the stream should capture the session-id token from the response
  response_translator_.InjectResponse(model_generator.MakeFirstPage(),
                                      kSessionToken1);
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  ASSERT_EQ(1, network_.send_query_call_count);
  EXPECT_TRUE(network_.query_request_sent->feed_request()
                  .client_info()
                  .client_instance_id()
                  .empty());
  EXPECT_FALSE(network_.query_request_sent->feed_request()
                   .client_info()
                   .has_chrome_client_info());
  EXPECT_EQ(kSessionToken1, stream_->GetMetadata().session_id().token());
  const base::Time kSessionToken1ExpiryTime =
      feedstore::GetSessionIdExpiryTime(stream_->GetMetadata());

  // (2) LoadMore: the server returns the same session-id token
  //     - this should trigger a network request
  //     - the request should not include client-instance-id
  //     - the request should include the first session-id
  //     - the stream should retain the first session-id
  //     - the session-id's expiry time should be unchanged
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));
  response_translator_.InjectResponse(model_generator.MakeNextPage(2),
                                      kSessionToken1);
  stream_->LoadMore(surface, base::DoNothing());
  WaitForIdleTaskQueue();
  ASSERT_EQ(2, network_.send_query_call_count);
  EXPECT_TRUE(network_.query_request_sent->feed_request()
                  .client_info()
                  .client_instance_id()
                  .empty());
  EXPECT_EQ(kSessionToken1, network_.query_request_sent->feed_request()
                                .client_info()
                                .chrome_client_info()
                                .session_id());
  EXPECT_EQ(kSessionToken1, stream_->GetMetadata().session_id().token());
  EXPECT_TIME_EQ(kSessionToken1ExpiryTime,
                 feedstore::GetSessionIdExpiryTime(stream_->GetMetadata()));

  // (3) LoadMore: the server omits returning a session-id token
  //     - this should trigger a network request
  //     - the request should not include client-instance-id
  //     - the request should include the first session-id
  //     - the stream should retain the first session-id
  //     - the session-id's expiry time should be unchanged
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));
  response_translator_.InjectResponse(model_generator.MakeNextPage(3));
  stream_->LoadMore(surface, base::DoNothing());
  WaitForIdleTaskQueue();
  ASSERT_EQ(3, network_.send_query_call_count);
  EXPECT_TRUE(network_.query_request_sent->feed_request()
                  .client_info()
                  .client_instance_id()
                  .empty());
  EXPECT_EQ(kSessionToken1, network_.query_request_sent->feed_request()
                                .client_info()
                                .chrome_client_info()
                                .session_id());
  EXPECT_EQ(kSessionToken1, stream_->GetMetadata().session_id().token());
  EXPECT_TIME_EQ(kSessionToken1ExpiryTime,
                 feedstore::GetSessionIdExpiryTime(stream_->GetMetadata()));

  // (4) LoadMore: the server returns new session id.
  //     - this should trigger a network request
  //     - the request should not include client-instance-id
  //     - the request should include the first session-id
  //     - the stream should retain the second session-id
  //     - the new session-id's expiry time should be updated
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));
  response_translator_.InjectResponse(model_generator.MakeNextPage(4),
                                      kSessionToken2);
  stream_->LoadMore(surface, base::DoNothing());
  WaitForIdleTaskQueue();
  ASSERT_EQ(4, network_.send_query_call_count);
  EXPECT_TRUE(network_.query_request_sent->feed_request()
                  .client_info()
                  .client_instance_id()
                  .empty());
  EXPECT_EQ(kSessionToken1, network_.query_request_sent->feed_request()
                                .client_info()
                                .chrome_client_info()
                                .session_id());
  EXPECT_EQ(kSessionToken2, stream_->GetMetadata().session_id().token());
  EXPECT_TIME_EQ(kSessionToken1ExpiryTime + base::TimeDelta::FromSeconds(3),
                 feedstore::GetSessionIdExpiryTime(stream_->GetMetadata()));
}

TEST_F(FeedApiTest, ClearAllResetsSessionId) {
  signed_in_gaia_ = "";

  // Initialize a session id.
  feedstore::Metadata metadata = stream_->GetMetadata();
  feedstore::MaybeUpdateSessionId(metadata, "session-id");
  stream_->SetMetadata(metadata);

  // Trigger a ClearAll.
  stream_->OnCacheDataCleared();
  WaitForIdleTaskQueue();

  // Session-ID should be wiped.
  EXPECT_TRUE(stream_->GetMetadata().session_id().token().empty());
  EXPECT_TRUE(
      feedstore::GetSessionIdExpiryTime(stream_->GetMetadata()).is_null());
}

TEST_F(FeedApiTest, SignedOutSessionIdExpiry) {
  const std::string kSessionToken1("session-token-1");
  const std::string kSessionToken2("session-token-2");

  signed_in_gaia_ = "";

  StreamModelUpdateRequestGenerator model_generator;
  model_generator.signed_in = false;

  // (1) Do an initial load of the store
  //     - this should trigger a network request
  //     - the request should not include a session-id
  //     - the stream should capture the session-id token from the response
  response_translator_.InjectResponse(model_generator.MakeFirstPage(),
                                      kSessionToken1);
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  ASSERT_EQ(1, network_.send_query_call_count);
  EXPECT_FALSE(network_.query_request_sent->feed_request()
                   .client_info()
                   .has_chrome_client_info());
  EXPECT_EQ(kSessionToken1, stream_->GetMetadata().session_id().token());

  // (2) Reload the stream from the network:
  //     - Detach the surface, advance the clock beyond the stale content
  //       threshold, re-attach the surface.
  //     - this should trigger a network request
  //     - the request should include kSessionToken1
  //     - the stream should retain the original session-id
  surface.Detach();
  task_environment_.FastForwardBy(GetFeedConfig().stale_content_threshold +
                                  base::TimeDelta::FromSeconds(1));
  response_translator_.InjectResponse(model_generator.MakeFirstPage());
  surface.Attach(stream_.get());
  WaitForIdleTaskQueue();
  ASSERT_EQ(2, network_.send_query_call_count);
  EXPECT_EQ(kSessionToken1, network_.query_request_sent->feed_request()
                                .client_info()
                                .chrome_client_info()
                                .session_id());
  EXPECT_EQ(kSessionToken1, stream_->GetMetadata().session_id().token());

  // (3) Reload the stream from the network:
  //     - Detach the surface, advance the clock beyond the session id max age
  //       threshold, re-attach the surface.
  //     - this should trigger a network request
  //     - the request should not include a session-id
  //     - the stream should get a new session-id
  surface.Detach();
  task_environment_.FastForwardBy(GetFeedConfig().session_id_max_age -
                                  GetFeedConfig().stale_content_threshold);
  ASSERT_LT(feedstore::GetSessionIdExpiryTime(stream_->GetMetadata()),
            base::Time::Now());
  response_translator_.InjectResponse(model_generator.MakeFirstPage(),
                                      kSessionToken2);
  surface.Attach(stream_.get());
  WaitForIdleTaskQueue();
  ASSERT_EQ(3, network_.send_query_call_count);
  EXPECT_FALSE(network_.query_request_sent->feed_request()
                   .client_info()
                   .has_chrome_client_info());
  EXPECT_EQ(kSessionToken2, stream_->GetMetadata().session_id().token());
}

TEST_F(FeedApiTest, SessionIdPersistsAcrossStreamLoads) {
  const std::string kSessionToken("session-token-ftw");
  const base::Time kExpiryTime =
      kTestTimeEpoch + GetFeedConfig().session_id_max_age;

  StreamModelUpdateRequestGenerator model_generator;
  model_generator.signed_in = false;
  signed_in_gaia_ = "";

  // (1) Do an initial load of the store
  //     - this should trigger a network request
  //     - the stream should capture the session-id token from the response
  response_translator_.InjectResponse(model_generator.MakeFirstPage(),
                                      kSessionToken);
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  ASSERT_EQ(1, network_.send_query_call_count);

  // (2) Reload the metadata from disk.
  //     - the network query call count should be unchanged
  //     - the session token and expiry time should have been reloaded.
  surface.Detach();
  CreateStream();
  WaitForIdleTaskQueue();
  ASSERT_EQ(1, network_.send_query_call_count);
  EXPECT_EQ(kSessionToken, stream_->GetMetadata().session_id().token());
  EXPECT_TIME_EQ(kExpiryTime,
                 feedstore::GetSessionIdExpiryTime(stream_->GetMetadata()));
}

TEST_F(FeedApiTest, PersistentKeyValueStoreIsClearedOnClearAll) {
  // Store some data and verify it exists.
  PersistentKeyValueStore& store = stream_->GetPersistentKeyValueStore();
  store.Put("x", "y", base::DoNothing());
  CallbackReceiver<PersistentKeyValueStore::Result> get_result;
  store.Get("x", get_result.Bind());
  ASSERT_EQ("y", *get_result.RunAndGetResult().get_result);

  stream_->OnCacheDataCleared();  // triggers ClearAll().
  WaitForIdleTaskQueue();

  // Verify ClearAll() deleted the data.
  get_result.Clear();
  store.Get("x", get_result.Bind());
  EXPECT_FALSE(get_result.RunAndGetResult().get_result);
}

TEST_F(FeedApiTest, LoadMultipleStreams) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  // WebFeed stream is only fetched when there's a subscription.
  network_.InjectListWebFeedsResponse({MakeWireWebFeed("cats")});

  TestForYouSurface for_you_surface(stream_.get());
  TestWebFeedSurface web_feed_surface(stream_.get());

  WaitForIdleTaskQueue();

  ASSERT_EQ("loading -> 2 slices", for_you_surface.DescribeUpdates());
  ASSERT_EQ("loading -> 2 slices", web_feed_surface.DescribeUpdates());
}

TEST_F(FeedApiTest, UnloadOnlyOneOfMultipleModels) {
  // WebFeed stream is only fetched when there's a subscription.
  network_.InjectListWebFeedsResponse({MakeWireWebFeed("cats")});

  Config config;
  config.model_unload_timeout = base::TimeDelta::FromSeconds(1);
  SetFeedConfigForTesting(config);

  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface for_you_surface(stream_.get());
  TestWebFeedSurface web_feed_surface(stream_.get());

  WaitForIdleTaskQueue();

  for_you_surface.Detach();

  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(2));
  WaitForIdleTaskQueue();

  EXPECT_TRUE(stream_->GetModel(kWebFeedStream));
  EXPECT_FALSE(stream_->GetModel(kForYouStream));
}

TEST_F(FeedApiTest, ExperimentsAreClearedOnClearAll) {
  Experiments e;
  e["Trial1"] = "Group1";
  e["Trial2"] = "Group2";
  prefs::SetExperiments(e, profile_prefs_);

  stream_->OnCacheDataCleared();  // triggers ClearAll().
  WaitForIdleTaskQueue();

  Experiments empty;
  Experiments got = prefs::GetExperiments(profile_prefs_);
  EXPECT_EQ(got, empty);
}

TEST_F(FeedApiTest, CreateAndCommitEphemeralChange) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  EphemeralChangeId change_id = stream_->CreateEphemeralChange(
      surface.GetStreamType(), {MakeOperation(MakeClearAll())});
  stream_->CommitEphemeralChange(surface.GetStreamType(), change_id);
  WaitForIdleTaskQueue();

  ASSERT_EQ("loading -> 2 slices -> no-cards -> no-cards",
            surface.DescribeUpdates());
}

TEST_F(FeedApiTest, RejectEphemeralChange) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  EphemeralChangeId change_id = stream_->CreateEphemeralChange(
      surface.GetStreamType(), {MakeOperation(MakeClearAll())});
  stream_->RejectEphemeralChange(surface.GetStreamType(), change_id);
  WaitForIdleTaskQueue();

  ASSERT_EQ("loading -> 2 slices -> no-cards -> 2 slices",
            surface.DescribeUpdates());
}

// Test that we overwrite stored stream data, even if ContentId's do not change.
TEST_F(FeedApiTest, StreamDataOverwritesOldStream) {
  // Inject two FeedQuery responses with some different data.
  {
    std::unique_ptr<StreamModelUpdateRequest> new_state =
        MakeTypicalInitialModelState();
    new_state->shared_states[0].set_shared_state_data("new-shared-data");
    new_state->content[0].set_frame("new-frame-data");

    response_translator_.InjectResponse(MakeTypicalInitialModelState());
    response_translator_.InjectResponse(std::move(new_state));
  }

  // Trigger stream load, unload stream, and wait until the stream data is
  // stale.
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  surface.Detach();
  task_environment_.FastForwardBy(base::TimeDelta::FromDays(20));

  // Trigger stream load again, it should refersh from the network.
  surface.Attach(stream_.get());
  WaitForIdleTaskQueue();

  // Verify the new data was stored.
  std::unique_ptr<StreamModelUpdateRequest> stored_data =
      StoredModelData(kForYouStream, store_.get());
  ASSERT_TRUE(stored_data);
  EXPECT_EQ("new-shared-data",
            stored_data->shared_states[0].shared_state_data());
  EXPECT_EQ("new-frame-data", stored_data->content[0].frame());
}

TEST_F(FeedApiTest, HasUnreadContentIsFalseAfterSliceView) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  EXPECT_EQ("loading -> 2 slices", surface.DescribeUpdates());
  ASSERT_TRUE(stream_->HasUnreadContent(kForYouStream));
  stream_->ReportSliceViewed(
      surface.GetSurfaceId(), surface.GetStreamType(),
      surface.initial_state->updated_slices(1).slice().slice_id());

  EXPECT_FALSE(stream_->HasUnreadContent(kForYouStream));
}

TEST_F(FeedApiTest,
       LoadingForYouStreamTriggersWebFeedRefreshIfNoUnreadContent) {
  // WebFeed stream is only fetched when there's a subscription.
  network_.InjectListWebFeedsResponse({MakeWireWebFeed("cats")});

  // Both streams should be fetched.
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  ASSERT_EQ(2, network_.send_query_call_count);
  EXPECT_EQ("loading -> 2 slices", surface.DescribeUpdates());
  EXPECT_EQ(LoadStreamStatus::kLoadedFromNetwork,
            metrics_reporter_->load_stream_status);

  // Attach a Web Feed surface, and verify content is loaded from the store.
  TestWebFeedSurface web_feed_surface(stream_.get());
  WaitForIdleTaskQueue();

  EXPECT_EQ("loading -> 2 slices", web_feed_surface.DescribeUpdates());
  EXPECT_EQ(LoadStreamStatus::kLoadedFromStore,
            metrics_reporter_->load_stream_status);
}

TEST_F(FeedApiTest,
       LoadingForYouStreamDoesNotTriggerWebFeedRefreshIfNoSubscriptions) {
  // Only for-you feed is fetched on load.
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  ASSERT_EQ(1, network_.send_query_call_count);
  EXPECT_EQ("loading -> 2 slices", surface.DescribeUpdates());
  EXPECT_EQ(LoadStreamStatus::kLoadedFromNetwork,
            metrics_reporter_->load_stream_status);
  EXPECT_EQ(
      LoadStreamStatus::kNotAWebFeedSubscriber,
      metrics_reporter_->Stream(kWebFeedStream).background_refresh_status);
}

TEST_F(
    FeedApiTest,
    LoadForYouStreamDoesNotTriggerWebFeedRefreshContentIfIsAlreadyAvailable) {
  // WebFeed stream is only fetched when there's a subscription.
  network_.InjectListWebFeedsResponse({MakeWireWebFeed("cats")});

  // Both streams should be fetched because there is no unread web-feed content.
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  ASSERT_EQ(2, network_.send_query_call_count);

  // Detach and re-attach the surface. The for-you feed should be loaded again,
  // but this time the web-feed is not refreshed.
  surface.Detach();
  UnloadModel(surface.GetStreamType());
  surface.Attach(stream_.get());
  WaitForIdleTaskQueue();
  // Neither stream type should be refreshed.
  ASSERT_EQ(2, network_.send_query_call_count);
}

TEST_F(FeedApiTest, WasUrlRecentlyNavigatedFromFeed) {
  const GURL url1("https://someurl1");
  const GURL url2("https://someurl2");
  EXPECT_FALSE(stream_->WasUrlRecentlyNavigatedFromFeed(url1));
  EXPECT_FALSE(stream_->WasUrlRecentlyNavigatedFromFeed(url2));

  stream_->ReportOpenAction(url1, kForYouStream, "slice");
  stream_->ReportOpenInNewTabAction(url2, kForYouStream, "slice");

  EXPECT_TRUE(stream_->WasUrlRecentlyNavigatedFromFeed(url1));
  EXPECT_TRUE(stream_->WasUrlRecentlyNavigatedFromFeed(url2));
}

// After 10 URLs are navigated, they are forgotten in FIFO order.
TEST_F(FeedApiTest, WasUrlRecentlyNavigatedFromFeedMaxHistory) {
  std::vector<GURL> urls;
  for (int i = 0; i < 11; ++i)
    urls.emplace_back("https://someurl" + base::NumberToString(i));

  for (const GURL& url : urls)
    stream_->ReportOpenAction(url, kForYouStream, "slice");

  EXPECT_FALSE(stream_->WasUrlRecentlyNavigatedFromFeed(urls[0]));
  for (size_t i = 1; i < urls.size(); ++i) {
    EXPECT_TRUE(stream_->WasUrlRecentlyNavigatedFromFeed(urls[i]));
  }
}

TEST_F(FeedApiTest, ClearAllOnStartupIfFeedIsDisabled) {
  CallbackReceiver<> on_clear_all;
  on_clear_all_ = on_clear_all.BindRepeating();

  // Fetch a feed, so that there's stored data.
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  // Turn off the feed, and re-create FeedStream. It should perform a ClearAll.
  profile_prefs_.SetBoolean(feed::prefs::kEnableSnippets, false);
  CreateStream();
  EXPECT_TRUE(on_clear_all.called());

  // Re-create the feed, and verify ClearAll isn't called again.
  on_clear_all.Clear();
  CreateStream();
  EXPECT_FALSE(on_clear_all.called());
}

TEST_F(FeedApiTest, ManualRefreshSuccess) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  ASSERT_EQ("loading -> 2 slices", surface.DescribeUpdates());

  response_translator_.InjectResponse(MakeTypicalRefreshModelState());
  CallbackReceiver<bool> callback;
  stream_->ManualRefresh(surface, callback.Bind());
  WaitForIdleTaskQueue();
  EXPECT_EQ(absl::optional<bool>(true), callback.GetResult());
  EXPECT_EQ("3 slices", surface.DescribeUpdates());
  EXPECT_EQ(LoadStreamStatus::kLoadedFromNetwork,
            metrics_reporter_->load_stream_status);
  // Verify stored state is equivalent to in-memory model.
  EXPECT_STRINGS_EQUAL(
      stream_->GetModel(surface.GetStreamType())->DumpStateForTesting(),
      ModelStateFor(kForYouStream, store_.get()));
}

TEST_F(FeedApiTest, ManualRefreshFailsBecauseNetworkRequestFails) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  ASSERT_EQ("loading -> 2 slices", surface.DescribeUpdates());
  EXPECT_EQ(LoadStreamStatus::kLoadedFromNetwork,
            metrics_reporter_->load_stream_status);
  std::string original_store_dump =
      ModelStateFor(surface.GetStreamType(), store_.get());
  EXPECT_STRINGS_EQUAL(
      stream_->GetModel(surface.GetStreamType())->DumpStateForTesting(),
      original_store_dump);

  // Since we didn't inject a network response, the network update will fail.
  // The store should not be updated.
  CallbackReceiver<bool> callback;
  stream_->ManualRefresh(surface, callback.Bind());
  WaitForIdleTaskQueue();
  EXPECT_EQ(absl::optional<bool>(false), callback.GetResult());
  EXPECT_EQ("cant-refresh", surface.DescribeUpdates());
  EXPECT_EQ(LoadStreamStatus::kProtoTranslationFailed,
            metrics_reporter_->load_stream_status);
  EXPECT_STRINGS_EQUAL(ModelStateFor(surface.GetStreamType(), store_.get()),
                       original_store_dump);
}

TEST_F(FeedApiTest, ManualRefreshSuccessAfterUnload) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  ASSERT_EQ("loading -> 2 slices", surface.DescribeUpdates());

  UnloadModel(surface.GetStreamType());
  WaitForIdleTaskQueue();

  response_translator_.InjectResponse(MakeTypicalRefreshModelState());
  CallbackReceiver<bool> callback;
  stream_->ManualRefresh(surface, callback.Bind());
  WaitForIdleTaskQueue();
  EXPECT_EQ(absl::optional<bool>(true), callback.GetResult());
  EXPECT_EQ("3 slices", surface.DescribeUpdates());
  EXPECT_EQ(LoadStreamStatus::kLoadedFromNetwork,
            metrics_reporter_->load_stream_status);
  // Verify stored state is equivalent to in-memory model.
  EXPECT_STRINGS_EQUAL(
      stream_->GetModel(surface.GetStreamType())->DumpStateForTesting(),
      ModelStateFor(kForYouStream, store_.get()));
}

TEST_F(FeedApiTest, ManualRefreshSuccessAfterPreviousLoadFailure) {
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  EXPECT_EQ("loading -> cant-refresh", surface.DescribeUpdates());

  response_translator_.InjectResponse(MakeTypicalRefreshModelState());
  CallbackReceiver<bool> callback;
  stream_->ManualRefresh(surface, callback.Bind());
  WaitForIdleTaskQueue();
  EXPECT_EQ(absl::optional<bool>(true), callback.GetResult());
  EXPECT_EQ("no-cards -> 3 slices", surface.DescribeUpdates());
  EXPECT_EQ(LoadStreamStatus::kLoadedFromNetwork,
            metrics_reporter_->load_stream_status);
  // Verify stored state is equivalent to in-memory model.
  EXPECT_STRINGS_EQUAL(
      stream_->GetModel(surface.GetStreamType())->DumpStateForTesting(),
      ModelStateFor(kForYouStream, store_.get()));
}

TEST_F(FeedApiTest, ManualRefreshFailesWhenLoadingInProgress) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  // Don't call WaitForIdleTaskQueue to finish the loading.

  response_translator_.InjectResponse(MakeTypicalRefreshModelState());
  CallbackReceiver<bool> callback;
  stream_->ManualRefresh(surface, callback.Bind());
  WaitForIdleTaskQueue();
  // Manual refresh should fail immediately when loading is still in progress.
  EXPECT_EQ(absl::optional<bool>(false), callback.GetResult());
  // The initial loading should finish.
  EXPECT_EQ("loading -> 2 slices", surface.DescribeUpdates());
}

// Keep instantiations at the bottom.
INSTANTIATE_TEST_SUITE_P(FeedApiTest,
                         FeedStreamTestForAllStreamTypes,
                         ::testing::Values(kForYouStream, kWebFeedStream),
                         ::testing::PrintToStringParamName());
INSTANTIATE_TEST_SUITE_P(FeedApiTest,
                         FeedNetworkEndpointTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool()));

}  // namespace
}  // namespace test
}  // namespace feed
