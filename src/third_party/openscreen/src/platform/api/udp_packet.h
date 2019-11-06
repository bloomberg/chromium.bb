// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file

#ifndef PLATFORM_API_UDP_PACKET_H_
#define PLATFORM_API_UDP_PACKET_H_

#include <utility>
#include <vector>

#include "platform/api/logging.h"
#include "platform/base/ip_address.h"

namespace openscreen {
namespace platform {

class UdpSocket;

static constexpr size_t kUdpMaxPacketSize = 1 << 16;

class UdpPacket : public std::vector<uint8_t> {
 public:
  explicit UdpPacket(size_t size) : std::vector<uint8_t>(size) {
    OSP_DCHECK(size <= kUdpMaxPacketSize);
  }
  UdpPacket() : UdpPacket(0) {}
  UdpPacket(UdpPacket&& other) = default;
  UdpPacket& operator=(UdpPacket&& other) = default;

  const IPEndpoint& source() const { return source_; }
  void set_source(IPEndpoint endpoint) { source_ = std::move(endpoint); }

  const IPEndpoint& destination() const { return destination_; }
  void set_destination(IPEndpoint endpoint) {
    destination_ = std::move(endpoint);
  }

  UdpSocket* socket() const { return socket_; }
  void set_socket(UdpSocket* socket) { socket_ = socket; }

 private:
  IPEndpoint source_ = {};
  IPEndpoint destination_ = {};
  UdpSocket* socket_ = nullptr;

  OSP_DISALLOW_COPY_AND_ASSIGN(UdpPacket);
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_API_UDP_PACKET_H_
