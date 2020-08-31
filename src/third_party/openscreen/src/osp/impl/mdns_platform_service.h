// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OSP_IMPL_MDNS_PLATFORM_SERVICE_H_
#define OSP_IMPL_MDNS_PLATFORM_SERVICE_H_

#include <vector>

#include "platform/api/network_interface.h"
#include "platform/api/udp_socket.h"

namespace openscreen {
namespace osp {

class MdnsPlatformService {
 public:
  struct BoundInterface {
    BoundInterface(const InterfaceInfo& interface_info,
                   const IPSubnet& subnet,
                   UdpSocket* socket);
    ~BoundInterface();

    bool operator==(const BoundInterface& other) const;
    bool operator!=(const BoundInterface& other) const;

    InterfaceInfo interface_info;
    IPSubnet subnet;
    UdpSocket* socket;
  };

  virtual ~MdnsPlatformService() = default;

  virtual std::vector<BoundInterface> RegisterInterfaces(
      const std::vector<NetworkInterfaceIndex>& whitelist) = 0;
  virtual void DeregisterInterfaces(
      const std::vector<BoundInterface>& registered_interfaces) = 0;
};

}  // namespace osp
}  // namespace openscreen

#endif  // OSP_IMPL_MDNS_PLATFORM_SERVICE_H_
