// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/intent_helper/metrics/intent_handling_metrics.h"

#include "base/metrics/histogram_macros.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/apps/intent_helper/apps_navigation_types.h"
#include "chrome/browser/apps/intent_helper/chromeos_intent_picker_helpers.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/components/arc/arc_prefs.h"
#include "ash/components/arc/metrics/arc_metrics_service.h"
#include "ash/components/arc/metrics/stability_metrics_manager.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "chrome/browser/ash/arc/intent_helper/arc_external_protocol_dialog.h"
#include "components/arc/intent_helper/arc_intent_helper_bridge.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace apps {

TEST(IntentHandlingMetricsTest, TestRecordIntentPickerMetrics) {
  struct TestCase {
    PickerEntryType entry_type;
    IntentPickerCloseReason close_reason;
    bool should_persist;

    IntentHandlingMetrics::IntentPickerAction expected_action;
    IntentHandlingMetrics::Platform expected_platform;
  };

  const TestCase kTestCases[]{
      // Open in ARC:
      {PickerEntryType::kArc, IntentPickerCloseReason::OPEN_APP, true,
       IntentHandlingMetrics::IntentPickerAction::kArcAppSelectedAndPreferred,
       IntentHandlingMetrics::Platform::ARC},

      {PickerEntryType::kArc, IntentPickerCloseReason::OPEN_APP, false,
       IntentHandlingMetrics::IntentPickerAction::kArcAppSelected,
       IntentHandlingMetrics::Platform::ARC},

      // Open in PWA:
      {PickerEntryType::kWeb, IntentPickerCloseReason::OPEN_APP, true,
       IntentHandlingMetrics::IntentPickerAction::kPwaSelectedAndPreferred,
       IntentHandlingMetrics::Platform::PWA},

      {PickerEntryType::kWeb, IntentPickerCloseReason::OPEN_APP, false,
       IntentHandlingMetrics::IntentPickerAction::kPwaSelected,
       IntentHandlingMetrics::Platform::PWA},

      // Stay in Chrome:
      {PickerEntryType::kWeb, IntentPickerCloseReason::STAY_IN_CHROME, true,
       IntentHandlingMetrics::IntentPickerAction::kChromeSelectedAndPreferred,
       IntentHandlingMetrics::Platform::CHROME},

      {PickerEntryType::kWeb, IntentPickerCloseReason::STAY_IN_CHROME, false,
       IntentHandlingMetrics::IntentPickerAction::kChromeSelected,
       IntentHandlingMetrics::Platform::CHROME},

      // Dismiss/error:
      {PickerEntryType::kWeb, IntentPickerCloseReason::DIALOG_DEACTIVATED, true,
       IntentHandlingMetrics::IntentPickerAction::kDialogDeactivated,
       IntentHandlingMetrics::Platform::CHROME},

      {PickerEntryType::kWeb, IntentPickerCloseReason::ERROR_AFTER_PICKER, true,
       IntentHandlingMetrics::IntentPickerAction::kError,
       IntentHandlingMetrics::Platform::CHROME},
  };

  for (const auto& test : kTestCases) {
    base::HistogramTester histogram_tester;

    IntentHandlingMetrics::RecordIntentPickerMetrics(
        test.entry_type, test.close_reason, test.should_persist,
        PickerShowState::kOmnibox);

    histogram_tester.ExpectBucketCount("ChromeOS.Intents.IntentPickerAction",
                                       test.expected_action, 1);
    histogram_tester.ExpectBucketCount(
        "ChromeOS.Apps.IntentPickerDestinationPlatform", test.expected_platform,
        1);
  }
}

TEST(IntentHandlingMetricsTest, TestRecordIntentPickerMetricsWithSource) {
  base::HistogramTester histogram_tester;

  IntentHandlingMetrics::RecordIntentPickerMetrics(
      PickerEntryType::kArc, IntentPickerCloseReason::OPEN_APP, true,
      PickerShowState::kOmnibox);

  histogram_tester.ExpectBucketCount(
      "ChromeOS.Intents.IntentPickerAction",
      IntentHandlingMetrics::IntentPickerAction::kArcAppSelectedAndPreferred,
      1);
  histogram_tester.ExpectBucketCount(
      "ChromeOS.Intents.IntentPickerAction.FromOmniboxIcon",
      IntentHandlingMetrics::IntentPickerAction::kArcAppSelectedAndPreferred,
      1);

  IntentHandlingMetrics::RecordIntentPickerMetrics(
      PickerEntryType::kArc, IntentPickerCloseReason::OPEN_APP, true,
      PickerShowState::kPopOut);

  histogram_tester.ExpectBucketCount(
      "ChromeOS.Intents.IntentPickerAction",
      IntentHandlingMetrics::IntentPickerAction::kArcAppSelectedAndPreferred,
      2);
  histogram_tester.ExpectBucketCount(
      "ChromeOS.Intents.IntentPickerAction.FromAutoPopOut",
      IntentHandlingMetrics::IntentPickerAction::kArcAppSelectedAndPreferred,
      1);
}

TEST(IntentHandlingMetricsTest, TestRecordPreferredAppLinkClickMetrics) {
  base::HistogramTester histogram_tester;

  IntentHandlingMetrics::RecordPreferredAppLinkClickMetrics(
      IntentHandlingMetrics::Platform::ARC);

  histogram_tester.ExpectBucketCount(
      "ChromeOS.Apps.IntentPickerDestinationPlatform",
      IntentHandlingMetrics::Platform::ARC, 1);
}

TEST(IntentHandlingMetricsTest, TestRecordOpenBrowserMetrics) {
  base::HistogramTester histogram_tester;

  IntentHandlingMetrics test;
  test.RecordOpenBrowserMetrics(IntentHandlingMetrics::AppType::kArc);

  histogram_tester.ExpectBucketCount("ChromeOS.Apps.OpenBrowser",
                                     IntentHandlingMetrics::AppType::kArc, 1);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)

// A fixture class that sets up arc::ArcMetricsService.
class IntentHandlingMetricsTestWithMetricsService : public testing::Test {
 protected:
  IntentHandlingMetricsTestWithMetricsService() = default;
  IntentHandlingMetricsTestWithMetricsService(
      const IntentHandlingMetricsTestWithMetricsService&) = delete;
  IntentHandlingMetricsTestWithMetricsService& operator=(
      const IntentHandlingMetricsTestWithMetricsService&) = delete;
  ~IntentHandlingMetricsTestWithMetricsService() override = default;

  void SetUp() override {
    arc::prefs::RegisterLocalStatePrefs(local_state_.registry());
    arc::StabilityMetricsManager::Initialize(&local_state_);
    arc::ArcMetricsService::GetForBrowserContextForTesting(profile());
  }

  Profile* profile() { return &profile_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  arc::ArcServiceManager arc_service_manager_;
  TestingPrefServiceSimple local_state_;
  TestingProfile profile_;
};

TEST_F(IntentHandlingMetricsTestWithMetricsService,
       TestRecordExternalProtocolUserInteractionMetrics) {
  struct TestCase {
    PickerEntryType entry_type;
    IntentPickerCloseReason close_reason;
    bool should_persist;

    IntentHandlingMetrics::PickerAction expected_action;
    bool expected_link_click;
  };

  const TestCase kTestCases[]{
      // Arc apps:
      {PickerEntryType::kArc, IntentPickerCloseReason::OPEN_APP,
       /*should_persist=*/true,
       IntentHandlingMetrics::PickerAction::ARC_APP_PREFERRED_PRESSED,
       /*expected_link_click=*/true},

      {PickerEntryType::kArc, IntentPickerCloseReason::OPEN_APP,
       /*should_persist=*/false,
       IntentHandlingMetrics::PickerAction::ARC_APP_PRESSED,
       /*expected_link_click=*/true},

      {PickerEntryType::kArc, IntentPickerCloseReason::PREFERRED_APP_FOUND,
       /*should_persist=*/false,
       IntentHandlingMetrics::PickerAction::PREFERRED_ARC_ACTIVITY_FOUND,
       /*expected_link_click=*/true},

      // Device:
      {PickerEntryType::kDevice, IntentPickerCloseReason::OPEN_APP,
       /*should_persist=*/false,
       IntentHandlingMetrics::PickerAction::DEVICE_PRESSED,
       /*expected_link_click=*/false},

      // Dismiss:
      {PickerEntryType::kArc, IntentPickerCloseReason::DIALOG_DEACTIVATED,
       /*should_persist=*/false,
       IntentHandlingMetrics::PickerAction::DIALOG_DEACTIVATED,
       /*expected_link_click=*/false},
  };

  for (const auto& test : kTestCases) {
    base::HistogramTester histogram_tester;

    IntentHandlingMetrics::RecordExternalProtocolUserInteractionMetrics(
        profile(), test.entry_type, test.close_reason, test.should_persist);

    histogram_tester.ExpectBucketCount(
        "Arc.UserInteraction", arc::UserInteractionType::APP_STARTED_FROM_LINK,
        test.expected_link_click);

    histogram_tester.ExpectBucketCount("ChromeOS.Apps.ExternalProtocolDialog",
                                       test.expected_action, 1);
  }
}

TEST(IntentHandlingMetricsTest, TestRecordExternalProtocolMetrics) {
  base::HistogramTester histogram_tester;

  IntentHandlingMetrics test = IntentHandlingMetrics();
  test.RecordExternalProtocolMetrics(arc::Scheme::IRC, PickerEntryType::kArc,
                                     /*accepted=*/true, /*persisted=*/true);

  histogram_tester.ExpectBucketCount(
      "ChromeOS.Apps.ExternalProtocolDialog.Accepted",
      arc::ProtocolAction::IRC_ACCEPTED_PERSISTED, 1);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace apps
