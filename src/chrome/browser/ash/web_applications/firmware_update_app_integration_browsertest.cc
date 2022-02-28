// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "ash/constants/ash_features.h"
#include "ash/webui/firmware_update_ui/url_constants.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/web_applications/system_web_app_integration_test.h"
#include "chrome/browser/ui/web_applications/system_web_app_ui_utils.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_base_features.h"
#include "url/gurl.h"

class FirmwareUpdateAppIntegrationTest : public SystemWebAppIntegrationTest {
 public:
  FirmwareUpdateAppIntegrationTest() {
    scoped_feature_list_.InitWithFeatures(
        {chromeos::features::kFirmwareUpdaterApp}, {});
  }

 protected:
  base::HistogramTester histogram_tester_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Test that the Firmware Update App installs and launches correctly by
// running some spot checks on the manifest.
IN_PROC_BROWSER_TEST_P(FirmwareUpdateAppIntegrationTest,
                       FirmwareUpdateAppValid) {
  const GURL url(ash::kChromeUIFirmwareUpdateAppURL);
  EXPECT_NO_FATAL_FAILURE(ExpectSystemWebAppValid(
      web_app::SystemAppType::FIRMWARE_UPDATE, url, "Firmware Updates"));

  histogram_tester_.ExpectBucketCount(
      "Webapp.InstallResult.System.Apps.FirmwareUpdate",
      web_app::InstallResultCode::kSuccessOfflineOnlyInstall, 1);
}

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    FirmwareUpdateAppIntegrationTest);
