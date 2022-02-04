// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/bluetooth_config/fake_fast_pair_delegate.h"

namespace chromeos {
namespace bluetooth_config {

FakeFastPairDelegate::FakeFastPairDelegate() = default;

FakeFastPairDelegate::~FakeFastPairDelegate() = default;

void FakeFastPairDelegate::SetDeviceImageInfo(const std::string& device_id,
                                              DeviceImageInfo& images) {
  device_id_to_images_[device_id] = images;
}

void FakeFastPairDelegate::SetDeviceNameManager(
    DeviceNameManager* device_name_manager) {
  device_name_manager_ = device_name_manager;
}

absl::optional<DeviceImageInfo> FakeFastPairDelegate::GetDeviceImageInfo(
    const std::string& device_id) {
  const auto it = device_id_to_images_.find(device_id);
  if (it == device_id_to_images_.end())
    return absl::nullopt;
  return it->second;
}

}  // namespace bluetooth_config
}  // namespace chromeos
