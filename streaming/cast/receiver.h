// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STREAMING_CAST_RECEIVER_H_
#define STREAMING_CAST_RECEIVER_H_

#include <stdint.h>

#include <array>
#include <chrono>
#include <memory>

#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "platform/api/time.h"
#include "streaming/cast/clock_drift_smoother.h"
#include "streaming/cast/compound_rtcp_builder.h"
#include "streaming/cast/environment.h"
#include "streaming/cast/frame_id.h"
#include "streaming/cast/packet_receive_stats_tracker.h"
#include "streaming/cast/rtcp_common.h"
#include "streaming/cast/rtcp_session.h"
#include "streaming/cast/sender_report_parser.h"
#include "streaming/cast/ssrc.h"
#include "util/alarm.h"

namespace openscreen {
namespace cast_streaming {

struct EncodedFrame;
class ReceiverPacketRouter;

// The Cast Streaming Receiver, a peer corresponding to some Cast Streaming
// Sender at the other end of a network link.
//
// Cast Streaming is a transport protocol which divides up the frames for one
// media stream (e.g., audio or video) into multiple RTP packets containing an
// encrypted payload. The Receiver is the peer responsible for collecting the
// RTP packets, decrypting the payload, and re-assembling a frame that can be
// passed to a decoder and played out.
//
// A Sender ↔ Receiver pair is used to transport each media stream. Typically,
// there are two pairs in a normal system, one for the audio stream and one for
// video stream. A local player is responsible for synchronizing the playout of
// the frames of each stream to achieve lip-sync. See the discussion in
// encoded_frame.h for how the |reference_time| and |rtp_timestamp| of the
// EncodedFrames are used to achieve this.
//
// See the Receiver Demo app for a reference implementation that both shows and
// explains how Receivers are properly configured and started, integrated with a
// decoder, and the resulting decoded media is played out. Also, here is a
// general usage example:
//
//   class MyPlayer : public cast_streaming::Receiver::Consumer {
//    public:
//     explicit MyPlayer(Receiver* receiver) : receiver_(receiver) {
//       recevier_->SetPlayerProcessingTime(std::chrono::milliseconds(10));
//       receiver_->SetConsumer(this);
//     }
//
//     ~MyPlayer() override {
//       receiver_->SetConsumer(nullptr);
//     }
//
//    private:
//     // Receiver::Consumer implementation.
//     void OnFramesComplete() override {
//       std::vector<uint8_t> buffer;
//       for (;;) {
//         const int buffer_size_needed = receiver_->AdvanceToNextFrame();
//         if (buffer_size == Receiver::kNoFramesReady) {
//           break;  // Consumed all the frames.
//         }
//
//         buffer.resize(buffer_size_needed);
//         openscreen::cast_streaming::EncodedFrame encoded_frame;
//         encoded_frame.data = absl::Span<uint8_t>(buffer);
//         receiver_->ConsumeNextFrame(&encoded_frame);
//
//         display_.RenderFrame(decoder_.DecodeFrame(encoded_frame.data));
//       }
//     }
//
//     Receiver* const receiver_;
//     MyDecoder decoder_;
//     MyDisplay display_;
//   };
//
// Internally, a queue of complete and partially-received frames is maintained.
// The queue is a circular queue of FrameCollectors that each maintain the
// individual receive state of each in-flight frame. There are three conceptual
// "pointers" that indicate what assumptions and operations are made on certain
// ranges of frames in the queue:
//
//   1. Latest Frame Expected: The FrameId of the latest frame whose existence
//      is known to this Receiver. This is the highest FrameId seen in any
//      successfully-parsed RTP packet.
//   2. Checkpoint Frame: Indicates that all of the RTP packets for all frames
//      up to and including the one having this FrameId have been successfully
//      received and processed.
//   3. Last Frame Consumed: The FrameId of last frame consumed (see
//      ConsumeNextFrame()). Once a frame is consumed, all internal resources
//      related to the frame can be freed and/or re-used for later frames.
class Receiver {
 public:
  class Consumer {
   public:
    virtual ~Consumer();
    virtual void OnFramesComplete() = 0;
  };

  // Constructs a Receiver that attaches to the given |environment| and
  // |packet_router|. The remaining arguments provide the configuration that was
  // agreed-upon by both sides from the OFFER/ANSWER exchange (i.e., the part of
  // the overall end-to-end connection process that occurs before Cast Streaming
  // is started).
  Receiver(Environment* environment,
           ReceiverPacketRouter* packet_router,
           Ssrc sender_ssrc,
           Ssrc receiver_ssrc,
           int rtp_timebase,
           std::chrono::milliseconds initial_target_playout_delay,
           const std::array<uint8_t, 16>& aes_key,
           const std::array<uint8_t, 16>& cast_iv_mask);

  ~Receiver();

  Ssrc ssrc() const { return rtcp_session_.receiver_ssrc(); }

  // Set the Consumer receiving notifications when new frames are ready for
  // consumption. Frames received before this method is called will remain in
  // the queue indefinitely.
  void SetConsumer(Consumer* consumer);

  // Sets how much time the consumer will need to decode/buffer/render/etc., and
  // otherwise fully process a frame for on-time playback. This information is
  // used by the Receiver to decide whether to skip past frames that have
  // arrived too late. This method can be called repeatedly to make adjustments
  // based on changing environmental conditions.
  //
  // Default setting: kDefaultPlayerProcessingTime
  void SetPlayerProcessingTime(platform::Clock::duration needed_time);

  // Propagates a "picture loss indicator" notification to the Sender,
  // requesting a key frame so that decode/playout can recover. It is safe to
  // call this redundantly. The Receiver will clear the picture loss condition
  // automatically, once a key frame is received (i.e., before
  // ConsumeNextFrame() is called to access it).
  void RequestKeyFrame();

  // Advances to the next frame ready for consumption. This may skip-over
  // incomplete frames that will not play out on-time; but only if there are
  // completed frames further down the queue that have no dependency
  // relationship with them (e.g., key frames).
  //
  // This method returns kNoFramesReady if there is not currently a frame ready
  // for consumption. The caller can wait for a Consumer::OnFramesComplete()
  // notification, or poll this method again later. Otherwise, the number of
  // bytes of encoded data is returned, and the caller should use this to ensure
  // the buffer it passes to ConsumeNextFrame() is large enough.
  int AdvanceToNextFrame();

  // Populates the given |frame| with the next frame, both metadata and payload
  // data. AdvanceToNextFrame() should have been called just before this method,
  // and |frame->data| must point to a sufficiently-sized buffer that will be
  // populated with the frame's payload data. Upon return |frame->data| will be
  // set to the portion of the buffer that was populated.
  void ConsumeNextFrame(EncodedFrame* frame);

  // The default "player processing time" amount. See SetPlayerProcessingTime().
  static constexpr std::chrono::milliseconds kDefaultPlayerProcessingTime{5};

  // Returned by AdvanceToNextFrame() when there are no frames currently ready
  // for consumption.
  static constexpr int kNoFramesReady = -1;

 protected:
  friend class ReceiverPacketRouter;

  // Called by ReceiverPacketRouter to provide this Receiver with what looks
  // like a RTP/RTCP packet meant for it specifically (among other Receivers).
  void OnReceivedRtpPacket(platform::Clock::time_point arrival_time,
                           std::vector<uint8_t> packet);
  void OnReceivedRtcpPacket(platform::Clock::time_point arrival_time,
                            std::vector<uint8_t> packet);

 private:
  // Get the checkpoint FrameId. This indicates that all of the packets for all
  // frames up to and including this FrameId have been successfully received (or
  // otherwise do not need to be re-transmitted).
  FrameId checkpoint_frame() const { return rtcp_builder_.checkpoint_frame(); }

  // Send an RTCP packet to the Sender immediately, to acknowledge the complete
  // reception of one or more additional frames, to reply to a Sender Report, or
  // to request re-transmits. Calling this also schedules additional RTCP
  // packets to be sent periodically for the life of this Receiver.
  void SendRtcp();

  const platform::ClockNowFunctionPtr now_;
  ReceiverPacketRouter* const packet_router_;
  RtcpSession rtcp_session_;
  SenderReportParser rtcp_parser_;
  CompoundRtcpBuilder rtcp_builder_;
  PacketReceiveStatsTracker stats_tracker_;  // Tracks transmission stats.

  // Buffer for serializing/sending RTCP packets.
  const int rtcp_buffer_capacity_;
  const std::unique_ptr<uint8_t[]> rtcp_buffer_;

  // Schedules tasks to ensure RTCP reports are sent within a bounded interval.
  // Not scheduled until after this Receiver has processed the first packet from
  // the Sender.
  Alarm rtcp_alarm_;
  platform::Clock::time_point last_rtcp_send_time_ =
      platform::Clock::time_point::min();

  // The last Sender Report received and when the packet containing it had
  // arrived. This contains lip-sync timestamps used as part of the calculation
  // of playout times for the received frames, as well as ping-pong data bounced
  // back to the Sender in the Receiver Reports. It is nullopt until the first
  // parseable Sender Report is received.
  absl::optional<SenderReportParser::SenderReportWithId> last_sender_report_;
  platform::Clock::time_point last_sender_report_arrival_time_;

  // Tracks the offset between the Receiver's [local] clock and the Sender's
  // clock. This is invalid until the first Sender Report has been successfully
  // processed (i.e., |last_sender_report_| is not nullopt).
  ClockDriftSmoother smoothed_clock_offset_;

  // The interval between sending ACK/NACK feedback RTCP messages while
  // incomplete frames exist in the queue.
  static constexpr std::chrono::milliseconds kNackFeedbackInterval{30};
};

}  // namespace cast_streaming
}  // namespace openscreen

#endif  // STREAMING_CAST_RECEIVER_H_
