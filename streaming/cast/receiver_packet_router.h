// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STREAMING_CAST_RECEIVER_PACKET_ROUTER_H_
#define STREAMING_CAST_RECEIVER_PACKET_ROUTER_H_

#include <stdint.h>

#include <utility>
#include <vector>

#include "absl/types/span.h"
#include "streaming/cast/environment.h"
#include "streaming/cast/ssrc.h"

namespace openscreen {
namespace cast_streaming {

class Receiver;

// Handles all network I/O among multiple Receivers meant for synchronized
// play-out (e.g., one Receiver for audio, one Receiver for video). Incoming
// traffic is dispatched to the appropriate Receiver, based on its SSRC. Also,
// all traffic not coming from the same source is filtered-out.
class ReceiverPacketRouter final : public Environment::PacketConsumer {
 public:
  explicit ReceiverPacketRouter(Environment* environment);
  ~ReceiverPacketRouter() final;

 protected:
  friend class Receiver;

  // Called from a Receiver constructor/destructor to register/deregister a
  // Receiver instance that processes RTP/RTCP packets for the given |ssrc|.
  void OnReceiverCreated(Ssrc ssrc, Receiver* receiver);
  void OnReceiverDestroyed(Ssrc ssrc);

  // Called by a Receiver to send a RTCP packet back to the source from which
  // earlier packets were received, or does nothing if OnReceivedPacket() has
  // not been called yet.
  void SendRtcpPacket(absl::Span<const uint8_t> packet);

 private:
  using ReceiverEntries = std::vector<std::pair<Ssrc, Receiver*>>;

  // Environment::PacketConsumer implementation.
  void OnReceivedPacket(const IPEndpoint& source,
                        platform::Clock::time_point arrival_time,
                        std::vector<uint8_t> packet) final;

  // Helper to return an iterator pointing to the entry having the given |ssrc|,
  // or "end" if not found.
  ReceiverEntries::iterator FindEntry(Ssrc ssrc);

  Environment* const environment_;

  ReceiverEntries receivers_;
};

}  // namespace cast_streaming
}  // namespace openscreen

#endif  // STREAMING_CAST_RECEIVER_PACKET_ROUTER_H_
