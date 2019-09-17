// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "streaming/cast/receiver.h"

#include <functional>

#include "absl/types/span.h"
#include "platform/api/logging.h"
#include "streaming/cast/constants.h"
#include "streaming/cast/receiver_packet_router.h"

using openscreen::platform::Clock;

using std::chrono::duration_cast;
using std::chrono::microseconds;
using std::chrono::milliseconds;

namespace openscreen {
namespace cast_streaming {

// Conveniences for ensuring logging output includes the SSRC of the Receiver,
// to help distinguish one out of multiple instances in a Cast Streaming
// session.
//
// TODO(miu): Replace RECEIVER_VLOG's with trace event logging once the tracing
// infrastructure is ready.
#define RECEIVER_LOG(level) OSP_LOG_##level << "[SSRC:" << ssrc() << "] "
#define RECEIVER_VLOG OSP_VLOG << "[SSRC:" << ssrc() << "] "
#define RECEIVER_LOG_IF(level, condition) \
  OSP_LOG_IF(level, condition) << "[SSRC:" << ssrc() << "] "

Receiver::Receiver(Environment* environment,
                   ReceiverPacketRouter* packet_router,
                   Ssrc sender_ssrc,
                   Ssrc receiver_ssrc,
                   int rtp_timebase,
                   milliseconds initial_target_playout_delay,
                   const std::array<uint8_t, 16>& aes_key,
                   const std::array<uint8_t, 16>& cast_iv_mask)
    : now_(environment->now_function()),
      packet_router_(packet_router),
      rtcp_session_(sender_ssrc, receiver_ssrc, now_()),
      rtcp_parser_(&rtcp_session_),
      rtcp_builder_(&rtcp_session_),
      stats_tracker_(rtp_timebase),
      rtcp_buffer_capacity_(environment->GetMaxPacketSize()),
      rtcp_buffer_(new uint8_t[rtcp_buffer_capacity_]),
      rtcp_alarm_(environment->now_function(), environment->task_runner()),
      smoothed_clock_offset_(ClockDriftSmoother::kDefaultTimeConstant) {
  OSP_DCHECK(packet_router_);
  OSP_DCHECK_EQ(checkpoint_frame(), FrameId::first() - 1);
  OSP_CHECK_GT(rtcp_buffer_capacity_, 0);
  OSP_CHECK(rtcp_buffer_);

  rtcp_builder_.SetPlayoutDelay(initial_target_playout_delay);

  packet_router_->OnReceiverCreated(rtcp_session_.sender_ssrc(), this);
}

Receiver::~Receiver() {
  packet_router_->OnReceiverDestroyed(rtcp_session_.sender_ssrc());
}

void Receiver::SetConsumer(Consumer* consumer) {
  OSP_UNIMPLEMENTED();
}

void Receiver::SetPlayerProcessingTime(Clock::duration needed_time) {
  OSP_UNIMPLEMENTED();
}

void Receiver::RequestKeyFrame() {
  rtcp_builder_.SetPictureLossIndicator(true);
}

int Receiver::AdvanceToNextFrame() {
  OSP_UNIMPLEMENTED();
  return -1;
}

void Receiver::ConsumeNextFrame(EncodedFrame* frame) {
  OSP_UNIMPLEMENTED();
}

void Receiver::OnReceivedRtpPacket(Clock::time_point arrival_time,
                                   std::vector<uint8_t> packet) {
  OSP_UNIMPLEMENTED();
}

void Receiver::OnReceivedRtcpPacket(Clock::time_point arrival_time,
                                    std::vector<uint8_t> packet) {
  absl::optional<SenderReportParser::SenderReportWithId> parsed_report =
      rtcp_parser_.Parse(packet);
  if (!parsed_report) {
    RECEIVER_LOG(WARN) << "Parsing of " << packet.size()
                       << " bytes as an RTCP packet failed.";
    return;
  }
  last_sender_report_ = std::move(parsed_report);
  last_sender_report_arrival_time_ = arrival_time;

  // Measure the offset between the Sender's clock and the Receiver's Clock.
  // This will be used to translate reference timestamps from the Sender into
  // timestamps that represent the exact same moment in time at the Receiver.
  //
  // Note: Due to design limitations in the Cast Streaming spec, the Receiver
  // has no way to compute how long it took the Sender Report to travel over the
  // network. The calculation here just ignores that, and so the
  // |measured_offset| below will be larger than the true value by that amount.
  // This will have the effect of a later-than-configured playout delay.
  const Clock::duration measured_offset =
      arrival_time - last_sender_report_->reference_time;
  smoothed_clock_offset_.Update(arrival_time, measured_offset);
  RECEIVER_VLOG
      << "Received Sender Report: Local clock is ahead of Sender's by "
      << duration_cast<microseconds>(smoothed_clock_offset_.Current()).count()
      << " µs (minus one-way network transit time).";

  RtcpReportBlock report;
  report.ssrc = rtcp_session_.sender_ssrc();
  stats_tracker_.PopulateNextReport(&report);
  report.last_status_report_id = last_sender_report_->report_id;
  report.SetDelaySinceLastReport(now_() - last_sender_report_arrival_time_);
  rtcp_builder_.IncludeReceiverReportInNextPacket(report);

  SendRtcp();
}

void Receiver::SendRtcp() {
  // Collect ACK/NACK feedback for all active frames in the queue.
  std::vector<PacketNack> packet_nacks;
  std::vector<FrameId> frame_acks;
  // TODO(miu): Scan frame queue for missing packets and completed frames in
  // soon-upcoming commits.

  // Build and send a compound RTCP packet.
  const bool no_nacks = packet_nacks.empty();
  rtcp_builder_.IncludeFeedbackInNextPacket(std::move(packet_nacks),
                                            std::move(frame_acks));
  last_rtcp_send_time_ = now_();
  packet_router_->SendRtcpPacket(rtcp_builder_.BuildPacket(
      last_rtcp_send_time_,
      absl::Span<uint8_t>(rtcp_buffer_.get(), rtcp_buffer_capacity_)));
  RECEIVER_VLOG << "Sent RTCP packet.";

  // Schedule the automatic sending of another RTCP packet, if this method is
  // not called within some bounded amount of time. While incomplete frames
  // exist in the queue, send RTCP packets (with ACK/NACK feedback) frequently.
  // When there are no incomplete frames, use a longer "keepalive" interval.
  const Clock::duration interval =
      (no_nacks ? kRtcpReportInterval : kNackFeedbackInterval);
  rtcp_alarm_.Schedule(std::bind(&Receiver::SendRtcp, this),
                       last_rtcp_send_time_ + interval);
}

Receiver::Consumer::~Consumer() = default;

// static
constexpr milliseconds Receiver::kNackFeedbackInterval;

}  // namespace cast_streaming
}  // namespace openscreen
