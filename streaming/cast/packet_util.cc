// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "streaming/cast/packet_util.h"

#include "streaming/cast/rtp_defines.h"

namespace openscreen {
namespace cast_streaming {

std::pair<ApparentPacketType, Ssrc> InspectPacketForRouting(
    absl::Span<const uint8_t> packet) {
  // TODO(miu): Detect RTCP packet type in soon-upcoming change.

  // See rtp_defines.h for wire-format diagram.
  if (packet.size() < kRtpPacketMinValidSize ||
      packet[0] != kRtpRequiredFirstByte ||
      !IsRtpPayloadType(packet[1] & kRtpPayloadTypeMask)) {
    return std::make_pair(ApparentPacketType::UNKNOWN, Ssrc{0});
  }
  constexpr int kOffsetToSsrcField = 8;
  return std::make_pair(
      ApparentPacketType::RTP,
      ReadBigEndian<uint32_t>(packet.data() + kOffsetToSsrcField));
}

}  // namespace cast_streaming
}  // namespace openscreen
