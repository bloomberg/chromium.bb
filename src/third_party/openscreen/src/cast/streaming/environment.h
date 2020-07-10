// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STREAMING_ENVIRONMENT_H_
#define CAST_STREAMING_ENVIRONMENT_H_

#include <stdint.h>

#include <functional>
#include <memory>

#include "absl/types/span.h"
#include "platform/api/time.h"
#include "platform/api/udp_socket.h"
#include "platform/base/ip_address.h"

namespace openscreen {
namespace platform {
class TaskRunner;
}  // namespace platform
}  // namespace openscreen

namespace cast {
namespace streaming {

// Provides the common environment for operating system resources shared by
// multiple components.
class Environment : public openscreen::platform::UdpSocket::Client {
 public:
  class PacketConsumer {
   public:
    virtual void OnReceivedPacket(
        const openscreen::IPEndpoint& source,
        openscreen::platform::Clock::time_point arrival_time,
        std::vector<uint8_t> packet) = 0;

   protected:
    virtual ~PacketConsumer();
  };

  // Construct with the given clock source and TaskRunner. Creates and
  // internally-owns a UdpSocket, and immediately binds it to the given
  // |local_endpoint|.
  Environment(openscreen::platform::ClockNowFunctionPtr now_function,
              openscreen::platform::TaskRunner* task_runner,
              const openscreen::IPEndpoint& local_endpoint);

  ~Environment() override;

  openscreen::platform::ClockNowFunctionPtr now_function() const {
    return now_function_;
  }
  openscreen::platform::TaskRunner* task_runner() const { return task_runner_; }

  // Returns the local endpoint the socket is bound to, or the zero IPEndpoint
  // if socket creation/binding failed.
  openscreen::IPEndpoint GetBoundLocalEndpoint() const;

  // Set a handler function to run whenever non-recoverable socket errors occur.
  // If never set, the default is to emit log messages at error priority.
  void set_socket_error_handler(
      std::function<void(openscreen::Error)> handler) {
    socket_error_handler_ = handler;
  }

  // Get/Set the remote endpoint. This is separate from the constructor because
  // the remote endpoint is, in some cases, discovered only after receiving a
  // packet.
  const openscreen::IPEndpoint& remote_endpoint() const {
    return remote_endpoint_;
  }
  void set_remote_endpoint(const openscreen::IPEndpoint& endpoint) {
    remote_endpoint_ = endpoint;
  }

  // Start/Resume delivery of incoming packets to the given |packet_consumer|.
  // Delivery will continue until DropIncomingPackets() is called.
  void ConsumeIncomingPackets(PacketConsumer* packet_consumer);

  // Stop delivery of incoming packets, dropping any that do come in. All
  // internal references to the PacketConsumer that was provided in the last
  // call to ConsumeIncomingPackets() are cleared.
  void DropIncomingPackets();

  // Returns the maximum packet size for the network. This will always return a
  // value of at least kRequiredNetworkPacketSize.
  int GetMaxPacketSize() const;

  // Sends the given |packet| to the remote endpoint, best-effort.
  // set_remote_endpoint() must be called beforehand with a valid IPEndpoint.
  //
  // Note: This method is virtual to allow unit tests to intercept packets
  // before they actually head-out through the socket.
  virtual void SendPacket(absl::Span<const uint8_t> packet);

 protected:
  // Common constructor that just stores the injected dependencies and does not
  // create a socket. Subclasses use this to provide an alternative packet
  // receive/send mechanism (e.g., for testing).
  Environment(openscreen::platform::ClockNowFunctionPtr now_function,
              openscreen::platform::TaskRunner* task_runner);

 private:
  // openscreen::platform::UdpSocket::Client implementation.
  void OnError(openscreen::platform::UdpSocket* socket,
               openscreen::Error error) final;
  void OnSendError(openscreen::platform::UdpSocket* socket,
                   openscreen::Error error) final;
  void OnRead(openscreen::platform::UdpSocket* socket,
              openscreen::ErrorOr<openscreen::platform::UdpPacket>
                  packet_or_error) final;

  const openscreen::platform::ClockNowFunctionPtr now_function_;
  openscreen::platform::TaskRunner* const task_runner_;

  // The UDP socket bound to the local endpoint that was passed into the
  // constructor, or null if socket creation failed.
  const std::unique_ptr<openscreen::platform::UdpSocket> socket_;

  // These are externally set/cleared. Behaviors are described in getter/setter
  // method comments above.
  std::function<void(openscreen::Error)> socket_error_handler_;
  openscreen::IPEndpoint remote_endpoint_{};
  PacketConsumer* packet_consumer_ = nullptr;
};

}  // namespace streaming
}  // namespace cast

#endif  // CAST_STREAMING_ENVIRONMENT_H_
