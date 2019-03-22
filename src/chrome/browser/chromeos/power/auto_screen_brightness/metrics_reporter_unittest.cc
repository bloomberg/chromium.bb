// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/auto_screen_brightness/metrics_reporter.h"

#include <memory>

#include "base/macros.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_task_environment.h"
#include "chrome/common/pref_names.h"
#include "chromeos/dbus/fake_power_manager_client.h"
#include "components/metrics/daily_event.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace power {
namespace auto_screen_brightness {

namespace {

constexpr auto kNoAls = MetricsReporter::UserAdjustment::kNoAls;
constexpr auto kSupportedAls = MetricsReporter::UserAdjustment::kSupportedAls;
constexpr auto kUnsupportedAls =
    MetricsReporter::UserAdjustment::kUnsupportedAls;

}  // namespace

class MetricsReporterTest : public testing::Test {
 public:
  MetricsReporterTest() {
    MetricsReporter::RegisterLocalStatePrefs(pref_service_.registry());
    ResetReporter();
  }

 protected:
  // Reinitialize |reporter_| without resetting underlying prefs. May be called
  // by tests to simulate a Chrome restart.
  void ResetReporter() {
    reporter_ = std::make_unique<MetricsReporter>(&power_manager_client_,
                                                  &pref_service_);
  }

  // Notifies |reporter_| that a user adjustment request is received.
  void SendOnUserBrightnessChangeRequested(
      MetricsReporter::UserAdjustment user_adjustment) {
    reporter_->OnUserBrightnessChangeRequested(user_adjustment);
  }

  // Instructs |reporter_| to report daily metrics for reason |type|.
  void TriggerDailyEvent(metrics::DailyEvent::IntervalType type) {
    reporter_->ReportDailyMetricsForTesting(type);
  }

  // Instructs |reporter_| to report daily metrics due to the passage of a day
  // and verifies that it reports one sample with each of the passed values.
  void TriggerDailyEventAndVerifyHistograms(int no_als_count,
                                            int supported_als_count,
                                            int unsupported_als_count) {
    base::HistogramTester histogram_tester;

    TriggerDailyEvent(metrics::DailyEvent::IntervalType::DAY_ELAPSED);
    histogram_tester.ExpectUniqueSample(
        MetricsReporter::kNoAlsUserAdjustmentName, no_als_count, 1);
    histogram_tester.ExpectUniqueSample(
        MetricsReporter::kSupportedAlsUserAdjustmentName, supported_als_count,
        1);
    histogram_tester.ExpectUniqueSample(
        MetricsReporter::kUnsupportedAlsUserAdjustmentName,
        unsupported_als_count, 1);
  }

  base::test::ScopedTaskEnvironment scoped_task_environment_;
  FakePowerManagerClient power_manager_client_;
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<MetricsReporter> reporter_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MetricsReporterTest);
};

TEST_F(MetricsReporterTest, CountAndReportEvents) {
  // Report the following user adjustments:
  // - 3 without ALS
  // - 2 with supported ALS
  // No user adjustment on unsupported ALS is reported.
  SendOnUserBrightnessChangeRequested(kNoAls);
  SendOnUserBrightnessChangeRequested(kSupportedAls);
  SendOnUserBrightnessChangeRequested(kNoAls);
  SendOnUserBrightnessChangeRequested(kNoAls);
  SendOnUserBrightnessChangeRequested(kSupportedAls);
  TriggerDailyEventAndVerifyHistograms(3, 2, 0);

  // The next day, the following user adjustments:
  // - 1 without ALS
  // - 1 with supported
  // - 3 with unsupported ALS
  SendOnUserBrightnessChangeRequested(kUnsupportedAls);
  SendOnUserBrightnessChangeRequested(kNoAls);
  SendOnUserBrightnessChangeRequested(kUnsupportedAls);
  SendOnUserBrightnessChangeRequested(kUnsupportedAls);
  SendOnUserBrightnessChangeRequested(kSupportedAls);
  TriggerDailyEventAndVerifyHistograms(1, 1, 3);

  // The next day, no user adjustment is reported.
  TriggerDailyEventAndVerifyHistograms(0, 0, 0);
}

TEST_F(MetricsReporterTest, LoadInitialCountsFromPrefs) {
  // Create a new reporter and check that it loads its initial event counts from
  // prefs.
  pref_service_.SetInteger(
      prefs::kAutoScreenBrightnessMetricsNoAlsUserAdjustmentCount, 1);
  pref_service_.SetInteger(
      prefs::kAutoScreenBrightnessMetricsSupportedAlsUserAdjustmentCount, 2);
  ResetReporter();
  TriggerDailyEventAndVerifyHistograms(1, 2, 0);

  // The previous report should've cleared the prefs, so a new reporter should
  // start out at zero.
  ResetReporter();
  TriggerDailyEventAndVerifyHistograms(0, 0, 0);
}

TEST_F(MetricsReporterTest, IgnoreDailyEventFirstRun) {
  // metrics::DailyEvent notifies observers immediately on first run. Histograms
  // shouldn't be sent in this case.
  base::HistogramTester tester;
  TriggerDailyEvent(metrics::DailyEvent::IntervalType::FIRST_RUN);
  tester.ExpectTotalCount(MetricsReporter::kNoAlsUserAdjustmentName, 0);
  tester.ExpectTotalCount(MetricsReporter::kSupportedAlsUserAdjustmentName, 0);
  tester.ExpectTotalCount(MetricsReporter::kUnsupportedAlsUserAdjustmentName,
                          0);
}

TEST_F(MetricsReporterTest, IgnoreDailyEventClockChanged) {
  SendOnUserBrightnessChangeRequested(kSupportedAls);

  // metrics::DailyEvent notifies observers if it sees that the system clock has
  // jumped back. Histograms shouldn't be sent in this case.
  base::HistogramTester tester;
  TriggerDailyEvent(metrics::DailyEvent::IntervalType::CLOCK_CHANGED);
  tester.ExpectTotalCount(MetricsReporter::kNoAlsUserAdjustmentName, 0);
  tester.ExpectTotalCount(MetricsReporter::kSupportedAlsUserAdjustmentName, 0);
  tester.ExpectTotalCount(MetricsReporter::kUnsupportedAlsUserAdjustmentName,
                          0);

  // The existing stats should be cleared when the clock change notification is
  // received, so the next report should only contain zeros.
  TriggerDailyEventAndVerifyHistograms(0, 0, 0);
}

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace chromeos
