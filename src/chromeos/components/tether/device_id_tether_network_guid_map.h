// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_DEVICE_ID_TETHER_NETWORK_GUID_MAP_H_
#define CHROMEOS_COMPONENTS_TETHER_DEVICE_ID_TETHER_NETWORK_GUID_MAP_H_

#include <string>

#include "base/macros.h"

namespace chromeos {

namespace tether {

// Keeps a mapping between device ID and the tether network GUID associated with
// tethering to that device.
// TODO(hansberry): Currently, this class is stubbed out by simply returning the
//                  same value for both device ID and tether network GUID.
//                  Figure out a real mapping system.
class DeviceIdTetherNetworkGuidMap {
 public:
  DeviceIdTetherNetworkGuidMap();
  virtual ~DeviceIdTetherNetworkGuidMap();

  // Returns the device ID for a given tether network GUID.
  virtual std::string GetDeviceIdForTetherNetworkGuid(
      const std::string& tether_network_guid);

  // Returns the tether network GUID for a given device ID.
  virtual std::string GetTetherNetworkGuidForDeviceId(
      const std::string& device_id);

 private:
  DISALLOW_COPY_AND_ASSIGN(DeviceIdTetherNetworkGuidMap);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_DEVICE_ID_TETHER_NETWORK_GUID_MAP_H_
