// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/rtp_defines.h"

namespace openscreen {
namespace cast {

bool IsRtpPayloadType(uint8_t raw_byte) {
  switch (static_cast<RtpPayloadType>(raw_byte)) {
    case RtpPayloadType::kAudioOpus:
    case RtpPayloadType::kAudioAac:
    case RtpPayloadType::kAudioPcm16:
    case RtpPayloadType::kAudioVarious:
    case RtpPayloadType::kVideoVp8:
    case RtpPayloadType::kVideoH264:
    case RtpPayloadType::kVideoVarious:
    case RtpPayloadType::kAudioHackForAndroidTV:
      // Note: RtpPayloadType::kVideoHackForAndroidTV has the same value as
      // kAudioOpus.
      return true;

    case RtpPayloadType::kNull:
      break;
  }
  return false;
}

bool IsRtcpPacketType(uint8_t raw_byte) {
  switch (static_cast<RtcpPacketType>(raw_byte)) {
    case RtcpPacketType::kSenderReport:
    case RtcpPacketType::kReceiverReport:
    case RtcpPacketType::kSourceDescription:
    case RtcpPacketType::kApplicationDefined:
    case RtcpPacketType::kPayloadSpecific:
    case RtcpPacketType::kExtendedReports:
      return true;

    case RtcpPacketType::kNull:
      break;
  }
  return false;
}

}  // namespace cast
}  // namespace openscreen
