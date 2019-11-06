// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_API_UDP_SOCKET_H_
#define PLATFORM_API_UDP_SOCKET_H_

#include <cstdint>
#include <functional>
#include <memory>

#include "platform/api/network_interface.h"
#include "platform/api/udp_read_callback.h"
#include "platform/base/error.h"
#include "platform/base/ip_address.h"
#include "platform/base/macros.h"

namespace openscreen {
namespace platform {

class TaskRunner;
class UdpSocket;

using UdpSocketUniquePtr = std::unique_ptr<UdpSocket>;

// An open UDP socket for sending/receiving datagrams to/from either specific
// endpoints or over IP multicast.
//
// Usage: The socket is created and opened by calling the Create() method. This
// returns a unique pointer that auto-closes/destroys the socket when it goes
// out-of-scope.
//
// Platform implementation note: There must only be one platform-specific
// implementation of UdpSocket linked into the library/application. For that
// reason, none of the methods here are declared virtual (i.e., the overhead is
// pure waste). However, UdpSocket can be subclassed to include all extra
// private state, such as OS-specific handles. See UdpSocketPosix for a
// reference implementation.
class UdpSocket {
 public:
  virtual ~UdpSocket();

  // Client for the UdpSocket class.
  class Client {
   public:
    virtual ~Client() = default;

    // Method called on socket configuration operations when an error occurs.
    // These specific APIs are:
    //   UdpSocket::Bind()
    //   UdpSocket::SetMulticastOutboundInterface(...)
    //   UdpSocket::JoinMulticastGroup(...)
    //   UdpSocket::SetDscp(...)
    virtual void OnError(UdpSocket* socket, Error error) = 0;

    // Method called when an error occurs during a SendMessage call.
    virtual void OnSendError(UdpSocket* socket, Error error) = 0;

    // Method called when a packet is read.
    virtual void OnRead(UdpSocket* socket, ErrorOr<UdpPacket> packet) = 0;
  };

  // Constants used to specify how we want packets sent from this socket.
  enum class DscpMode : uint8_t {
    // Default value set by the system on creation of a new socket.
    kUnspecified = 0x0,

    // Mode for Audio only.
    kAudioOnly = 0xb8,

    // Mode for Audio + Video.
    kAudioVideo = 0x88,

    // Mode for low priority operations such as trace log data.
    kLowPriority = 0x20
  };

  using Version = IPAddress::Version;

  // Creates a new, scoped UdpSocket within the IPv4 or IPv6 family.
  // |local_endpoint| may be zero (see comments for Bind()). This method must be
  // defined in the platform-level implementation. All client_ methods called
  // will be queued on the provided task_runner. For this reason, the provided
  // task_runner and client must exist for the duration of the created socket's
  // lifetime.
  static ErrorOr<UdpSocketUniquePtr> Create(TaskRunner* task_runner,
                                            Client* client,
                                            const IPEndpoint& local_endpoint);

  // Returns true if |socket| belongs to the IPv4/IPv6 address family.
  virtual bool IsIPv4() const = 0;
  virtual bool IsIPv6() const = 0;

  // Returns the current local endpoint's address and port. Initially, this will
  // be the same as the value that was passed into Create(). However, it can
  // later change after certain operations, such as Bind(), are executed.
  virtual IPEndpoint GetLocalEndpoint() const = 0;

  // Binds to the address specified in the constructor. If the local endpoint's
  // address is zero, the operating system will bind to all interfaces. If the
  // local endpoint's port is zero, the operating system will automatically find
  // a free local port and bind to it. Future calls to local_endpoint() will
  // reflect the resolved port.
  virtual Error Bind() = 0;

  // Sets the device to use for outgoing multicast packets on the socket.
  virtual Error SetMulticastOutboundInterface(
      NetworkInterfaceIndex ifindex) = 0;

  // Joins to the multicast group at the given address, using the specified
  // interface.
  virtual Error JoinMulticastGroup(const IPAddress& address,
                                   NetworkInterfaceIndex ifindex) = 0;

  // Sends a message and returns the number of bytes sent, on success.
  // Error::Code::kAgain might be returned to indicate the operation would
  // block, which can be expected during normal operation.
  virtual Error SendMessage(const void* data,
                            size_t length,
                            const IPEndpoint& dest) = 0;

  // Sets the DSCP value to use for all messages sent from this socket.
  virtual Error SetDscp(DscpMode state) = 0;

  // Sets the callback that should be called upon deletion of this socket. This
  // allows other objects to observe the socket's destructor and act when it is
  // called.
  void SetDeletionCallback(std::function<void(UdpSocket*)> callback);

 protected:
  // Creates a new UdpSocket. The provided client and task_runner must exist for
  // the duration of this socket's lifetime.
  UdpSocket(TaskRunner* task_runner, Client* client);

  // Methods to take care of posting UdpSocket::Client callbacks for client_ to
  // task_runner_.
  void OnError(Error error);
  void OnSendError(Error error);
  void OnRead(ErrorOr<UdpPacket> read_data);

 private:
  // This callback allows other objects to observe the socket's destructor and
  // act when it is called.
  std::function<void(UdpSocket*)> deletion_callback_;

  // Client to use for callbacks.
  // NOTE: client_ can be nullptr if the user does not want any callbacks (for
  // example, in the send-only case).
  Client* const client_;

  // Task runner to use for queuing client_ callbacks.
  TaskRunner* const task_runner_;

  OSP_DISALLOW_COPY_AND_ASSIGN(UdpSocket);
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_API_UDP_SOCKET_H_
