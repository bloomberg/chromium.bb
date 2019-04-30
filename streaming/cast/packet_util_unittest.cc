// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "streaming/cast/packet_util.h"

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

namespace openscreen {
namespace cast_streaming {
namespace {

// Tests that an RTP packet can be identified.
TEST(PacketUtilTest, InspectsRtpPacket) {
  // clang-format off
  const uint8_t kInput[] = {
    0b10000000,  // Version/Padding byte.
    96,  // Payload type byte.
    0xbe, 0xef,  // Sequence number.
    9, 8, 7, 6,  // RTP timestamp.
    1, 2, 3, 4,  // SSRC.
    0b10000000,  // Is key frame, no extensions.
    5,  // Frame ID.
    0xa, 0xb,  // Packet ID.
    0xa, 0xc,  // Max packet ID.
    0xf, 0xe, 0xd, 0xc, 0xb, 0xa, 0x9, 0x8,  // Payload.
  };
  // clang-format on
  const Ssrc kSenderSsrc = 0x01020304;

  const auto result = InspectPacketForRouting(kInput);
  EXPECT_EQ(ApparentPacketType::RTP, result.first);
  EXPECT_EQ(kSenderSsrc, result.second);
}

// Tests that a malformed RTP packet can be identified.
TEST(PacketUtilTest, InspectsMalformedRtpPacket) {
  // clang-format off
  const uint8_t kInput[] = {
    0b11000000,  // BAD: Version/Padding byte.
    96,  // Payload type byte.
    0xbe, 0xef,  // Sequence number.
    9, 8, 7, 6,  // RTP timestamp.
    1, 2, 3, 4,  // SSRC.
    0b10000000,  // Is key frame, no extensions.
    5,  // Frame ID.
    0xa, 0xb,  // Packet ID.
    0xa, 0xc,  // Max packet ID.
    0xf, 0xe, 0xd, 0xc, 0xb, 0xa, 0x9, 0x8,  // Payload.
  };
  // clang-format on

  const auto result = InspectPacketForRouting(kInput);
  EXPECT_EQ(ApparentPacketType::UNKNOWN, result.first);
}

// Tests that an empty packet is classified as unknown.
TEST(PacketUtilTest, InspectsEmptyPacket) {
  const uint8_t kInput[] = {};

  const auto result =
      InspectPacketForRouting(absl::Span<const uint8_t>(kInput, 0));
  EXPECT_EQ(ApparentPacketType::UNKNOWN, result.first);
}

// Tests that an packet with garbage is classified as unknown.
TEST(PacketUtilTest, InspectsGarbagePacket) {
  // clang-format off
  const uint8_t kInput[] = {
    0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef,
    0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef,
    0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef,
    0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef,
  };
  // clang-format on

  const auto result = InspectPacketForRouting(kInput);
  EXPECT_EQ(ApparentPacketType::UNKNOWN, result.first);
}

}  // namespace
}  // namespace cast_streaming
}  // namespace openscreen
