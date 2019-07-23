// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_IMPL_UDP_SOCKET_POSIX_H_
#define PLATFORM_IMPL_UDP_SOCKET_POSIX_H_

#include "platform/api/udp_socket.h"

namespace openscreen {
namespace platform {

struct UdpSocketPosix : public UdpSocket {
 public:
  UdpSocketPosix(int fd, Version version);
  ~UdpSocketPosix() final;

  // Implementations of UdpSocket methods.
  bool IsIPv4() const final;
  bool IsIPv6() const final;
  Error Bind(const IPEndpoint& local_endpoint) final;
  Error SetMulticastOutboundInterface(NetworkInterfaceIndex ifindex) final;
  Error JoinMulticastGroup(const IPAddress& address,
                           NetworkInterfaceIndex ifindex) final;
  ErrorOr<UdpPacket> ReceiveMessage() final;
  Error SendMessage(const void* data,
                    size_t length,
                    const IPEndpoint& dest) final;
  Error SetDscp(DscpMode state) final;

  int GetFd() const { return fd_; }

 private:
  const int fd_;
  const UdpSocket::Version version_;
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_IMPL_UDP_SOCKET_POSIX_H_
