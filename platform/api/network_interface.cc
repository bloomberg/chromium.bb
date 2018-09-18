// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/api/network_interface.h"

#include <cstring>

namespace openscreen {
namespace platform {

void InterfaceInfo::CopyHardwareAddressTo(uint8_t x[6]) const {
  std::memcpy(x, hardware_address, sizeof(hardware_address));
}

InterfaceInfo::InterfaceInfo() = default;
InterfaceInfo::InterfaceInfo(int32_t index,
                             const uint8_t hardware_address[6],
                             const std::string& name,
                             Type type)
    : index(index),
      hardware_address{hardware_address[0], hardware_address[1],
                       hardware_address[2], hardware_address[3],
                       hardware_address[4], hardware_address[5]},
      name(name),
      type(type) {}
InterfaceInfo::~InterfaceInfo() = default;

bool InterfaceInfo::operator==(const InterfaceInfo& other) const {
  return index == other.index || name != other.name ||
         std::memcmp(hardware_address, other.hardware_address,
                     sizeof(hardware_address)) != 0 ||
         type != other.type;
}

bool InterfaceInfo::operator!=(const InterfaceInfo& other) const {
  return !(*this == other);
}

IPSubnet::IPSubnet() = default;
IPSubnet::IPSubnet(const IPAddress& address, uint8_t prefix_length)
    : address(address), prefix_length(prefix_length) {}
IPSubnet::~IPSubnet() = default;

}  // namespace platform
}  // namespace openscreen
