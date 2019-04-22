/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <inttypes.h>
#include <stdio.h>
#include <algorithm>
#include <string>

#include "modules/congestion_controller/goog_cc/congestion_window_pushback_controller.h"
#include "rtc_base/checks.h"
#include "rtc_base/experiments/rate_control_settings.h"

namespace webrtc {

CongestionWindowPushbackController::CongestionWindowPushbackController(
    const WebRtcKeyValueConfig* key_value_config)
    : add_pacing_(
          key_value_config->Lookup("WebRTC-AddPacingToCongestionWindowPushback")
              .find("Enabled") == 0),
      min_pushback_target_bitrate_bps_(
          RateControlSettings::ParseFromKeyValueConfig(key_value_config)
              .CongestionWindowMinPushbackTargetBitrateBps()) {}

CongestionWindowPushbackController::CongestionWindowPushbackController(
    const WebRtcKeyValueConfig* key_value_config,
    uint32_t min_pushback_target_bitrate_bps)
    : add_pacing_(
          key_value_config->Lookup("WebRTC-AddPacingToCongestionWindowPushback")
              .find("Enabled") == 0),
      min_pushback_target_bitrate_bps_(min_pushback_target_bitrate_bps) {}

void CongestionWindowPushbackController::UpdateOutstandingData(
    int64_t outstanding_bytes) {
  outstanding_bytes_ = outstanding_bytes;
}
void CongestionWindowPushbackController::UpdatePacingQueue(
    int64_t pacing_bytes) {
  pacing_bytes_ = pacing_bytes;
}

void CongestionWindowPushbackController::UpdateMaxOutstandingData(
    size_t max_outstanding_bytes) {
  DataSize data_window = DataSize::bytes(max_outstanding_bytes);
  if (current_data_window_) {
    data_window = (data_window + current_data_window_.value()) / 2;
  }
  current_data_window_ = data_window;
}

void CongestionWindowPushbackController::SetDataWindow(DataSize data_window) {
  current_data_window_ = data_window;
}

uint32_t CongestionWindowPushbackController::UpdateTargetBitrate(
    uint32_t bitrate_bps) {
  if (!current_data_window_ || current_data_window_->IsZero())
    return bitrate_bps;
  int64_t total_bytes = outstanding_bytes_;
  if (add_pacing_)
    total_bytes += pacing_bytes_;
  double fill_ratio =
      total_bytes / static_cast<double>(current_data_window_->bytes());
  if (fill_ratio > 1.5) {
    encoding_rate_ratio_ *= 0.9;
  } else if (fill_ratio > 1) {
    encoding_rate_ratio_ *= 0.95;
  } else if (fill_ratio < 0.1) {
    encoding_rate_ratio_ = 1.0;
  } else {
    encoding_rate_ratio_ *= 1.05;
    encoding_rate_ratio_ = std::min(encoding_rate_ratio_, 1.0);
  }
  uint32_t adjusted_target_bitrate_bps =
      static_cast<uint32_t>(bitrate_bps * encoding_rate_ratio_);

  // Do not adjust below the minimum pushback bitrate but do obey if the
  // original estimate is below it.
  bitrate_bps = adjusted_target_bitrate_bps < min_pushback_target_bitrate_bps_
                    ? std::min(bitrate_bps, min_pushback_target_bitrate_bps_)
                    : adjusted_target_bitrate_bps;
  return bitrate_bps;
}

}  // namespace webrtc
