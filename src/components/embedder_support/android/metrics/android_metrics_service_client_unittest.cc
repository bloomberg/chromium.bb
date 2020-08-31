// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/embedder_support/android/metrics/android_metrics_service_client.h"

#include <memory>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_simple_task_runner.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/metrics_switches.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {
namespace {

// For client ID format, see:
// https://en.wikipedia.org/wiki/Universally_unique_identifier#Version_4_(random)
const char kTestClientId[] = "01234567-89ab-40cd-80ef-0123456789ab";

class TestClient : public AndroidMetricsServiceClient {
 public:
  TestClient()
      : sample_bucket_value_(0),
        sampled_in_rate_per_mille_(1000),
        package_name_rate_per_mille_(1000),
        record_package_name_for_app_type_(true) {}

  ~TestClient() override = default;

  bool IsRecordingActive() {
    auto* service = GetMetricsService();
    if (service)
      return service->recording_active();
    return false;
  }

  void SetSampleRatePerMille(int per_mille) {
    sampled_in_rate_per_mille_ = per_mille;
  }

  void SetInSample(bool value) {
    sampled_in_rate_per_mille_ = value ? 1000 : 0;
  }

  void SetRecordPackageNameForAppType(bool value) {
    record_package_name_for_app_type_ = value;
  }

  void SetPackageNameSamplePerMille(int per_mille) {
    package_name_rate_per_mille_ = per_mille;
  }

  void SetInPackageNameSample(bool value) {
    package_name_rate_per_mille_ = value ? 1000 : 0;
  }

  void SetSampleBucketValue(int per_mille) { sample_bucket_value_ = per_mille; }

  // Expose the super class implementation for testing.
  using AndroidMetricsServiceClient::GetAppPackageNameInternal;
  using AndroidMetricsServiceClient::IsInPackageNameSample;
  using AndroidMetricsServiceClient::IsInSample;

 protected:
  void OnMetricsStart() override {}

  void OnMetricsNotStarted() override {}

  int GetSampleBucketValue() override { return sample_bucket_value_; }

  int GetSampleRatePerMille() override { return sampled_in_rate_per_mille_; }

  bool CanRecordPackageNameForAppType() override {
    return record_package_name_for_app_type_;
  }

  // AndroidMetricsServiceClient:
  int32_t GetProduct() override {
    return metrics::ChromeUserMetricsExtension::CHROME;
  }

  int GetPackageNameLimitRatePerMille() override {
    return package_name_rate_per_mille_;
  }

  void RegisterAdditionalMetricsProviders(MetricsService* service) override {}

 private:
  int sample_bucket_value_;
  int sampled_in_rate_per_mille_;
  int package_name_rate_per_mille_;
  bool record_package_name_for_app_type_;
  DISALLOW_COPY_AND_ASSIGN(TestClient);
};

std::unique_ptr<TestingPrefServiceSimple> CreateTestPrefs() {
  auto prefs = std::make_unique<TestingPrefServiceSimple>();
  metrics::MetricsService::RegisterPrefs(prefs->registry());
  return prefs;
}

std::unique_ptr<TestClient> CreateAndInitTestClient(PrefService* prefs) {
  auto client = std::make_unique<TestClient>();
  client->Initialize(prefs);
  return client;
}

}  // namespace

class AndroidMetricsServiceClientTest : public testing::Test {
 public:
  AndroidMetricsServiceClientTest()
      : test_begin_time_(base::Time::Now().ToTimeT()),
        task_runner_(new base::TestSimpleTaskRunner) {
    // Required by MetricsService.
    base::SetRecordActionTaskRunner(task_runner_);
  }

  const int64_t test_begin_time_;

 protected:
  ~AndroidMetricsServiceClientTest() override = default;

 private:
  content::BrowserTaskEnvironment task_environment_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(AndroidMetricsServiceClientTest);
};

TEST_F(AndroidMetricsServiceClientTest, TestSetConsentTrueBeforeInit) {
  auto prefs = CreateTestPrefs();
  auto client = std::make_unique<TestClient>();
  client->SetHaveMetricsConsent(true, true);
  client->Initialize(prefs.get());
  EXPECT_TRUE(client->IsRecordingActive());
  EXPECT_TRUE(prefs->HasPrefPath(metrics::prefs::kMetricsClientID));
  EXPECT_TRUE(
      prefs->HasPrefPath(metrics::prefs::kMetricsReportingEnabledTimestamp));
}

TEST_F(AndroidMetricsServiceClientTest, TestSetConsentFalseBeforeInit) {
  auto prefs = CreateTestPrefs();
  auto client = std::make_unique<TestClient>();
  client->SetHaveMetricsConsent(false, false);
  client->Initialize(prefs.get());
  EXPECT_FALSE(client->IsRecordingActive());
  EXPECT_FALSE(prefs->HasPrefPath(metrics::prefs::kMetricsClientID));
  EXPECT_FALSE(
      prefs->HasPrefPath(metrics::prefs::kMetricsReportingEnabledTimestamp));
}

TEST_F(AndroidMetricsServiceClientTest, TestSetConsentTrueAfterInit) {
  auto prefs = CreateTestPrefs();
  auto client = CreateAndInitTestClient(prefs.get());
  client->SetHaveMetricsConsent(true, true);
  EXPECT_TRUE(client->IsRecordingActive());
  EXPECT_TRUE(prefs->HasPrefPath(metrics::prefs::kMetricsClientID));
  EXPECT_GE(prefs->GetInt64(prefs::kMetricsReportingEnabledTimestamp),
            test_begin_time_);
}

TEST_F(AndroidMetricsServiceClientTest, TestSetConsentFalseAfterInit) {
  auto prefs = CreateTestPrefs();
  auto client = CreateAndInitTestClient(prefs.get());
  client->SetHaveMetricsConsent(false, false);
  EXPECT_FALSE(client->IsRecordingActive());
  EXPECT_FALSE(prefs->HasPrefPath(metrics::prefs::kMetricsClientID));
  EXPECT_FALSE(prefs->HasPrefPath(prefs::kMetricsReportingEnabledTimestamp));
}

// If there is already a valid client ID and enabled date, they should be
// reused.
TEST_F(AndroidMetricsServiceClientTest,
       TestKeepExistingClientIdAndEnabledDate) {
  auto prefs = CreateTestPrefs();
  prefs->SetString(metrics::prefs::kMetricsClientID, kTestClientId);
  int64_t enabled_date = 12345;
  prefs->SetInt64(metrics::prefs::kMetricsReportingEnabledTimestamp,
                  enabled_date);
  auto client = CreateAndInitTestClient(prefs.get());
  client->SetHaveMetricsConsent(true, true);
  EXPECT_TRUE(client->IsRecordingActive());
  EXPECT_TRUE(prefs->HasPrefPath(metrics::prefs::kMetricsClientID));
  EXPECT_EQ(kTestClientId, prefs->GetString(metrics::prefs::kMetricsClientID));
  EXPECT_EQ(enabled_date,
            prefs->GetInt64(metrics::prefs::kMetricsReportingEnabledTimestamp));
}

TEST_F(AndroidMetricsServiceClientTest,
       TestSetConsentFalseClearsIdAndEnabledDate) {
  auto prefs = CreateTestPrefs();
  prefs->SetString(metrics::prefs::kMetricsClientID, kTestClientId);
  auto client = CreateAndInitTestClient(prefs.get());
  client->SetHaveMetricsConsent(false, false);
  EXPECT_FALSE(client->IsRecordingActive());
  EXPECT_FALSE(prefs->HasPrefPath(metrics::prefs::kMetricsClientID));
  EXPECT_FALSE(
      prefs->HasPrefPath(metrics::prefs::kMetricsReportingEnabledTimestamp));
}

TEST_F(AndroidMetricsServiceClientTest,
       TestShouldNotUploadPackageName_AppType) {
  auto prefs = CreateTestPrefs();
  prefs->SetString(metrics::prefs::kMetricsClientID, kTestClientId);
  auto client = CreateAndInitTestClient(prefs.get());
  client->SetHaveMetricsConsent(true, true);
  client->SetRecordPackageNameForAppType(false);
  client->SetInPackageNameSample(true);
  std::string package_name = client->GetAppPackageName();
  EXPECT_TRUE(package_name.empty());
}

TEST_F(AndroidMetricsServiceClientTest,
       TestShouldNotUploadPackageName_SampledOut) {
  auto prefs = CreateTestPrefs();
  prefs->SetString(metrics::prefs::kMetricsClientID, kTestClientId);
  auto client = CreateAndInitTestClient(prefs.get());
  client->SetHaveMetricsConsent(true, true);
  client->SetRecordPackageNameForAppType(true);
  client->SetInPackageNameSample(false);
  std::string package_name = client->GetAppPackageName();
  EXPECT_TRUE(package_name.empty());
}

TEST_F(AndroidMetricsServiceClientTest, TestCanUploadPackageName) {
  auto prefs = CreateTestPrefs();
  prefs->SetString(metrics::prefs::kMetricsClientID, kTestClientId);
  auto client = CreateAndInitTestClient(prefs.get());
  client->SetHaveMetricsConsent(true, true);
  client->SetRecordPackageNameForAppType(true);
  client->SetInPackageNameSample(true);
  std::string package_name = client->GetAppPackageName();
  EXPECT_FALSE(package_name.empty());
}

TEST_F(AndroidMetricsServiceClientTest, TestGetPackageNameInternal) {
  auto prefs = CreateTestPrefs();
  prefs->SetString(metrics::prefs::kMetricsClientID, kTestClientId);
  auto client = CreateAndInitTestClient(prefs.get());
  // Make sure GetPackageNameInternal returns a non-empty string.
  EXPECT_FALSE(client->GetAppPackageNameInternal().empty());
}

TEST_F(AndroidMetricsServiceClientTest,
       TestPackageNameLogic_SampleRateBelowPackageNameRate) {
  auto prefs = CreateTestPrefs();
  prefs->SetString(metrics::prefs::kMetricsClientID, kTestClientId);
  auto client = CreateAndInitTestClient(prefs.get());
  client->SetSampleRatePerMille(80);
  client->SetPackageNameSamplePerMille(100);

  // When GetSampleRatePerMille() <= 100, everything in-sample should also be in
  // the package name sample.
  for (int value = 0; value < 80; value += 10) {
    client->SetSampleBucketValue(value);
    EXPECT_TRUE(client->IsInSample())
        << "Value " << value << " should be in-sample";
    EXPECT_TRUE(client->IsInPackageNameSample())
        << "Value " << value << " should be in the package name sample";
  }
  // After this, the only thing we care about is that we're out of sample (the
  // package name logic shouldn't matter at this point, because we won't upload
  // any records).
  for (int value = 80; value < 1000; value += 10) {
    client->SetSampleBucketValue(value);
    EXPECT_FALSE(client->IsInSample())
        << "Value " << value << " should be out of sample";
  }
}

TEST_F(AndroidMetricsServiceClientTest,
       TestPackageNameLogic_SampleRateAbovePackageNameRate) {
  auto prefs = CreateTestPrefs();
  prefs->SetString(metrics::prefs::kMetricsClientID, kTestClientId);
  auto client = CreateAndInitTestClient(prefs.get());
  client->SetSampleRatePerMille(900);
  client->SetPackageNameSamplePerMille(100);

  // When GetSampleRate() > 0.10, only values up to 0.10 should be in the
  // package name sample.
  for (int value = 0; value < 10; value += 10) {
    client->SetSampleBucketValue(value);
    EXPECT_TRUE(client->IsInSample())
        << "Value " << value << " should be in-sample";
    EXPECT_TRUE(client->IsInPackageNameSample())
        << "Value " << value << " should be in the package name sample";
  }
  // After this (but until we hit the sample rate), clients should be in sample
  // but not upload the package name.
  for (int value = 100; value < 900; value += 10) {
    client->SetSampleBucketValue(value);
    EXPECT_TRUE(client->IsInSample())
        << "Value " << value << " should be in-sample";
    EXPECT_FALSE(client->IsInPackageNameSample())
        << "Value " << value << " should be out of the package name sample";
  }
  // After this, the only thing we care about is that we're out of sample (the
  // package name logic shouldn't matter at this point, because we won't upload
  // any records).
  for (int value = 900; value < 1000; value += 10) {
    client->SetSampleBucketValue(value);
    EXPECT_FALSE(client->IsInSample())
        << "Value " << value << " should be out of sample";
  }
}

TEST_F(AndroidMetricsServiceClientTest, TestCanForceEnableMetrics) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      metrics::switches::kForceEnableMetricsReporting);

  auto prefs = CreateTestPrefs();
  auto client = std::make_unique<TestClient>();

  // Flag should have higher precedence than sampling or user consent (but not
  // app consent, so we set that to 'true' for this case).
  client->SetHaveMetricsConsent(false, /* app_consent */ true);
  client->SetInSample(false);
  client->Initialize(prefs.get());

  EXPECT_TRUE(client->IsReportingEnabled());
  EXPECT_TRUE(client->IsRecordingActive());
}

TEST_F(AndroidMetricsServiceClientTest,
       TestCanForceEnableMetricsIfAlreadyEnabled) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      metrics::switches::kForceEnableMetricsReporting);

  auto prefs = CreateTestPrefs();
  auto client = std::make_unique<TestClient>();

  // This is a sanity check: flip consent and sampling to true, just to make
  // sure the flag continues to work.
  client->SetHaveMetricsConsent(true, true);
  client->SetInSample(true);
  client->Initialize(prefs.get());

  EXPECT_TRUE(client->IsReportingEnabled());
  EXPECT_TRUE(client->IsRecordingActive());
}

TEST_F(AndroidMetricsServiceClientTest,
       TestCannotForceEnableMetricsIfAppOptsOut) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      metrics::switches::kForceEnableMetricsReporting);

  auto prefs = CreateTestPrefs();
  auto client = std::make_unique<TestClient>();

  // Even with the flag, app consent should be respected.
  client->SetHaveMetricsConsent(true, /* app_consent */ false);
  client->SetInSample(true);
  client->Initialize(prefs.get());

  EXPECT_FALSE(client->IsReportingEnabled());
  EXPECT_FALSE(client->IsRecordingActive());
}

}  // namespace metrics
