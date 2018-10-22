/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"

namespace webrtc {

StreamDataCounters::StreamDataCounters() : first_packet_time_ms(-1) {}

constexpr size_t StreamId::kMaxSize;

// Check if passed character is a "token-char" from RFC 4566.
static bool IsTokenChar(char ch) {
  return ch == 0x21 || (ch >= 0x23 && ch <= 0x27) || ch == 0x2a || ch == 0x2b ||
         ch == 0x2d || ch == 0x2e || (ch >= 0x30 && ch <= 0x39) ||
         (ch >= 0x41 && ch <= 0x5a) || (ch >= 0x5e && ch <= 0x7e);
}

bool StreamId::IsLegalMidName(rtc::ArrayView<const char> name) {
  return (name.size() <= kMaxSize && name.size() > 0 &&
          std::all_of(name.data(), name.data() + name.size(), IsTokenChar));
}

bool StreamId::IsLegalRsidName(rtc::ArrayView<const char> name) {
  return (name.size() <= kMaxSize && name.size() > 0 &&
          std::all_of(name.data(), name.data() + name.size(), isalnum));
}

void StreamId::Set(const char* data, size_t size) {
  // If |data| contains \0, the stream id size might become less than |size|.
  RTC_CHECK_LE(size, kMaxSize);
  memcpy(value_, data, size);
  if (size < kMaxSize)
    value_[size] = 0;
}

// StreamId is used as member of RTPHeader that is sometimes copied with memcpy
// and thus assume trivial destructibility.
static_assert(std::is_trivially_destructible<StreamId>::value, "");

PayloadUnion::PayloadUnion(const AudioPayload& payload) : payload_(payload) {}
PayloadUnion::PayloadUnion(const VideoPayload& payload) : payload_(payload) {}
PayloadUnion::PayloadUnion(const PayloadUnion&) = default;
PayloadUnion::PayloadUnion(PayloadUnion&&) = default;
PayloadUnion::~PayloadUnion() = default;

PayloadUnion& PayloadUnion::operator=(const PayloadUnion&) = default;
PayloadUnion& PayloadUnion::operator=(PayloadUnion&&) = default;

}  // namespace webrtc
