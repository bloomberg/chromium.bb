// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/keyed_service/fast_pair_bluetooth_config_delegate.h"

#include "ash/quick_pair/repository/fast_pair_repository.h"
#include "base/callback_helpers.h"
#include "chromeos/services/bluetooth_config/device_name_manager.h"
#include "chromeos/services/bluetooth_config/public/cpp/device_image_info.h"

namespace ash {
namespace quick_pair {

FastPairBluetoothConfigDelegate::FastPairBluetoothConfigDelegate() = default;

FastPairBluetoothConfigDelegate::~FastPairBluetoothConfigDelegate() = default;

void FastPairBluetoothConfigDelegate::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FastPairBluetoothConfigDelegate::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

absl::optional<chromeos::bluetooth_config::DeviceImageInfo>
FastPairBluetoothConfigDelegate::GetDeviceImageInfo(
    const std::string& device_id) {
  return FastPairRepository::Get()->GetImagesForDevice(device_id);
}

void FastPairBluetoothConfigDelegate::ForgetDevice(
    const std::string& mac_address) {
  FastPairRepository::Get()->DeleteAssociatedDevice(mac_address,
                                                    base::DoNothing());
}

void FastPairBluetoothConfigDelegate::SetAdapterStateController(
    chromeos::bluetooth_config::AdapterStateController*
        adapter_state_controller) {
  adapter_state_controller_ = adapter_state_controller;
  for (auto& observer : observers_) {
    observer.OnAdapterStateControllerChanged(adapter_state_controller_);
  }
}

void FastPairBluetoothConfigDelegate::SetDeviceNameManager(
    chromeos::bluetooth_config::DeviceNameManager* device_name_manager) {
  device_name_manager_ = device_name_manager;
}

}  // namespace quick_pair
}  // namespace ash
