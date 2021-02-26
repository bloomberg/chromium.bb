// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TEST_POWER_MONITOR_TEST_BASE_H_
#define BASE_TEST_POWER_MONITOR_TEST_BASE_H_

#include "base/power_monitor/power_monitor.h"
#include "base/power_monitor/power_monitor_source.h"

namespace base {

class PowerMonitorTestSource : public PowerMonitorSource {
 public:
  PowerMonitorTestSource();
  ~PowerMonitorTestSource() override;
  PowerObserver::DeviceThermalState GetCurrentThermalState() override;

  void GeneratePowerStateEvent(bool on_battery_power);
  void GenerateSuspendEvent();
  void GenerateResumeEvent();
  void GenerateThermalThrottlingEvent(
      PowerObserver::DeviceThermalState new_thermal_state);

 protected:
  bool IsOnBatteryPowerImpl() override;

  bool test_on_battery_power_ = false;
  PowerObserver::DeviceThermalState current_thermal_state_ =
      PowerObserver::DeviceThermalState::kUnknown;
};

class PowerMonitorTestObserver : public PowerObserver {
 public:
  PowerMonitorTestObserver();
  ~PowerMonitorTestObserver() override;

  // PowerObserver callbacks.
  void OnPowerStateChange(bool on_battery_power) override;
  void OnSuspend() override;
  void OnResume() override;
  void OnThermalStateChange(
      PowerObserver::DeviceThermalState new_state) override;

  // Test status counts.
  bool last_power_state() const { return last_power_state_; }
  int power_state_changes() const { return power_state_changes_; }
  int suspends() const { return suspends_; }
  int resumes() const { return resumes_; }
  PowerObserver::DeviceThermalState last_thermal_state() const {
    return last_thermal_state_;
  }

 private:
  bool last_power_state_;    // Last power state we were notified of.
  int power_state_changes_;  // Count of OnPowerStateChange notifications.
  int suspends_;             // Count of OnSuspend notifications.
  int resumes_;              // Count of OnResume notifications.
  PowerObserver::DeviceThermalState last_thermal_state_;
};

}  // namespace base

#endif  // BASE_TEST_POWER_MONITOR_TEST_BASE_H_
