// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/telemetry/android/android_telemetry_service.h"

#include <memory>

#include "base/task/post_task.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_service.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/download/public/common/mock_download_item.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "components/safe_browsing/features.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

class AndroidTelemetryServiceTest : public testing::Test {
 public:
  AndroidTelemetryServiceTest() = default;

 protected:
  Profile* profile() { return profile_.get(); }

  void SetUp() override {
    browser_process_ = TestingBrowserProcess::GetGlobal();

    system_request_context_getter_ =
        base::MakeRefCounted<net::TestURLRequestContextGetter>(
            base::CreateSingleThreadTaskRunnerWithTraits(
                {content::BrowserThread::IO}));
    browser_process_->SetSystemRequestContext(
        system_request_context_getter_.get());
    sb_service_ =
        safe_browsing::SafeBrowsingService::CreateSafeBrowsingService();
    browser_process_->SetSafeBrowsingService(sb_service_.get());
    sb_service_->Initialize();
    base::RunLoop().RunUntilIdle();

    download_item_.reset(new ::testing::NiceMock<download::MockDownloadItem>());
    profile_.reset(new TestingProfile());

    telemetry_service_ =
        std::make_unique<AndroidTelemetryService>(sb_service_.get(), profile());
  }

  void TearDown() override {}

  bool CanSendPing(download::DownloadItem* item) {
    return telemetry_service_->CanSendPing(item);
  }

  void SetOffTheRecordProfile() {
    telemetry_service_->profile_ = profile()->GetOffTheRecordProfile();
  }

  void ResetProfile() { telemetry_service_->profile_ = profile(); }

 protected:
  content::TestBrowserThreadBundle test_browser_thread_bundle_;
  scoped_refptr<safe_browsing::SafeBrowsingService> sb_service_;
  std::unique_ptr<download::MockDownloadItem> download_item_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<AndroidTelemetryService> telemetry_service_;
  TestingBrowserProcess* browser_process_;
  scoped_refptr<net::URLRequestContextGetter> system_request_context_getter_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(AndroidTelemetryServiceTest, CantSendPing_NonApk) {
  ON_CALL(*download_item_, GetMimeType())
      .WillByDefault(testing::Return("text/plain"));
  EXPECT_FALSE(CanSendPing(download_item_.get()));
}

TEST_F(AndroidTelemetryServiceTest, CantSendPing_SafeBrowsingDisabled) {
  // Disable Safe Browsing.
  profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled, false);

  // Enable Scout Reporting.
  profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingScoutReportingEnabled,
                                    true);
  // Enable feature.
  scoped_feature_list_.InitAndEnableFeature(kTelemetryForApkDownloads);
  // Simulate APK download.
  ON_CALL(*download_item_, GetMimeType())
      .WillByDefault(
          testing::Return("application/vnd.android.package-archive"));

  EXPECT_FALSE(CanSendPing(download_item_.get()));
}

TEST_F(AndroidTelemetryServiceTest, CantSendPing_IncognitoMode) {
  // No event is triggered if in incognito mode..
  SetOffTheRecordProfile();

  // Enable Safe Browsing.
  profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled, true);
  // Enable Scout Reporting.
  profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingScoutReportingEnabled,
                                    true);
  // Enable feature.
  scoped_feature_list_.InitAndEnableFeature(kTelemetryForApkDownloads);
  // Simulate APK download.
  ON_CALL(*download_item_, GetMimeType())
      .WillByDefault(
          testing::Return("application/vnd.android.package-archive"));

  EXPECT_FALSE(CanSendPing(download_item_.get()));

  ResetProfile();
}

TEST_F(AndroidTelemetryServiceTest, CantSendPing_SBERDisabled) {
  // Disable Scout Reporting.
  profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingScoutReportingEnabled,
                                    false);

  // Enable Safe Browsing.
  profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled, true);
  // Enable feature.
  scoped_feature_list_.InitAndEnableFeature(kTelemetryForApkDownloads);
  // Simulate APK download.
  ON_CALL(*download_item_, GetMimeType())
      .WillByDefault(
          testing::Return("application/vnd.android.package-archive"));

  EXPECT_FALSE(CanSendPing(download_item_.get()));
}

TEST_F(AndroidTelemetryServiceTest, CantSendPing_FeatureDisabled) {
  // Disable feature.
  scoped_feature_list_.InitAndDisableFeature(kTelemetryForApkDownloads);

  // Enable Safe Browsing.
  profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled, true);
  // Enable Scout Reporting.
  profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingScoutReportingEnabled,
                                    true);
  // Simulate APK download.
  ON_CALL(*download_item_, GetMimeType())
      .WillByDefault(
          testing::Return("application/vnd.android.package-archive"));

  EXPECT_FALSE(CanSendPing(download_item_.get()));
}

TEST_F(AndroidTelemetryServiceTest, CanSendPing_AllConditionsMet) {
  // Enable Safe Browsing.
  profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled, true);
  // Enable Scout Reporting.
  profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingScoutReportingEnabled,
                                    true);
  // Enable feature.
  scoped_feature_list_.InitAndEnableFeature(kTelemetryForApkDownloads);
  // Simulate APK download.
  ON_CALL(*download_item_, GetMimeType())
      .WillByDefault(
          testing::Return("application/vnd.android.package-archive"));

  // The ping should be sent.
  EXPECT_TRUE(CanSendPing(download_item_.get()));
}

}  // namespace safe_browsing
