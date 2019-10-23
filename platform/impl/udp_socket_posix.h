// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_IMPL_UDP_SOCKET_POSIX_H_
#define PLATFORM_IMPL_UDP_SOCKET_POSIX_H_

#include "absl/types/optional.h"
#include "platform/api/udp_socket.h"
#include "platform/impl/platform_client_posix.h"
#include "platform/impl/socket_handle_posix.h"

namespace openscreen {
namespace platform {

class UdpSocketReaderPosix;

// Threading: All public methods must be called on the same thread--the one
// executing the TaskRunner. All non-public methods, except ReceiveMessage(),
// are also assumed to be called on that thread.
class UdpSocketPosix : public UdpSocket {
 public:
  // TODO: Remove this. See comments for SetLifetimeObserver().
  class LifetimeObserver {
   public:
    virtual ~LifetimeObserver() = default;

    // Function to call upon creation of a new UdpSocket.
    virtual void OnCreate(UdpSocket* socket) = 0;

    // Function to call upon deletion of a UdpSocket.
    virtual void OnDestroy(UdpSocket* socket) = 0;
  };

  // Creates a new UdpSocketPosix. The provided client and task_runner must
  // exist for the duration of this socket's lifetime.
  UdpSocketPosix(TaskRunner* task_runner,
                 Client* client,
                 SocketHandle handle,
                 const IPEndpoint& local_endpoint,
                 PlatformClientPosix* platform_client =
                     PlatformClientPosix::GetInstance());

  ~UdpSocketPosix() override;

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

 protected:
  friend class UdpSocketReaderPosix;

  // Called by UdpSocketReaderPosix to perform a non-blocking read on the socket
  // and then dispatch the packet to this socket's Client. This method is the
  // only one in this class possibly being called from another thread.
  void ReceiveMessage();

  // The LifetimeObserver set here must exist during ANY future UdpSocketPosix
  // creations. SetLifetimeObserver(nullptr) must be called before any future
  // socket creations on destructions after the observer is destroyed
  //
  // TODO: UdpSocketPosix should create/destroy the UdpSocketReaderPosix global
  // instance automatically; so that it only exists while it is being used; and
  // so client-side code need not memory-manage parts of UdpSocketPosix's
  // internal object graph.
  static void SetLifetimeObserver(LifetimeObserver* observer);

 private:
  // Helper to close the socket if |error| is fatal, in addition to dispatching
  // an Error to the |client_|.
  void OnError(Error::Code error);

  bool is_closed() const { return handle_.fd < 0; }
  void Close();

  // Task runner to use for queuing |client_| callbacks.
  TaskRunner* const task_runner_;

  // Client to use for callbacks. This can be nullptr if the user does not want
  // any callbacks (for example, in the send-only case).
  Client* const client_;

  // Holds the POSIX file descriptor, or -1 if the socket is closed.
  SocketHandle handle_;

  // Cached value of current local endpoint. This can change (e.g., when the
  // operating system auto-assigns a free local port when Bind() is called). If
  // the port is zero, getsockname() is called to try to resolve it. Once the
  // port is non-zero, it is assumed never to change again.
  mutable IPEndpoint local_endpoint_;

  // TODO: Remove these. See comments for SetLifetimeObserver().
  PlatformClientPosix* platform_client_;
  static LifetimeObserver* lifetime_observer_;
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_IMPL_UDP_SOCKET_POSIX_H_
