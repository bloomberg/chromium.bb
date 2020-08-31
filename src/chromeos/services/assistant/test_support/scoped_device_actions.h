// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_TEST_SUPPORT_SCOPED_DEVICE_ACTIONS_H_
#define CHROMEOS_SERVICES_ASSISTANT_TEST_SUPPORT_SCOPED_DEVICE_ACTIONS_H_

#include <string>
#include <vector>

#include "chromeos/services/assistant/public/cpp/device_actions.h"

namespace chromeos {
namespace assistant {

class ScopedDeviceActions : DeviceActions {
 public:
  ScopedDeviceActions() = default;
  ~ScopedDeviceActions() override = default;

  // chromeos::assistant::DeviceActions overrides:
  void SetWifiEnabled(bool enabled) override {}
  void SetBluetoothEnabled(bool enabled) override {}
  void GetScreenBrightnessLevel(
      GetScreenBrightnessLevelCallback callback) override;
  void SetScreenBrightnessLevel(double level, bool gradual) override {}
  void SetNightLightEnabled(bool enabled) override {}
  void SetSwitchAccessEnabled(bool enabled) override {}
  bool OpenAndroidApp(
      chromeos::assistant::mojom::AndroidAppInfoPtr app_info) override;
  mojom::AppStatus GetAndroidAppStatus(
      const mojom::AndroidAppInfo& app_info) override;
  void LaunchAndroidIntent(const std::string& intent) override {}
  void AddAppListEventSubscriber(AppListEventSubscriber* subscriber) override {}
  void RemoveAppListEventSubscriber(
      AppListEventSubscriber* subscriber) override {}

  // Set the brightness value that will be returned by
  // GetScreenBrightnessLevel();
  void set_current_brightness(double value) { current_brightness_ = value; }

 private:
  double current_brightness_ = 0.0;
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_TEST_SUPPORT_SCOPED_DEVICE_ACTIONS_H_
