// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/safe_browsing/chrome_ping_manager_factory.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_service.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/safe_browsing/core/browser/ping_manager.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using network::GetUploadData;

namespace safe_browsing {

class TestSafeBrowsingTokenFetcher;

class ChromePingManagerTest : public testing::Test {
 protected:
  void SetUp() override;
  void TearDown() override;
  void RunReportThreatDetailsTest(bool is_enhanced_protection,
                                  bool is_signed_in,
                                  bool is_csbrr_token_feature_enabled,
                                  bool is_remove_cookies_feature_enabled,
                                  bool expect_access_token,
                                  bool expect_cookies_removed);

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfileManager> profile_manager_;

 private:
  void SetUpFeatureList(bool should_enable_csbrr_with_token,
                        bool should_enable_remove_cookies);
  raw_ptr<TestingProfile> SetUpProfile(bool is_enhanced_protection,
                                       bool is_signed_in);
  TestSafeBrowsingTokenFetcher* SetUpTokenFetcher(PingManager* ping_manager);

  scoped_refptr<safe_browsing::SafeBrowsingService> sb_service_;
  base::test::ScopedFeatureList feature_list_;
};

class TestSafeBrowsingTokenFetcher : public SafeBrowsingTokenFetcher {
 public:
  TestSafeBrowsingTokenFetcher() = default;
  ~TestSafeBrowsingTokenFetcher() override { RunAccessTokenCallback(""); }

  void Start(Callback callback) override {
    callback_ = std::move(callback);
    was_start_called_ = true;
  }
  void RunAccessTokenCallback(std::string token) {
    if (callback_) {
      std::move(callback_).Run(token);
    }
  }
  bool WasStartCalled() { return was_start_called_; }
  MOCK_METHOD1(OnInvalidAccessToken, void(const std::string&));

 private:
  Callback callback_;
  bool was_start_called_ = false;
};

void ChromePingManagerTest::SetUp() {
  profile_manager_ = std::make_unique<TestingProfileManager>(
      TestingBrowserProcess::GetGlobal());
  ASSERT_TRUE(profile_manager_->SetUp());
  ASSERT_TRUE(g_browser_process->profile_manager());

  sb_service_ = base::MakeRefCounted<safe_browsing::TestSafeBrowsingService>();
  TestingBrowserProcess::GetGlobal()->SetSafeBrowsingService(sb_service_.get());
  g_browser_process->safe_browsing_service()->Initialize();
}

void ChromePingManagerTest::TearDown() {
  base::RunLoop().RunUntilIdle();

  if (TestingBrowserProcess::GetGlobal()->safe_browsing_service()) {
    TestingBrowserProcess::GetGlobal()->safe_browsing_service()->ShutDown();
    TestingBrowserProcess::GetGlobal()->SetSafeBrowsingService(nullptr);
  }

  feature_list_.Reset();
}

void ChromePingManagerTest::SetUpFeatureList(
    bool should_enable_csbrr_with_token,
    bool should_enable_remove_cookies) {
  std::vector<base::Feature> enabled_features = {};
  std::vector<base::Feature> disabled_features = {};
  if (should_enable_csbrr_with_token) {
    enabled_features.push_back(kSafeBrowsingCsbrrWithToken);
  } else {
    disabled_features.push_back(kSafeBrowsingCsbrrWithToken);
  }
  if (should_enable_remove_cookies) {
    enabled_features.push_back(kSafeBrowsingRemoveCookiesInAuthRequests);
  } else {
    disabled_features.push_back(kSafeBrowsingRemoveCookiesInAuthRequests);
  }
  feature_list_.InitWithFeatures(enabled_features, disabled_features);
}

raw_ptr<TestingProfile> ChromePingManagerTest::SetUpProfile(
    bool is_enhanced_protection,
    bool is_signed_in) {
  raw_ptr<TestingProfile> profile = profile_manager_->CreateTestingProfile(
      "testing_profile", IdentityTestEnvironmentProfileAdaptor::
                             GetIdentityTestEnvironmentFactories());
  if (is_enhanced_protection) {
    SetSafeBrowsingState(profile->GetPrefs(),
                         SafeBrowsingState::ENHANCED_PROTECTION);
  }
  if (is_signed_in) {
    IdentityTestEnvironmentProfileAdaptor adaptor(profile);
    adaptor.identity_test_env()->MakePrimaryAccountAvailable(
        "testing@gmail.com", signin::ConsentLevel::kSync);
  }
  return profile;
}

TestSafeBrowsingTokenFetcher* ChromePingManagerTest::SetUpTokenFetcher(
    PingManager* ping_manager) {
  auto token_fetcher = std::make_unique<TestSafeBrowsingTokenFetcher>();
  auto* raw_token_fetcher = token_fetcher.get();
  ping_manager->SetTokenFetcherForTesting(std::move(token_fetcher));
  return raw_token_fetcher;
}

void ChromePingManagerTest::RunReportThreatDetailsTest(
    bool is_enhanced_protection,
    bool is_signed_in,
    bool is_csbrr_token_feature_enabled,
    bool is_remove_cookies_feature_enabled,
    bool expect_access_token,
    bool expect_cookies_removed) {
  base::HistogramTester histogram_tester;
  SetUpFeatureList(is_csbrr_token_feature_enabled,
                   is_remove_cookies_feature_enabled);
  raw_ptr<TestingProfile> profile =
      SetUpProfile(is_enhanced_protection, is_signed_in);
  auto* ping_manager = ChromePingManagerFactory::GetForBrowserContext(profile);
  auto* raw_token_fetcher = SetUpTokenFetcher(ping_manager);

  std::string access_token = "testing_access_token";
  std::string report_content = "testing_report_content";
  network::TestURLLoaderFactory test_url_loader_factory;
  test_url_loader_factory.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        EXPECT_EQ(GetUploadData(request), report_content);
        std::string header_value;
        bool found_header = request.headers.GetHeader(
            net::HttpRequestHeaders::kAuthorization, &header_value);
        EXPECT_EQ(found_header, expect_access_token);
        if (expect_access_token) {
          EXPECT_EQ(header_value, "Bearer " + access_token);
        }
        EXPECT_EQ(request.credentials_mode,
                  expect_cookies_removed
                      ? network::mojom::CredentialsMode::kOmit
                      : network::mojom::CredentialsMode::kInclude);
        histogram_tester.ExpectUniqueSample(
            "SafeBrowsing.ClientSafeBrowsingReport.RequestHasToken",
            /*sample=*/expect_access_token,
            /*expected_bucket_count=*/1);
      }));
  ping_manager->SetURLLoaderFactoryForTesting(
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory));

  ping_manager->ReportThreatDetails(report_content);
  EXPECT_EQ(raw_token_fetcher->WasStartCalled(), expect_access_token);
  if (expect_access_token) {
    raw_token_fetcher->RunAccessTokenCallback(access_token);
  }
}

TEST_F(ChromePingManagerTest, ReportThreatDetailsWithAccessToken) {
  RunReportThreatDetailsTest(/*is_enhanced_protection=*/true,
                             /*is_signed_in=*/true,
                             /*is_csbrr_token_feature_enabled=*/true,
                             /*is_remove_cookies_feature_enabled=*/true,
                             /*expect_access_token=*/true,
                             /*expect_cookies_removed=*/true);
}
TEST_F(ChromePingManagerTest,
       ReportThreatDetailsWithAccessToken_RemoveCookiesFeatureDisabled) {
  RunReportThreatDetailsTest(/*is_enhanced_protection=*/true,
                             /*is_signed_in=*/true,
                             /*is_csbrr_token_feature_enabled=*/true,
                             /*is_remove_cookies_feature_enabled=*/false,
                             /*expect_access_token=*/true,
                             /*expect_cookies_removed=*/false);
}
TEST_F(ChromePingManagerTest,
       ReportThreatDetailsWithoutAccessToken_NotSignedIn) {
  RunReportThreatDetailsTest(/*is_enhanced_protection=*/true,
                             /*is_signed_in=*/false,
                             /*is_csbrr_token_feature_enabled=*/true,
                             /*is_remove_cookies_feature_enabled=*/true,
                             /*expect_access_token=*/false,
                             /*expect_cookies_removed=*/false);
}
TEST_F(ChromePingManagerTest,
       ReportThreatDetailsWithoutAccessToken_NotEnhancedProtection) {
  RunReportThreatDetailsTest(/*is_enhanced_protection=*/false,
                             /*is_signed_in=*/true,
                             /*is_csbrr_token_feature_enabled=*/true,
                             /*is_remove_cookies_feature_enabled=*/true,
                             /*expect_access_token=*/false,
                             /*expect_cookies_removed=*/false);
}
TEST_F(ChromePingManagerTest, ReportThreatDetailsWithoutAccessToken_Incognito) {
  raw_ptr<TestingProfile> profile = TestingProfile::Builder().BuildIncognito(
      profile_manager_->CreateTestingProfile("testing_profile"));
  EXPECT_EQ(ChromePingManagerFactory::GetForBrowserContext(profile), nullptr);
}
// TODO(crbug.com/1296615): remove test case when deprecating
// kSafeBrowsingCsbrrWithToken feature
TEST_F(ChromePingManagerTest,
       ReportThreatDetailsWithoutAccessToken_CsbrrTokenFeatureDisabled) {
  RunReportThreatDetailsTest(/*is_enhanced_protection=*/true,
                             /*is_signed_in=*/true,
                             /*is_csbrr_token_feature_enabled=*/false,
                             /*is_remove_cookies_feature_enabled=*/true,
                             /*expect_access_token=*/false,
                             /*expect_cookies_removed=*/false);
}

TEST_F(ChromePingManagerTest, ReportSafeBrowsingHit) {
  raw_ptr<TestingProfile> profile =
      profile_manager_->CreateTestingProfile("testing_profile");
  auto* ping_manager = ChromePingManagerFactory::GetForBrowserContext(profile);

  HitReport hit_report;
  hit_report.post_data = "testing_hit_report_post_data";
  // Threat type and source are arbitrary but specified so that determining the
  // URL does not does throw an error due to input validation.
  hit_report.threat_type = SB_THREAT_TYPE_URL_PHISHING;
  hit_report.threat_source = ThreatSource::LOCAL_PVER4;

  network::TestURLLoaderFactory test_url_loader_factory;
  test_url_loader_factory.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        EXPECT_EQ(GetUploadData(request), hit_report.post_data);
      }));
  ping_manager->SetURLLoaderFactoryForTesting(
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory));

  ping_manager->ReportSafeBrowsingHit(hit_report);
}

}  // namespace safe_browsing
