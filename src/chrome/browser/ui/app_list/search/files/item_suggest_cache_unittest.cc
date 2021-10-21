// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/files/item_suggest_cache.h"

#include <vector>

#include "base/json/json_reader.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/chrome_signin_client_test_util.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/drive/drive_pref_names.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/http/http_util.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kEmail[] = "test-user@example.com";
constexpr char16_t kEmail16[] = u"test-user@example.com";
constexpr char kRequestUrl[] =
    "https://appsitemsuggest-pa.googleapis.com/v1/items";
constexpr char kValidJsonResponse[] = R"(
    {
      "item": [
        {
          "itemId": "item id 1",
          "displayText": "display text 1"
        },
        {
          "itemId": "item id 2",
          "displayText": "display text 2",
          "predictionReason": "unused field"
        },
        {
          "itemId": "item id 3",
          "displayText": "display text 3"
        }
      ],
      "suggestionSessionId": "suggestion id 1"
    })";
constexpr char kStatusHistogramName[] = "Apps.AppList.ItemSuggestCache.Status";
constexpr char kResponseSizeHistogramName[] =
    "Apps.AppList.ItemSuggestCache.ResponseSize";
}  // namespace

namespace app_list {
using base::test::ScopedFeatureList;

// TODO(crbug.com/1034842): Add test checking we make no requests when disabled
// by experiment or policy.

class ItemSuggestCacheTest : public testing::Test {
 protected:
  ItemSuggestCacheTest() = default;
  ~ItemSuggestCacheTest() override = default;

  base::Value Parse(const std::string& json) {
    return base::JSONReader::Read(json).value();
  }

  void ResultMatches(const ItemSuggestCache::Result& actual,
                     const std::string& id,
                     const std::string& title) {
    EXPECT_EQ(actual.id, id);
    EXPECT_EQ(actual.title, title);
  }

  void ResultsMatch(
      const absl::optional<ItemSuggestCache::Results>& actual,
      const std::string& suggestion_id,
      const std::vector<std::pair<std::string, std::string>>& results) {
    EXPECT_TRUE(actual.has_value());

    EXPECT_EQ(actual->suggestion_id, suggestion_id);
    ASSERT_EQ(actual->results.size(), results.size());
    for (int i = 0; i < results.size(); ++i) {
      ResultMatches(actual->results[i], results[i].first, results[i].second);
    }
  }

  void SetUp() override {
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    TestingProfile::TestingFactories factories =
        IdentityTestEnvironmentProfileAdaptor::
            GetIdentityTestEnvironmentFactories();
    factories.push_back(
        {ChromeSigninClientFactory::GetInstance(),
         base::BindRepeating(&BuildChromeSigninClientWithURLLoader,
                             &url_loader_factory_)});
    profile_ = profile_manager_->CreateTestingProfile(
        kEmail, /*prefs=*/{}, kEmail16,
        /*avatar_id=*/0, /*supervised_user_id=*/{}, factories);

    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_);
    identity_test_env_ = identity_test_env_adaptor_->identity_test_env();
    identity_test_env_->SetTestURLLoaderFactory(&url_loader_factory_);
    shared_url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &url_loader_factory_);
  }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  signin::IdentityTestEnvironment* identity_test_env_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;

  std::unique_ptr<TestingProfileManager> profile_manager_;
  TestingProfile* profile_;

  network::TestURLLoaderFactory url_loader_factory_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;

  ScopedFeatureList scoped_feature_list_;
  base::Feature feature_ = ItemSuggestCache::kExperiment;

  const base::HistogramTester histogram_tester_;
};

TEST_F(ItemSuggestCacheTest, ConvertJsonSuccess) {
  const base::Value full = Parse(kValidJsonResponse);
  ResultsMatch(ItemSuggestCache::ConvertJsonForTest(&full), "suggestion id 1",
               {{"item id 1", "display text 1"},
                {"item id 2", "display text 2"},
                {"item id 3", "display text 3"}});

  const base::Value empty_items = Parse(R"(
    {
      "item": [],
      "suggestionSessionId": "the suggestion id"
    })");
  ResultsMatch(ItemSuggestCache::ConvertJsonForTest(&empty_items),
               "the suggestion id", {});

  const base::Value no_items = Parse(R"(
    {
      "suggestionSessionId": "the suggestion id"
    })");
  ResultsMatch(ItemSuggestCache::ConvertJsonForTest(&no_items),
               "the suggestion id", {});
}

TEST_F(ItemSuggestCacheTest, ConvertJsonFailure) {
  const base::Value no_display_text = Parse(R"(
    {
      "item": [
        {
          "itemId": "item id 1"
        }
      ],
      "suggestionSessionId": "the suggestion id"
    })");
  EXPECT_FALSE(
      ItemSuggestCache::ConvertJsonForTest(&no_display_text).has_value());

  const base::Value no_item_id = Parse(R"(
    {
      "item": [
        {
          "displayText": "display text 1"
        }
      ],
      "suggestionSessionId": "the suggestion id"
    })");
  EXPECT_FALSE(ItemSuggestCache::ConvertJsonForTest(&no_item_id).has_value());

  const base::Value no_session_id = Parse(R"(
    {
      "item": [
        {
          "itemId": "item id 1",
          "displayText": "display text 2"
        }
      ]
    })");
  EXPECT_FALSE(
      ItemSuggestCache::ConvertJsonForTest(&no_session_id).has_value());
}

TEST_F(ItemSuggestCacheTest, UpdateCacheDisabledByExperiment) {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      feature_, {{"enabled", "false"}});
  std::unique_ptr<ItemSuggestCache> itemSuggestCache =
      std::make_unique<ItemSuggestCache>(profile_, shared_url_loader_factory_);
  itemSuggestCache->UpdateCache();
  task_environment_.RunUntilIdle();
  histogram_tester_.ExpectUniqueSample(
      kStatusHistogramName, ItemSuggestCache::Status::kDisabledByExperiment, 1);
}

TEST_F(ItemSuggestCacheTest, UpdateCacheDisabledByPolicy) {
  profile_->GetPrefs()->SetBoolean(drive::prefs::kDisableDrive, true);
  std::unique_ptr<ItemSuggestCache> itemSuggestCache =
      std::make_unique<ItemSuggestCache>(profile_, shared_url_loader_factory_);
  itemSuggestCache->UpdateCache();
  task_environment_.RunUntilIdle();
  histogram_tester_.ExpectUniqueSample(
      kStatusHistogramName, ItemSuggestCache::Status::kDisabledByPolicy, 1);
}

TEST_F(ItemSuggestCacheTest, UpdateCacheServerUrlIsNotHttps) {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      feature_,
      {{"server_url", "http://appsitemsuggest-pa.googleapis.com/v1/items"}});
  std::unique_ptr<ItemSuggestCache> itemSuggestCache =
      std::make_unique<ItemSuggestCache>(profile_, shared_url_loader_factory_);
  itemSuggestCache->UpdateCache();
  task_environment_.RunUntilIdle();
  histogram_tester_.ExpectUniqueSample(
      kStatusHistogramName, ItemSuggestCache::Status::kInvalidServerUrl, 1);
}

TEST_F(ItemSuggestCacheTest, UpdateCacheServerUrlIsNotGoogleDomain) {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      feature_, {{"server_url", "https://foo.com"}});
  std::unique_ptr<ItemSuggestCache> itemSuggestCache =
      std::make_unique<ItemSuggestCache>(profile_, shared_url_loader_factory_);
  itemSuggestCache->UpdateCache();
  task_environment_.RunUntilIdle();
  histogram_tester_.ExpectUniqueSample(
      kStatusHistogramName, ItemSuggestCache::Status::kInvalidServerUrl, 1);
}

TEST_F(ItemSuggestCacheTest, UpdateCacheServerNoAuthToken) {
  std::unique_ptr<ItemSuggestCache> itemSuggestCache =
      std::make_unique<ItemSuggestCache>(profile_, shared_url_loader_factory_);
  itemSuggestCache->UpdateCache();
  task_environment_.RunUntilIdle();
  histogram_tester_.ExpectUniqueSample(
      kStatusHistogramName, ItemSuggestCache::Status::kGoogleAuthError, 1);
}

TEST_F(ItemSuggestCacheTest, UpdateCacheInsufficientResourcesError) {
  std::unique_ptr<ItemSuggestCache> itemSuggestCache =
      std::make_unique<ItemSuggestCache>(profile_, shared_url_loader_factory_);
  identity_test_env_->MakePrimaryAccountAvailable(kEmail,
                                                  signin::ConsentLevel::kSync);
  identity_test_env_->SetAutomaticIssueOfAccessTokens(true);

  auto head = network::CreateURLResponseHead(net::HTTP_OK);
  network::URLLoaderCompletionStatus status(net::ERR_INSUFFICIENT_RESOURCES);
  url_loader_factory_.AddResponse(GURL(kRequestUrl), std::move(head), "content",
                                  status);
  itemSuggestCache->UpdateCache();

  task_environment_.RunUntilIdle();
  histogram_tester_.ExpectUniqueSample(
      kStatusHistogramName, ItemSuggestCache::Status::kResponseTooLarge, 1);
}

TEST_F(ItemSuggestCacheTest, UpdateCacheNetError) {
  std::unique_ptr<ItemSuggestCache> itemSuggestCache =
      std::make_unique<ItemSuggestCache>(profile_, shared_url_loader_factory_);
  identity_test_env_->MakePrimaryAccountAvailable(kEmail,
                                                  signin::ConsentLevel::kSync);
  identity_test_env_->SetAutomaticIssueOfAccessTokens(true);

  auto head = network::CreateURLResponseHead(net::HTTP_OK);
  network::URLLoaderCompletionStatus status(net::ERR_FAILED);
  url_loader_factory_.AddResponse(GURL(kRequestUrl), std::move(head), "content",
                                  status);
  itemSuggestCache->UpdateCache();

  task_environment_.RunUntilIdle();
  histogram_tester_.ExpectUniqueSample(kStatusHistogramName,
                                       ItemSuggestCache::Status::kNetError, 1);
}

TEST_F(ItemSuggestCacheTest, UpdateCache5kkError) {
  std::unique_ptr<ItemSuggestCache> itemSuggestCache =
      std::make_unique<ItemSuggestCache>(profile_, shared_url_loader_factory_);
  identity_test_env_->MakePrimaryAccountAvailable(kEmail,
                                                  signin::ConsentLevel::kSync);
  identity_test_env_->SetAutomaticIssueOfAccessTokens(true);

  auto head = network::mojom::URLResponseHead::New();
  std::string headers("HTTP/1.1 500 Owiee\nContent-type: application/json\n\n");
  head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(headers));

  url_loader_factory_.AddResponse(GURL(kRequestUrl), std::move(head),
                                  /* content= */ "",
                                  network::URLLoaderCompletionStatus(net::OK));
  itemSuggestCache->UpdateCache();

  task_environment_.RunUntilIdle();
  histogram_tester_.ExpectUniqueSample(kStatusHistogramName,
                                       ItemSuggestCache::Status::k5xxStatus, 1);
}

TEST_F(ItemSuggestCacheTest, UpdateCache4kkError) {
  std::unique_ptr<ItemSuggestCache> itemSuggestCache =
      std::make_unique<ItemSuggestCache>(profile_, shared_url_loader_factory_);
  identity_test_env_->MakePrimaryAccountAvailable(kEmail,
                                                  signin::ConsentLevel::kSync);
  identity_test_env_->SetAutomaticIssueOfAccessTokens(true);

  auto head = network::mojom::URLResponseHead::New();
  std::string headers("HTTP/1.1 400 Owiee\nContent-type: application/json\n\n");
  head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(headers));

  url_loader_factory_.AddResponse(GURL(kRequestUrl), std::move(head),
                                  /* content= */ "",
                                  network::URLLoaderCompletionStatus(net::OK));
  itemSuggestCache->UpdateCache();

  task_environment_.RunUntilIdle();
  histogram_tester_.ExpectUniqueSample(kStatusHistogramName,
                                       ItemSuggestCache::Status::k4xxStatus, 1);
}

TEST_F(ItemSuggestCacheTest, UpdateCache3kkError) {
  std::unique_ptr<ItemSuggestCache> itemSuggestCache =
      std::make_unique<ItemSuggestCache>(profile_, shared_url_loader_factory_);
  identity_test_env_->MakePrimaryAccountAvailable(kEmail,
                                                  signin::ConsentLevel::kSync);
  identity_test_env_->SetAutomaticIssueOfAccessTokens(true);

  auto head = network::mojom::URLResponseHead::New();
  std::string headers("HTTP/1.1 300 Owiee\nContent-type: application/json\n\n");
  head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(headers));

  url_loader_factory_.AddResponse(GURL(kRequestUrl), std::move(head),
                                  /* content= */ "",
                                  network::URLLoaderCompletionStatus(net::OK));
  itemSuggestCache->UpdateCache();

  task_environment_.RunUntilIdle();
  histogram_tester_.ExpectUniqueSample(kStatusHistogramName,
                                       ItemSuggestCache::Status::k3xxStatus, 1);
}

TEST_F(ItemSuggestCacheTest, UpdateCacheEmptyResponse) {
  std::unique_ptr<ItemSuggestCache> itemSuggestCache =
      std::make_unique<ItemSuggestCache>(profile_, shared_url_loader_factory_);
  identity_test_env_->MakePrimaryAccountAvailable(kEmail,
                                                  signin::ConsentLevel::kSync);
  identity_test_env_->SetAutomaticIssueOfAccessTokens(true);
  url_loader_factory_.AddResponse(kRequestUrl,
                                  /* content= */ "", net::HTTP_OK);

  itemSuggestCache->UpdateCache();

  task_environment_.RunUntilIdle();
  histogram_tester_.ExpectUniqueSample(
      kStatusHistogramName, ItemSuggestCache::Status::kEmptyResponse, 1);
}

TEST_F(ItemSuggestCacheTest, UpdateCacheInvalidResponse) {
  std::unique_ptr<ItemSuggestCache> itemSuggestCache =
      std::make_unique<ItemSuggestCache>(profile_, shared_url_loader_factory_);
  identity_test_env_->MakePrimaryAccountAvailable(kEmail,
                                                  signin::ConsentLevel::kSync);
  identity_test_env_->SetAutomaticIssueOfAccessTokens(true);
  url_loader_factory_.AddResponse(kRequestUrl, "invalid = json", net::HTTP_OK);

  itemSuggestCache->UpdateCache();

  task_environment_.RunUntilIdle();
  histogram_tester_.ExpectUniqueSample(kResponseSizeHistogramName,
                                       /* sample= */ 14,
                                       /* expected_count= */ 1);
  histogram_tester_.ExpectUniqueSample(
      kStatusHistogramName, ItemSuggestCache::Status::kJsonParseFailure, 1);
}

TEST_F(ItemSuggestCacheTest, UpdateCacheConversionFailure) {
  std::unique_ptr<ItemSuggestCache> itemSuggestCache =
      std::make_unique<ItemSuggestCache>(profile_, shared_url_loader_factory_);
  identity_test_env_->MakePrimaryAccountAvailable(kEmail,
                                                  signin::ConsentLevel::kSync);
  identity_test_env_->SetAutomaticIssueOfAccessTokens(true);
  url_loader_factory_.AddResponse(kRequestUrl,
                                  R"(
        {
          "a": ""
        }
      )",
                                  net::HTTP_OK);

  itemSuggestCache->UpdateCache();

  task_environment_.RunUntilIdle();
  histogram_tester_.ExpectUniqueSample(kResponseSizeHistogramName, 45, 1);
  histogram_tester_.ExpectUniqueSample(
      kStatusHistogramName, ItemSuggestCache::Status::kJsonConversionFailure,
      1);
}

TEST_F(ItemSuggestCacheTest, UpdateCacheConversionEmptyResults) {
  std::unique_ptr<ItemSuggestCache> itemSuggestCache =
      std::make_unique<ItemSuggestCache>(profile_, shared_url_loader_factory_);
  identity_test_env_->MakePrimaryAccountAvailable(kEmail,
                                                  signin::ConsentLevel::kSync);
  identity_test_env_->SetAutomaticIssueOfAccessTokens(true);
  url_loader_factory_.AddResponse(kRequestUrl,
                                  R"(
    {
      "item": [],
      "suggestionSessionId": "the suggestion id"
    })",
                                  net::HTTP_OK);

  itemSuggestCache->UpdateCache();

  task_environment_.RunUntilIdle();
  histogram_tester_.ExpectUniqueSample(kResponseSizeHistogramName,
                                       /* sample= */ 79,
                                       /* expected_count= */ 1);
  histogram_tester_.ExpectUniqueSample(
      kStatusHistogramName, ItemSuggestCache::Status::kNoResultsInResponse, 1);
}

TEST_F(ItemSuggestCacheTest, UpdateCacheSavesResults) {
  std::unique_ptr<ItemSuggestCache> itemSuggestCache =
      std::make_unique<ItemSuggestCache>(profile_, shared_url_loader_factory_);
  identity_test_env_->MakePrimaryAccountAvailable(kEmail,
                                                  signin::ConsentLevel::kSync);
  identity_test_env_->SetAutomaticIssueOfAccessTokens(true);
  url_loader_factory_.AddResponse(kRequestUrl, kValidJsonResponse,
                                  net::HTTP_OK);

  itemSuggestCache->UpdateCache();

  task_environment_.RunUntilIdle();
  histogram_tester_.ExpectUniqueSample(kResponseSizeHistogramName,
                                       /* sample= */ 419,
                                       /* expected_count= */ 1);
  ResultsMatch(itemSuggestCache->GetResults(), "suggestion id 1",
               {{"item id 1", "display text 1"},
                {"item id 2", "display text 2"},
                {"item id 3", "display text 3"}});
  histogram_tester_.ExpectUniqueSample(kStatusHistogramName,
                                       ItemSuggestCache::Status::kOk, 1);
}

TEST_F(ItemSuggestCacheTest, UpdateCacheSmallTimeBetweenUpdates) {
  std::unique_ptr<ItemSuggestCache> itemSuggestCache =
      std::make_unique<ItemSuggestCache>(profile_, shared_url_loader_factory_);
  identity_test_env_->MakePrimaryAccountAvailable(kEmail,
                                                  signin::ConsentLevel::kSync);
  identity_test_env_->SetAutomaticIssueOfAccessTokens(true);
  url_loader_factory_.AddResponse(kRequestUrl,
                                  R"(
    {
      "item": [
        {
          "itemId": "item id 1",
          "displayText": "display text 1"
        }
      ],
      "suggestionSessionId": "suggestion id 1"
    })",
                                  net::HTTP_OK);

  itemSuggestCache->UpdateCache();
  task_environment_.RunUntilIdle();
  ResultsMatch(itemSuggestCache->GetResults(), "suggestion id 1",
               {{"item id 1", "display text 1"}});

  task_environment_.AdvanceClock(base::Minutes(2));

  url_loader_factory_.AddResponse(kRequestUrl,
                                  R"(
    {
      "item": [
        {
          "itemId": "item id 2",
          "displayText": "display text 2"
        }
      ],
      "suggestionSessionId": "suggestion id 2"
    })",
                                  net::HTTP_OK);
  itemSuggestCache->UpdateCache();
  task_environment_.RunUntilIdle();
  // The first set of results are in the cache since the second update occurred
  // before the minimum time between updates.
  ResultsMatch(itemSuggestCache->GetResults(), "suggestion id 1",
               {{"item id 1", "display text 1"}});
}

}  // namespace app_list
