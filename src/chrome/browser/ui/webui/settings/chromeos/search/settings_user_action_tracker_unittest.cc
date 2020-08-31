// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/search/settings_user_action_tracker.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace settings {

class SettingsUserActionTrackerTest : public testing::Test {
 protected:
  SettingsUserActionTrackerTest() = default;
  ~SettingsUserActionTrackerTest() override = default;

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::HistogramTester histogram_tester_;
  SettingsUserActionTracker tracker_;
};

TEST_F(SettingsUserActionTrackerTest, TestRecordMetrics) {
  // Focus the page, perform some tasks, and change a setting.
  tracker_.RecordPageFocus();
  tracker_.RecordClick();
  tracker_.RecordNavigation();
  tracker_.RecordSearch();
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(10));
  tracker_.RecordSettingChange();

  // The "first change" metrics should have been logged.
  histogram_tester_.ExpectTotalCount(
      "ChromeOS.Settings.NumClicksUntilChange.FirstChange",
      /*count=*/1);
  histogram_tester_.ExpectTotalCount(
      "ChromeOS.Settings.NumNavigationsUntilChange.FirstChange",
      /*count=*/1);
  histogram_tester_.ExpectTotalCount(
      "ChromeOS.Settings.NumSearchesUntilChange.FirstChange",
      /*count=*/1);
  histogram_tester_.ExpectTimeBucketCount(
      "ChromeOS.Settings.TimeUntilChange.FirstChange",
      /*sample=*/base::TimeDelta::FromSeconds(10),
      /*count=*/1);

  // Without leaving the page, perform some more tasks, and change another
  // setting.
  tracker_.RecordClick();
  tracker_.RecordNavigation();
  tracker_.RecordSearch();
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(10));
  tracker_.RecordSettingChange();

  // The "subsequent change" metrics should have been logged.
  histogram_tester_.ExpectTotalCount(
      "ChromeOS.Settings.NumClicksUntilChange.SubsequentChange",
      /*count=*/1);
  histogram_tester_.ExpectTotalCount(
      "ChromeOS.Settings.NumNavigationsUntilChange.SubsequentChange",
      /*count=*/1);
  histogram_tester_.ExpectTotalCount(
      "ChromeOS.Settings.NumSearchesUntilChange.SubsequentChange",
      /*count=*/1);
  histogram_tester_.ExpectTimeBucketCount(
      "ChromeOS.Settings.TimeUntilChange.SubsequentChange",
      /*sample=*/base::TimeDelta::FromSeconds(10),
      /*count=*/1);

  // Repeat this, but only after 100ms. This is lower than the minimum value
  // required for this metric, so it should be ignored.
  tracker_.RecordClick();
  tracker_.RecordNavigation();
  tracker_.RecordSearch();
  task_environment_.FastForwardBy(base::TimeDelta::FromMilliseconds(100));
  tracker_.RecordSettingChange();

  // No additional logging should have occurred, so make the same verifications
  // as above.
  histogram_tester_.ExpectTotalCount(
      "ChromeOS.Settings.NumClicksUntilChange.SubsequentChange",
      /*count=*/1);
  histogram_tester_.ExpectTotalCount(
      "ChromeOS.Settings.NumNavigationsUntilChange.SubsequentChange",
      /*count=*/1);
  histogram_tester_.ExpectTotalCount(
      "ChromeOS.Settings.NumSearchesUntilChange.SubsequentChange",
      /*count=*/1);
  histogram_tester_.ExpectTimeBucketCount(
      "ChromeOS.Settings.TimeUntilChange.SubsequentChange",
      /*sample=*/base::TimeDelta::FromSeconds(10),
      /*count=*/1);

  // Repeat this once more, and verify that the counts increased.
  tracker_.RecordClick();
  tracker_.RecordNavigation();
  tracker_.RecordSearch();
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(10));
  tracker_.RecordSettingChange();

  // The "subsequent change" metrics should have been logged.
  histogram_tester_.ExpectTotalCount(
      "ChromeOS.Settings.NumClicksUntilChange.SubsequentChange",
      /*count=*/2);
  histogram_tester_.ExpectTotalCount(
      "ChromeOS.Settings.NumNavigationsUntilChange.SubsequentChange",
      /*count=*/2);
  histogram_tester_.ExpectTotalCount(
      "ChromeOS.Settings.NumSearchesUntilChange.SubsequentChange",
      /*count=*/2);
  histogram_tester_.ExpectTimeBucketCount(
      "ChromeOS.Settings.TimeUntilChange.SubsequentChange",
      /*sample=*/base::TimeDelta::FromSeconds(10),
      /*count=*/2);
}

TEST_F(SettingsUserActionTrackerTest, TestBlurAndFocus) {
  // Focus the page, click, and change a setting.
  tracker_.RecordPageFocus();
  tracker_.RecordClick();
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));
  tracker_.RecordSettingChange();
  histogram_tester_.ExpectTotalCount(
      "ChromeOS.Settings.NumClicksUntilChange.FirstChange",
      /*count=*/1);
  histogram_tester_.ExpectTimeBucketCount(
      "ChromeOS.Settings.TimeUntilChange.FirstChange",
      /*sample=*/base::TimeDelta::FromSeconds(1),
      /*count=*/1);

  // Blur for 59 seconds (not quite a minute), click, and change a setting.
  // Since the blur was under a minute, this should count for the "subsequent
  // change" metrics.
  tracker_.RecordPageBlur();
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(59));
  tracker_.RecordPageFocus();
  tracker_.RecordClick();
  tracker_.RecordSettingChange();
  histogram_tester_.ExpectTimeBucketCount(
      "ChromeOS.Settings.BlurredWindowDuration",
      /*sample=*/base::TimeDelta::FromSeconds(59),
      /*count=*/1);
  histogram_tester_.ExpectTotalCount(
      "ChromeOS.Settings.NumClicksUntilChange.SubsequentChange",
      /*count=*/1);
  histogram_tester_.ExpectTimeBucketCount(
      "ChromeOS.Settings.TimeUntilChange.SubsequentChange",
      /*sample=*/base::TimeDelta::FromSeconds(59),
      /*count=*/1);

  // Now, blur for a full minute, click, and change a setting. Since the blur
  // was a full minute, this should count for the "first change" metrics.
  tracker_.RecordPageBlur();
  task_environment_.FastForwardBy(base::TimeDelta::FromMinutes(1));
  tracker_.RecordPageFocus();
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(5));
  tracker_.RecordClick();
  tracker_.RecordSettingChange();
  histogram_tester_.ExpectTimeBucketCount(
      "ChromeOS.Settings.BlurredWindowDuration",
      /*sample=*/base::TimeDelta::FromMinutes(1),
      /*count=*/2);
  histogram_tester_.ExpectTotalCount(
      "ChromeOS.Settings.NumClicksUntilChange.FirstChange",
      /*count=*/2);
  histogram_tester_.ExpectTimeBucketCount(
      "ChromeOS.Settings.TimeUntilChange.FirstChange",
      /*sample=*/base::TimeDelta::FromSeconds(5),
      /*count=*/1);
}

}  // namespace settings.
}  // namespace chromeos.
