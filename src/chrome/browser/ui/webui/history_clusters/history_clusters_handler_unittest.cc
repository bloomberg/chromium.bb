// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/history_clusters/history_clusters_handler.h"

#include <memory>
#include <string>
#include <vector>

#include "base/test/bind.h"
#include "base/time/time.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/webui/history_clusters/history_clusters_handler.h"
#include "chrome/test/base/testing_profile.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/url_row.h"
#include "components/history_clusters/core/history_clusters_types.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace history_clusters {

namespace {

history::ClusterVisit CreateVisit(
    std::string url,
    float score,
    std::vector<std::string> related_searches = {}) {
  history::ClusterVisit visit;
  visit.annotated_visit = {
      {GURL{url}, 0}, {}, {}, {}, 0, 0, history::VisitSource::SOURCE_BROWSED};
  visit.annotated_visit.content_annotations.related_searches = related_searches;
  visit.score = score;
  visit.normalized_url = GURL{url};
  return visit;
}

class HistoryClustersHandlerTest : public testing::Test {
 public:
  HistoryClustersHandlerTest() {
    TemplateURLServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        &profile_,
        base::BindRepeating(&TemplateURLServiceFactory::BuildInstanceFor));
  }

  // Everything must be called on Chrome_UIThread.
  content::BrowserTaskEnvironment task_environment_;

  TestingProfile profile_;
};

TEST_F(HistoryClustersHandlerTest, QueryClustersResultToMojom_BelowTheFold) {
  QueryClustersResult result;

  // High scoring visits should always be above the fold.
  history::Cluster cluster1;
  cluster1.cluster_id = 4;
  cluster1.visits.push_back(CreateVisit("https://high-score-1", 1));
  cluster1.visits.push_back(CreateVisit("https://high-score-2", .8));
  cluster1.visits.push_back(CreateVisit("https://high-score-3", .5));
  cluster1.visits.push_back(CreateVisit("https://high-score-4", .5));
  cluster1.visits.push_back(CreateVisit("https://high-score-5", .5));
  cluster1.keywords.push_back(u"keyword");

  // Low scoring visits should be above the fold only if they're one of top 4.
  history::Cluster cluster2;
  cluster2.cluster_id = 6;
  cluster2.visits.push_back(CreateVisit("https://low-score-1", .4));
  cluster2.visits.push_back(CreateVisit("https://low-score-2", .4));
  cluster2.visits.push_back(CreateVisit("https://low-score-3", .4));
  cluster2.visits.push_back(CreateVisit("https://low-score-4", .4));
  cluster2.visits.push_back(CreateVisit("https://low-score-5", .4));
  cluster2.keywords.push_back(u"keyword");

  // 0 scoring visits should be above the fold only if they're 1st.
  history::Cluster cluster3;
  cluster3.cluster_id = 8;
  cluster3.visits.push_back(CreateVisit("https://zero-score-1", 0));
  cluster3.visits.push_back(CreateVisit("https://zero-score-2", 0));
  cluster3.keywords.push_back(u"keyword");

  result.clusters.push_back(cluster1);
  result.clusters.push_back(cluster2);
  result.clusters.push_back(cluster3);
  result.continuation_end_time = base::Time::FromDoubleT(10);

  mojom::QueryResultPtr mojom_result =
      QueryClustersResultToMojom(&profile_, "query", false, result);

  EXPECT_EQ(mojom_result->query, "query");
  EXPECT_EQ(mojom_result->is_continuation, false);
  EXPECT_EQ(mojom_result->continuation_end_time, base::Time::FromDoubleT(10));

  ASSERT_EQ(mojom_result->clusters.size(), 3u);

  {
    EXPECT_EQ(mojom_result->clusters[0]->id, 4);
    EXPECT_EQ(mojom_result->clusters[0]->visit->below_the_fold, false);
    const auto& related_visits =
        mojom_result->clusters[0]->visit->related_visits;
    ASSERT_EQ(related_visits.size(), 4u);
    EXPECT_EQ(related_visits[0]->below_the_fold, false);
    EXPECT_EQ(related_visits[1]->below_the_fold, false);
    EXPECT_EQ(related_visits[2]->below_the_fold, false);
    EXPECT_EQ(related_visits[3]->below_the_fold, false);
  }

  {
    EXPECT_EQ(mojom_result->clusters[1]->id, 6);
    EXPECT_EQ(mojom_result->clusters[0]->visit->below_the_fold, false);
    const auto& related_visits =
        mojom_result->clusters[1]->visit->related_visits;
    ASSERT_EQ(related_visits.size(), 4u);
    EXPECT_EQ(related_visits[0]->below_the_fold, false);
    EXPECT_EQ(related_visits[1]->below_the_fold, false);
    EXPECT_EQ(related_visits[2]->below_the_fold, false);
    EXPECT_EQ(related_visits[3]->below_the_fold, true);
  }

  {
    EXPECT_EQ(mojom_result->clusters[2]->id, 8);
    ASSERT_EQ(mojom_result->clusters[2]->visit->related_visits.size(), 1u);
    EXPECT_EQ(mojom_result->clusters[2]->visit->below_the_fold, false);
    const auto& related_visits =
        mojom_result->clusters[02]->visit->related_visits;
    EXPECT_EQ(related_visits[0]->below_the_fold, true);
  }
}

TEST_F(HistoryClustersHandlerTest, QueryClustersResultToMojom_RelatedSearches) {
  QueryClustersResult result;

  history::Cluster cluster;
  cluster.cluster_id = 4;
  // Should include the top visit's related searches.
  cluster.visits.push_back(
      CreateVisit("https://high-score-1", 0, {"one", "two"}));
  // Should exclude duplicates (i.e. "one").
  cluster.visits.push_back(CreateVisit(
      "https://high-score-2", 0, {"one", "three", "four", "five", "six"}));
  // Visits without related searches shouldn't interrupt the coalescing.
  cluster.visits.push_back(CreateVisit("https://high-score-3", 0));
  // Should include all related searches of a visit even if doing so surpasses
  // the max related searches (8).
  cluster.visits.push_back(
      CreateVisit("https://high-score-4", 0, {"seven", "eight", "nine"}));
  // Should not include related searches of more visits once the max related
  // searches has been met.
  cluster.visits.push_back(CreateVisit("https://high-score-5", 0, {"ten"}));

  result.clusters.push_back(cluster);

  mojom::QueryResultPtr mojom_result =
      QueryClustersResultToMojom(&profile_, "query", false, result);

  ASSERT_EQ(mojom_result->clusters.size(), 1u);
  EXPECT_EQ(mojom_result->clusters[0]->id, 4);
  const auto& top_visit = mojom_result->clusters[0]->visit;
  ASSERT_EQ(top_visit->related_searches.size(), 9u);
  EXPECT_EQ(top_visit->related_searches[0]->query, "one");
  EXPECT_EQ(top_visit->related_searches[1]->query, "two");
  EXPECT_EQ(top_visit->related_searches[2]->query, "three");
  EXPECT_EQ(top_visit->related_searches[3]->query, "four");
  EXPECT_EQ(top_visit->related_searches[4]->query, "five");
  EXPECT_EQ(top_visit->related_searches[5]->query, "six");
  EXPECT_EQ(top_visit->related_searches[6]->query, "seven");
  EXPECT_EQ(top_visit->related_searches[7]->query, "eight");
  EXPECT_EQ(top_visit->related_searches[8]->query, "nine");
}

// TODO(manukh) Add a test case for `VisitToMojom`.

}  // namespace

}  // namespace history_clusters
