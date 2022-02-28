// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/bluetooth_config/device_cache.h"

#include "chromeos/services/bluetooth_config/adapter_state_controller.h"
#include "chromeos/services/bluetooth_config/public/cpp/cros_bluetooth_config_util.h"

namespace chromeos {
namespace bluetooth_config {

DeviceCache::DeviceCache(AdapterStateController* adapter_state_controller)
    : adapter_state_controller_(adapter_state_controller) {}

DeviceCache::~DeviceCache() = default;

std::vector<mojom::PairedBluetoothDevicePropertiesPtr>
DeviceCache::GetPairedDevices() const {
  // If Bluetooth is not enabled or enabling, return an empty list. This
  // addresses an edge case: when the user disables Bluetooth, there is a short
  // amount of time in which Bluetooth is still enabled but is in the process of
  // turning off. We should still return an empty list in this case to ensure
  // that the UI does not show a list of devices when the toggle is off.
  if (!IsBluetoothEnabledOrEnabling())
    return {};

  return PerformGetPairedDevices();
}

std::vector<mojom::BluetoothDevicePropertiesPtr>
DeviceCache::GetUnpairedDevices() const {
  // If Bluetooth is not enabled or enabling, return an empty list. This
  // addresses an edge case: when the user disables Bluetooth, there is a short
  // amount of time in which Bluetooth is still enabled but is in the process of
  // turning off. We should still return an empty list in this case to ensure
  // that the UI does not show a list of devices when the toggle is off.
  if (!IsBluetoothEnabledOrEnabling())
    return {};

  return PerformGetUnpairedDevices();
}

void DeviceCache::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void DeviceCache::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void DeviceCache::NotifyPairedDevicesListChanged() {
  for (auto& observer : observers_)
    observer.OnPairedDevicesListChanged();
}

void DeviceCache::NotifyUnpairedDevicesListChanged() {
  for (auto& observer : observers_)
    observer.OnUnpairedDevicesListChanged();
}

bool DeviceCache::IsBluetoothEnabledOrEnabling() const {
  const mojom::BluetoothSystemState adapter_state =
      adapter_state_controller_->GetAdapterState();
  return ::chromeos::bluetooth_config::IsBluetoothEnabledOrEnabling(
      adapter_state);
}

}  // namespace bluetooth_config
}  // namespace chromeos
