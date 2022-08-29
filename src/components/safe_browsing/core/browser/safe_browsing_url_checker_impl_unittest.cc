// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/safe_browsing_url_checker_impl.h"
#include <memory>

#include "base/bind.h"
#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/safe_browsing/core/browser/db/test_database_manager.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/browser/realtime/url_lookup_service.h"
#include "components/safe_browsing/core/browser/safe_browsing_token_fetcher.h"
#include "components/safe_browsing/core/browser/url_checker_delegate.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/security_interstitials/core/unsafe_resource.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

using security_interstitials::UnsafeResource;
using ::testing::_;

namespace safe_browsing {

namespace {

// A matcher for threat source in UnsafeResource.
MATCHER_P(IsSameThreatSource, threatSource, "") {
  return arg.threat_source == threatSource;
}

class MockSafeBrowsingDatabaseManager : public TestSafeBrowsingDatabaseManager {
 public:
  MockSafeBrowsingDatabaseManager()
      : TestSafeBrowsingDatabaseManager(
            base::SequencedTaskRunnerHandle::Get(),
            base::SequencedTaskRunnerHandle::Get()) {}
  // SafeBrowsingDatabaseManager implementation.
  // Checks the threat type of |gurl| previously set by |SetThreatTypeForUrl|.
  // It crashes if the threat type of |gurl| is not set in advance.
  bool CheckBrowseUrl(const GURL& gurl,
                      const safe_browsing::SBThreatTypeSet& threat_types,
                      Client* client) override {
    std::string url = gurl.spec();
    DCHECK(base::Contains(urls_threat_type_, url));
    DCHECK(base::Contains(urls_delayed_callback_, url));
    if (urls_threat_type_[url] == SB_THREAT_TYPE_SAFE) {
      return true;
    }
    if (!urls_delayed_callback_[url]) {
      base::SequencedTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::BindOnce(&MockSafeBrowsingDatabaseManager::OnCheckBrowseURLDone,
                         this, gurl, client));
    } else {
      // If delayed callback is set to true, store the client in |urls_client_|.
      // The callback can be triggered by |RestartDelayedCallback|.
      urls_client_[url] = client;
    }
    return false;
  }

  bool CanCheckRequestDestination(
      network::mojom::RequestDestination request_destination) const override {
    return true;
  }

  bool ChecksAreAlwaysAsync() const override { return false; }

  ThreatSource GetThreatSource() const override {
    return ThreatSource::UNKNOWN;
  }

  // Returns the allowlist match result previously set by
  // |SetAllowlistResultForUrl|. It crashes if the allowlist match result for
  // the |gurl| is not set in advance.
  AsyncMatch CheckUrlForHighConfidenceAllowlist(const GURL& gurl,
                                                Client* client) override {
    std::string url = gurl.spec();
    DCHECK(base::Contains(urls_allowlist_match_, url));
    return urls_allowlist_match_[url];
  }

  // Helper functions.
  // Restart the previous delayed callback for |gurl|. This is useful to test
  // the asynchronous URL check, i.e. the database manager is still checking the
  // previous URL and the new redirect URL arrives.
  void RestartDelayedCallback(const GURL& gurl) {
    std::string url = gurl.spec();
    DCHECK(base::Contains(urls_delayed_callback_, url));
    DCHECK_EQ(true, urls_delayed_callback_[url]);
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&MockSafeBrowsingDatabaseManager::OnCheckBrowseURLDone,
                       this, gurl, urls_client_[url]));
  }

  void SetThreatTypeForUrl(const GURL& gurl,
                           SBThreatType threat_type,
                           bool delayed_callback) {
    std::string url = gurl.spec();
    urls_threat_type_[url] = threat_type;
    urls_delayed_callback_[url] = delayed_callback;
  }

  void SetAllowlistResultForUrl(const GURL& gurl, AsyncMatch match) {
    std::string url = gurl.spec();
    urls_allowlist_match_[url] = match;
  }

 protected:
  ~MockSafeBrowsingDatabaseManager() override = default;

 private:
  void OnCheckBrowseURLDone(const GURL& gurl, Client* client) {
    std::string url = gurl.spec();
    DCHECK(base::Contains(urls_threat_type_, url));
    ThreatMetadata metadata;
    client->OnCheckBrowseUrlResult(gurl, urls_threat_type_[url], metadata);
  }
  base::flat_map<std::string, SBThreatType> urls_threat_type_;
  base::flat_map<std::string, bool> urls_delayed_callback_;
  base::flat_map<std::string, Client*> urls_client_;
  base::flat_map<std::string, AsyncMatch> urls_allowlist_match_;
};

class MockUrlCheckerDelegate : public UrlCheckerDelegate {
 public:
  explicit MockUrlCheckerDelegate(SafeBrowsingDatabaseManager* database_manager)
      : database_manager_(database_manager),
        threat_types_(
            SBThreatTypeSet({safe_browsing::SB_THREAT_TYPE_URL_PHISHING})) {}

  MOCK_METHOD1(MaybeDestroyNoStatePrefetchContents,
               void(base::OnceCallback<content::WebContents*()>));
  MOCK_METHOD5(StartDisplayingBlockingPageHelper,
               void(const security_interstitials::UnsafeResource&,
                    const std::string&,
                    const net::HttpRequestHeaders&,
                    bool,
                    bool));
  MOCK_METHOD2(StartObservingInteractionsForDelayedBlockingPageHelper,
               void(const security_interstitials::UnsafeResource&, bool));
  MOCK_METHOD5(ShouldSkipRequestCheck, bool(const GURL&, int, int, int, bool));
  MOCK_METHOD1(NotifySuspiciousSiteDetected,
               void(const base::RepeatingCallback<content::WebContents*()>&));
  MOCK_METHOD0(GetUIManager, BaseUIManager*());

  bool IsUrlAllowlisted(const GURL& url) override { return false; }
  void SetPolicyAllowlistDomains(
      const std::vector<std::string>& allowlist_domains) override {}
  const SBThreatTypeSet& GetThreatTypes() override { return threat_types_; }
  SafeBrowsingDatabaseManager* GetDatabaseManager() override {
    return database_manager_;
  }

 protected:
  ~MockUrlCheckerDelegate() override = default;

 private:
  raw_ptr<SafeBrowsingDatabaseManager> database_manager_;
  SBThreatTypeSet threat_types_;
};

class MockRealTimeUrlLookupService : public RealTimeUrlLookupServiceBase {
 public:
  MockRealTimeUrlLookupService()
      : RealTimeUrlLookupServiceBase(
            /*url_loader_factory=*/nullptr,
            /*cache_manager=*/nullptr,
            /*get_user_population_callback=*/base::BindRepeating([]() {
              return ChromeUserPopulation();
            }),
            /*referrer_chain_provider=*/nullptr) {}
  // Returns the threat type previously set by |SetThreatTypeForUrl|. It crashes
  // if the threat type for the |gurl| is not set in advance.
  void StartLookup(
      const GURL& gurl,
      const GURL& last_committed_url,
      bool is_mainframe,
      RTLookupRequestCallback request_callback,
      RTLookupResponseCallback response_callback,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner) override {
    std::string url = gurl.spec();
    DCHECK(base::Contains(urls_threat_type_, url));
    auto response = std::make_unique<RTLookupResponse>();
    RTLookupResponse::ThreatInfo* new_threat_info = response->add_threat_info();
    RTLookupResponse::ThreatInfo threat_info;
    RTLookupResponse::ThreatInfo::ThreatType threat_type;
    RTLookupResponse::ThreatInfo::VerdictType verdict_type;
    SBThreatType sb_threat_type = urls_threat_type_[url];
    switch (sb_threat_type) {
      case SB_THREAT_TYPE_URL_PHISHING:
        threat_type = RTLookupResponse::ThreatInfo::SOCIAL_ENGINEERING;
        verdict_type = RTLookupResponse::ThreatInfo::DANGEROUS;
        break;
      case SB_THREAT_TYPE_SAFE:
        threat_type = RTLookupResponse::ThreatInfo::THREAT_TYPE_UNSPECIFIED;
        verdict_type = RTLookupResponse::ThreatInfo::SAFE;
        break;
      default:
        NOTREACHED();
        threat_type = RTLookupResponse::ThreatInfo::THREAT_TYPE_UNSPECIFIED;
        verdict_type = RTLookupResponse::ThreatInfo::SAFE;
    }
    threat_info.set_threat_type(threat_type);
    threat_info.set_verdict_type(verdict_type);
    *new_threat_info = threat_info;
    callback_task_runner->PostTask(
        FROM_HERE, base::BindOnce(std::move(response_callback),
                                  /* is_rt_lookup_successful */ true,
                                  /* is_cached_response */ is_cached_response_,
                                  std::move(response)));
  }

  void SendSampledRequest(
      const GURL& gurl,
      const GURL& last_committed_url,
      bool is_mainframe,
      RTLookupRequestCallback request_callback,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner) override {}

  void SetThreatTypeForUrl(const GURL& gurl, SBThreatType threat_type) {
    urls_threat_type_[gurl.spec()] = threat_type;
  }

  void SetIsCachedResponse(bool is_cached_response) {
    is_cached_response_ = is_cached_response;
  }

  // RealTimeUrlLookupServiceBase:
  bool CanPerformFullURLLookup() const override { return true; }
  bool CanCheckSubresourceURL() const override { return false; }
  bool CanCheckSafeBrowsingDb() const override { return true; }
  bool CanSendRTSampleRequest() const override { return true; }

 private:
  // RealTimeUrlLookupServiceBase:
  GURL GetRealTimeLookupUrl() const override { return GURL(); }
  net::NetworkTrafficAnnotationTag GetTrafficAnnotationTag() const override {
    return TRAFFIC_ANNOTATION_FOR_TESTS;
  }
  bool CanPerformFullURLLookupWithToken() const override { return false; }
  int GetReferrerUserGestureLimit() const override { return 0; }
  bool CanSendPageLoadToken() const override { return false; }
  void GetAccessToken(
      const GURL& url,
      const GURL& last_committed_url,
      bool is_mainframe,
      RTLookupRequestCallback request_callback,
      RTLookupResponseCallback response_callback,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner) override {}
  absl::optional<std::string> GetDMTokenString() const override {
    return absl::nullopt;
  }
  std::string GetMetricSuffix() const override { return ""; }
  bool ShouldIncludeCredentials() const override { return false; }
  double GetMinAllowedTimestampForReferrerChains() const override { return 0; }

  base::flat_map<std::string, SBThreatType> urls_threat_type_;
  bool is_cached_response_ = false;
};

}  // namespace

class SafeBrowsingUrlCheckerTest : public PlatformTest {
 public:
  SafeBrowsingUrlCheckerTest() = default;

  void SetUp() override {
    PlatformTest::SetUp();
    database_manager_ = new MockSafeBrowsingDatabaseManager();
    url_checker_delegate_ = new MockUrlCheckerDelegate(database_manager_.get());
    url_lookup_service_ = std::make_unique<MockRealTimeUrlLookupService>();
  }

  std::unique_ptr<SafeBrowsingUrlCheckerImpl> CreateSafeBrowsingUrlChecker(
      bool real_time_lookup_enabled,
      bool can_check_safe_browsing_db) {
    base::MockCallback<base::RepeatingCallback<content::WebContents*()>>
        mock_web_contents_getter;
    return std::make_unique<SafeBrowsingUrlCheckerImpl>(
        net::HttpRequestHeaders(), /*load_flags=*/0,
        network::mojom::RequestDestination::kDocument,
        /*has_user_gesture=*/false, url_checker_delegate_,
        mock_web_contents_getter.Get(), UnsafeResource::kNoRenderProcessId,
        UnsafeResource::kNoRenderFrameId, UnsafeResource::kNoFrameTreeNodeId,
        real_time_lookup_enabled,
        /*can_rt_check_subresource_url=*/false, can_check_safe_browsing_db,
        /*last_committed_url=*/GURL(), base::SequencedTaskRunnerHandle::Get(),
        real_time_lookup_enabled ? url_lookup_service_->GetWeakPtr() : nullptr,
        /*webui_delegate_=*/nullptr);
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<MockSafeBrowsingDatabaseManager> database_manager_;
  scoped_refptr<MockUrlCheckerDelegate> url_checker_delegate_;
  std::unique_ptr<MockRealTimeUrlLookupService> url_lookup_service_;
};

TEST_F(SafeBrowsingUrlCheckerTest, CheckUrl_SafeUrl) {
  auto safe_browsing_url_checker = CreateSafeBrowsingUrlChecker(
      /*real_time_lookup_enabled=*/false, /*can_check_safe_browsing_db=*/true);

  GURL url("https://example.test/");
  database_manager_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_SAFE,
                                         /*delayed_callback=*/false);
  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
      callback;
  EXPECT_CALL(callback,
              Run(nullptr, /*proceed=*/true, /*showed_interstitial=*/false));
  EXPECT_CALL(*url_checker_delegate_,
              StartDisplayingBlockingPageHelper(_, _, _, _, _))
      .Times(0);

  safe_browsing_url_checker->CheckUrl(url, "GET", callback.Get());
  task_environment_.RunUntilIdle();
}

TEST_F(SafeBrowsingUrlCheckerTest, CheckUrl_DangerousUrl) {
  auto safe_browsing_url_checker = CreateSafeBrowsingUrlChecker(
      /*real_time_lookup_enabled=*/false, /*can_check_safe_browsing_db=*/true);

  GURL url("https://example.test/");
  database_manager_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_URL_PHISHING,
                                         /*delayed_callback=*/false);

  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
      callback;
  EXPECT_CALL(callback,
              Run(_, /*proceed=*/false, /*showed_interstitial=*/false));
  EXPECT_CALL(*url_checker_delegate_,
              StartDisplayingBlockingPageHelper(
                  IsSameThreatSource(ThreatSource::UNKNOWN), _, _, _, _))
      .Times(1);
  safe_browsing_url_checker->CheckUrl(url, "GET", callback.Get());
  task_environment_.RunUntilIdle();
}

TEST_F(SafeBrowsingUrlCheckerTest, CheckUrl_RedirectUrlsSafe) {
  auto safe_browsing_url_checker = CreateSafeBrowsingUrlChecker(
      /*real_time_lookup_enabled=*/false, /*can_check_safe_browsing_db=*/true);

  GURL origin_url("https://example.test/");
  database_manager_->SetThreatTypeForUrl(origin_url, SB_THREAT_TYPE_SAFE,
                                         /*delayed_callback=*/false);

  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
      origin_callback;
  EXPECT_CALL(origin_callback,
              Run(_, /*proceed=*/true, /*showed_interstitial=*/false));
  EXPECT_CALL(*url_checker_delegate_,
              StartDisplayingBlockingPageHelper(_, _, _, _, _))
      .Times(0);
  safe_browsing_url_checker->CheckUrl(origin_url, "GET", origin_callback.Get());

  GURL redirect_url("https://example.redirect.test/");
  database_manager_->SetThreatTypeForUrl(redirect_url, SB_THREAT_TYPE_SAFE,
                                         /*delayed_callback=*/false);

  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
      redirect_callback;
  EXPECT_CALL(redirect_callback,
              Run(_, /*proceed=*/true, /*showed_interstitial=*/false));
  safe_browsing_url_checker->CheckUrl(redirect_url, "GET",
                                      redirect_callback.Get());

  task_environment_.RunUntilIdle();
}

TEST_F(SafeBrowsingUrlCheckerTest,
       CheckUrl_RedirectUrlsOriginDangerousRedirectSafe) {
  auto safe_browsing_url_checker = CreateSafeBrowsingUrlChecker(
      /*real_time_lookup_enabled=*/false, /*can_check_safe_browsing_db=*/true);

  GURL origin_url("https://example.test/");
  database_manager_->SetThreatTypeForUrl(
      origin_url, SB_THREAT_TYPE_URL_PHISHING, /*delayed_callback=*/true);

  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
      origin_callback;
  EXPECT_CALL(origin_callback,
              Run(_, /*proceed=*/false, /*showed_interstitial=*/false));
  // Not displayed yet, because the callback is not returned.
  EXPECT_CALL(*url_checker_delegate_,
              StartDisplayingBlockingPageHelper(_, _, _, _, _))
      .Times(0);
  safe_browsing_url_checker->CheckUrl(origin_url, "GET", origin_callback.Get());

  task_environment_.RunUntilIdle();

  GURL redirect_url("https://example.redirect.test/");
  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
      redirect_callback;
  // Not called because it is blocked by the first URL.
  EXPECT_CALL(redirect_callback, Run(_, _, _)).Times(0);
  safe_browsing_url_checker->CheckUrl(redirect_url, "GET",
                                      redirect_callback.Get());

  // The blocking page should be displayed when the origin callback is returned.
  EXPECT_CALL(*url_checker_delegate_,
              StartDisplayingBlockingPageHelper(_, _, _, _, _))
      .Times(1);
  database_manager_->RestartDelayedCallback(origin_url);
  task_environment_.RunUntilIdle();
}

TEST_F(SafeBrowsingUrlCheckerTest, CheckUrl_RealTimeEnabledAllowlistMatch) {
  auto safe_browsing_url_checker = CreateSafeBrowsingUrlChecker(
      /*real_time_lookup_enabled=*/true, /*can_check_safe_browsing_db=*/true);

  GURL url("https://example.test/");
  database_manager_->SetAllowlistResultForUrl(url, AsyncMatch::MATCH);
  // To make sure hash based check is not skipped when the URL is in the
  // allowlist, set threat type to phishing for hash based check.
  database_manager_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_URL_PHISHING,
                                         /*delayed_callback=*/false);

  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
      callback;
  // Note that the callback is not called, because resource fetch is not blocked
  // while we perform a real time URL check.
  EXPECT_CALL(callback, Run(_, _, _)).Times(0);
  EXPECT_CALL(*url_checker_delegate_,
              StartDisplayingBlockingPageHelper(
                  IsSameThreatSource(ThreatSource::UNKNOWN), _, _, _, _))
      .Times(1);
  safe_browsing_url_checker->CheckUrl(url, "GET", callback.Get());

  task_environment_.RunUntilIdle();
}

TEST_F(SafeBrowsingUrlCheckerTest, CheckUrl_RealTimeEnabledSafeUrl) {
  base::HistogramTester histograms;
  auto safe_browsing_url_checker = CreateSafeBrowsingUrlChecker(
      /*real_time_lookup_enabled=*/true, /*can_check_safe_browsing_db=*/true);

  GURL url("https://example.test/");
  database_manager_->SetAllowlistResultForUrl(url, AsyncMatch::NO_MATCH);
  url_lookup_service_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_SAFE);

  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
      callback;
  EXPECT_CALL(callback,
              Run(_, /*proceed=*/true, /*showed_interstitial=*/false));
  EXPECT_CALL(*url_checker_delegate_,
              StartDisplayingBlockingPageHelper(_, _, _, _, _))
      .Times(0);
  safe_browsing_url_checker->CheckUrl(url, "GET", callback.Get());

  task_environment_.RunUntilIdle();

  // The false positive metric should not be logged, because the
  // verdict is not from cache.
  histograms.ExpectTotalCount("SafeBrowsing.RT.GetCache.FallbackThreatType",
                              /* total_count */ 0);
}

TEST_F(SafeBrowsingUrlCheckerTest, CheckUrl_RealTimeEnabledSafeUrlFromCache) {
  base::HistogramTester histograms;
  auto safe_browsing_url_checker = CreateSafeBrowsingUrlChecker(
      /*real_time_lookup_enabled=*/true, /*can_check_safe_browsing_db=*/true);

  GURL url("https://example.test/");
  database_manager_->SetAllowlistResultForUrl(url, AsyncMatch::NO_MATCH);
  database_manager_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_SAFE,
                                         /*delayed_callback=*/false);
  url_lookup_service_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_SAFE);
  url_lookup_service_->SetIsCachedResponse(true);

  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
      callback;
  EXPECT_CALL(callback,
              Run(_, /*proceed=*/true, /*showed_interstitial=*/false));
  EXPECT_CALL(*url_checker_delegate_,
              StartDisplayingBlockingPageHelper(_, _, _, _, _))
      .Times(0);
  safe_browsing_url_checker->CheckUrl(url, "GET", callback.Get());

  task_environment_.RunUntilIdle();

  histograms.ExpectUniqueSample("SafeBrowsing.RT.GetCache.FallbackThreatType",
                                /* sample */ SB_THREAT_TYPE_SAFE,
                                /* expected_count */ 1);
}

TEST_F(SafeBrowsingUrlCheckerTest,
       CheckUrl_RealTimeEnabledSafeUrlFromCacheFalsePositive) {
  base::HistogramTester histograms;
  auto safe_browsing_url_checker = CreateSafeBrowsingUrlChecker(
      /*real_time_lookup_enabled=*/true, /*can_check_safe_browsing_db=*/true);

  GURL url("https://example.test/");
  database_manager_->SetAllowlistResultForUrl(url, AsyncMatch::NO_MATCH);
  database_manager_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_URL_PHISHING,
                                         /*delayed_callback=*/false);
  url_lookup_service_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_SAFE);
  url_lookup_service_->SetIsCachedResponse(true);

  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
      callback;
  EXPECT_CALL(*url_checker_delegate_,
              StartDisplayingBlockingPageHelper(
                  IsSameThreatSource(ThreatSource::UNKNOWN), _, _, _, _))
      .Times(1);
  safe_browsing_url_checker->CheckUrl(url, "GET", callback.Get());

  task_environment_.RunUntilIdle();

  histograms.ExpectUniqueSample("SafeBrowsing.RT.GetCache.FallbackThreatType",
                                /* sample */ SB_THREAT_TYPE_URL_PHISHING,
                                /* expected_count */ 1);
}

TEST_F(SafeBrowsingUrlCheckerTest,
       CheckUrl_RealTimeEnabledSafeBrowsingDisabled) {
  auto safe_browsing_url_checker = CreateSafeBrowsingUrlChecker(
      /*real_time_lookup_enabled=*/true, /*can_check_safe_browsing_db=*/false);

  GURL url("https://example.test/");
  url_lookup_service_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_URL_PHISHING);

  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
      callback;
  // Should still show blocking page because real time lookup is enabled.
  EXPECT_CALL(
      *url_checker_delegate_,
      StartDisplayingBlockingPageHelper(
          IsSameThreatSource(ThreatSource::REAL_TIME_CHECK), _, _, _, _))
      .Times(1);
  safe_browsing_url_checker->CheckUrl(url, "GET", callback.Get());

  task_environment_.RunUntilIdle();
}

}  // namespace safe_browsing
