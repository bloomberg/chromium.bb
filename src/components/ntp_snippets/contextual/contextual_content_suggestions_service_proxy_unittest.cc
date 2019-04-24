// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/contextual/contextual_content_suggestions_service_proxy.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "components/ntp_snippets/contextual/contextual_content_suggestions_service.h"
#include "components/ntp_snippets/contextual/contextual_suggestions_test_utils.h"
#include "components/ntp_snippets/contextual/reporting/contextual_suggestions_metrics_reporter.h"
#include "components/ntp_snippets/remote/remote_suggestions_database.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::IsEmpty;
using testing::Pointee;

namespace contextual_suggestions {

namespace {

static constexpr char kTestPeekText[] = "Test peek test";
static constexpr char kValidFromUrl[] = "http://some.url";

static constexpr char kImageId[] = "id";
// Keep the string after tbn: in sync with the above constant for image id.
static constexpr char kImageUrl[] = "http://www.google.com/images?q=tbn:id";
static constexpr char kFaviconUrl[] = "http://google.com/cats.fav";

class FakeContextualContentSuggestionsService
    : public ContextualContentSuggestionsService {
 public:
  FakeContextualContentSuggestionsService();
  ~FakeContextualContentSuggestionsService() override;

  void FetchContextualSuggestionClusters(
      const GURL& url,
      FetchClustersCallback callback,
      ReportFetchMetricsCallback metrics_callback) override {
    clusters_callback_ = std::move(callback);
  }

  void RunClustersCallback(ContextualSuggestionsResult result) {
    std::move(clusters_callback_).Run(std::move(result));
  }

 private:
  FetchClustersCallback clusters_callback_;
};

FakeContextualContentSuggestionsService::
    FakeContextualContentSuggestionsService()
    : ContextualContentSuggestionsService(nullptr, nullptr) {}

FakeContextualContentSuggestionsService::
    ~FakeContextualContentSuggestionsService() {}
}  // namespace

class ContextualContentSuggestionsServiceProxyTest : public testing::Test {
 public:
  void SetUp() override;

  FakeContextualContentSuggestionsService* service() { return service_.get(); }

  ContextualContentSuggestionsServiceProxy* proxy() { return proxy_.get(); }

 private:
  std::unique_ptr<FakeContextualContentSuggestionsService> service_;
  std::unique_ptr<ContextualContentSuggestionsServiceProxy> proxy_;
};

void ContextualContentSuggestionsServiceProxyTest::SetUp() {
  service_ = std::make_unique<FakeContextualContentSuggestionsService>();
  auto metrics_reporter =
      std::make_unique<ContextualSuggestionsMetricsReporter>();
  proxy_ = std::make_unique<ContextualContentSuggestionsServiceProxy>(
      service_.get(), std::move(metrics_reporter));
}

TEST_F(ContextualContentSuggestionsServiceProxyTest,
       FetchSuggestionsWhenEmpty) {
  MockClustersCallback mock_cluster_callback;

  proxy()->FetchContextualSuggestions(GURL(kValidFromUrl),
                                      mock_cluster_callback.ToOnceCallback());
  service()->RunClustersCallback(
      ContextualSuggestionsResult(kTestPeekText, std::vector<Cluster>(),
                                  PeekConditions(), ServerExperimentInfos()));

  EXPECT_TRUE(mock_cluster_callback.has_run);
}

TEST_F(ContextualContentSuggestionsServiceProxyTest, GetImageUrl) {
  auto suggestion = SuggestionBuilder(GURL(kValidFromUrl))
                        .ImageId(kImageId)
                        .FaviconImageUrl(kFaviconUrl)
                        .Build();
  auto cluster =
      ClusterBuilder(kValidFromUrl).AddSuggestion(suggestion).Build();
  MockClustersCallback mock_cluster_callback;

  proxy()->FetchContextualSuggestions(GURL(kValidFromUrl),
                                      mock_cluster_callback.ToOnceCallback());
  service()->RunClustersCallback(ContextualSuggestionsResult(
      kTestPeekText, std::vector<Cluster>({cluster}), PeekConditions(),
      ServerExperimentInfos()));

  EXPECT_EQ(kImageUrl, proxy()->GetContextualSuggestionImageUrl(suggestion.id));
  EXPECT_EQ(kFaviconUrl,
            proxy()->GetContextualSuggestionFaviconUrl(suggestion.id));

  // Id not found.
  EXPECT_EQ("", proxy()->GetContextualSuggestionImageUrl(""));
  EXPECT_EQ("", proxy()->GetContextualSuggestionFaviconUrl(""));
}

// TODO(fgorski): More tests will be added, once we have the suggestions
// restructured.

}  // namespace contextual_suggestions
