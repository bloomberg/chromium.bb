// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/session_config.h"

namespace openscreen {
namespace cast {

SessionConfig::SessionConfig(Ssrc sender_ssrc,
                             Ssrc receiver_ssrc,
                             int rtp_timebase,
                             int channels,
                             std::chrono::milliseconds target_playout_delay,
                             std::array<uint8_t, 16> aes_secret_key,
                             std::array<uint8_t, 16> aes_iv_mask)
    : sender_ssrc(sender_ssrc),
      receiver_ssrc(receiver_ssrc),
      rtp_timebase(rtp_timebase),
      channels(channels),
      target_playout_delay(target_playout_delay),
      aes_secret_key(aes_secret_key),
      aes_iv_mask(aes_iv_mask) {}

}  // namespace cast
}  // namespace openscreen
