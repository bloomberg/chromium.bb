// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "streaming/cast/environment.h"

#include "platform/api/logging.h"
#include "platform/api/task_runner.h"
#include "platform/api/udp_socket.h"
#include "streaming/cast/rtp_defines.h"

namespace openscreen {
namespace cast_streaming {

Environment::Environment(platform::ClockNowFunctionPtr now_function,
                         platform::TaskRunner* task_runner,
                         platform::UdpSocket* socket)
    : now_function_(now_function), task_runner_(task_runner), socket_(socket) {
  OSP_DCHECK(now_function_);
  OSP_DCHECK(task_runner_);
  OSP_DCHECK(socket_);
}

Environment::~Environment() = default;

void Environment::ResumeIncomingPackets(PacketConsumer* packet_consumer) {
  OSP_DCHECK(packet_consumer);
  OSP_DCHECK(!packet_consumer_);
  packet_consumer_ = packet_consumer;

  // TODO: Hook-up with platform APIs to receive packets.
  OSP_UNIMPLEMENTED();
}

void Environment::SuspendIncomingPackets() {
  // TODO: Hook-up with platform APIs to stop receiving packets.
  OSP_UNIMPLEMENTED();

  packet_consumer_ = nullptr;
}

int Environment::GetMaxPacketSize() const {
  // Return hard-coded values for UDP over wired Ethernet (which is a smaller
  // MTU than typical defaults for UDP over 802.11 wireless). Performance would
  // be more-optimized if the network were probed for the actual value. See
  // discussion in rtp_defines.h.
  switch (remote_endpoint_.address.version()) {
    case IPAddress::Version::kV4:
      return kMaxRtpPacketSizeForIpv4UdpOnEthernet;
    case IPAddress::Version::kV6:
      return kMaxRtpPacketSizeForIpv6UdpOnEthernet;
    default:
      OSP_NOTREACHED();
      return 0;
  }
}

void Environment::SendPacket(absl::Span<const uint8_t> packet) {
  OSP_DCHECK(remote_endpoint_.address);
  OSP_DCHECK_NE(remote_endpoint_.port, 0);
  socket_->SendMessage(packet.data(), packet.size(), remote_endpoint_);
}

Environment::PacketConsumer::~PacketConsumer() = default;

}  // namespace cast_streaming
}  // namespace openscreen
