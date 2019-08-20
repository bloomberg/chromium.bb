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

  ~UdpSocketPosix() override;

  // Performs a non-blocking read on the socket, returning the number of bytes
  // received. Note that a non-Error return value of 0 is a valid result,
  // indicating an empty message has been received. Also note that
  // Error::Code::kAgain might be returned if there is no message currently
  // ready for receive, which can be expected during normal operation.
  virtual ErrorOr<UdpPacket> ReceiveMessage();

  // Implementations of UdpSocket methods.
  bool IsIPv4() const override;
  bool IsIPv6() const override;
  IPEndpoint GetLocalEndpoint() const override;
  Error Bind() override;
  Error SetMulticastOutboundInterface(NetworkInterfaceIndex ifindex) override;
  Error JoinMulticastGroup(const IPAddress& address,
                           NetworkInterfaceIndex ifindex) override;
  Error SendMessage(const void* data,
                    size_t length,
                    const IPEndpoint& dest) override;
  Error SetDscp(DscpMode state) override;

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
