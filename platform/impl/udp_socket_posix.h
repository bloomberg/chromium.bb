// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_IMPL_UDP_SOCKET_POSIX_H_
#define PLATFORM_IMPL_UDP_SOCKET_POSIX_H_

#include "absl/types/optional.h"
#include "platform/api/udp_socket.h"
#include "platform/impl/socket_handle_posix.h"

namespace openscreen {
namespace platform {

struct UdpSocketPosix : public UdpSocket {
 public:
  // Creates a new UdpSocketPosix. The provided client and task_runner must
  // exist for the duration of this socket's lifetime.
  UdpSocketPosix(TaskRunner* task_runner,
                 Client* client,
                 SocketHandle handle,
                 const IPEndpoint& local_endpoint);

  ~UdpSocketPosix() override;

  // Performs a non-blocking read on the socket and then process the read packet
  // using this socket's Client.
  virtual void ReceiveMessage();

  // Implementations of UdpSocket methods.
  bool IsIPv4() const override;
  bool IsIPv6() const override;
  IPEndpoint GetLocalEndpoint() const override;
  void Bind() override;
  void SetMulticastOutboundInterface(NetworkInterfaceIndex ifindex) override;
  void JoinMulticastGroup(const IPAddress& address,
                          NetworkInterfaceIndex ifindex) override;
  void SendMessage(const void* data,
                   size_t length,
                   const IPEndpoint& dest) override;
  void SetDscp(DscpMode state) override;

  const SocketHandle& GetHandle() const;

 private:
  void Close() override;

  // Creates an error to be used in above methods.
  Error CreateError(Error::Code code);

  const SocketHandle handle_;

  // Cached value of current local endpoint. This can change (e.g., when the
  // operating system auto-assigns a free local port when Bind() is called). If
  // the port is zero, getsockname() is called to try to resolve it. Once the
  // port is non-zero, it is assumed never to change again.
  mutable IPEndpoint local_endpoint_;
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_IMPL_UDP_SOCKET_POSIX_H_
