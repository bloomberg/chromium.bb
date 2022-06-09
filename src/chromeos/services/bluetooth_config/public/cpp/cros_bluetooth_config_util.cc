// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/bluetooth_config/public/cpp/cros_bluetooth_config_util.h"

#include "base/strings/utf_string_conversions.h"

namespace chromeos {
namespace bluetooth_config {

bool IsBluetoothEnabledOrEnabling(
    const mojom::BluetoothSystemState system_state) {
  return system_state == mojom::BluetoothSystemState::kEnabled ||
         system_state == mojom::BluetoothSystemState::kEnabling;
}

std::u16string GetPairedDeviceName(
    const mojom::PairedBluetoothDeviceProperties* paired_device_properties) {
  if (paired_device_properties->nickname.has_value())
    return base::ASCIIToUTF16(paired_device_properties->nickname.value());
  return paired_device_properties->device_properties->public_name;
}

}  // namespace bluetooth_config
}  // namespace chromeos
