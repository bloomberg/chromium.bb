// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "streaming/cast/receiver.h"

#include "platform/api/logging.h"
#include "streaming/cast/receiver_packet_router.h"

namespace openscreen {
namespace cast_streaming {

Receiver::Receiver(Environment* environment,
                   ReceiverPacketRouter* packet_router,
                   Ssrc sender_ssrc,
                   Ssrc receiver_ssrc,
                   int rtp_timebase,
                   std::chrono::milliseconds initial_target_playout_delay,
                   const std::array<uint8_t, 16>& aes_key,
                   const std::array<uint8_t, 16>& cast_iv_mask)
    : receiver_ssrc_(receiver_ssrc), packet_router_(packet_router) {
  OSP_DCHECK(packet_router_);
  packet_router_->OnReceiverCreated(ssrc(), this);
}

Receiver::~Receiver() {
  packet_router_->OnReceiverDestroyed(ssrc());
}

void Receiver::SetConsumer(Consumer* consumer) {
  OSP_UNIMPLEMENTED();
}

void Receiver::SetPlayerProcessingTime(platform::Clock::duration needed_time) {
  OSP_UNIMPLEMENTED();
}

void Receiver::RequestKeyFrame() {
  OSP_UNIMPLEMENTED();
}

int Receiver::AdvanceToNextFrame() {
  OSP_UNIMPLEMENTED();
  return -1;
}

void Receiver::ConsumeNextFrame(EncodedFrame* frame) {
  OSP_UNIMPLEMENTED();
}

void Receiver::OnReceivedRtpPacket(platform::Clock::time_point arrival_time,
                                   std::vector<uint8_t> packet) {
  OSP_UNIMPLEMENTED();
}

void Receiver::OnReceivedRtcpPacket(platform::Clock::time_point arrival_time,
                                    std::vector<uint8_t> packet) {
  OSP_UNIMPLEMENTED();
}

Receiver::Consumer::~Consumer() = default;

}  // namespace cast_streaming
}  // namespace openscreen
