// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/cryptauth_device_registry.h"

#include "base/stl_util.h"

namespace chromeos {

namespace device_sync {

CryptAuthDeviceRegistry::CryptAuthDeviceRegistry() = default;

CryptAuthDeviceRegistry::~CryptAuthDeviceRegistry() = default;

const base::flat_map<std::string, CryptAuthDevice>&
CryptAuthDeviceRegistry::instance_id_to_device_map() const {
  return instance_id_to_device_map_;
}

const CryptAuthDevice* CryptAuthDeviceRegistry::GetDevice(
    const std::string& instance_id) const {
  auto it = instance_id_to_device_map_.find(instance_id);
  if (it == instance_id_to_device_map_.end())
    return nullptr;

  return &it->second;
}

void CryptAuthDeviceRegistry::AddDevice(const CryptAuthDevice& device) {
  instance_id_to_device_map_.insert_or_assign(device.instance_id(), device);

  OnDeviceRegistryUpdated();
}

void CryptAuthDeviceRegistry::DeleteDevice(const std::string& instance_id) {
  DCHECK(base::ContainsKey(instance_id_to_device_map_, instance_id));
  instance_id_to_device_map_.erase(instance_id);

  OnDeviceRegistryUpdated();
}

void CryptAuthDeviceRegistry::SetRegistry(
    const base::flat_map<std::string, CryptAuthDevice>&
        instance_id_to_device_map) {
  instance_id_to_device_map_ = instance_id_to_device_map;

  OnDeviceRegistryUpdated();
}

}  // namespace device_sync

}  // namespace chromeos
