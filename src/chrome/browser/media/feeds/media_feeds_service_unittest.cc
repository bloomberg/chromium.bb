// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/feeds/media_feeds_service.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/media/feeds/media_feeds_service_factory.h"
#include "chrome/browser/media/feeds/media_feeds_store.mojom-shared.h"
#include "chrome/browser/media/history/media_history_keyed_service.h"
#include "chrome/browser/media/history/media_history_test_utils.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/safe_search_api/stub_url_checker.h"
#include "components/safe_search_api/url_checker.h"
#include "content/public/browser/storage_partition.h"
#include "media/base/media_switches.h"
#include "net/base/load_flags.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media_feeds {

namespace {

constexpr size_t kCacheSize = 2;

constexpr base::FilePath::CharType kMediaFeedsTestFileName[] =
    FILE_PATH_LITERAL("chrome/test/data/media/feeds/media-feed.json");

const char kFirstItemActionURL[] = "https://www.example.com/action";
const char kFirstItemPlayNextActionURL[] = "https://www.example.com/next";

}  // namespace

class MediaFeedsServiceTest : public ChromeRenderViewHostTestHarness {
 public:
  MediaFeedsServiceTest() = default;

  void SetUp() override {
    features_.InitWithFeatures(
        {media::kMediaFeeds, media::kMediaFeedsSafeSearch}, {});

    ChromeRenderViewHostTestHarness::SetUp();

    stub_url_checker_ = std::make_unique<safe_search_api::StubURLChecker>();

    GetMediaFeedsService()->SetSafeSearchURLCheckerForTest(
        stub_url_checker_->BuildURLChecker(kCacheSize));

    GetMediaFeedsService()->test_url_loader_factory_for_fetcher_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &url_loader_factory_);
  }

  void WaitForDB() {
    base::RunLoop run_loop;
    GetMediaHistoryService()->PostTaskToDBForTest(run_loop.QuitClosure());
    run_loop.Run();
  }

  void SimulateOnCheckURLDone(const int64_t id,
                              const GURL& url,
                              safe_search_api::Classification classification,
                              bool uncertain) {
    GetMediaFeedsService()->OnCheckURLDone(id, url, url, classification,
                                           uncertain);
  }

  bool AddInflightSafeSearchCheck(const int64_t id,
                                  const std::set<GURL>& urls) {
    return GetMediaFeedsService()->AddInflightSafeSearchCheck(id, urls);
  }

  media_history::MediaHistoryKeyedService::MediaFeedFetchResult
  SuccessfulResultWithItems(
      std::vector<media_feeds::mojom::MediaFeedItemPtr> items,
      const int64_t feed_id) {
    media_history::MediaHistoryKeyedService::MediaFeedFetchResult result;
    result.feed_id = feed_id;
    result.items = std::move(items);
    result.status = media_feeds::mojom::FetchResult::kSuccess;
    result.display_name = "test";
    result.reset_token = media_history::test::GetResetTokenSync(
        GetMediaHistoryService(), feed_id);
    return result;
  }

  bool GetCurrentRequestHasBypassCacheFlag() {
    return GetCurrentRequest().load_flags & net::LOAD_BYPASS_CACHE;
  }

  media_history::MediaHistoryKeyedService::PendingSafeSearchCheckList
  GetPendingSafeSearchCheckMediaFeedItemsSync() {
    base::RunLoop run_loop;
    media_history::MediaHistoryKeyedService::PendingSafeSearchCheckList out;

    GetMediaHistoryService()->GetPendingSafeSearchCheckMediaFeedItems(
        base::BindLambdaForTesting([&](media_history::MediaHistoryKeyedService::
                                           PendingSafeSearchCheckList rows) {
          out = std::move(rows);
          run_loop.Quit();
        }));

    run_loop.Run();
    return out;
  }

  std::vector<media_feeds::mojom::MediaFeedItemPtr> GetItemsForMediaFeedSync(
      const int64_t feed_id) {
    base::RunLoop run_loop;
    std::vector<media_feeds::mojom::MediaFeedItemPtr> out;

    GetMediaHistoryService()->GetItemsForMediaFeedForDebug(
        feed_id,
        base::BindLambdaForTesting(
            [&](std::vector<media_feeds::mojom::MediaFeedItemPtr> rows) {
              out = std::move(rows);
              run_loop.Quit();
            }));

    run_loop.Run();
    return out;
  }

  std::vector<media_feeds::mojom::MediaFeedPtr> GetMediaFeedsSync(
      const media_history::MediaHistoryKeyedService::GetMediaFeedsRequest&
          request =
              media_history::MediaHistoryKeyedService::GetMediaFeedsRequest()) {
    base::RunLoop run_loop;
    std::vector<media_feeds::mojom::MediaFeedPtr> out;

    GetMediaHistoryService()->GetMediaFeeds(
        request, base::BindLambdaForTesting(
                     [&](std::vector<media_feeds::mojom::MediaFeedPtr> rows) {
                       out = std::move(rows);
                       run_loop.Quit();
                     }));

    run_loop.Run();
    return out;
  }

  void SetSafeSearchEnabled(bool enabled) {
    profile()->GetPrefs()->SetBoolean(prefs::kMediaFeedsSafeSearchEnabled,
                                      enabled);
  }

  bool RespondToPendingFeedFetch(const GURL& feed_url,
                                 bool from_cache = false) {
    base::FilePath file;
    base::PathService::Get(base::DIR_SOURCE_ROOT, &file);
    file = file.Append(kMediaFeedsTestFileName);

    std::string response_body;
    base::ReadFileToString(file, &response_body);

    auto response_head =
        ::network::CreateURLResponseHead(net::HttpStatusCode::HTTP_OK);
    response_head->was_fetched_via_cache = from_cache;

    bool rv = url_loader_factory_.SimulateResponseForPendingRequest(
        feed_url, network::URLLoaderCompletionStatus(net::OK),
        std::move(response_head), response_body);
    return rv;
  }

  bool RespondToPendingFeedFetchWithStatus(const GURL& feed_url,
                                           net::HttpStatusCode code) {
    bool rv = url_loader_factory_.SimulateResponseForPendingRequest(
        feed_url, network::URLLoaderCompletionStatus(net::OK),
        network::CreateURLResponseHead(code), std::string());
    return rv;
  }

  safe_search_api::StubURLChecker* safe_search_checker() {
    return stub_url_checker_.get();
  }

  MediaFeedsService* GetMediaFeedsService() {
    return MediaFeedsServiceFactory::GetInstance()->GetForProfile(profile());
  }

  media_history::MediaHistoryKeyedService* GetMediaHistoryService() {
    return media_history::MediaHistoryKeyedService::Get(profile());
  }

  static media_feeds::mojom::MediaFeedItemPtr GetSingleExpectedItem() {
    auto item = media_feeds::mojom::MediaFeedItem::New();
    item->name = base::ASCIIToUTF16("The Movie");
    item->type = media_feeds::mojom::MediaFeedItemType::kMovie;
    item->date_published = base::Time::FromDeltaSinceWindowsEpoch(
        base::TimeDelta::FromMinutes(10));
    item->action_status =
        media_feeds::mojom::MediaFeedItemActionStatus::kPotential;
    item->action = media_feeds::mojom::Action::New();
    item->action->url = GURL(kFirstItemActionURL);
    item->play_next_candidate = media_feeds::mojom::PlayNextCandidate::New();
    item->play_next_candidate->action = media_feeds::mojom::Action::New();
    item->play_next_candidate->action->url = GURL(kFirstItemPlayNextActionURL);

    return item;
  }

  static std::vector<media_feeds::mojom::MediaFeedItemPtr> GetExpectedItems() {
    std::vector<media_feeds::mojom::MediaFeedItemPtr> items;
    items.push_back(GetSingleExpectedItem());

    {
      auto item = media_feeds::mojom::MediaFeedItem::New();
      item->type = media_feeds::mojom::MediaFeedItemType::kTVSeries;
      item->name = base::ASCIIToUTF16("The TV Series");
      item->action_status =
          media_feeds::mojom::MediaFeedItemActionStatus::kActive;
      item->action = media_feeds::mojom::Action::New();
      item->action->url = GURL("https://www.example.com/action2");
      item->author = media_feeds::mojom::Author::New();
      item->author->name = "Media Site";
      items.push_back(std::move(item));
    }

    {
      auto item = media_feeds::mojom::MediaFeedItem::New();
      item->type = media_feeds::mojom::MediaFeedItemType::kTVSeries;
      item->name = base::ASCIIToUTF16("The Live TV Series");
      item->action_status =
          media_feeds::mojom::MediaFeedItemActionStatus::kPotential;
      item->action = media_feeds::mojom::Action::New();
      item->action->url = GURL("https://www.example.com/action3");
      item->live = media_feeds::mojom::LiveDetails::New();
      items.push_back(std::move(item));
    }

    return items;
  }

  void CreateCookies(const std::vector<GURL>& urls,
                     bool domain_cookies = false,
                     bool expired = false) {
    const base::Time creation_time = base::Time::Now();

    base::RunLoop run_loop;
    int tasks = urls.size();

    for (auto& url : urls) {
      std::vector<std::string> cookie_line;
      cookie_line.push_back("A=1");

      if (url.SchemeIsCryptographic())
        cookie_line.push_back("Secure");

      if (domain_cookies)
        cookie_line.push_back("Domain=" + url.host());

      if (expired)
        cookie_line.push_back("Expires=Wed, 21 Oct 2015 07:28:00 GMT");

      std::unique_ptr<net::CanonicalCookie> cookie =
          net::CanonicalCookie::Create(url, base::JoinString(cookie_line, ";"),
                                       creation_time, creation_time);

      EXPECT_EQ(domain_cookies, cookie->IsDomainCookie());
      EXPECT_EQ(!domain_cookies, cookie->IsHostCookie());

      net::CookieOptions options;
      GetCookieManager()->SetCanonicalCookie(
          *cookie, url, options,
          base::BindLambdaForTesting(
              [&](net::CanonicalCookie::CookieInclusionStatus status) {
                if (--tasks == 0)
                  run_loop.Quit();
              }));
    }

    run_loop.Run();
  }

  uint32_t DeleteCookies(network::mojom::CookieDeletionFilterPtr filter) {
    base::RunLoop run_loop;
    uint32_t result_out = 0u;

    GetCookieManager()->DeleteCookies(
        std::move(filter),
        base::BindLambdaForTesting([&run_loop, &result_out](uint32_t result) {
          result_out = result;
          run_loop.Quit();
        }));

    run_loop.Run();
    return result_out;
  }

  network::mojom::CookieManager* GetCookieManager() {
    auto* partition =
        content::BrowserContext::GetDefaultStoragePartition(profile());
    return partition->GetCookieManagerForBrowserProcess();
  }

  bool IsCookieObserverEnabled() const { return true; }

 private:
  const ::network::ResourceRequest& GetCurrentRequest() {
    return url_loader_factory_.pending_requests()->front().request;
  }

  base::test::ScopedFeatureList features_;

  network::TestURLLoaderFactory url_loader_factory_;

  std::unique_ptr<safe_search_api::StubURLChecker> stub_url_checker_;

  data_decoder::test::InProcessDataDecoder data_decoder_;
};

TEST_F(MediaFeedsServiceTest, GetForProfile) {
  EXPECT_NE(nullptr, MediaFeedsServiceFactory::GetForProfile(profile()));

  Profile* otr_profile = profile()->GetOffTheRecordProfile();
  EXPECT_EQ(nullptr, MediaFeedsServiceFactory::GetForProfile(otr_profile));
}

TEST_F(MediaFeedsServiceTest, FetchFeed_Success) {
  base::HistogramTester histogram_tester;

  const GURL feed_url("https://www.google.com/feed");

  // Store a Media Feed.
  GetMediaHistoryService()->DiscoverMediaFeed(feed_url);
  WaitForDB();

  // Fetch the Media Feed.
  base::RunLoop run_loop;
  GetMediaFeedsService()->FetchMediaFeed(1, run_loop.QuitClosure());
  WaitForDB();
  ASSERT_TRUE(RespondToPendingFeedFetch(feed_url));
  run_loop.Run();

  auto feeds = GetMediaFeedsSync();

  EXPECT_EQ(1u, feeds.size());
  EXPECT_TRUE(feeds[0]->last_fetch_time_not_cache_hit);
  EXPECT_EQ(media_feeds::mojom::FetchResult::kSuccess,
            feeds[0]->last_fetch_result);

  histogram_tester.ExpectUniqueSample(
      MediaFeedsFetcher::kFetchSizeKbHistogramName, 15, 1);
}

TEST_F(MediaFeedsServiceTest, FetchFeed_SuccessFromCache) {
  const GURL feed_url("https://www.google.com/feed");

  // Store a Media Feed.
  GetMediaHistoryService()->DiscoverMediaFeed(feed_url);
  WaitForDB();

  // Fetch the Media Feed.
  base::RunLoop run_loop;
  GetMediaFeedsService()->FetchMediaFeed(1, run_loop.QuitClosure());
  WaitForDB();
  ASSERT_TRUE(RespondToPendingFeedFetch(feed_url, true));
  run_loop.Run();

  auto feeds = GetMediaFeedsSync();

  EXPECT_EQ(1u, feeds.size());
  EXPECT_EQ(media_feeds::mojom::FetchResult::kSuccess,
            feeds[0]->last_fetch_result);

  // This should not be set yet because the only fetch for this feed was from
  // the cache.
  EXPECT_FALSE(feeds[0]->last_fetch_time_not_cache_hit);
}

TEST_F(MediaFeedsServiceTest, FetchFeed_BackendError) {
  const GURL feed_url("https://www.google.com/feed");

  // Store a Media Feed.
  GetMediaHistoryService()->DiscoverMediaFeed(feed_url);
  WaitForDB();

  // Fetch the Media Feed.
  base::RunLoop run_loop;
  GetMediaFeedsService()->FetchMediaFeed(1, run_loop.QuitClosure());
  WaitForDB();
  ASSERT_TRUE(RespondToPendingFeedFetchWithStatus(
      feed_url, net::HTTP_INTERNAL_SERVER_ERROR));
  run_loop.Run();

  auto feeds = GetMediaFeedsSync();

  EXPECT_EQ(1u, feeds.size());
  EXPECT_EQ(media_feeds::mojom::FetchResult::kFailedBackendError,
            feeds[0]->last_fetch_result);
}

TEST_F(MediaFeedsServiceTest, FetchFeed_NotFoundError) {
  const GURL feed_url("https://www.google.com/feed");

  SetSafeSearchEnabled(true);
  safe_search_checker()->SetUpValidResponse(/* is_porn= */ false);

  // Store a Media Feed.
  GetMediaHistoryService()->DiscoverMediaFeed(feed_url);
  WaitForDB();

  // Fetch the Media Feed.
  base::RunLoop run_loop;
  GetMediaFeedsService()->FetchMediaFeed(1, run_loop.QuitClosure());
  WaitForDB();
  ASSERT_TRUE(RespondToPendingFeedFetchWithStatus(feed_url, net::HTTP_OK));
  run_loop.Run();

  auto feeds = GetMediaFeedsSync();

  EXPECT_EQ(1u, feeds.size());
  EXPECT_EQ(media_feeds::mojom::FetchResult::kFailedNetworkError,
            feeds[0]->last_fetch_result);
}

TEST_F(MediaFeedsServiceTest, SafeSearch_AllSafe) {
  base::HistogramTester histogram_tester;

  SetSafeSearchEnabled(true);
  safe_search_checker()->SetUpValidResponse(/* is_porn= */ false);

  // Store a Media Feed.
  GetMediaHistoryService()->DiscoverMediaFeed(
      GURL("https://www.google.com/feed"));
  WaitForDB();

  // Store some media feed items.
  GetMediaHistoryService()->StoreMediaFeedFetchResult(
      SuccessfulResultWithItems(GetExpectedItems(), 1), base::DoNothing());
  WaitForDB();

  base::RunLoop run_loop;
  GetMediaFeedsService()->SetSafeSearchCompletionCallbackForTest(
      run_loop.QuitClosure());

  {
    // Get the pending items and check them against Safe Search.
    auto pending_items = GetPendingSafeSearchCheckMediaFeedItemsSync();
    EXPECT_EQ(3u, pending_items.size());
    GetMediaFeedsService()->CheckItemsAgainstSafeSearch(
        std::move(pending_items));
  }

  // Wait for the service and DB to finish.
  run_loop.Run();
  WaitForDB();

  {
    // The pending items should be empty.
    auto pending_items = GetPendingSafeSearchCheckMediaFeedItemsSync();
    EXPECT_TRUE(pending_items.empty());
  }

  // Check the items were updated.
  auto items = GetItemsForMediaFeedSync(1);
  EXPECT_EQ(3u, items.size());
  EXPECT_EQ(media_feeds::mojom::SafeSearchResult::kSafe,
            items[0]->safe_search_result);
  EXPECT_EQ(media_feeds::mojom::SafeSearchResult::kSafe,
            items[1]->safe_search_result);
  EXPECT_EQ(media_feeds::mojom::SafeSearchResult::kSafe,
            items[2]->safe_search_result);

  histogram_tester.ExpectUniqueSample(
      MediaFeedsService::kSafeSearchResultHistogramName,
      media_feeds::mojom::SafeSearchResult::kSafe, 3);
}

TEST_F(MediaFeedsServiceTest, SafeSearch_AllUnsafe) {
  base::HistogramTester histogram_tester;

  SetSafeSearchEnabled(true);
  safe_search_checker()->SetUpValidResponse(/* is_porn= */ true);

  // Store a Media Feed.
  GetMediaHistoryService()->DiscoverMediaFeed(
      GURL("https://www.google.com/feed"));
  WaitForDB();

  // Store some media feed items.
  GetMediaHistoryService()->StoreMediaFeedFetchResult(
      SuccessfulResultWithItems(GetExpectedItems(), 1), base::DoNothing());
  WaitForDB();

  base::RunLoop run_loop;
  GetMediaFeedsService()->SetSafeSearchCompletionCallbackForTest(
      run_loop.QuitClosure());

  {
    // Get the pending items and check them against Safe Search.
    auto pending_items = GetPendingSafeSearchCheckMediaFeedItemsSync();
    EXPECT_EQ(3u, pending_items.size());
    GetMediaFeedsService()->CheckItemsAgainstSafeSearch(
        std::move(pending_items));
  }

  // Wait for the service and DB to finish.
  run_loop.Run();
  WaitForDB();

  {
    // The pending items should be empty.
    auto pending_items = GetPendingSafeSearchCheckMediaFeedItemsSync();
    EXPECT_TRUE(pending_items.empty());
  }

  // Check the items were updated.
  auto items = GetItemsForMediaFeedSync(1);
  EXPECT_EQ(3u, items.size());
  EXPECT_EQ(media_feeds::mojom::SafeSearchResult::kUnsafe,
            items[0]->safe_search_result);
  EXPECT_EQ(media_feeds::mojom::SafeSearchResult::kUnsafe,
            items[1]->safe_search_result);
  EXPECT_EQ(media_feeds::mojom::SafeSearchResult::kUnsafe,
            items[2]->safe_search_result);

  histogram_tester.ExpectUniqueSample(
      MediaFeedsService::kSafeSearchResultHistogramName,
      media_feeds::mojom::SafeSearchResult::kUnsafe, 3);
}

TEST_F(MediaFeedsServiceTest, SafeSearch_Failed_Request) {
  base::HistogramTester histogram_tester;

  SetSafeSearchEnabled(true);
  safe_search_checker()->SetUpFailedResponse();

  // Store a Media Feed.
  GetMediaHistoryService()->DiscoverMediaFeed(
      GURL("https://www.google.com/feed"));
  WaitForDB();

  // Store some media feed items.
  GetMediaHistoryService()->StoreMediaFeedFetchResult(
      SuccessfulResultWithItems(GetExpectedItems(), 1), base::DoNothing());
  WaitForDB();

  base::RunLoop run_loop;
  GetMediaFeedsService()->SetSafeSearchCompletionCallbackForTest(
      run_loop.QuitClosure());

  {
    // Get the pending items and check them against Safe Search.
    auto pending_items = GetPendingSafeSearchCheckMediaFeedItemsSync();
    EXPECT_EQ(3u, pending_items.size());
    GetMediaFeedsService()->CheckItemsAgainstSafeSearch(
        std::move(pending_items));
  }

  // Wait for the service and DB to finish.
  run_loop.Run();
  WaitForDB();

  {
    // The pending items should still be 3.
    auto pending_items = GetPendingSafeSearchCheckMediaFeedItemsSync();
    EXPECT_EQ(3u, pending_items.size());
  }

  // Check the items were updated.
  auto items = GetItemsForMediaFeedSync(1);
  EXPECT_EQ(3u, items.size());
  EXPECT_EQ(media_feeds::mojom::SafeSearchResult::kUnknown,
            items[0]->safe_search_result);
  EXPECT_EQ(media_feeds::mojom::SafeSearchResult::kUnknown,
            items[1]->safe_search_result);
  EXPECT_EQ(media_feeds::mojom::SafeSearchResult::kUnknown,
            items[2]->safe_search_result);

  histogram_tester.ExpectUniqueSample(
      MediaFeedsService::kSafeSearchResultHistogramName,
      media_feeds::mojom::SafeSearchResult::kUnknown, 3);
}

TEST_F(MediaFeedsServiceTest, SafeSearch_Failed_Pref) {
  base::HistogramTester histogram_tester;

  // Store a Media Feed.
  GetMediaHistoryService()->DiscoverMediaFeed(
      GURL("https://www.google.com/feed"));
  WaitForDB();

  // Store some media feed items.
  GetMediaHistoryService()->StoreMediaFeedFetchResult(
      SuccessfulResultWithItems(GetExpectedItems(), 1), base::DoNothing());
  WaitForDB();

  base::RunLoop run_loop;
  GetMediaFeedsService()->SetSafeSearchCompletionCallbackForTest(
      run_loop.QuitClosure());

  {
    // Get the pending items and check them against Safe Search.
    auto pending_items = GetPendingSafeSearchCheckMediaFeedItemsSync();
    EXPECT_EQ(3u, pending_items.size());
    GetMediaFeedsService()->CheckItemsAgainstSafeSearch(
        std::move(pending_items));
  }

  // Wait for the service and DB to finish.
  run_loop.Run();
  WaitForDB();

  {
    // The pending items should still be 3.
    auto pending_items = GetPendingSafeSearchCheckMediaFeedItemsSync();
    EXPECT_EQ(3u, pending_items.size());
  }

  // Check the items were updated.
  auto items = GetItemsForMediaFeedSync(1);
  EXPECT_EQ(3u, items.size());
  EXPECT_EQ(media_feeds::mojom::SafeSearchResult::kUnknown,
            items[0]->safe_search_result);
  EXPECT_EQ(media_feeds::mojom::SafeSearchResult::kUnknown,
            items[1]->safe_search_result);
  EXPECT_EQ(media_feeds::mojom::SafeSearchResult::kUnknown,
            items[2]->safe_search_result);

  histogram_tester.ExpectTotalCount(
      MediaFeedsService::kSafeSearchResultHistogramName, 0);
}

TEST_F(MediaFeedsServiceTest, SafeSearch_CheckTwice_Inflight) {
  SetSafeSearchEnabled(true);
  safe_search_checker()->SetUpValidResponse(/* is_porn= */ false);

  // Store a Media Feed.
  GetMediaHistoryService()->DiscoverMediaFeed(
      GURL("https://www.google.com/feed"));
  WaitForDB();

  // Store some media feed items.
  GetMediaHistoryService()->StoreMediaFeedFetchResult(
      SuccessfulResultWithItems(GetExpectedItems(), 1), base::DoNothing());
  WaitForDB();

  base::RunLoop run_loop;
  GetMediaFeedsService()->SetSafeSearchCompletionCallbackForTest(
      run_loop.QuitClosure());

  {
    // Get the pending items and check them against Safe Search.
    auto pending_items = GetPendingSafeSearchCheckMediaFeedItemsSync();
    EXPECT_EQ(3u, pending_items.size());
    GetMediaFeedsService()->CheckItemsAgainstSafeSearch(
        std::move(pending_items));
  }

  {
    // This checks we ignore the duplicate items for inflight checks.
    auto pending_items = GetPendingSafeSearchCheckMediaFeedItemsSync();
    EXPECT_EQ(3u, pending_items.size());
    GetMediaFeedsService()->CheckItemsAgainstSafeSearch(
        std::move(pending_items));
  }

  // Wait for the service and DB to finish.
  run_loop.Run();
  WaitForDB();

  {
    // The pending items should be empty.
    auto pending_items = GetPendingSafeSearchCheckMediaFeedItemsSync();
    EXPECT_TRUE(pending_items.empty());
  }
}

TEST_F(MediaFeedsServiceTest, SafeSearch_CheckTwice_Committed) {
  SetSafeSearchEnabled(true);
  safe_search_checker()->SetUpValidResponse(/* is_porn= */ false);

  // Store a Media Feed.
  GetMediaHistoryService()->DiscoverMediaFeed(
      GURL("https://www.google.com/feed"));
  WaitForDB();

  // Store some media feed items.
  GetMediaHistoryService()->StoreMediaFeedFetchResult(
      SuccessfulResultWithItems(GetExpectedItems(), 1), base::DoNothing());
  WaitForDB();

  auto pending_items_a = GetPendingSafeSearchCheckMediaFeedItemsSync();
  EXPECT_EQ(3u, pending_items_a.size());

  auto pending_items_b = GetPendingSafeSearchCheckMediaFeedItemsSync();
  EXPECT_EQ(3u, pending_items_b.size());

  {
    base::RunLoop run_loop;
    GetMediaFeedsService()->SetSafeSearchCompletionCallbackForTest(
        run_loop.QuitClosure());
    GetMediaFeedsService()->CheckItemsAgainstSafeSearch(
        std::move(pending_items_a));

    // Wait for the service and DB to finish.
    run_loop.Run();
    WaitForDB();
  }

  {
    base::RunLoop run_loop;
    GetMediaFeedsService()->SetSafeSearchCompletionCallbackForTest(
        run_loop.QuitClosure());
    GetMediaFeedsService()->CheckItemsAgainstSafeSearch(
        std::move(pending_items_b));

    // Wait for the service and DB to finish.
    run_loop.Run();
    WaitForDB();
  }

  {
    // The pending items should be empty.
    auto pending_items = GetPendingSafeSearchCheckMediaFeedItemsSync();
    EXPECT_TRUE(pending_items.empty());
  }
}

TEST_F(MediaFeedsServiceTest, SafeSearch_Mixed_SafeUnsafe) {
  SetSafeSearchEnabled(true);
  base::HistogramTester histogram_tester;

  // Store a Media Feed.
  GetMediaHistoryService()->DiscoverMediaFeed(
      GURL("https://www.google.com/feed"));
  WaitForDB();

  // Store some media feed items.
  std::vector<media_feeds::mojom::MediaFeedItemPtr> items;
  items.push_back(GetSingleExpectedItem());
  GetMediaHistoryService()->StoreMediaFeedFetchResult(
      SuccessfulResultWithItems(std::move(items), 1), base::DoNothing());
  WaitForDB();

  {
    // Get the pending items and check them against Safe Search.
    auto pending_items = GetPendingSafeSearchCheckMediaFeedItemsSync();
    EXPECT_EQ(1u, pending_items.size());
    EXPECT_TRUE(AddInflightSafeSearchCheck(pending_items[0]->id,
                                           pending_items[0]->urls));
  }

  SimulateOnCheckURLDone(1, GURL(kFirstItemActionURL),
                         safe_search_api::Classification::SAFE,
                         /*uncertain=*/false);
  SimulateOnCheckURLDone(1, GURL(kFirstItemPlayNextActionURL),
                         safe_search_api::Classification::UNSAFE,
                         /*uncertain=*/false);

  WaitForDB();

  {
    // Check the result of the item we stored.
    auto items = GetItemsForMediaFeedSync(1);
    EXPECT_EQ(1u, items.size());
    EXPECT_EQ(media_feeds::mojom::SafeSearchResult::kUnsafe,
              items[0]->safe_search_result);
  }

  histogram_tester.ExpectUniqueSample(
      MediaFeedsService::kSafeSearchResultHistogramName,
      media_feeds::mojom::SafeSearchResult::kUnsafe, 1);
}

TEST_F(MediaFeedsServiceTest, SafeSearch_Mixed_SafeUncertain) {
  SetSafeSearchEnabled(true);
  base::HistogramTester histogram_tester;

  // Store a Media Feed.
  GetMediaHistoryService()->DiscoverMediaFeed(
      GURL("https://www.google.com/feed"));
  WaitForDB();

  // Store some media feed items.
  std::vector<media_feeds::mojom::MediaFeedItemPtr> items;
  items.push_back(GetSingleExpectedItem());
  GetMediaHistoryService()->StoreMediaFeedFetchResult(
      SuccessfulResultWithItems(std::move(items), 1), base::DoNothing());
  WaitForDB();

  {
    // Get the pending items and check them against Safe Search.
    auto pending_items = GetPendingSafeSearchCheckMediaFeedItemsSync();
    EXPECT_EQ(1u, pending_items.size());
    EXPECT_TRUE(AddInflightSafeSearchCheck(pending_items[0]->id,
                                           pending_items[0]->urls));
  }

  SimulateOnCheckURLDone(1, GURL(kFirstItemActionURL),
                         safe_search_api::Classification::SAFE,
                         /*uncertain=*/false);
  SimulateOnCheckURLDone(1, GURL(kFirstItemPlayNextActionURL),
                         safe_search_api::Classification::SAFE,
                         /*uncertain=*/true);

  WaitForDB();

  {
    // Check the result of the item we stored.
    auto items = GetItemsForMediaFeedSync(1);
    EXPECT_EQ(1u, items.size());
    EXPECT_EQ(media_feeds::mojom::SafeSearchResult::kUnknown,
              items[0]->safe_search_result);
  }

  histogram_tester.ExpectUniqueSample(
      MediaFeedsService::kSafeSearchResultHistogramName,
      media_feeds::mojom::SafeSearchResult::kUnknown, 1);
}

TEST_F(MediaFeedsServiceTest, SafeSearch_Mixed_UnsafeUncertain) {
  SetSafeSearchEnabled(true);
  base::HistogramTester histogram_tester;

  // Store a Media Feed.
  GetMediaHistoryService()->DiscoverMediaFeed(
      GURL("https://www.google.com/feed"));
  WaitForDB();

  // Store some media feed items.
  std::vector<media_feeds::mojom::MediaFeedItemPtr> items;
  items.push_back(GetSingleExpectedItem());
  GetMediaHistoryService()->StoreMediaFeedFetchResult(
      SuccessfulResultWithItems(std::move(items), 1), base::DoNothing());
  WaitForDB();

  {
    // Get the pending items and check them against Safe Search.
    auto pending_items = GetPendingSafeSearchCheckMediaFeedItemsSync();
    EXPECT_EQ(1u, pending_items.size());
    EXPECT_TRUE(AddInflightSafeSearchCheck(pending_items[0]->id,
                                           pending_items[0]->urls));
  }

  SimulateOnCheckURLDone(1, GURL(kFirstItemActionURL),
                         safe_search_api::Classification::UNSAFE,
                         /*uncertain=*/false);
  SimulateOnCheckURLDone(1, GURL(kFirstItemPlayNextActionURL),
                         safe_search_api::Classification::SAFE,
                         /*uncertain=*/true);

  WaitForDB();

  {
    // Check the result of the item we stored.
    auto items = GetItemsForMediaFeedSync(1);
    EXPECT_EQ(1u, items.size());
    EXPECT_EQ(media_feeds::mojom::SafeSearchResult::kUnsafe,
              items[0]->safe_search_result);
  }

  histogram_tester.ExpectUniqueSample(
      MediaFeedsService::kSafeSearchResultHistogramName,
      media_feeds::mojom::SafeSearchResult::kUnsafe, 1);
}

TEST_F(MediaFeedsServiceTest, SafeSearch_Failed_Feature) {
  SetSafeSearchEnabled(true);

  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(media::kMediaFeedsSafeSearch);

  base::HistogramTester histogram_tester;

  // Store a Media Feed.
  GetMediaHistoryService()->DiscoverMediaFeed(
      GURL("https://www.google.com/feed"));
  WaitForDB();

  // Store some media feed items.
  GetMediaHistoryService()->StoreMediaFeedFetchResult(
      SuccessfulResultWithItems(GetExpectedItems(), 1), base::DoNothing());
  WaitForDB();

  base::RunLoop run_loop;
  GetMediaFeedsService()->SetSafeSearchCompletionCallbackForTest(
      run_loop.QuitClosure());

  {
    // Get the pending items and check them against Safe Search.
    auto pending_items = GetPendingSafeSearchCheckMediaFeedItemsSync();
    EXPECT_EQ(3u, pending_items.size());
    GetMediaFeedsService()->CheckItemsAgainstSafeSearch(
        std::move(pending_items));
  }

  // Wait for the service and DB to finish.
  run_loop.Run();
  WaitForDB();

  {
    // The pending items should still be 3.
    auto pending_items = GetPendingSafeSearchCheckMediaFeedItemsSync();
    EXPECT_EQ(3u, pending_items.size());
  }

  // Check the items were updated.
  auto items = GetItemsForMediaFeedSync(1);
  EXPECT_EQ(3u, items.size());
  EXPECT_EQ(media_feeds::mojom::SafeSearchResult::kUnknown,
            items[0]->safe_search_result);
  EXPECT_EQ(media_feeds::mojom::SafeSearchResult::kUnknown,
            items[1]->safe_search_result);
  EXPECT_EQ(media_feeds::mojom::SafeSearchResult::kUnknown,
            items[2]->safe_search_result);

  histogram_tester.ExpectTotalCount(
      MediaFeedsService::kSafeSearchResultHistogramName, 0);
}

TEST_F(MediaFeedsServiceTest, FetcherShouldTriggerSafeSearch) {
  const GURL feed_url("https://www.google.com/feed");

  SetSafeSearchEnabled(true);
  safe_search_checker()->SetUpValidResponse(/* is_porn= */ false);

  // Store a Media Feed.
  GetMediaHistoryService()->DiscoverMediaFeed(feed_url);
  WaitForDB();

  base::RunLoop run_loop;
  GetMediaFeedsService()->SetSafeSearchCompletionCallbackForTest(
      run_loop.QuitClosure());

  {
    // Fetch the Media Feed.
    base::RunLoop run_loop;
    GetMediaFeedsService()->FetchMediaFeed(1, run_loop.QuitClosure());
    WaitForDB();
    ASSERT_TRUE(RespondToPendingFeedFetch(feed_url));
    run_loop.Run();
  }

  // Wait for the items to be checked against safe search
  run_loop.Run();
  WaitForDB();

  {
    // The pending items should be empty.
    auto pending_items = GetPendingSafeSearchCheckMediaFeedItemsSync();
    EXPECT_TRUE(pending_items.empty());
  }

  // Check the items were updated.
  auto items = GetItemsForMediaFeedSync(1);
  EXPECT_EQ(7u, items.size());

  for (auto& item : items) {
    EXPECT_EQ(media_feeds::mojom::SafeSearchResult::kSafe,
              item->safe_search_result);
  }
}

TEST_F(MediaFeedsServiceTest, FetcherShouldDeleteFeedIfGone) {
  const GURL feed_url("https://www.google.com/feed");

  safe_search_checker()->SetUpValidResponse(/* is_porn= */ false);

  // Store a Media Feed.
  GetMediaHistoryService()->DiscoverMediaFeed(feed_url);
  WaitForDB();

  // Store some media feed items.
  GetMediaHistoryService()->StoreMediaFeedFetchResult(
      SuccessfulResultWithItems(GetExpectedItems(), 1), base::DoNothing());
  WaitForDB();

  base::RunLoop run_loop;
  GetMediaFeedsService()->SetSafeSearchCompletionCallbackForTest(
      run_loop.QuitClosure());

  {
    // Check there are pending items.
    auto pending_items = GetPendingSafeSearchCheckMediaFeedItemsSync();
    EXPECT_EQ(3u, pending_items.size());
  }

  // Enable the safe search pref. This should trigger a refetch.
  SetSafeSearchEnabled(true);

  // Wait for the service and DB to finish.
  run_loop.Run();
  WaitForDB();

  {
    // The pending items should be empty.
    auto pending_items = GetPendingSafeSearchCheckMediaFeedItemsSync();
    EXPECT_TRUE(pending_items.empty());
  }

  {
    // Check the items were updated.
    auto items = GetItemsForMediaFeedSync(1);
    EXPECT_EQ(3u, items.size());

    for (auto& item : items) {
      EXPECT_EQ(media_feeds::mojom::SafeSearchResult::kSafe,
                item->safe_search_result);
    }
  }

  // Store some new media feed items.
  GetMediaHistoryService()->StoreMediaFeedFetchResult(
      SuccessfulResultWithItems(GetExpectedItems(), 1), base::DoNothing());
  WaitForDB();

  {
    // Check there are pending items.
    auto pending_items = GetPendingSafeSearchCheckMediaFeedItemsSync();
    EXPECT_EQ(3u, pending_items.size());
  }
}

TEST_F(MediaFeedsServiceTest, FetcherShouldSupportMultipleFetchesForSameFeed) {
  const GURL feed_url("https://www.google.com/feed");

  SetSafeSearchEnabled(true);
  safe_search_checker()->SetUpValidResponse(/* is_porn= */ false);

  // Store a Media Feed.
  GetMediaHistoryService()->DiscoverMediaFeed(feed_url);
  WaitForDB();

  // Fetch the same feed twice.
  base::RunLoop run_loop;
  GetMediaFeedsService()->FetchMediaFeed(1, run_loop.QuitClosure());
  WaitForDB();

  base::RunLoop run_loop_alt;
  GetMediaFeedsService()->FetchMediaFeed(1, run_loop_alt.QuitClosure());
  WaitForDB();

  // Respond and make sure both run loop quit closures were called.
  ASSERT_TRUE(RespondToPendingFeedFetch(feed_url));
  run_loop.Run();
  run_loop_alt.Run();
}

TEST_F(MediaFeedsServiceTest, FetcherShouldHandleReset) {
  const GURL feed_url("https://www.google.com/feed");

  SetSafeSearchEnabled(true);
  safe_search_checker()->SetUpValidResponse(/* is_porn= */ false);

  // Store a Media Feed.
  GetMediaHistoryService()->DiscoverMediaFeed(feed_url);
  WaitForDB();

  // Store some media feed items.
  GetMediaHistoryService()->StoreMediaFeedFetchResult(
      SuccessfulResultWithItems(GetExpectedItems(), 1), base::DoNothing());
  WaitForDB();

  {
    // Check the feed and items are stored correctly.
    auto feeds = GetMediaFeedsSync();
    ASSERT_EQ(1u, feeds.size());
    EXPECT_EQ(media_feeds::mojom::ResetReason::kNone, feeds[0]->reset_reason);
    EXPECT_EQ(media_feeds::mojom::FetchResult::kSuccess,
              feeds[0]->last_fetch_result);

    auto items = GetItemsForMediaFeedSync(1);
    EXPECT_EQ(GetExpectedItems(), items);
  }

  // Start fetching the feed but do not resolve the request.
  base::RunLoop run_loop;
  GetMediaFeedsService()->FetchMediaFeed(1, run_loop.QuitClosure());
  WaitForDB();

  // The last request was successful so we can hit the cache.
  EXPECT_FALSE(GetCurrentRequestHasBypassCacheFlag());

  // Reset the feed.
  GetMediaHistoryService()->ResetMediaFeed(
      url::Origin::Create(feed_url), media_feeds::mojom::ResetReason::kVisit);
  WaitForDB();

  {
    // Check the feed was reset.
    auto feeds = GetMediaFeedsSync();
    ASSERT_EQ(1u, feeds.size());
    EXPECT_EQ(media_feeds::mojom::ResetReason::kVisit, feeds[0]->reset_reason);
    EXPECT_EQ(media_feeds::mojom::FetchResult::kNone,
              feeds[0]->last_fetch_result);

    auto items = GetItemsForMediaFeedSync(1);
    EXPECT_TRUE(items.empty());
  }

  // Respond to the pending fetch.
  ASSERT_TRUE(RespondToPendingFeedFetch(feed_url));
  run_loop.Run();
  WaitForDB();

  {
    // The feed should have still been reset since the fetch was started with
    // outdated information.
    auto feeds = GetMediaFeedsSync();
    ASSERT_EQ(1u, feeds.size());
    EXPECT_EQ(media_feeds::mojom::ResetReason::kVisit, feeds[0]->reset_reason);
    EXPECT_EQ(media_feeds::mojom::FetchResult::kFailedDueToResetWhileInflight,
              feeds[0]->last_fetch_result);

    auto items = GetItemsForMediaFeedSync(1);
    EXPECT_TRUE(items.empty());
  }

  // Start fetching the feed but do not resolve the request.
  base::RunLoop run_loop_alt;
  GetMediaFeedsService()->FetchMediaFeed(1, run_loop_alt.QuitClosure());
  WaitForDB();

  // The last request failed so we should not hit the cache.
  EXPECT_TRUE(GetCurrentRequestHasBypassCacheFlag());

  // Respond to the pending fetch.
  ASSERT_TRUE(RespondToPendingFeedFetch(feed_url));
  run_loop_alt.Run();
  WaitForDB();

  {
    // The feed should have been successfully stored.
    auto feeds = GetMediaFeedsSync();
    ASSERT_EQ(1u, feeds.size());
    EXPECT_EQ(media_feeds::mojom::ResetReason::kNone, feeds[0]->reset_reason);
    EXPECT_EQ(media_feeds::mojom::FetchResult::kSuccess,
              feeds[0]->last_fetch_result);

    auto items = GetItemsForMediaFeedSync(1);
    EXPECT_FALSE(items.empty());
  }
}

TEST_F(MediaFeedsServiceTest, ResetOnCookieChange_ExplicitDeletion_All) {
  if (!GetMediaFeedsService()->HasCookieObserverForTest())
    return;

  const GURL feed_url("https://www.google.com/feed");

  // Store a Media Feed.
  GetMediaHistoryService()->DiscoverMediaFeed(feed_url);
  WaitForDB();

  // Store some media feed items.
  GetMediaHistoryService()->StoreMediaFeedFetchResult(
      SuccessfulResultWithItems(GetExpectedItems(), 1), base::DoNothing());
  WaitForDB();

  {
    auto feeds = GetMediaFeedsSync();
    ASSERT_EQ(1u, feeds.size());
    EXPECT_EQ(media_feeds::mojom::ResetReason::kNone, feeds[0]->reset_reason);
  }

  base::RunLoop run_loop;
  GetMediaFeedsService()->SetCookieChangeCallbackForTest(
      run_loop.QuitClosure());

  {
    // Store some cookies on the feed URL and another URL.
    std::vector<GURL> cookie_urls;
    cookie_urls.push_back(feed_url);
    cookie_urls.push_back(GURL("https://www.example.com"));
    CreateCookies(cookie_urls);
  }

  EXPECT_EQ(2u, DeleteCookies(network::mojom::CookieDeletionFilter::New()));
  run_loop.Run();
  WaitForDB();

  {
    auto feeds = GetMediaFeedsSync();
    ASSERT_EQ(1u, feeds.size());
    EXPECT_EQ(media_feeds::mojom::ResetReason::kCookies,
              feeds[0]->reset_reason);
  }
}

TEST_F(MediaFeedsServiceTest,
       ResetOnCookieChange_ExplicitDeletion_SingleHostMatch) {
  if (!GetMediaFeedsService()->HasCookieObserverForTest())
    return;

  const GURL feed_url("https://www.google.com/feed");

  // Store a Media Feed.
  GetMediaHistoryService()->DiscoverMediaFeed(feed_url);
  WaitForDB();

  // Store some media feed items.
  GetMediaHistoryService()->StoreMediaFeedFetchResult(
      SuccessfulResultWithItems(GetExpectedItems(), 1), base::DoNothing());
  WaitForDB();

  {
    auto feeds = GetMediaFeedsSync();
    ASSERT_EQ(1u, feeds.size());
    EXPECT_EQ(media_feeds::mojom::ResetReason::kNone, feeds[0]->reset_reason);
  }

  base::RunLoop run_loop;
  GetMediaFeedsService()->SetCookieChangeCallbackForTest(
      run_loop.QuitClosure());

  {
    // Store some cookies on the feed URL and another URL.
    std::vector<GURL> cookie_urls;
    cookie_urls.push_back(feed_url);
    cookie_urls.push_back(GURL("https://www.example.com"));
    CreateCookies(cookie_urls);
  }

  auto filter = network::mojom::CookieDeletionFilter::New();
  filter->url = feed_url;
  EXPECT_EQ(1u, DeleteCookies(std::move(filter)));
  run_loop.Run();
  WaitForDB();

  {
    auto feeds = GetMediaFeedsSync();
    ASSERT_EQ(1u, feeds.size());
    EXPECT_EQ(media_feeds::mojom::ResetReason::kCookies,
              feeds[0]->reset_reason);
  }
}

TEST_F(MediaFeedsServiceTest,
       ResetOnCookieChange_ExplicitDeletion_SingleHostMatch_Insecure) {
  if (!GetMediaFeedsService()->HasCookieObserverForTest())
    return;

  const GURL feed_url("https://www.google.com/feed");

  // Store a Media Feed.
  GetMediaHistoryService()->DiscoverMediaFeed(feed_url);
  WaitForDB();

  // Store some media feed items.
  GetMediaHistoryService()->StoreMediaFeedFetchResult(
      SuccessfulResultWithItems(GetExpectedItems(), 1), base::DoNothing());
  WaitForDB();

  {
    auto feeds = GetMediaFeedsSync();
    ASSERT_EQ(1u, feeds.size());
    EXPECT_EQ(media_feeds::mojom::ResetReason::kNone, feeds[0]->reset_reason);
  }

  base::RunLoop run_loop;
  GetMediaFeedsService()->SetCookieChangeCallbackForTest(
      run_loop.QuitClosure());

  {
    // Store some cookies on the feed URL and another URL.
    std::vector<GURL> cookie_urls;
    cookie_urls.push_back(GURL("http://www.google.com"));
    cookie_urls.push_back(GURL("https://www.example.com"));
    CreateCookies(cookie_urls);
  }

  auto filter = network::mojom::CookieDeletionFilter::New();
  filter->url = feed_url;
  EXPECT_EQ(1u, DeleteCookies(std::move(filter)));
  run_loop.Run();
  WaitForDB();

  {
    auto feeds = GetMediaFeedsSync();
    ASSERT_EQ(1u, feeds.size());
    EXPECT_EQ(media_feeds::mojom::ResetReason::kCookies,
              feeds[0]->reset_reason);
  }
}

TEST_F(MediaFeedsServiceTest, ResetOnCookieChange_Expired) {
  if (!GetMediaFeedsService()->HasCookieObserverForTest())
    return;

  const GURL feed_url("https://www.google.com/feed");

  // Store a Media Feed.
  GetMediaHistoryService()->DiscoverMediaFeed(feed_url);
  WaitForDB();

  // Store some media feed items.
  GetMediaHistoryService()->StoreMediaFeedFetchResult(
      SuccessfulResultWithItems(GetExpectedItems(), 1), base::DoNothing());
  WaitForDB();

  {
    auto feeds = GetMediaFeedsSync();
    ASSERT_EQ(1u, feeds.size());
    EXPECT_EQ(media_feeds::mojom::ResetReason::kNone, feeds[0]->reset_reason);
  }

  base::RunLoop run_loop;
  GetMediaFeedsService()->SetCookieChangeCallbackForTest(
      run_loop.QuitClosure());

  {
    // Store some cookies on the feed URL and another URL.
    std::vector<GURL> cookie_urls;
    cookie_urls.push_back(feed_url);
    cookie_urls.push_back(GURL("https://www.example.com"));
    CreateCookies(cookie_urls);
    CreateCookies(cookie_urls, false, true);
  }

  run_loop.Run();
  WaitForDB();

  {
    auto feeds = GetMediaFeedsSync();
    ASSERT_EQ(1u, feeds.size());
    EXPECT_EQ(media_feeds::mojom::ResetReason::kCookies,
              feeds[0]->reset_reason);
  }
}

TEST_F(MediaFeedsServiceTest,
       ResetOnCookieChange_ExplicitDeletion_AssociatedHostMatch) {
  if (!GetMediaFeedsService()->HasCookieObserverForTest())
    return;

  const GURL feed_url("https://www.google.com/feed");
  const GURL alt_url("https://www.example.com");

  // Store a Media Feed.
  GetMediaHistoryService()->DiscoverMediaFeed(feed_url);
  WaitForDB();

  std::set<url::Origin> origins;
  origins.insert(url::Origin::Create(alt_url));

  auto result = SuccessfulResultWithItems(GetExpectedItems(), 1);
  result.associated_origins = origins;

  // Store some media feed items.
  GetMediaHistoryService()->StoreMediaFeedFetchResult(std::move(result),
                                                      base::DoNothing());
  WaitForDB();

  {
    auto feeds = GetMediaFeedsSync();
    ASSERT_EQ(1u, feeds.size());
    EXPECT_EQ(media_feeds::mojom::ResetReason::kNone, feeds[0]->reset_reason);
  }

  base::RunLoop run_loop;
  GetMediaFeedsService()->SetCookieChangeCallbackForTest(
      run_loop.QuitClosure());

  {
    // Store some cookies on the feed URL and another URL.
    std::vector<GURL> cookie_urls;
    cookie_urls.push_back(feed_url);
    cookie_urls.push_back(alt_url);
    CreateCookies(cookie_urls);
  }

  auto filter = network::mojom::CookieDeletionFilter::New();
  filter->url = alt_url;
  EXPECT_EQ(1u, DeleteCookies(std::move(filter)));
  run_loop.Run();
  WaitForDB();

  {
    auto feeds = GetMediaFeedsSync();
    ASSERT_EQ(1u, feeds.size());
    EXPECT_EQ(media_feeds::mojom::ResetReason::kCookies,
              feeds[0]->reset_reason);
  }
}

TEST_F(MediaFeedsServiceTest,
       ResetOnCookieChange_ExplicitDeletion_SingleHostNoMatch) {
  if (!GetMediaFeedsService()->HasCookieObserverForTest())
    return;

  const GURL feed_url("https://www.google.com/feed");
  const GURL alt_url("https://www.example.com");

  // Store a Media Feed.
  GetMediaHistoryService()->DiscoverMediaFeed(feed_url);
  WaitForDB();

  // Store some media feed items.
  GetMediaHistoryService()->StoreMediaFeedFetchResult(
      SuccessfulResultWithItems(GetExpectedItems(), 1), base::DoNothing());
  WaitForDB();

  {
    auto feeds = GetMediaFeedsSync();
    ASSERT_EQ(1u, feeds.size());
    EXPECT_EQ(media_feeds::mojom::ResetReason::kNone, feeds[0]->reset_reason);
  }

  {
    // Store some cookies on the feed URL and another URL.
    std::vector<GURL> cookie_urls;
    cookie_urls.push_back(feed_url);
    cookie_urls.push_back(alt_url);
    CreateCookies(cookie_urls);
  }

  auto filter = network::mojom::CookieDeletionFilter::New();
  filter->url = alt_url;
  EXPECT_EQ(1u, DeleteCookies(std::move(filter)));
  WaitForDB();

  {
    auto feeds = GetMediaFeedsSync();
    ASSERT_EQ(1u, feeds.size());
    EXPECT_EQ(media_feeds::mojom::ResetReason::kNone, feeds[0]->reset_reason);
  }
}

TEST_F(MediaFeedsServiceTest, ResetOnCookieChange_Overwrite) {
  const GURL feed_url("https://www.google.com/feed");

  // Store a Media Feed.
  GetMediaHistoryService()->DiscoverMediaFeed(feed_url);
  WaitForDB();

  // Store some media feed items.
  GetMediaHistoryService()->StoreMediaFeedFetchResult(
      SuccessfulResultWithItems(GetExpectedItems(), 1), base::DoNothing());
  WaitForDB();

  {
    auto feeds = GetMediaFeedsSync();
    ASSERT_EQ(1u, feeds.size());
    EXPECT_EQ(media_feeds::mojom::ResetReason::kNone, feeds[0]->reset_reason);
  }

  {
    // Store some cookies on the feed URL and another URL.
    std::vector<GURL> cookie_urls;
    cookie_urls.push_back(feed_url);
    cookie_urls.push_back(GURL("https://www.example.com"));
    CreateCookies(cookie_urls);
    CreateCookies(cookie_urls);
    WaitForDB();
  }

  {
    auto feeds = GetMediaFeedsSync();
    ASSERT_EQ(1u, feeds.size());
    EXPECT_EQ(media_feeds::mojom::ResetReason::kNone, feeds[0]->reset_reason);
  }
}

TEST_F(MediaFeedsServiceTest,
       ResetOnCookieChange_ExplicitDeletion_DomainMatch) {
  if (!GetMediaFeedsService()->HasCookieObserverForTest())
    return;

  const GURL feed_url("https://www.google.com/feed");

  // Store a Media Feed.
  GetMediaHistoryService()->DiscoverMediaFeed(feed_url);
  WaitForDB();

  // Store some media feed items.
  GetMediaHistoryService()->StoreMediaFeedFetchResult(
      SuccessfulResultWithItems(GetExpectedItems(), 1), base::DoNothing());
  WaitForDB();

  {
    auto feeds = GetMediaFeedsSync();
    ASSERT_EQ(1u, feeds.size());
    EXPECT_EQ(media_feeds::mojom::ResetReason::kNone, feeds[0]->reset_reason);
  }

  base::RunLoop run_loop;
  GetMediaFeedsService()->SetCookieChangeCallbackForTest(
      run_loop.QuitClosure());

  {
    // Store some domain cookies.
    std::vector<GURL> cookie_urls;
    cookie_urls.push_back(GURL("https://google.com"));
    CreateCookies(cookie_urls, true);
  }

  EXPECT_EQ(1u, DeleteCookies(network::mojom::CookieDeletionFilter::New()));
  run_loop.Run();
  WaitForDB();

  {
    auto feeds = GetMediaFeedsSync();
    ASSERT_EQ(1u, feeds.size());
    EXPECT_EQ(media_feeds::mojom::ResetReason::kCookies,
              feeds[0]->reset_reason);
  }
}

TEST_F(MediaFeedsServiceTest,
       ResetOnCookieChange_ExplicitDeletion_DomainMatch_FullDomain) {
  if (!GetMediaFeedsService()->HasCookieObserverForTest())
    return;

  const GURL feed_url("https://www.google.com/feed");

  // Store a Media Feed.
  GetMediaHistoryService()->DiscoverMediaFeed(feed_url);
  WaitForDB();

  // Store some media feed items.
  GetMediaHistoryService()->StoreMediaFeedFetchResult(
      SuccessfulResultWithItems(GetExpectedItems(), 1), base::DoNothing());
  WaitForDB();

  {
    auto feeds = GetMediaFeedsSync();
    ASSERT_EQ(1u, feeds.size());
    EXPECT_EQ(media_feeds::mojom::ResetReason::kNone, feeds[0]->reset_reason);
  }

  base::RunLoop run_loop;
  GetMediaFeedsService()->SetCookieChangeCallbackForTest(
      run_loop.QuitClosure());

  {
    // Store some domain cookies.
    std::vector<GURL> cookie_urls;
    cookie_urls.push_back(GURL("https://www.google.com"));
    CreateCookies(cookie_urls, true);
  }

  EXPECT_EQ(1u, DeleteCookies(network::mojom::CookieDeletionFilter::New()));
  run_loop.Run();
  WaitForDB();

  {
    auto feeds = GetMediaFeedsSync();
    ASSERT_EQ(1u, feeds.size());
    EXPECT_EQ(media_feeds::mojom::ResetReason::kCookies,
              feeds[0]->reset_reason);
  }
}

TEST_F(MediaFeedsServiceTest,
       ResetOnCookieChange_ExplicitDeletion_DomainMatch_Insecure) {
  if (!GetMediaFeedsService()->HasCookieObserverForTest())
    return;

  const GURL feed_url("https://www.google.com/feed");

  // Store a Media Feed.
  GetMediaHistoryService()->DiscoverMediaFeed(feed_url);
  WaitForDB();

  // Store some media feed items.
  GetMediaHistoryService()->StoreMediaFeedFetchResult(
      SuccessfulResultWithItems(GetExpectedItems(), 1), base::DoNothing());
  WaitForDB();

  {
    auto feeds = GetMediaFeedsSync();
    ASSERT_EQ(1u, feeds.size());
    EXPECT_EQ(media_feeds::mojom::ResetReason::kNone, feeds[0]->reset_reason);
  }

  base::RunLoop run_loop;
  GetMediaFeedsService()->SetCookieChangeCallbackForTest(
      run_loop.QuitClosure());

  {
    // Store some domain cookies.
    std::vector<GURL> cookie_urls;
    cookie_urls.push_back(GURL("http://google.com"));
    CreateCookies(cookie_urls, true);
  }

  EXPECT_EQ(1u, DeleteCookies(network::mojom::CookieDeletionFilter::New()));
  run_loop.Run();
  WaitForDB();

  {
    auto feeds = GetMediaFeedsSync();
    ASSERT_EQ(1u, feeds.size());
    EXPECT_EQ(media_feeds::mojom::ResetReason::kCookies,
              feeds[0]->reset_reason);
  }
}

TEST_F(MediaFeedsServiceTest,
       ResetOnCookieChange_ExplicitDeletion_DomainMatch_AssociatedDomain) {
  if (!GetMediaFeedsService()->HasCookieObserverForTest())
    return;

  const GURL feed_url("https://www.google.com/feed");
  const GURL alt_url("https://www.example.com");

  // Store a Media Feed.
  GetMediaHistoryService()->DiscoverMediaFeed(feed_url);
  WaitForDB();

  std::set<url::Origin> origins;
  origins.insert(url::Origin::Create(alt_url));

  auto result = SuccessfulResultWithItems(GetExpectedItems(), 1);
  result.associated_origins = origins;

  // Store some media feed items.
  GetMediaHistoryService()->StoreMediaFeedFetchResult(std::move(result),
                                                      base::DoNothing());
  WaitForDB();

  {
    auto feeds = GetMediaFeedsSync();
    ASSERT_EQ(1u, feeds.size());
    EXPECT_EQ(media_feeds::mojom::ResetReason::kNone, feeds[0]->reset_reason);
  }

  base::RunLoop run_loop;
  GetMediaFeedsService()->SetCookieChangeCallbackForTest(
      run_loop.QuitClosure());

  {
    // Store some domain cookies.
    std::vector<GURL> cookie_urls;
    cookie_urls.push_back(alt_url);
    CreateCookies(cookie_urls, true);
  }

  EXPECT_EQ(1u, DeleteCookies(network::mojom::CookieDeletionFilter::New()));
  run_loop.Run();
  WaitForDB();

  {
    auto feeds = GetMediaFeedsSync();
    ASSERT_EQ(1u, feeds.size());
    EXPECT_EQ(media_feeds::mojom::ResetReason::kCookies,
              feeds[0]->reset_reason);
  }
}

TEST_F(MediaFeedsServiceTest,
       ResetOnCookieChange_ExplicitDeletion_DomainMatch_NoMatch) {
  if (!GetMediaFeedsService()->HasCookieObserverForTest())
    return;

  const GURL feed_url("https://www.google.com/feed");

  // Store a Media Feed.
  GetMediaHistoryService()->DiscoverMediaFeed(feed_url);
  WaitForDB();

  // Store some media feed items.
  GetMediaHistoryService()->StoreMediaFeedFetchResult(
      SuccessfulResultWithItems(GetExpectedItems(), 1), base::DoNothing());
  WaitForDB();

  {
    auto feeds = GetMediaFeedsSync();
    ASSERT_EQ(1u, feeds.size());
    EXPECT_EQ(media_feeds::mojom::ResetReason::kNone, feeds[0]->reset_reason);
  }

  base::RunLoop run_loop;
  GetMediaFeedsService()->SetCookieChangeCallbackForTest(
      run_loop.QuitClosure());

  {
    // Store some domain cookies.
    std::vector<GURL> cookie_urls;
    cookie_urls.push_back(GURL("https://example.com"));
    CreateCookies(cookie_urls, true);
  }

  EXPECT_EQ(1u, DeleteCookies(network::mojom::CookieDeletionFilter::New()));
  run_loop.Run();
  WaitForDB();

  {
    auto feeds = GetMediaFeedsSync();
    ASSERT_EQ(1u, feeds.size());
    EXPECT_EQ(media_feeds::mojom::ResetReason::kNone, feeds[0]->reset_reason);
  }
}

}  // namespace media_feeds
