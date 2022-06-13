// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/metrics/aw_metrics_service_client.h"

#include <memory>

#include "android_webview/common/aw_features.h"
#include "android_webview/common/metrics/app_package_name_logging_rule.h"
#include "base/metrics/user_metrics.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_simple_task_runner.h"
#include "base/time/time.h"
#include "base/version.h"
#include "components/embedder_support/android/metrics/android_metrics_service_client.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace android_webview {

using AppPackageNameLoggingRuleStatus =
    AwMetricsServiceClient::AppPackageNameLoggingRuleStatus;

using InstallerPackageType =
    metrics::AndroidMetricsServiceClient::InstallerPackageType;

namespace {

constexpr char kTestAllowlistVersion[] = "123.456.789.10";

class AwMetricsServiceClientTestDelegate
    : public AwMetricsServiceClient::Delegate {
  void RegisterAdditionalMetricsProviders(
      metrics::MetricsService* service) override {}
  void AddWebViewAppStateObserver(WebViewAppStateObserver* observer) override {}
  bool HasAwContentsEverCreated() const override { return false; }
};

class AwMetricsServiceClientTest : public testing::Test {
  AwMetricsServiceClientTest& operator=(const AwMetricsServiceClientTest&) =
      delete;
  AwMetricsServiceClientTest(AwMetricsServiceClientTest&&) = delete;
  AwMetricsServiceClientTest& operator=(AwMetricsServiceClientTest&&) = delete;

 protected:
  AwMetricsServiceClientTest()
      : task_runner_(new base::TestSimpleTaskRunner),
        prefs_(std::make_unique<TestingPrefServiceSimple>()),
        client_(std::make_unique<AwMetricsServiceClient>(
            std::make_unique<AwMetricsServiceClientTestDelegate>())) {
    // Required by MetricsService.
    base::SetRecordActionTaskRunner(task_runner_);
    AwMetricsServiceClient::RegisterMetricsPrefs(prefs_->registry());
    client_->Initialize(prefs_.get());
  }

  AwMetricsServiceClient* GetClient() { return client_.get(); }
  TestingPrefServiceSimple* GetPrefs() { return prefs_.get(); }

 private:
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  std::unique_ptr<TestingPrefServiceSimple> prefs_;
  std::unique_ptr<AwMetricsServiceClient> client_;
};

}  // namespace

TEST_F(AwMetricsServiceClientTest, TestShouldRecordPackageName_CacheNotSet) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(
      android_webview::features::kWebViewAppsPackageNamesAllowlist);

  AwMetricsServiceClient* client = GetClient();
  EXPECT_FALSE(client->ShouldRecordPackageName());
  EXPECT_FALSE(client->GetCachedAppPackageNameLoggingRule().has_value());

  histogram_tester.ExpectTotalCount(
      "Android.WebView.Metrics.PackagesAllowList.ResultReceivingDelay", 0);
  histogram_tester.ExpectBucketCount(
      "Android.WebView.Metrics.PackagesAllowList.RecordStatus",
      AppPackageNameLoggingRuleStatus::kNotLoadedNoCache, 1);
  histogram_tester.ExpectTotalCount(
      "Android.WebView.Metrics.PackagesAllowList.RecordStatus", 1);
}

TEST_F(AwMetricsServiceClientTest, TestShouldRecordPackageName_WithCache) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(
      android_webview::features::kWebViewAppsPackageNamesAllowlist);

  AwMetricsServiceClient* client = GetClient();
  TestingPrefServiceSimple* prefs = GetPrefs();

  auto one_day_from_now = base::Time::Now() + base::Days(1);
  AppPackageNameLoggingRule expected_record(
      base::Version(kTestAllowlistVersion), one_day_from_now);
  prefs->Set(prefs::kMetricsAppPackageNameLoggingRule,
             expected_record.ToDictionary());

  absl::optional<AppPackageNameLoggingRule> cached_record =
      client->GetCachedAppPackageNameLoggingRule();
  EXPECT_TRUE(client->ShouldRecordPackageName());
  ASSERT_TRUE(cached_record.has_value());
  EXPECT_TRUE(expected_record.IsSameAs(cached_record.value()));

  histogram_tester.ExpectTotalCount(
      "Android.WebView.Metrics.PackagesAllowList.ResultReceivingDelay", 0);
  histogram_tester.ExpectBucketCount(
      "Android.WebView.Metrics.PackagesAllowList.RecordStatus",
      AppPackageNameLoggingRuleStatus::kNotLoadedUseCache, 1);
  histogram_tester.ExpectTotalCount(
      "Android.WebView.Metrics.PackagesAllowList.RecordStatus", 1);
}

TEST_F(AwMetricsServiceClientTest,
       TestShouldRecordPackageName_TestShouldNotRecordPackageName) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(
      android_webview::features::kWebViewAppsPackageNamesAllowlist);

  AwMetricsServiceClient* client = GetClient();
  AppPackageNameLoggingRule expected_record(
      base::Version(kTestAllowlistVersion), base::Time::Min());
  client->SetAppPackageNameLoggingRule(expected_record);
  absl::optional<AppPackageNameLoggingRule> cached_record =
      client->GetCachedAppPackageNameLoggingRule();

  EXPECT_FALSE(client->ShouldRecordPackageName());
  ASSERT_TRUE(cached_record.has_value());
  EXPECT_TRUE(expected_record.IsSameAs(cached_record.value()));

  histogram_tester.ExpectTotalCount(
      "Android.WebView.Metrics.PackagesAllowList.ResultReceivingDelay", 1);
  histogram_tester.ExpectBucketCount(
      "Android.WebView.Metrics.PackagesAllowList.RecordStatus",
      AppPackageNameLoggingRuleStatus::kNewVersionLoaded, 1);
  histogram_tester.ExpectTotalCount(
      "Android.WebView.Metrics.PackagesAllowList.RecordStatus", 1);
}

TEST_F(AwMetricsServiceClientTest,
       TestShouldRecordPackageName_TestShouldRecordPackageName) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(
      android_webview::features::kWebViewAppsPackageNamesAllowlist);

  auto one_day_from_now = base::Time::Now() + base::Days(1);

  AwMetricsServiceClient* client = GetClient();
  AppPackageNameLoggingRule expected_record(
      base::Version(kTestAllowlistVersion), one_day_from_now);
  client->SetAppPackageNameLoggingRule(expected_record);
  absl::optional<AppPackageNameLoggingRule> cached_record =
      client->GetCachedAppPackageNameLoggingRule();

  EXPECT_TRUE(client->ShouldRecordPackageName());
  ASSERT_TRUE(cached_record.has_value());
  EXPECT_TRUE(expected_record.IsSameAs(cached_record.value()));

  histogram_tester.ExpectTotalCount(
      "Android.WebView.Metrics.PackagesAllowList.ResultReceivingDelay", 1);
  histogram_tester.ExpectBucketCount(
      "Android.WebView.Metrics.PackagesAllowList.RecordStatus",
      AppPackageNameLoggingRuleStatus::kNewVersionLoaded, 1);
  histogram_tester.ExpectTotalCount(
      "Android.WebView.Metrics.PackagesAllowList.RecordStatus", 1);
}

TEST_F(AwMetricsServiceClientTest,
       TestShouldRecordPackageName_TestFailureAfterValidResult) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(
      android_webview::features::kWebViewAppsPackageNamesAllowlist);

  auto one_day_from_now = base::Time::Now() + base::Days(1);

  AwMetricsServiceClient* client = GetClient();
  AppPackageNameLoggingRule expected_record(
      base::Version(kTestAllowlistVersion), one_day_from_now);
  client->SetAppPackageNameLoggingRule(expected_record);
  client->SetAppPackageNameLoggingRule(
      absl::optional<AppPackageNameLoggingRule>());
  absl::optional<AppPackageNameLoggingRule> cached_record =
      client->GetCachedAppPackageNameLoggingRule();

  EXPECT_TRUE(client->ShouldRecordPackageName());
  ASSERT_TRUE(cached_record.has_value());
  EXPECT_TRUE(expected_record.IsSameAs(cached_record.value()));

  histogram_tester.ExpectTotalCount(
      "Android.WebView.Metrics.PackagesAllowList.ResultReceivingDelay", 1);
  histogram_tester.ExpectBucketCount(
      "Android.WebView.Metrics.PackagesAllowList.RecordStatus",
      AppPackageNameLoggingRuleStatus::kNewVersionFailedUseCache, 1);
  histogram_tester.ExpectTotalCount(
      "Android.WebView.Metrics.PackagesAllowList.RecordStatus", 1);
}

TEST_F(AwMetricsServiceClientTest, TestShouldRecordPackageName_FailedResult) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(
      android_webview::features::kWebViewAppsPackageNamesAllowlist);

  AwMetricsServiceClient* client = GetClient();
  client->SetAppPackageNameLoggingRule(
      absl::optional<AppPackageNameLoggingRule>());

  EXPECT_FALSE(client->ShouldRecordPackageName());

  histogram_tester.ExpectTotalCount(
      "Android.WebView.Metrics.PackagesAllowList.ResultReceivingDelay", 0);
  histogram_tester.ExpectBucketCount(
      "Android.WebView.Metrics.PackagesAllowList.RecordStatus",
      AppPackageNameLoggingRuleStatus::kNewVersionFailedNoCache, 1);
  histogram_tester.ExpectTotalCount(
      "Android.WebView.Metrics.PackagesAllowList.RecordStatus", 1);
}

TEST_F(AwMetricsServiceClientTest, TestShouldRecordPackageName_SameAsCache) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(
      android_webview::features::kWebViewAppsPackageNamesAllowlist);

  AwMetricsServiceClient* client = GetClient();
  TestingPrefServiceSimple* prefs = GetPrefs();

  AppPackageNameLoggingRule record(base::Version(kTestAllowlistVersion),
                                   base::Time::Now() + base::Days(1));
  prefs->Set(prefs::kMetricsAppPackageNameLoggingRule, record.ToDictionary());
  client->SetAppPackageNameLoggingRule(record);

  EXPECT_TRUE(client->ShouldRecordPackageName());

  histogram_tester.ExpectTotalCount(
      "Android.WebView.Metrics.PackagesAllowList.ResultReceivingDelay", 0);
  histogram_tester.ExpectBucketCount(
      "Android.WebView.Metrics.PackagesAllowList.RecordStatus",
      AppPackageNameLoggingRuleStatus::kSameVersionAsCache, 1);
  histogram_tester.ExpectTotalCount(
      "Android.WebView.Metrics.PackagesAllowList.RecordStatus", 1);
}

TEST_F(AwMetricsServiceClientTest, TestGetAppPackageNameIfLoggable) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(
      android_webview::features::kWebViewAppsPackageNamesAllowlist);

  class TestClient : public AwMetricsServiceClient {
   public:
    TestClient()
        : AwMetricsServiceClient(
              std::make_unique<AwMetricsServiceClientTestDelegate>()) {}
    ~TestClient() override = default;

    bool ShouldRecordPackageName() override {
      return should_record_package_name_;
    }

    void SetShouldRecordPackageName(bool value) {
      should_record_package_name_ = value;
    }

    InstallerPackageType GetInstallerPackageType() override {
      return installer_type_;
    }

    void SetInstallerPackageType(InstallerPackageType installer_type) {
      installer_type_ = installer_type;
    }

   private:
    bool should_record_package_name_;
    InstallerPackageType installer_type_;
  };

  TestClient client;

  // Package names of system apps are always loggable even if they are not in
  // the allowlist of apps.
  client.SetInstallerPackageType(InstallerPackageType::SYSTEM_APP);
  client.SetShouldRecordPackageName(false);
  EXPECT_FALSE(client.GetAppPackageNameIfLoggable().empty());
  client.SetShouldRecordPackageName(true);
  EXPECT_FALSE(client.GetAppPackageNameIfLoggable().empty());

  // Package names of APPs that are installed by the Play Store are loggable if
  // they are in the allowlist of apps.
  client.SetInstallerPackageType(InstallerPackageType::GOOGLE_PLAY_STORE);
  client.SetShouldRecordPackageName(false);
  EXPECT_TRUE(client.GetAppPackageNameIfLoggable().empty());
  client.SetShouldRecordPackageName(true);
  EXPECT_FALSE(client.GetAppPackageNameIfLoggable().empty());

  // Package names of APPs that are not system apps nor installed by the Play
  // Store are not loggable.
  client.SetInstallerPackageType(InstallerPackageType::OTHER);
  client.SetShouldRecordPackageName(false);
  EXPECT_TRUE(client.GetAppPackageNameIfLoggable().empty());
  client.SetShouldRecordPackageName(true);
  EXPECT_TRUE(client.GetAppPackageNameIfLoggable().empty());
}

}  // namespace android_webview
