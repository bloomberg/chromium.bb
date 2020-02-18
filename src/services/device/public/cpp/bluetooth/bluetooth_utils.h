// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_PUBLIC_CPP_BLUETOOTH_BLUETOOTH_UTILS_H_
#define SERVICES_DEVICE_PUBLIC_CPP_BLUETOOTH_BLUETOOTH_UTILS_H_

#include "base/strings/string16.h"
#include "services/device/public/mojom/bluetooth_system.mojom.h"

namespace device {

// Returns the address suitable for displaying e.g. "AA:BB:CC:DD:00:11".
base::string16 GetBluetoothAddressForDisplay(
    const std::array<uint8_t, 6>& address);

// Returns the name of the device suitable for displaying, this may
// be a synthesized string containing the address and localized type name
// if the device has no obtained name.
base::string16 GetBluetoothDeviceNameForDisplay(
    const mojom::BluetoothDeviceInfoPtr& device_info);

// Returns an accessibility label for the device based on name or address and
// device type.
base::string16 GetBluetoothDeviceLabelForAccessibility(
    const mojom::BluetoothDeviceInfoPtr& device_info);

}  // namespace device

#endif  // SERVICES_DEVICE_PUBLIC_CPP_BLUETOOTH_BLUETOOTH_UTILS_H_
