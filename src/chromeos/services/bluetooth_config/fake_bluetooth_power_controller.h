// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_BLUETOOTH_CONFIG_FAKE_BLUETOOTH_POWER_CONTROLLER_H_
#define CHROMEOS_SERVICES_BLUETOOTH_CONFIG_FAKE_BLUETOOTH_POWER_CONTROLLER_H_

#include "chromeos/services/bluetooth_config/adapter_state_controller.h"
#include "chromeos/services/bluetooth_config/bluetooth_power_controller.h"

namespace chromeos {
namespace bluetooth_config {

class FakeBluetoothPowerController : public BluetoothPowerController {
 public:
  explicit FakeBluetoothPowerController(
      AdapterStateController* adapter_state_controller);
  ~FakeBluetoothPowerController() override;

 private:
  // BluetoothPowerController:
  void SetBluetoothEnabledState(bool enabled) override;
  void SetPrefs(PrefService* logged_in_profile_prefs,
                PrefService* local_state) override {}

  AdapterStateController* adapter_state_controller_;
};

}  // namespace bluetooth_config
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_BLUETOOTH_CONFIG_FAKE_BLUETOOTH_POWER_CONTROLLER_H_
