// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/search/settings_user_action_tracker.h"

#include "base/metrics/histogram_functions.h"

namespace chromeos {
namespace settings {

namespace {

// The maximum amount of time that the settings window can be blurred to be
// considered short enough for the "first change" metric.
constexpr base::TimeDelta kShortBlurTimeLimit = base::TimeDelta::FromMinutes(1);

// The minimum amount of time between a setting change and a subsequent setting
// change. If two changes occur les than this amount of time from each other,
// they are ignored by metrics. See https://crbug.com/1073714 for details.
constexpr base::TimeDelta kMinSubsequentChange =
    base::TimeDelta::FromMilliseconds(200);

// Min/max values for the duration metrics. Note that these values are tied to
// the metrics defined below; if these ever change, the metric names must also
// be updated.
constexpr base::TimeDelta kMinDurationMetric =
    base::TimeDelta::FromMilliseconds(100);
constexpr base::TimeDelta kMaxDurationMetric = base::TimeDelta::FromMinutes(10);

void LogDurationMetric(const char* metric_name, base::TimeDelta duration) {
  base::UmaHistogramCustomTimes(metric_name, duration, kMinDurationMetric,
                                kMaxDurationMetric, /*buckets=*/50);
}

}  // namespace

SettingsUserActionTracker::SettingsUserActionTracker(
    mojo::PendingReceiver<mojom::UserActionRecorder> pending_receiver)
    : SettingsUserActionTracker() {
  receiver_.Bind(std::move(pending_receiver));
}

SettingsUserActionTracker::SettingsUserActionTracker()
    : metric_start_time_(base::TimeTicks::Now()) {}

SettingsUserActionTracker::~SettingsUserActionTracker() = default;

void SettingsUserActionTracker::RecordPageFocus() {
  if (last_blur_timestamp_.is_null())
    return;

  // Log the duration of being blurred.
  const base::TimeDelta blurred_duration =
      base::TimeTicks::Now() - last_blur_timestamp_;
  LogDurationMetric("ChromeOS.Settings.BlurredWindowDuration",
                    blurred_duration);

  // If the window was blurred for more than |kShortBlurTimeLimit|,
  // the user was away from the window for long enough that we consider the
  // user coming back to the window a new session for the purpose of metrics.
  if (blurred_duration >= kShortBlurTimeLimit) {
    ResetMetricsCountersAndTimestamp();
    last_record_setting_changed_timestamp_ = base::TimeTicks();
  }
}

void SettingsUserActionTracker::RecordPageBlur() {
  last_blur_timestamp_ = base::TimeTicks::Now();
}

void SettingsUserActionTracker::RecordClick() {
  ++num_clicks_since_start_time_;
}

void SettingsUserActionTracker::RecordNavigation() {
  ++num_navigations_since_start_time_;
}

void SettingsUserActionTracker::RecordSearch() {
  ++num_searches_since_start_time_;
}

void SettingsUserActionTracker::RecordSettingChange() {
  base::TimeTicks now = base::TimeTicks::Now();

  if (!last_record_setting_changed_timestamp_.is_null()) {
    // If it has been less than |kMinSubsequentChange| since the last recorded
    // setting change, this change is discarded. See https://crbug.com/1073714
    // for details.
    if (now - last_record_setting_changed_timestamp_ < kMinSubsequentChange)
      return;

    base::UmaHistogramCounts1000(
        "ChromeOS.Settings.NumClicksUntilChange.SubsequentChange",
        num_clicks_since_start_time_);
    base::UmaHistogramCounts1000(
        "ChromeOS.Settings.NumNavigationsUntilChange.SubsequentChange",
        num_navigations_since_start_time_);
    base::UmaHistogramCounts1000(
        "ChromeOS.Settings.NumSearchesUntilChange.SubsequentChange",
        num_searches_since_start_time_);
    LogDurationMetric("ChromeOS.Settings.TimeUntilChange.SubsequentChange",
                      now - metric_start_time_);
  } else {
    base::UmaHistogramCounts1000(
        "ChromeOS.Settings.NumClicksUntilChange.FirstChange",
        num_clicks_since_start_time_);
    base::UmaHistogramCounts1000(
        "ChromeOS.Settings.NumNavigationsUntilChange.FirstChange",
        num_navigations_since_start_time_);
    base::UmaHistogramCounts1000(
        "ChromeOS.Settings.NumSearchesUntilChange.FirstChange",
        num_searches_since_start_time_);
    LogDurationMetric("ChromeOS.Settings.TimeUntilChange.FirstChange",
                      now - metric_start_time_);
  }

  ResetMetricsCountersAndTimestamp();
  last_record_setting_changed_timestamp_ = now;
}

void SettingsUserActionTracker::ResetMetricsCountersAndTimestamp() {
  metric_start_time_ = base::TimeTicks::Now();
  num_clicks_since_start_time_ = 0u;
  num_navigations_since_start_time_ = 0u;
  num_searches_since_start_time_ = 0u;
}

}  // namespace settings
}  // namespace chromeos
