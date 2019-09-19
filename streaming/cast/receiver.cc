// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "streaming/cast/receiver.h"

#include <algorithm>
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
      rtp_parser_(sender_ssrc),
      rtp_timebase_(rtp_timebase),
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
  consumer_ = consumer;
}

void Receiver::SetPlayerProcessingTime(Clock::duration needed_time) {
  OSP_UNIMPLEMENTED();
}

void Receiver::RequestKeyFrame() {
  if (!rtcp_builder_.is_picture_loss_indicator_set()) {
    rtcp_builder_.SetPictureLossIndicator(true);
    SendRtcp();
  }
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
  const absl::optional<RtpPacketParser::ParseResult> part =
      rtp_parser_.Parse(packet);
  if (!part) {
    RECEIVER_LOG(WARN) << "Parsing of " << packet.size()
                       << " bytes as an RTP packet failed.";
    return;
  }
  stats_tracker_.OnReceivedValidRtpPacket(part->sequence_number,
                                          part->rtp_timestamp, arrival_time);

  // Ignore packets for frames the Receiver is no longer interested in.
  if (part->frame_id <= checkpoint_frame()) {
    return;
  }

  // Extend the range of frames known to this Receiver, within the capacity of
  // this Receiver's queue. Prepare the FrameCollectors to receive any
  // newly-discovered frames.
  if (part->frame_id > latest_frame_expected_) {
    const FrameId max_allowed_frame_id =
        last_frame_consumed_ + kMaxUnackedFrames;
    if (part->frame_id > max_allowed_frame_id) {
      RECEIVER_VLOG << "Dropping RTP packet for " << part->frame_id
                    << ": Too many frames are already in-flight.";
      return;
    }
    do {
      ++latest_frame_expected_;
      GetQueueEntry(latest_frame_expected_)
          ->collector.set_frame_id(latest_frame_expected_);
    } while (latest_frame_expected_ < part->frame_id);
    RECEIVER_VLOG << "Advanced latest frame expected to "
                  << latest_frame_expected_;
  }

  // Start-up edge case: Blatantly drop the first packet of all frames until the
  // Receiver has processed at least one Sender Report containing the necessary
  // clock-drift and lip-sync information (see OnReceivedRtcpPacket()). This is
  // an inescapable data dependency. Note that this special case should almost
  // never trigger, since a well-behaving Sender will send the first Sender
  // Report RTCP packet before any of the RTP packets.
  if (!last_sender_report_ && part->packet_id == FramePacketId{0}) {
    RECEIVER_LOG(WARN) << "Dropping packet 0 of frame " << part->frame_id
                       << " because it arrived before the first Sender Report.";
    // Note: The Sender will have to re-transmit this dropped packet after the
    // Sender Report to allow the Receiver to move forward.
    return;
  }

  PendingFrame* const pending_frame = GetQueueEntry(part->frame_id);
  FrameCollector& collector = pending_frame->collector;
  if (collector.is_complete()) {
    // An extra, redundant |packet| was received. Do nothing since the frame was
    // already complete.
    return;
  }

  if (!collector.CollectRtpPacket(*part, &packet)) {
    return;  // Bad data in the parsed packet. Ignore it.
  }

  // The first packet in a frame contains timing information critical for
  // computing this frame's (and all future frames') playout time. Process that,
  // but only once.
  if (part->packet_id == FramePacketId{0} &&
      !pending_frame->estimated_capture_time) {
    // Estimate the original capture time of this frame (at the Sender), in
    // terms of the Receiver's clock: First, start with a reference time point
    // from the Sender's clock (the one from the last Sender Report). Then,
    // translate it into the equivalent reference time point in terms of the
    // Receiver's clock by applying the measured offset between the two clocks.
    // Finally, apply the RTP timestamp difference between the Sender Report and
    // this frame to determine what the original capture time of this frame was.
    pending_frame->estimated_capture_time =
        last_sender_report_->reference_time + smoothed_clock_offset_.Current() +
        (part->rtp_timestamp - last_sender_report_->rtp_timestamp)
            .ToDuration<Clock::duration>(rtp_timebase_);

    // If a target playout delay change was included in this packet, record it.
    if (part->new_playout_delay > milliseconds::zero()) {
      RECEIVER_LOG(INFO) << "Target playout delay changes to "
                         << part->new_playout_delay.count() << " ms, as of "
                         << part->frame_id;
      // TODO(miu): In a soon-upcoming commit:
      // RecordNewTargetPlayoutDelay(part->frame_id, part->new_playout_delay);
    }
  }

  if (!collector.is_complete()) {
    return;  // Wait for the rest of the packets to come in.
  }
  const EncryptedFrame& encrypted_frame = collector.PeekAtAssembledFrame();

  // Whenever a key frame has been received, the decoder has what it needs to
  // recover. In this case, clear the PLI condition.
  if (encrypted_frame.dependency == EncryptedFrame::KEY) {
    rtcp_builder_.SetPictureLossIndicator(false);
  }

  // Advance the checkpoint FrameId to the most-recent in the sequence of
  // contiguously completed frames. If the checkpoint advances, send an ACK to
  // the Sender and then notify the Consumer.
  FrameId updated_checkpoint = checkpoint_frame();
  while (updated_checkpoint < latest_frame_expected_) {
    const FrameId next = updated_checkpoint + 1;
    if (!GetQueueEntry(next)->collector.is_complete()) {
      break;
    }
    updated_checkpoint = next;
  }
  if (checkpoint_frame() != updated_checkpoint) {
    RECEIVER_VLOG << "Advancing checkpoint to " << updated_checkpoint;
    AdvanceCheckpointTo(updated_checkpoint);
    if (consumer_) {
      consumer_->OnFramesComplete();
      // WARNING: Nothing should come after this, since the call to
      // OnFramesComplete() could result in re-entrant calls into this Receiver!
      return;
    }
  }
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
  for (FrameId f = checkpoint_frame() + 1; f <= latest_frame_expected_; ++f) {
    FrameCollector& collector = GetQueueEntry(f)->collector;
    if (collector.is_complete()) {
      frame_acks.push_back(f);
    } else {
      collector.GetMissingPackets(&packet_nacks);
    }
  }

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

Receiver::PendingFrame* Receiver::GetQueueEntry(FrameId frame_id) {
  return &pending_frames_[(frame_id - FrameId::first()) %
                          pending_frames_.size()];
}

void Receiver::AdvanceCheckpointTo(FrameId new_checkpoint) {
  OSP_DCHECK_GT(new_checkpoint, checkpoint_frame());
  OSP_DCHECK_LE(new_checkpoint, latest_frame_expected_);

  set_checkpoint_frame(new_checkpoint);
  // TODO(miu): In a soon-upcoming commit:
  // rtcp_builder_.SetPlayoutDelay(ResolveTargetPlayoutDelay(new_checkpoint));
  SendRtcp();
}

Receiver::Consumer::~Consumer() = default;

Receiver::PendingFrame::PendingFrame() = default;
Receiver::PendingFrame::~PendingFrame() = default;

// static
constexpr milliseconds Receiver::kNackFeedbackInterval;

}  // namespace cast_streaming
}  // namespace openscreen
