// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/history_clusters_service.h"

#include <memory>
#include <string>

#include "base/containers/contains.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/history/core/browser/history_context.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/url_row.h"
#include "components/history/core/test/history_service_test_util.h"
#include "components/history_clusters/core/clustering_backend.h"
#include "components/history_clusters/core/history_clusters.mojom.h"
#include "components/history_clusters/core/history_clusters_service_test_api.h"
#include "components/history_clusters/core/memories_features.h"
#include "components/history_clusters/core/visit_data.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace history_clusters {

namespace {

// Trivial backend to allow us to specifically test just the service behavior.
class TestClusteringBackend : public ClusteringBackend {
 public:
  void GetClusters(
      ClustersCallback callback,
      const std::vector<history::AnnotatedVisit>& visits) override {
    callback_ = std::move(callback);
    last_clustered_visits_ = visits;
  }

  void FulfillCallback(const std::vector<history::Cluster>& clusters) {
    std::move(callback_).Run(clusters);
  }

  const std::vector<history::AnnotatedVisit>& last_clustered_visits() const {
    return last_clustered_visits_;
  }

  // Fetches a scored visit by an ID. `visit_id` must be valid. This is a
  // convenience method used for constructing the fake response.
  history::ScoredAnnotatedVisit GetVisitById(int visit_id, float score = 0.5) {
    for (auto& visit : last_clustered_visits_) {
      if (visit.visit_row.visit_id == visit_id)
        return {visit, score};
    }

    NOTREACHED() << "TestClusteringBackend::GetVisitById "
                 << "could not find visit_id: " << visit_id;
    return history::ScoredAnnotatedVisit();
  }

 private:
  ClustersCallback callback_;
  std::vector<history::AnnotatedVisit> last_clustered_visits_;
};

class HistoryClustersServiceTest : public testing::Test {
 public:
  HistoryClustersServiceTest()
      : task_environment_(
            base::test::SingleThreadTaskEnvironment::TimeSource::MOCK_TIME),
        run_loop_quit_(run_loop_.QuitClosure()) {
    scoped_feature_list_ = std::make_unique<base::test::ScopedFeatureList>();
    scoped_feature_list_->InitAndEnableFeature(kMemories);

    CHECK(history_dir_.CreateUniqueTempDir());
    history_service_ =
        history::CreateHistoryService(history_dir_.GetPath(), true);
    history_clusters_service_ = std::make_unique<HistoryClustersService>(
        history_service_.get(), nullptr);
    history_clusters_service_test_api_ =
        std::make_unique<HistoryClustersServiceTestApi>(
            history_clusters_service_.get(), history_service_.get());
    auto test_backend = std::make_unique<TestClusteringBackend>();
    test_clustering_backend_ = test_backend.get();
    history_clusters_service_test_api_->SetClusteringBackendForTest(
        std::move(test_backend));
  }

  HistoryClustersServiceTest(const HistoryClustersServiceTest&) = delete;
  HistoryClustersServiceTest& operator=(const HistoryClustersServiceTest&) =
      delete;

  void AddHardcodedTestDataToHistoryService() {
    history::ContextID context_id = reinterpret_cast<history::ContextID>(1);

    for (auto& visit : GetHardcodedTestVisits()) {
      history::HistoryAddPageArgs add_page_args;
      add_page_args.context_id = context_id;
      add_page_args.nav_entry_id = next_navigation_id_;
      add_page_args.url = visit.url_row.url();
      add_page_args.title = visit.url_row.title();
      add_page_args.time = visit.visit_row.visit_time;
      history_service_->AddPage(add_page_args);
      history_service_->UpdateWithPageEndTime(
          context_id, next_navigation_id_, visit.url_row.url(),
          visit.visit_row.visit_time + visit.visit_row.visit_duration);

      auto& incomplete_visit_context_annotations =
          history_clusters_service_
              ->GetOrCreateIncompleteVisitContextAnnotations(
                  next_navigation_id_);
      incomplete_visit_context_annotations.visit_row = visit.visit_row;
      incomplete_visit_context_annotations.url_row = visit.url_row;
      incomplete_visit_context_annotations.context_annotations =
          visit.context_annotations;
      incomplete_visit_context_annotations.status.history_rows = true;
      incomplete_visit_context_annotations.status.navigation_ended = true;
      incomplete_visit_context_annotations.status.navigation_end_signals = true;
      history_clusters_service_->CompleteVisitContextAnnotationsIfReady(
          next_navigation_id_);
      next_navigation_id_++;
    }
  }

  // Verifies that the hardcoded visits were passed to the clustering backend.
  void VerifyTestClusteringBackendRequest() {
    std::vector<history::AnnotatedVisit> visits =
        test_clustering_backend_->last_clustered_visits();
    ASSERT_EQ(visits.size(), 2u);
    auto& visit = visits[0];
    EXPECT_EQ(visit.visit_row.visit_id, 2);
    EXPECT_EQ(visit.visit_row.visit_time,
              GetHardcodedTestVisits()[1].visit_row.visit_time);
    EXPECT_EQ(visit.visit_row.visit_duration, base::TimeDelta::FromSeconds(20));
    EXPECT_EQ(visit.url_row.url(), "https://github.com/");
    EXPECT_EQ(visit.context_annotations.page_end_reason, 5);

    visit = visits[1];
    EXPECT_EQ(visit.visit_row.visit_id, 1);
    EXPECT_EQ(visit.visit_row.visit_time,
              GetHardcodedTestVisits()[0].visit_row.visit_time);
    EXPECT_EQ(visit.visit_row.visit_duration,
              base::TimeDelta::FromMilliseconds(5600));
    EXPECT_EQ(visit.url_row.url(), "https://google.com/");
    EXPECT_EQ(visit.context_annotations.page_end_reason, 3);

    // TODO(tommycli): Add back visit.referring_visit_id() check after updating
    //  the HistoryService test methods to support that field.
  }

 protected:
  base::test::TaskEnvironment task_environment_;

  // Used to construct a `HistoryClustersService`.
  base::ScopedTempDir history_dir_;
  std::unique_ptr<history::HistoryService> history_service_;

  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_list_;

  std::unique_ptr<HistoryClustersService> history_clusters_service_;
  std::unique_ptr<HistoryClustersServiceTestApi>
      history_clusters_service_test_api_;

  // Non-owning pointer. The actual owner is `history_clusters_service_`.
  TestClusteringBackend* test_clustering_backend_;

  base::CancelableTaskTracker task_tracker_;

  // Used to verify the async callback is invoked.
  base::RunLoop run_loop_;
  base::RepeatingClosure run_loop_quit_;

  // Tracks the next available navigation ID to be associated with visits.
  int64_t next_navigation_id_ = 0;
};

TEST_F(HistoryClustersServiceTest, ClusterAndVisitSorting) {
  AddHardcodedTestDataToHistoryService();

  history_clusters_service_->QueryClusters(
      /*query=*/"", /*max_time=*/base::Time::Now(), /* max_count=*/0,
      // This "expect" block is not run until after the fake response is sent
      // further down in this method.
      base::BindLambdaForTesting(
          [&](std::vector<mojom::ClusterPtr> clusters) {
            ASSERT_EQ(clusters.size(), 2u);

            ASSERT_EQ(clusters[0]->visits.size(), 1u);
            EXPECT_EQ(clusters[0]->visits[0]->normalized_url,
                      "https://github.com/");
            EXPECT_FLOAT_EQ(clusters[0]->visits[0]->score, 0.1);

            ASSERT_EQ(clusters[1]->visits.size(), 2u);
            EXPECT_EQ(clusters[1]->visits[0]->normalized_url,
                      "https://google.com/");
            EXPECT_FLOAT_EQ(clusters[1]->visits[0]->score, 0.9);
            EXPECT_EQ(clusters[1]->visits[1]->normalized_url,
                      "https://github.com/");
            EXPECT_FLOAT_EQ(clusters[1]->visits[1]->score, 0.5);

            run_loop_quit_.Run();
          }),
      &task_tracker_);

  history::BlockUntilHistoryProcessesPendingRequests(history_service_.get());
  VerifyTestClusteringBackendRequest();

  std::vector<history::Cluster> clusters;
  // This first cluster is meant to validate that the higher scoring "visit 1"
  // gets sorted to the top, even though "visit 1" is older in the hardcoded
  // test data. It's to validate the within-cluster sorting.
  clusters.push_back(
      history::Cluster(0,
                       {
                           test_clustering_backend_->GetVisitById(2, 0.5),
                           test_clustering_backend_->GetVisitById(1, 0.9),
                       },
                       {}));
  clusters.push_back(
      history::Cluster(0,
                       {
                           test_clustering_backend_->GetVisitById(2, 0.1),
                       },
                       {}));
  test_clustering_backend_->FulfillCallback(clusters);

  // Verify the callback is invoked.
  run_loop_.Run();
}

TEST_F(HistoryClustersServiceTest, QueryClustersVariousQueries) {
  AddHardcodedTestDataToHistoryService();

  struct TestData {
    std::string query;
    const bool expect_first_cluster;
    const bool expect_second_cluster;
  } test_data[] = {
      // Empty query should get both.
      {"", true, true},
      // Non matching query should get none.
      {"non_matching_query", false, false},
      // Query matching one cluster.
      {"oran", true, false},
      // This verifies the memory doesn't flicker away as the user is typing
      // out: "red oran" one key at a time. Also tests out multi-term queries.
      {"red", true, false},
      {"red ", true, false},
      {"red o", true, false},
      {"red or", true, false},
      {"red ora", true, false},
      {"red oran", true, false},
  };

  for (size_t i = 0; i < base::size(test_data); ++i) {
    SCOPED_TRACE(base::StringPrintf("Testing case i=%d, query=%s", int(i),
                                    test_data[i].query.c_str()));

    base::RunLoop run_loop;
    auto run_loop_quit = run_loop.QuitClosure();

    history_clusters_service_->QueryClusters(
        test_data[i].query, /*max_time=*/base::Time::Now(),
        /* max_count=*/0,
        // This "expect" block is not run until after the fake response is sent
        // further down in this method.
        base::BindLambdaForTesting(
            [&](std::vector<mojom::ClusterPtr> clusters) {
              size_t expected_size = int(test_data[i].expect_first_cluster) +
                                     int(test_data[i].expect_second_cluster);
              ASSERT_EQ(clusters.size(), expected_size);

              if (test_data[i].expect_first_cluster) {
                const auto& cluster = clusters[0];
                ASSERT_EQ(cluster->visits.size(), 2u);
                EXPECT_EQ(cluster->visits[0]->normalized_url,
                          "https://github.com/");
                EXPECT_EQ(cluster->visits[0]->raw_urls.size(), 1u);
                EXPECT_EQ(cluster->visits[0]->last_visit_time,
                          GetHardcodedTestVisits()[1].visit_row.visit_time);
                EXPECT_EQ(cluster->visits[0]->first_visit_time,
                          GetHardcodedTestVisits()[1].visit_row.visit_time);
                EXPECT_EQ(cluster->visits[0]->page_title, "Github title");
                EXPECT_FALSE(base::Contains(
                    cluster->visits[0]->annotations,
                    history_clusters::mojom::Annotation::kBookmarked));
                EXPECT_TRUE(base::Contains(
                    cluster->visits[0]->annotations,
                    history_clusters::mojom::Annotation::kTabGrouped));
                EXPECT_FLOAT_EQ(cluster->visits[0]->score, 0.5);

                EXPECT_EQ(cluster->visits[1]->normalized_url,
                          "https://google.com/");
                EXPECT_EQ(cluster->visits[1]->raw_urls.size(), 1u);
                EXPECT_EQ(cluster->visits[1]->last_visit_time,
                          GetHardcodedTestVisits()[0].visit_row.visit_time);
                EXPECT_EQ(cluster->visits[1]->first_visit_time,
                          GetHardcodedTestVisits()[0].visit_row.visit_time);
                EXPECT_EQ(cluster->visits[1]->page_title, "Google title");
                EXPECT_TRUE(base::Contains(
                    cluster->visits[1]->annotations,
                    history_clusters::mojom::Annotation::kBookmarked));
                EXPECT_FALSE(base::Contains(
                    cluster->visits[1]->annotations,
                    history_clusters::mojom::Annotation::kTabGrouped));
                EXPECT_FLOAT_EQ(cluster->visits[1]->score, 0.5);

                ASSERT_EQ(cluster->keywords.size(), 2u);
                EXPECT_EQ(cluster->keywords[0], u"apples");
                EXPECT_EQ(cluster->keywords[1], u"Red Oranges");
              }

              if (test_data[i].expect_second_cluster) {
                const auto& cluster = test_data[i].expect_first_cluster
                                          ? clusters[1]
                                          : clusters[0];
                ASSERT_EQ(cluster->visits.size(), 1u);
                EXPECT_EQ(cluster->visits[0]->normalized_url,
                          "https://github.com/");
                EXPECT_EQ(cluster->visits[0]->raw_urls.size(), 1u);
                EXPECT_EQ(cluster->visits[0]->last_visit_time,
                          GetHardcodedTestVisits()[1].visit_row.visit_time);
                EXPECT_EQ(cluster->visits[0]->first_visit_time,
                          GetHardcodedTestVisits()[1].visit_row.visit_time);
                EXPECT_EQ(cluster->visits[0]->page_title, "Github title");
                EXPECT_TRUE(cluster->keywords.empty());
              }

              run_loop_quit.Run();
            }),
        &task_tracker_);

    history::BlockUntilHistoryProcessesPendingRequests(history_service_.get());
    VerifyTestClusteringBackendRequest();

    std::vector<history::Cluster> clusters;
    clusters.push_back(
        history::Cluster(0,
                         {
                             test_clustering_backend_->GetVisitById(1),
                             test_clustering_backend_->GetVisitById(2),
                         },
                         {u"apples", u"Red Oranges"}));
    clusters.push_back(
        history::Cluster(0,
                         {
                             test_clustering_backend_->GetVisitById(2),
                         },
                         {}));
    test_clustering_backend_->FulfillCallback(clusters);

    // Verify the callback is invoked.
    run_loop.Run();
  }
}

TEST_F(HistoryClustersServiceTest, CompleteVisitContextAnnotationsIfReady) {
  auto test = [&](RecordingStatus status, bool expected_complete) {
    auto& incomplete_visit_context_annotations =
        history_clusters_service_->GetOrCreateIncompleteVisitContextAnnotations(
            0);
    incomplete_visit_context_annotations.url_row.set_id(1);
    incomplete_visit_context_annotations.visit_row.visit_id = 1;
    incomplete_visit_context_annotations.status = status;
    history_clusters_service_->CompleteVisitContextAnnotationsIfReady(0);
    EXPECT_NE(
        history_clusters_service_->HasIncompleteVisitContextAnnotations(0),
        expected_complete);
  };

  // Complete cases:
  {
    SCOPED_TRACE("Complete without UKM");
    test({true, true, true}, true);
  }
  {
    SCOPED_TRACE("Complete with UKM");
    test({true, true, true, true, true}, true);
  }

  // Incomplete without UKM cases:
  {
    SCOPED_TRACE("Incomplete, missing history rows");
    test({false, true}, false);
  }
  {
    SCOPED_TRACE("Incomplete, navigation hasn't ended");
    test({true}, false);
  }
  {
    SCOPED_TRACE("Incomplete, navigation end metrics haven't been recorded");
    test({true, true}, false);
  }

  // Incomplete with UKM cases:
  {
    SCOPED_TRACE("Incomplete, missing history rows");
    test({false, true, false, true, true}, false);
  }
  {
    SCOPED_TRACE("Incomplete, navigation hasn't ended");
    test({true, false, false, true, true}, false);
  }
  {
    SCOPED_TRACE("Incomplete, navigation end metrics haven't been recorded");
    test({true, true, false, true, true}, false);
  }
  {
    SCOPED_TRACE("Incomplete, UKM page end missing");
    test({true, true, true, true, false}, false);
  }

  auto test_dcheck = [&](RecordingStatus status) {
    auto& incomplete_visit_context_annotations =
        history_clusters_service_->GetOrCreateIncompleteVisitContextAnnotations(
            0);
    incomplete_visit_context_annotations.url_row.set_id(1);
    incomplete_visit_context_annotations.visit_row.visit_id = 1;
    incomplete_visit_context_annotations.status = status;
    EXPECT_DCHECK_DEATH(
        history_clusters_service_->CompleteVisitContextAnnotationsIfReady(0));
    EXPECT_TRUE(
        history_clusters_service_->HasIncompleteVisitContextAnnotations(0));
  };

  // Impossible cases:
  {
    SCOPED_TRACE(
        "Impossible, navigation end signals recorded before navigation ended");
    test_dcheck({true, false, true});
  }
  {
    SCOPED_TRACE(
        "Impossible, navigation end signals recorded before history rows");
    test_dcheck({false, true, true});
  }
  {
    SCOPED_TRACE("Impossible, unexpected UKM page end recorded");
    test_dcheck({false, false, false, false, true});
  }
}

TEST_F(HistoryClustersServiceTest,
       CompleteVisitContextAnnotationsIfReadyWhenFeatureDisabled) {
  history_service_->AddPageWithDetails(GURL("https://fake.com"), u"Test 1", 1,
                                       1, base::Time::Now(), false,
                                       history::SOURCE_BROWSED);

  // When the feature is disabled, the `IncompleteVisitContextAnnotations`
  // should be removed but not added to visits.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{}, /*disabled_features=*/{
          kMemories,
          kPersistContextAnnotationsInHistoryDb,
      });
  auto& incomplete_visit_context_annotations =
      history_clusters_service_->GetOrCreateIncompleteVisitContextAnnotations(
          0);
  incomplete_visit_context_annotations.url_row.set_id(1);
  incomplete_visit_context_annotations.visit_row.visit_id = 1;
  incomplete_visit_context_annotations.status = {true, true, true};
  history_clusters_service_->CompleteVisitContextAnnotationsIfReady(0);
  EXPECT_FALSE(
      history_clusters_service_->HasIncompleteVisitContextAnnotations(0));
  EXPECT_TRUE(history_clusters_service_test_api_->GetVisits().empty());
}

TEST_F(HistoryClustersServiceTest,
       CompleteVisitContextAnnotationsIfReadyWhenFeatureEnabled) {
  history_service_->AddPageWithDetails(GURL("https://fake.com"), u"Test 1", 1,
                                       1, base::Time::Now(), false,
                                       history::SOURCE_BROWSED);

  // When the feature is enabled, the `IncompleteVisitContextAnnotations`
  // should be removed and added to visits.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kMemories);
  auto& incomplete_visit_context_annotations =
      history_clusters_service_->GetOrCreateIncompleteVisitContextAnnotations(
          0);
  incomplete_visit_context_annotations.url_row.set_id(1);
  incomplete_visit_context_annotations.visit_row.visit_id = 1;
  incomplete_visit_context_annotations.status = {true, true, true};
  history_clusters_service_->CompleteVisitContextAnnotationsIfReady(0);
  EXPECT_FALSE(
      history_clusters_service_->HasIncompleteVisitContextAnnotations(0));
  EXPECT_EQ(history_clusters_service_test_api_->GetVisits().size(), 1u);
}

TEST_F(HistoryClustersServiceTest, DoesQueryMatchAnyCluster) {
  AddHardcodedTestDataToHistoryService();

  // Verify that initially, the test keyword doesn't match anything, but this
  // query should have kicked off a cache population request.
  EXPECT_FALSE(history_clusters_service_->DoesQueryMatchAnyCluster("appl"));

  // Providing the response and running the task loop should populate the cache.
  history::BlockUntilHistoryProcessesPendingRequests(history_service_.get());
  VerifyTestClusteringBackendRequest();

  std::vector<history::Cluster> clusters;
  clusters.push_back(
      history::Cluster(0,
                       {
                           test_clustering_backend_->GetVisitById(1),
                           test_clustering_backend_->GetVisitById(2),
                       },
                       {u"apples"}));
  test_clustering_backend_->FulfillCallback(clusters);

  // Now the query should match the populated cache.
  EXPECT_TRUE(history_clusters_service_->DoesQueryMatchAnyCluster("appl"));

  // Also verify that queries that are too short don't match clusters.
  EXPECT_FALSE(history_clusters_service_->DoesQueryMatchAnyCluster("ap"));

  // But verify that it's okay to accept a short second query word.
  // We need this to prevent flicker as the user types in the omnibox.
  EXPECT_TRUE(history_clusters_service_->DoesQueryMatchAnyCluster("appl ap"));
}

}  // namespace

}  // namespace history_clusters
