// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/hid_detection/fake_bluetooth_hid_detector.h"

namespace ash {
namespace hid_detection {

FakeBluetoothHidDetector::FakeBluetoothHidDetector() = default;

FakeBluetoothHidDetector::~FakeBluetoothHidDetector() = default;

void FakeBluetoothHidDetector::SetInputDevicesStatus(
    InputDevicesStatus input_devices_status) {
  input_devices_status_ = input_devices_status;
}

const BluetoothHidDetector::BluetoothHidDetectionStatus
FakeBluetoothHidDetector::GetBluetoothHidDetectionStatus() {
  absl::optional<BluetoothHidMetadata> current_pairing_device;
  if (current_pairing_device_.has_value()) {
    current_pairing_device =
        BluetoothHidMetadata{current_pairing_device_.value().name,
                             current_pairing_device_.value().type};
  }

  absl::optional<BluetoothHidPairingState> pairing_state;
  if (current_pairing_state_.has_value()) {
    pairing_state = BluetoothHidPairingState{
        current_pairing_state_.value().code,
        current_pairing_state_.value().num_keys_entered};
  }

  return BluetoothHidDetectionStatus{std::move(current_pairing_device),
                                     std::move(pairing_state)};
}

void FakeBluetoothHidDetector::SetBluetoothHidDetectionStatus(
    absl::optional<BluetoothHidDetector::BluetoothHidMetadata> pairing_device,
    absl::optional<BluetoothHidPairingState> pairing_state) {
  current_pairing_device_ = std::move(pairing_device);
  current_pairing_state_ = std::move(pairing_state);
  NotifyBluetoothHidDetectionStatusChanged();
}

void FakeBluetoothHidDetector::PerformStartBluetoothHidDetection(
    InputDevicesStatus input_devices_status) {
  input_devices_status_ = input_devices_status;
  is_bluetooth_hid_detection_active_ = true;
}

void FakeBluetoothHidDetector::PerformStopBluetoothHidDetection() {
  is_bluetooth_hid_detection_active_ = false;
}

}  // namespace hid_detection
}  // namespace ash
