// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STREAMING_RTCP_COMMON_H_
#define CAST_STREAMING_RTCP_COMMON_H_

#include <stdint.h>

#include <tuple>
#include <vector>

#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "cast/streaming/frame_id.h"
#include "cast/streaming/ntp_time.h"
#include "cast/streaming/rtp_defines.h"
#include "cast/streaming/rtp_time.h"
#include "cast/streaming/ssrc.h"

namespace openscreen {
namespace cast {

struct RtcpCommonHeader {
  RtcpCommonHeader();
  ~RtcpCommonHeader();

  RtcpPacketType packet_type = RtcpPacketType::kNull;

  union {
    // The number of report blocks if |packet_type| is kSenderReport or
    // kReceiverReport.
    int report_count;

    // Indicates the type of an application-defined message if |packet_type| is
    // kApplicationDefined or kPayloadSpecific.
    RtcpSubtype subtype;

    // Otherwise, not used.
  } with{0};

  // The size (in bytes) of the RTCP packet, not including the header.
  int payload_size = 0;

  // Serializes this header into the first |kRtcpCommonHeaderSize| bytes of the
  // given |buffer| and adjusts |buffer| to point to the first byte after it.
  void AppendFields(absl::Span<uint8_t>* buffer) const;

  // Parse from the 4-byte wire format in |buffer|. Returns nullopt if the data
  // is corrupt.
  static absl::optional<RtcpCommonHeader> Parse(
      absl::Span<const uint8_t> buffer);
};

// The middle 32-bits of the 64-bit NtpTimestamp field from the Sender Reports.
// This is used as an opaque identifier that the Receiver will use in its
// reports to refer to specific previous Sender Reports.
using StatusReportId = uint32_t;
constexpr StatusReportId ToStatusReportId(NtpTimestamp ntp_timestamp) {
  return static_cast<uint32_t>(ntp_timestamp >> 16);
}

// One of these is optionally included with a Sender Report or a Receiver
// Report. See: https://tools.ietf.org/html/rfc3550#section-6.4.1
struct RtcpReportBlock {
  RtcpReportBlock();
  ~RtcpReportBlock();

  // The intended recipient of this report block.
  Ssrc ssrc = 0;

  // The fraction of RTP packets lost since the last report, specified as a
  // variable numerator and fixed denominator. The numerator will always be in
  // the range [0,255] since, semantically:
  //
  //   a. Negative values are impossible.
  //   b. Values greater than 255 would indicate 100% packet loss, and so a
  //      report block would not be generated in the first place.
  int packet_fraction_lost_numerator = 0;
  static constexpr int kPacketFractionLostDenominator = 256;

  // The total number of RTP packets lost since the start of the session. This
  // value will always be in the range [0,2^24-1], as the wire format only
  // provides 24 bits; so, wrap-around is possible.
  int cumulative_packets_lost = 0;

  // The highest sequence number received in any RTP packet. Wrap-around is
  // possible.
  uint32_t extended_high_sequence_number = 0;

  // An estimate of the recent variance in RTP packet arrival times.
  RtpTimeDelta jitter;

  // The last Status Report received.
  StatusReportId last_status_report_id{};

  // The delay between when the peer received the most-recent Status Report and
  // when this report was sent. The timebase is 65536 ticks per second and,
  // because of the wire format, this value will always be in the range
  // [0,65536) seconds.
  using Delay = std::chrono::duration<int64_t, std::ratio<1, 65536>>;
  Delay delay_since_last_report{};

  // Convenience helper to compute/assign the |packet_fraction_lost_numerator|,
  // based on the |num_apparently_sent| and |num_received| packet counts since
  // the last report was sent.
  void SetPacketFractionLostNumerator(int64_t num_apparently_sent,
                                      int64_t num_received);

  // Convenience helper to compute/assign the |cumulative_packets_lost|, based
  // on the |num_apparently_sent| and |num_received| packet counts since the
  // start of the entire session.
  void SetCumulativePacketsLost(int64_t num_apparently_sent,
                                int64_t num_received);

  // Convenience helper to convert the given |local_clock_delay| to the
  // RtcpReportBlock::Delay timebase, then clamp and assign it to
  // |delay_since_last_report|.
  void SetDelaySinceLastReport(Clock::duration local_clock_delay);

  // Serializes this report block in the first |kRtcpReportBlockSize| bytes of
  // the given |buffer| and adjusts |buffer| to point to the first byte after
  // it.
  void AppendFields(absl::Span<uint8_t>* buffer) const;

  // Scans the wire-format report blocks in |buffer|, searching for one with the
  // matching |ssrc| and, if found, returns the parse result. Returns nullopt if
  // the data is corrupt or no report block with the matching SSRC was found.
  static absl::optional<RtcpReportBlock>
  ParseOne(absl::Span<const uint8_t> buffer, int report_count, Ssrc ssrc);
};

struct RtcpSenderReport {
  RtcpSenderReport();
  ~RtcpSenderReport();

  // The point-in-time at which this report was sent, according to both: 1) the
  // common reference clock shared by all RTP streams; 2) the RTP timestamp on
  // the media capture/playout timeline. Together, these are used by a Receiver
  // to achieve A/V synchronization across RTP streams for playout.
  Clock::time_point reference_time{};
  RtpTimeTicks rtp_timestamp;

  // The total number of RTP packets transmitted since the start of the session
  // (wrap-around is possible).
  uint32_t send_packet_count = 0;

  // The total number of payload bytes transmitted in RTP packets since the
  // start of the session (wrap-around is possible).
  uint32_t send_octet_count = 0;

  // The report block, if present. While the RTCP spec allows for zero or
  // multiple reports, Cast Streaming only uses zero or one.
  absl::optional<RtcpReportBlock> report_block;
};

// A pair of IDs that refers to a specific missing packet within a frame. If
// |packet_id| is kAllPacketsLost, then it represents all the packets of a
// frame.
struct PacketNack {
  FrameId frame_id;
  FramePacketId packet_id;

  constexpr bool operator==(const PacketNack& other) const {
    return frame_id == other.frame_id && packet_id == other.packet_id;
  }
  constexpr bool operator!=(const PacketNack& other) const {
    return frame_id != other.frame_id || packet_id != other.packet_id;
  }
  constexpr bool operator<(const PacketNack& other) const {
    return (frame_id < other.frame_id) ||
           (frame_id == other.frame_id && packet_id < other.packet_id);
  }
};

}  // namespace cast
}  // namespace openscreen

#endif  // CAST_STREAMING_RTCP_COMMON_H_
