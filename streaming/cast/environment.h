// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STREAMING_CAST_ENVIRONMENT_H_
#define STREAMING_CAST_ENVIRONMENT_H_

#include "absl/types/span.h"
#include "platform/api/time.h"
#include "platform/base/ip_address.h"

namespace openscreen {

namespace platform {
class TaskRunner;
class UdpSocket;
}  // namespace platform

namespace cast_streaming {

// Provides the common environment for operating system resources shared by
// multiple components.
class Environment {
 public:
  class PacketConsumer {
   public:
    virtual void OnReceivedPacket(absl::Span<const uint8_t> packet,
                                  const IPEndpoint& source,
                                  platform::Clock::time_point arrival_time) = 0;

   protected:
    virtual ~PacketConsumer();
  };

  // |socket| should be bound to the appropriate interface and port for
  // receiving packets from the remote endpoint beforehand.
  Environment(platform::ClockNowFunctionPtr now_function,
              platform::TaskRunner* task_runner,
              platform::UdpSocket* socket);
  ~Environment();

  platform::ClockNowFunctionPtr now_function() const { return now_function_; }
  platform::TaskRunner* task_runner() const { return task_runner_; }

  // Get/Set the remote endpoint.
  const IPEndpoint& remote_endpoint() const { return remote_endpoint_; }
  void set_remote_endpoint(const IPEndpoint& endpoint) {
    remote_endpoint_ = endpoint;
  }

  // Resume delivery of incoming packets to the given |packet_consumer|.
  // Delivery will continue until SuspendIncomingPackets() is called.
  void ResumeIncomingPackets(PacketConsumer* packet_consumer);

  // Suspend delivery of incoming packets. All internal references to the
  // PacketConsumer that was provided in the last call to
  // ResumeIncomingPackets() are cleared.
  void SuspendIncomingPackets();

  // Returns the maximum packet size for the network. This will always return a
  // value of at least kRequiredNetworkPacketSize.
  int GetMaxPacketSize() const;

  // Sends the given |packet| to the remote endpoint, best-effort.
  // set_remote_endpoint() must be called beforehand with a valid IPEndpoint.
  void SendPacket(absl::Span<const uint8_t> packet);

 private:
  const platform::ClockNowFunctionPtr now_function_;
  platform::TaskRunner* const task_runner_;
  platform::UdpSocket* const socket_;

  IPEndpoint remote_endpoint_{};

  PacketConsumer* packet_consumer_ = nullptr;
};

}  // namespace cast_streaming
}  // namespace openscreen

#endif  // STREAMING_CAST_ENVIRONMENT_H_
