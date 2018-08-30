// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_API_NETWORK_INTERFACE_H_
#define PLATFORM_API_NETWORK_INTERFACE_H_

#include <vector>

#include "base/ip_address.h"

namespace openscreen {
namespace platform {

struct InterfaceInfo {
  enum class Type {
    kEthernet = 0,
    kWifi,
    kOther,
  };

  void CopyHardwareAddressTo(uint8_t x[6]) const;

  // Interface index, typically as specified by the operating system,
  // identifying this interface on the host machine.
  int32_t index = 0;

  // MAC address of the interface.  All 0s if unavailable.
  uint8_t hardware_address[6] = {};

  // Interface name (e.g. eth0) if available.
  std::string name;

  // Hardware type of the interface.
  Type type = Type::kOther;
};

struct IPv4Subnet {
  IPv4Address address;

  // Prefix length of |address|, which is another way of specifying a subnet
  // mask.  For example, 192.168.0.10/24 is a common representation of the
  // address 192.168.0.10 with a 24-bit prefix, and therefore a subnet mask of
  // 255.255.255.0.
  uint8_t prefix_length = 0;
};

struct IPv6Subnet {
  IPv6Address address;

  // Prefix length of |address|.  This works the same was as
  // IPv4PrefixedAddress.prefix_length, but can be larger.
  uint8_t prefix_length = 0;
};

struct InterfaceAddresses {
  InterfaceInfo info = {};

  // All IPv4 addresses associated with the interface identified by |info|.
  std::vector<IPv4Subnet> ipv4_addresses;

  // All IPv6 addresses associated with the interface identified by |info|.
  std::vector<IPv6Subnet> ipv6_addresses;
};

// Returns at most one InterfaceAddresses per interface, so there will be no
// duplicate InterfaceInfo entries.
std::vector<InterfaceAddresses> GetInterfaceAddresses();

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_API_NETWORK_INTERFACE_H_
