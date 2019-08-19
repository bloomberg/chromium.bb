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
  // Creates a new UdpSocketPosix. The provided client and task_runner must
  // exist for the duration of this socket's lifetime.
  UdpSocketPosix(TaskRunner* task_runner,
                 Client* client,
                 int fd,
                 const IPEndpoint& local_endpoint);
  ~UdpSocketPosix() final;

  // Implementations of UdpSocket methods.
  bool IsIPv4() const final;
  bool IsIPv6() const final;
  IPEndpoint GetLocalEndpoint() const final;
  Error Bind() final;
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

  // Cached value of current local endpoint. This can change (e.g., when the
  // operating system auto-assigns a free local port when Bind() is called). If
  // the port is zero, getsockname() is called to try to resolve it. Once the
  // port is non-zero, it is assumed never to change again.
  mutable IPEndpoint local_endpoint_;
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_IMPL_UDP_SOCKET_POSIX_H_
