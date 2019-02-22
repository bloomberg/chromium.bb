// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/bluetooth/tray_bluetooth_helper_experimental.h"

#include <string>

namespace ash {

TrayBluetoothHelperExperimental::TrayBluetoothHelperExperimental() = default;

TrayBluetoothHelperExperimental::~TrayBluetoothHelperExperimental() = default;

void TrayBluetoothHelperExperimental::Initialize() {}

BluetoothDeviceList
TrayBluetoothHelperExperimental::GetAvailableBluetoothDevices() const {
  NOTIMPLEMENTED();
  return BluetoothDeviceList();
}

void TrayBluetoothHelperExperimental::StartBluetoothDiscovering() {
  NOTIMPLEMENTED();
}

void TrayBluetoothHelperExperimental::StopBluetoothDiscovering() {
  NOTIMPLEMENTED();
}

void TrayBluetoothHelperExperimental::ConnectToBluetoothDevice(
    const std::string& address) {
  NOTIMPLEMENTED();
}

device::mojom::BluetoothSystem::State
TrayBluetoothHelperExperimental::GetBluetoothState() {
  NOTIMPLEMENTED();
  return device::mojom::BluetoothSystem::State::kUnavailable;
}

void TrayBluetoothHelperExperimental::SetBluetoothEnabled(bool enabled) {
  NOTIMPLEMENTED();
}

bool TrayBluetoothHelperExperimental::HasBluetoothDiscoverySession() {
  NOTIMPLEMENTED();
  return false;
}

}  // namespace ash
