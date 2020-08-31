// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/receiver_packet_router.h"

#include <algorithm>

#include "cast/streaming/packet_util.h"
#include "cast/streaming/receiver.h"
#include "util/osp_logging.h"
#include "util/stringprintf.h"

namespace openscreen {
namespace cast {

ReceiverPacketRouter::ReceiverPacketRouter(Environment* environment)
    : environment_(environment) {
  OSP_DCHECK(environment_);
}

ReceiverPacketRouter::~ReceiverPacketRouter() {
  OSP_DCHECK(receivers_.empty());
}

void ReceiverPacketRouter::OnReceiverCreated(Ssrc sender_ssrc,
                                             Receiver* receiver) {
  OSP_DCHECK(FindEntry(sender_ssrc) == receivers_.end());
  receivers_.emplace_back(sender_ssrc, receiver);

  // If there were no Receiver instances before, resume receiving packets for
  // dispatch. Reset/Clear the remote endpoint, in preparation for later setting
  // it to the source of the first packet received.
  if (receivers_.size() == 1) {
    environment_->set_remote_endpoint(IPEndpoint{});
    environment_->ConsumeIncomingPackets(this);
  }
}

void ReceiverPacketRouter::OnReceiverDestroyed(Ssrc sender_ssrc) {
  const auto it = FindEntry(sender_ssrc);
  OSP_DCHECK(it != receivers_.end());
  receivers_.erase(it);

  // If there are no longer any Receivers, suspend receiving packets.
  if (receivers_.empty()) {
    environment_->DropIncomingPackets();
  }
}

void ReceiverPacketRouter::SendRtcpPacket(absl::Span<const uint8_t> packet) {
  OSP_DCHECK(InspectPacketForRouting(packet).first == ApparentPacketType::RTCP);

  // Do not proceed until the remote endpoint is known. See OnReceivedPacket().
  if (environment_->remote_endpoint().port == 0) {
    return;
  }

  environment_->SendPacket(packet);
}

void ReceiverPacketRouter::OnReceivedPacket(const IPEndpoint& source,
                                            Clock::time_point arrival_time,
                                            std::vector<uint8_t> packet) {
  OSP_DCHECK_NE(source.port, uint16_t{0});

  // If the sender endpoint is known, ignore any packet that did not come from
  // that same endpoint.
  if (environment_->remote_endpoint().port != 0) {
    if (source != environment_->remote_endpoint()) {
      return;
    }
  }

  const std::pair<ApparentPacketType, Ssrc> seems_like =
      InspectPacketForRouting(packet);
  if (seems_like.first == ApparentPacketType::UNKNOWN) {
    constexpr int kMaxPartiaHexDumpSize = 96;
    OSP_LOG_WARN << "UNKNOWN packet of " << packet.size()
                 << " bytes. Partial hex dump: "
                 << HexEncode(absl::Span<const uint8_t>(packet).subspan(
                        0, kMaxPartiaHexDumpSize));
    return;
  }
  const auto it = FindEntry(seems_like.second);
  if (it != receivers_.end()) {
    // At this point, a valid packet has been matched with a receiver. Lock-in
    // the remote endpoint as the |source| of this |packet| so that only packets
    // from the same source are permitted from here onwards.
    if (environment_->remote_endpoint().port == 0) {
      environment_->set_remote_endpoint(source);
    }

    if (seems_like.first == ApparentPacketType::RTP) {
      it->second->OnReceivedRtpPacket(arrival_time, std::move(packet));
    } else if (seems_like.first == ApparentPacketType::RTCP) {
      it->second->OnReceivedRtcpPacket(arrival_time, std::move(packet));
    }
  }
}

ReceiverPacketRouter::ReceiverEntries::iterator ReceiverPacketRouter::FindEntry(
    Ssrc sender_ssrc) {
  return std::find_if(receivers_.begin(), receivers_.end(),
                      [sender_ssrc](const ReceiverEntries::value_type& entry) {
                        return entry.first == sender_ssrc;
                      });
}

}  // namespace cast
}  // namespace openscreen
