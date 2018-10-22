/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/goog_cc/acknowledged_bitrate_estimator.h"

#include <utility>

#include "absl/memory/memory.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "rtc_base/numerics/safe_conversions.h"

namespace webrtc {

namespace {
bool IsInSendTimeHistory(const PacketFeedback& packet) {
  return packet.send_time_ms != PacketFeedback::kNoSendTime;
}
}  // namespace

AcknowledgedBitrateEstimator::AcknowledgedBitrateEstimator()
    : AcknowledgedBitrateEstimator(absl::make_unique<BitrateEstimator>()) {}

AcknowledgedBitrateEstimator::~AcknowledgedBitrateEstimator() {}

AcknowledgedBitrateEstimator::AcknowledgedBitrateEstimator(
    std::unique_ptr<BitrateEstimator> bitrate_estimator)
    : bitrate_estimator_(std::move(bitrate_estimator)) {}

void AcknowledgedBitrateEstimator::IncomingPacketFeedbackVector(
    const std::vector<PacketFeedback>& packet_feedback_vector) {
  RTC_DCHECK(std::is_sorted(packet_feedback_vector.begin(),
                            packet_feedback_vector.end(),
                            PacketFeedbackComparator()));
  for (const auto& packet : packet_feedback_vector) {
    if (IsInSendTimeHistory(packet)) {
      MaybeExpectFastRateChange(packet.send_time_ms);
      bitrate_estimator_->Update(packet.arrival_time_ms,
                                 rtc::dchecked_cast<int>(packet.payload_size));
    }
  }
}

absl::optional<uint32_t> AcknowledgedBitrateEstimator::bitrate_bps() const {
  auto estimated_bitrate = bitrate_estimator_->bitrate_bps();
  return estimated_bitrate
             ? *estimated_bitrate +
                   allocated_bitrate_without_feedback_bps_.value_or(0)
             : estimated_bitrate;
}

void AcknowledgedBitrateEstimator::SetAlrEndedTimeMs(
    int64_t alr_ended_time_ms) {
  alr_ended_time_ms_.emplace(alr_ended_time_ms);
}

void AcknowledgedBitrateEstimator::SetAllocatedBitrateWithoutFeedback(
    uint32_t bitrate_bps) {
  allocated_bitrate_without_feedback_bps_.emplace(bitrate_bps);
}

void AcknowledgedBitrateEstimator::MaybeExpectFastRateChange(
    int64_t packet_send_time_ms) {
  if (alr_ended_time_ms_ && packet_send_time_ms > *alr_ended_time_ms_) {
    bitrate_estimator_->ExpectFastRateChange();
    alr_ended_time_ms_.reset();
  }
}

}  // namespace webrtc
