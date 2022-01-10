// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/clusterer.h"

#include "base/test/task_environment.h"
#include "components/history_clusters/core/clustering_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace history_clusters {
namespace {

using ::testing::ElementsAre;

class ClustererTest : public ::testing::Test {
 public:
  void SetUp() override { clusterer_ = std::make_unique<Clusterer>(); }

  void TearDown() override { clusterer_.reset(); }

  std::vector<history::Cluster> CreateInitialClustersFromVisits(
      const std::vector<history::ClusterVisit>& visits) {
    return clusterer_->CreateInitialClustersFromVisits(visits);
  }

 private:
  std::unique_ptr<Clusterer> clusterer_;
  base::test::TaskEnvironment task_environment_;
};

TEST_F(ClustererTest, ClusterOneVisit) {
  std::vector<history::ClusterVisit> visits;

  // Fill in the visits vector with 1 visit.
  history::AnnotatedVisit visit =
      testing::CreateDefaultAnnotatedVisit(1, GURL("https://google.com/"));
  visits.push_back(testing::CreateClusterVisit(visit));

  std::vector<history::Cluster> result_clusters =
      CreateInitialClustersFromVisits(visits);
  EXPECT_THAT(testing::ToVisitResults(result_clusters),
              ElementsAre(ElementsAre(testing::VisitResult(1, 1.0))));
}

TEST_F(ClustererTest, ClusterTwoVisitsTiedByReferringVisit) {
  std::vector<history::ClusterVisit> visits;

  // Visit2's referrer is visit 1 and are close together.
  history::AnnotatedVisit visit =
      testing::CreateDefaultAnnotatedVisit(1, GURL("https://google.com/"));
  visits.push_back(testing::CreateClusterVisit(visit));

  history::AnnotatedVisit visit2 =
      testing::CreateDefaultAnnotatedVisit(2, GURL("https://google.com/next"));
  visit2.referring_visit_of_redirect_chain_start = 1;
  visits.push_back(testing::CreateClusterVisit(visit2));

  std::vector<history::Cluster> result_clusters =
      CreateInitialClustersFromVisits(visits);
  EXPECT_THAT(testing::ToVisitResults(result_clusters),
              ElementsAre(ElementsAre(testing::VisitResult(1, 1.0),
                                      testing::VisitResult(2, 1.0))));
}

TEST_F(ClustererTest, ClusterTwoVisitsTiedByOpenerVisit) {
  std::vector<history::ClusterVisit> visits;

  // Visit2's referrer is visit 1 and are close together.
  history::AnnotatedVisit visit =
      testing::CreateDefaultAnnotatedVisit(1, GURL("https://google.com/"));
  visits.push_back(testing::CreateClusterVisit(visit));

  history::AnnotatedVisit visit2 =
      testing::CreateDefaultAnnotatedVisit(2, GURL("https://google.com/next"));
  visit2.opener_visit_of_redirect_chain_start = 1;
  visits.push_back(testing::CreateClusterVisit(visit2));

  std::vector<history::Cluster> result_clusters =
      CreateInitialClustersFromVisits(visits);
  EXPECT_THAT(testing::ToVisitResults(result_clusters),
              ElementsAre(ElementsAre(testing::VisitResult(1, 1.0),
                                      testing::VisitResult(2, 1.0))));
}

TEST_F(ClustererTest, ClusterTwoVisitsTiedByURL) {
  std::vector<history::ClusterVisit> visits;

  // Visit2 has the same URL as Visit1.
  history::AnnotatedVisit visit =
      testing::CreateDefaultAnnotatedVisit(1, GURL("https://google.com/"));
  visits.push_back(testing::CreateClusterVisit(visit));

  history::AnnotatedVisit visit2 =
      testing::CreateDefaultAnnotatedVisit(2, GURL("https://google.com/"));
  visits.push_back(testing::CreateClusterVisit(visit2));

  std::vector<history::Cluster> result_clusters =
      CreateInitialClustersFromVisits(visits);
  EXPECT_THAT(testing::ToVisitResults(result_clusters),
              ElementsAre(ElementsAre(testing::VisitResult(1, 1.0),
                                      testing::VisitResult(2, 1.0))));
}

TEST_F(ClustererTest, ClusterTwoVisitsTiedByNormalizedURL) {
  std::vector<history::ClusterVisit> visits;

  // Visit2 has the same URL as Visit1.
  history::AnnotatedVisit visit = testing::CreateDefaultAnnotatedVisit(
      1, GURL("https://example.com/normalized?q=whatever"));
  visits.push_back(testing::CreateClusterVisit(
      visit, GURL("https://example.com/normalized")));

  history::AnnotatedVisit visit2 = testing::CreateDefaultAnnotatedVisit(
      2, GURL("https://example.com/normalized"));
  visits.push_back(testing::CreateClusterVisit(visit2));

  std::vector<history::Cluster> result_clusters =
      CreateInitialClustersFromVisits(visits);
  EXPECT_THAT(testing::ToVisitResults(result_clusters),
              ElementsAre(ElementsAre(testing::VisitResult(1, 1.0),
                                      testing::VisitResult(2, 1.0))));
}

TEST_F(ClustererTest, MultipleClusters) {
  std::vector<history::ClusterVisit> visits;

  // Visit2's referrer is visit 1 and visit 4 is a back navigation from visit 2.
  // Visit 3 is a different journey altogether. Visit 10 is referring to a
  // missing visit and should be considered as in its own cluster.
  // Also, make sure these aren't sorted so we test that we are sorting the
  // visits by visit ID.
  history::AnnotatedVisit visit =
      testing::CreateDefaultAnnotatedVisit(1, GURL("https://github.com/"));
  visits.push_back(testing::CreateClusterVisit(visit));

  history::AnnotatedVisit visit2 =
      testing::CreateDefaultAnnotatedVisit(2, GURL("https://google.com/"));
  visit2.referring_visit_of_redirect_chain_start = 1;
  // Set the visit duration to be 2x the default so it has the same duration
  // after |visit| and |visit4| are deduped.
  visit2.visit_row.visit_duration = base::Seconds(20);
  visits.push_back(testing::CreateClusterVisit(visit2));

  history::AnnotatedVisit visit4 =
      testing::CreateDefaultAnnotatedVisit(4, GURL("https://github.com/"));
  visits.push_back(testing::CreateClusterVisit(visit4));

  history::AnnotatedVisit visit5 = testing::CreateDefaultAnnotatedVisit(
      10, GURL("https://nonexistentreferrer.com/"));
  visit5.referring_visit_of_redirect_chain_start = 6;
  visits.push_back(testing::CreateClusterVisit(visit5));

  history::AnnotatedVisit visit3 =
      testing::CreateDefaultAnnotatedVisit(3, GURL("https://whatever.com/"));
  visits.push_back(testing::CreateClusterVisit(visit3));

  std::vector<history::Cluster> result_clusters =
      CreateInitialClustersFromVisits(visits);
  EXPECT_THAT(testing::ToVisitResults(result_clusters),
              ElementsAre(ElementsAre(testing::VisitResult(1, 1.0),
                                      testing::VisitResult(2, 1.0),
                                      testing::VisitResult(4, 1.0)),
                          ElementsAre(testing::VisitResult(3, 1.0)),
                          ElementsAre(testing::VisitResult(10, 1.0))));
}

TEST_F(ClustererTest, SplitClusterOnNavigationTime) {
  std::vector<history::ClusterVisit> visits;

  history::AnnotatedVisit visit =
      testing::CreateDefaultAnnotatedVisit(1, GURL("https://google.com/"));
  visit.visit_row.visit_time = base::Time::Now();
  visits.push_back(testing::CreateClusterVisit(visit));

  // Visit2 has a different URL but is linked by referring id to visit.
  history::AnnotatedVisit visit2 =
      testing::CreateDefaultAnnotatedVisit(2, GURL("https://bar.com/"));
  visit2.referring_visit_of_redirect_chain_start = 1;
  visit2.visit_row.visit_time = base::Time::Now() + base::Minutes(5);
  visits.push_back(testing::CreateClusterVisit(visit2));

  // Visit3 has a different URL but is linked by referring id to visit but the
  // cutoff has passed so it should be in a different cluster.
  history::AnnotatedVisit visit3 =
      testing::CreateDefaultAnnotatedVisit(3, GURL("https://foo.com/"));
  visit3.referring_visit_of_redirect_chain_start = 1;
  visit3.visit_row.visit_time = base::Time::Now() + base::Hours(2);
  visits.push_back(testing::CreateClusterVisit(visit3));

  std::vector<history::Cluster> result_clusters =
      CreateInitialClustersFromVisits(visits);
  EXPECT_THAT(testing::ToVisitResults(result_clusters),
              ElementsAre(ElementsAre(testing::VisitResult(1, 1.0),
                                      testing::VisitResult(2, 1.0)),
                          ElementsAre(testing::VisitResult(3, 1.0))));
}

}  // namespace
}  // namespace history_clusters
