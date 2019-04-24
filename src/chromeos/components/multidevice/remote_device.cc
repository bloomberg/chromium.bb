// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/multidevice/remote_device.h"

#include "base/base64.h"
#include "base/stl_util.h"

namespace chromeos {

namespace multidevice {

// static
std::string RemoteDevice::GenerateDeviceId(const std::string& public_key) {
  std::string device_id;
  base::Base64Encode(public_key, &device_id);
  return device_id;
}

RemoteDevice::RemoteDevice() : last_update_time_millis(0L) {}

RemoteDevice::RemoteDevice(
    const std::string& user_id,
    const std::string& name,
    const std::string& public_key,
    const std::string& persistent_symmetric_key,
    int64_t last_update_time_millis,
    const std::map<SoftwareFeature, SoftwareFeatureState>& software_features,
    const std::vector<BeaconSeed>& beacon_seeds)
    : user_id(user_id),
      name(name),
      public_key(public_key),
      persistent_symmetric_key(persistent_symmetric_key),
      last_update_time_millis(last_update_time_millis),
      software_features(software_features),
      beacon_seeds(beacon_seeds) {}

RemoteDevice::RemoteDevice(const RemoteDevice& other) = default;

RemoteDevice::~RemoteDevice() {}

std::string RemoteDevice::GetDeviceId() const {
  return RemoteDevice::GenerateDeviceId(public_key);
}

bool RemoteDevice::operator==(const RemoteDevice& other) const {
  return user_id == other.user_id && name == other.name &&
         public_key == other.public_key &&
         persistent_symmetric_key == other.persistent_symmetric_key &&
         last_update_time_millis == other.last_update_time_millis &&
         software_features == other.software_features &&
         beacon_seeds == other.beacon_seeds;
}

bool RemoteDevice::operator<(const RemoteDevice& other) const {
  // |public_key| is the only field guaranteed to be set and is also unique to
  // each RemoteDevice. However, since it can contain null bytes, use
  // GetDeviceId(), which cannot contain null bytes, to compare devices.
  return GetDeviceId().compare(other.GetDeviceId()) < 0;
}

}  // namespace multidevice

}  // namespace chromeos
