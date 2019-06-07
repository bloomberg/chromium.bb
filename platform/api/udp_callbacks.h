// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file

#ifndef PLATFORM_API_UDP_CALLBACKS_H_
#define PLATFORM_API_UDP_CALLBACKS_H_

#include <array>
#include <cstdint>
#include <memory>

#include "osp_base/ip_address.h"

namespace openscreen {
namespace platform {

class NetworkRunner;
class UdpSocket;

static constexpr int kUdpMaxPacketSize = 1 << 16;

class UdpReadCallback {
 public:
  struct Packet : std::array<uint8_t, kUdpMaxPacketSize> {
    Packet();
    ~Packet();

    IPEndpoint source;
    IPEndpoint original_destination;
    ssize_t length;
    // TODO(btolsch): When this gets to implementation, make sure the callback
    // is never called with a |socket| that could have been destroyed (e.g.
    // between queueing the read data and running the task).
    UdpSocket* socket;
  };

  virtual ~UdpReadCallback() = default;

  virtual void OnRead(std::unique_ptr<Packet> data,
                      NetworkRunner* network_runner) = 0;
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_API_UDP_CALLBACKS_H_
