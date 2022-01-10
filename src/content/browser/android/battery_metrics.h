// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_BATTERY_METRICS_H_
#define CONTENT_BROWSER_ANDROID_BATTERY_METRICS_H_

#include "base/android/radio_utils.h"
#include "base/no_destructor.h"
#include "base/power_monitor/power_observer.h"
#include "base/sequence_checker.h"
#include "base/timer/timer.h"
#include "content/common/process_visibility_tracker.h"

namespace content {

// Records metrics around battery usage on Android. The metrics are only tracked
// while the device is not charging and the app is visible. This class is not
// thread-safe.
class AndroidBatteryMetrics
    : public base::PowerStateObserver,
      public ProcessVisibilityTracker::ProcessVisibilityObserver {
 public:
  static AndroidBatteryMetrics* GetInstance();

  AndroidBatteryMetrics(const AndroidBatteryMetrics&) = delete;
  AndroidBatteryMetrics& operator=(const AndroidBatteryMetrics&) = delete;

  // ProcessVisibilityTracker::ProcessVisibilityObserver implementation:
  void OnVisibilityChanged(bool visible) override;

 private:
  friend class base::NoDestructor<AndroidBatteryMetrics>;
  AndroidBatteryMetrics();
  ~AndroidBatteryMetrics() override;

  // base::PowerStateObserver implementation:
  void OnPowerStateChange(bool on_battery_power) override;

  void UpdateMetricsEnabled();
  void CaptureAndReportMetrics(bool disabling);
  void UpdateAndReportRadio();
  void MonitorRadioState();

  // Whether or not we've seen at least two consecutive capacity drops while
  // the embedding app was visible. Battery drain reported prior to this could
  // be caused by a different app.
  bool IsMeasuringDrainExclusively() const;

  // Battery drain is captured and reported periodically in this interval while
  // the device is on battery power and the app is visible.
  static constexpr base::TimeDelta kMetricsInterval = base::Seconds(30);

  // Radio state is polled with this interval to count radio wakeups.
  static constexpr base::TimeDelta kRadioStateInterval = base::Seconds(1);

  bool app_visible_;
  bool on_battery_power_;
  int last_remaining_capacity_uah_ = 0;
  int64_t last_tx_bytes_ = -1;
  int64_t last_rx_bytes_ = -1;
  base::android::RadioDataActivity last_activity_ =
      base::android::RadioDataActivity::kNone;
  int radio_wakeups_ = 0;
  base::RepeatingTimer metrics_timer_;
  base::RepeatingTimer radio_state_timer_;
  int skipped_timers_ = 0;

  // Number of consecutive charge drops seen while the app has been visible.
  int observed_capacity_drops_ = 0;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_BATTERY_METRICS_H_
