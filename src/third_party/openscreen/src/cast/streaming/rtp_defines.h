// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STREAMING_RTP_DEFINES_H_
#define CAST_STREAMING_RTP_DEFINES_H_

#include <stdint.h>

#include "cast/streaming/constants.h"

namespace openscreen {
namespace cast {

// Note: Cast Streaming uses a subset of the messages in the RTP/RTCP
// specification, but also adds some of its own extensions. See:
// https://tools.ietf.org/html/rfc3550

// Uniquely identifies one packet within a frame. These are sequence numbers,
// starting at 0. Each Cast RTP packet also includes the "last ID" so that a
// receiver always knows the range of valid FramePacketIds for a given frame.
using FramePacketId = uint16_t;

// A special FramePacketId value meant to represent "all packets lost" in Cast
// RTCP Feedback messages.
constexpr FramePacketId kAllPacketsLost = 0xffff;
constexpr FramePacketId kMaxAllowedFramePacketId = kAllPacketsLost - 1;

// The maximum size of any RTP or RTCP packet, in bytes. The calculation below
// is: Standard Ethernet MTU bytes minus IP header bytes minus UDP header bytes.
// The remainder is available for RTP/RTCP packet data (header + payload).
//
// A nice explanation of this: https://jvns.ca/blog/2017/02/07/mtu/
//
// Constants are provided here for UDP over IPv4 and IPv6 on Ethernet. Other
// transports and network mediums will need additional consideration, alternate
// calculations. Note that MTU is dynamic, depending on the path the packets
// take between two endpoints (the 1500 here is just a commonly-used value for
// LAN Ethernet).
constexpr int kMaxRtpPacketSizeForIpv4UdpOnEthernet = 1500 - 20 - 8;
constexpr int kMaxRtpPacketSizeForIpv6UdpOnEthernet = 1500 - 40 - 8;

// The Cast RTP packet header:
//
//  0                   1                   2                   3
//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ ^
// |V=2|P|X| CC=0  |M|      PT     |      sequence number          | |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+RTP
// +                         RTP timestamp                         |Spec
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ |
// +         synchronization source (SSRC) identifier              | v
// +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
// |K|R| EXT count |  FID          |              PID              | ^
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+Cast
// |             Max PID           |  optional fields, extensions,  Spec
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+  then payload...                v
//
// Byte 0:  Version 2, no padding, no RTP extensions, no CSRCs.
// Byte 1:  Marker bit indicates whether this is the last packet, followed by a
//          7-bit payload type.
// Byte 12: Key Frame bit, followed by "RFID will be provided" bit, followed by
//          6 bits specifying the number of extensions that will be provided.

// The minimum-possible valid size of a Cast RTP packet (i.e., no optional
// fields, extensions, nor payload).
constexpr int kRtpPacketMinValidSize = 18;

// All Cast RTP packets must carry the version 2 flag, not use padding, not use
// RTP extensions, and have zero CSRCs.
constexpr uint8_t kRtpRequiredFirstByte = 0b10000000;

// Bitmasks to isolate fields within byte 2 of the Cast RTP header.
constexpr uint8_t kRtpMarkerBitMask = 0b10000000;
constexpr uint8_t kRtpPayloadTypeMask = 0b01111111;

// Describes the content being transported over RTP streams. These are Cast
// Streaming specific assignments, within the "dynamic" range provided by
// IANA. Note that this Cast Streaming implementation does not manipulate
// already-encoded data, and so these payload types are only "informative" in
// purpose and can be used to check for corruption while parsing packets.
enum class RtpPayloadType : uint8_t {
  kNull = 0,

  kAudioFirst = 96,
  kAudioOpus = 96,
  kAudioAac = 97,
  kAudioPcm16 = 98,
  kAudioVarious = 99,  // Codec being used is not fixed.

  kVideoFirst = 100,
  kVideoVp8 = 100,
  kVideoH264 = 101,
  kVideoVarious = 102,  // Codec being used is not fixed.
  kVideoVp9 = 103,
  kVideoAv1 = 104,
  kVideoLast = kVideoAv1,

  // Some AndroidTV receivers require the payload type for audio to be 127, and
  // video to be 96; regardless of the codecs actually being used. This is
  // definitely out-of-spec, and inconsistent with the audio versus video range
  // of values, but must be taken into account for backwards-compatibility.
  kAudioHackForAndroidTV = 127,
  kVideoHackForAndroidTV = 96,
};

// Setting |use_android_rtp_hack| to true means that we match the legacy Chrome
// sender's behavior of always sending the audio and video hacks for AndroidTV,
// as some legacy android receivers require these.
// TODO(issuetracker.google.com/184438154): we need to figure out what receivers
// need this still, if any. The hack should be removed when possible.
RtpPayloadType GetPayloadType(AudioCodec codec, bool use_android_rtp_hack);
RtpPayloadType GetPayloadType(VideoCodec codec, bool use_android_rtp_hack);

// Returns true if the |raw_byte| can be type-casted to a RtpPayloadType, and is
// also not RtpPayloadType::kNull. The caller should mask the byte, to select
// the lower 7 bits, if applicable.
bool IsRtpPayloadType(uint8_t raw_byte);

// Bitmasks to isolate fields within byte 12 of the Cast RTP header.
constexpr uint8_t kRtpKeyFrameBitMask = 0b10000000;
constexpr uint8_t kRtpHasReferenceFrameIdBitMask = 0b01000000;
constexpr uint8_t kRtpExtensionCountMask = 0b00111111;

// Cast extensions. This implementation supports only the Adaptive Latency
// extension, and ignores all others:
//
//  0                   1                   2                   3
//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |  TYPE = 1 | Ext data SIZE = 2 |Playout Delay (unsigned millis)|
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//
// The Adaptive Latency extension permits changing the fixed end-to-end playout
// delay of a single RTP stream.
constexpr uint8_t kAdaptiveLatencyRtpExtensionType = 1;
constexpr int kNumExtensionDataSizeFieldBits = 10;

// RTCP Common Header:
//
//  0                   1                   2                   3
//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |V=2|P|RC/Subtyp|  Packet Type  |            Length             |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
constexpr int kRtcpCommonHeaderSize = 4;
// All RTCP packets must carry the version 2 flag and not use padding.
constexpr uint8_t kRtcpRequiredVersionAndPaddingBits = 0b100;
constexpr int kRtcpReportCountFieldNumBits = 5;

// https://www.iana.org/assignments/rtp-parameters/rtp-parameters.xhtml
enum class RtcpPacketType : uint8_t {
  kNull = 0,

  kSenderReport = 200,
  kReceiverReport = 201,
  kSourceDescription = 202,
  kApplicationDefined = 204,
  kPayloadSpecific = 206,
  kExtendedReports = 207,
};

// Returns true if the |raw_byte| can be type-casted to a RtcpPacketType, and is
// also not RtcpPacketType::kNull.
bool IsRtcpPacketType(uint8_t raw_byte);

// Supported subtype values in the RTCP Common Header when the packet type is
// kApplicationDefined or kPayloadSpecific.
enum class RtcpSubtype : uint8_t {
  kNull = 0,

  kPictureLossIndicator = 1,
  kReceiverLog = 2,
  kFeedback = 15,
};

// RTCP Sender Report:
//
//  0                   1                   2                   3
//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |                        SSRC of Sender                         |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |                                                               |
// |                         NTP Timestamp                         |
// |                                                               |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |                         RTP Timestamp                         |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |                     Sender's Packet Count                     |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |                     Sender's Octet Count                      |
// +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
//        ...Followed by zero or more "Report Blocks"...
constexpr int kRtcpSenderReportSize = 24;

// RTCP Receiver Report:
//
//  0                   1                   2                   3
//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |                       SSRC of Receiver                        |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//        ...Followed by zero or more "Report Blocks"...
constexpr int kRtcpReceiverReportSize = 4;

// RTCP Report Block. For Cast Streaming, zero or one of these accompanies a
// Sender or Receiver Report, which is different than the RTCP spec (which
// allows zero or more).
//
//  0                   1                   2                   3
//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |                           "To" SSRC                           |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// | Fraction Lost |       Cumulative Number of Packets Lost       |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |      [32-bit extended] Highest Sequence Number Received       |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// | Interarrival Jitter Mean Absolute Deviation (in RTP Timebase) |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |    Middle 32-bits of NTP Timestamp from last Sender Report    |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |     Delay since last Sender Report (1/65536 sec timebase)     |
// +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
constexpr int kRtcpReportBlockSize = 24;
constexpr int kRtcpCumulativePacketsFieldNumBits = 24;

// Cast Feedback Message:
//
//  0                   1                   2                   3
//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |                       SSRC of Receiver                        |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |                        SSRC of Sender                         |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |               Unique identifier 'C' 'A' 'S' 'T'               |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// | CkPt Frame ID | # Loss Fields | Current Playout Delay (msec)  |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
constexpr int kRtcpFeedbackHeaderSize = 16;
constexpr uint32_t kRtcpCastIdentifierWord =
    (uint32_t{'C'} << 24) | (uint32_t{'A'} << 16) | (uint32_t{'S'} << 8) |
    uint32_t{'T'};
//
// "Checkpoint Frame ID" indicates that all frames prior to and including this
// one have been fully received. Unfortunately, the Frame ID is truncated to its
// lower 8 bits in the packet, and 8 bits is not really enough: If a RTCP packet
// is received very late (e.g., more than 1.2 seconds late for 100 FPS audio),
// the Checkpoint Frame ID here will be mis-interpreted as representing a
// higher-numbered frame than what was intended. This could make the sender's
// tracking of "completely received" frames inconsistent, and Cast Streaming
// would live-lock. However, this design issue has been baked into the spec and
// millions of deployments over several years, and so there's no changing it
// now. See kMaxUnackedFrames in constants.h.
//
// "# Loss fields" indicates the number of packet-level NACK words, 0 to 255:
//
//  0                   1                   2                   3
//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// | w/in Frame ID | Lost Frame Packet ID          | PID BitVector |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
constexpr int kRtcpFeedbackLossFieldSize = 4;
//
// "Within Frame ID" is a truncated-to-8-bits frame ID field and, when
// bit-expanded should always be interpreted to represent a value greater than
// the Checkpoint Frame ID. "Lost Frame Packet ID" is either a specific packet
// (within the frame) that has not been received, or kAllPacketsLost to indicate
// none the packets for the frame have been received yet. In the former case,
// "PID Bit Vector" then represents which of the next 8 packets are also
// missing.
//
// Finally, all of the above is optionally followed by a frame-level ACK bit
// vector:
//
//  0                   1                   2                   3
//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |               Unique identifier 'C' 'S' 'T' '2'               |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |Feedback Count | # BVectOctets | ACK BitVect (2 to 254 bytes)...
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ → zero-padded to word boundary
constexpr int kRtcpFeedbackAckHeaderSize = 6;
constexpr uint32_t kRtcpCst2IdentifierWord =
    (uint32_t{'C'} << 24) | (uint32_t{'S'} << 16) | (uint32_t{'T'} << 8) |
    uint32_t{'2'};
constexpr int kRtcpMinAckBitVectorOctets = 2;
constexpr int kRtcpMaxAckBitVectorOctets = 254;
//
// "Feedback Count" is a wrap-around counter indicating the number of Cast
// Feedbacks that have been sent before this one. "# Bit Vector Octets"
// indicates the number of bytes of ACK bit vector following. Cast RTCP
// alignment/padding requirements (to 4-byte boundaries) dictates the following
// rules for generating the ACK bit vector:
//
//   1. There must be at least 2 bytes of ACK bit vector, if only to pad the 6
//      byte header with two more bytes.
//   2. If more than 2 bytes are needed, they must be added 4 at a time to
//      maintain the 4-byte alignment of the overall RTCP packet.
//   3. The total number of octets may not exceed 255; but, because of #2, 254
//      is effectively the limit.
//   4. The first bit in the first octet represents "Checkpoint Frame ID" plus
//      two. "Plus two" and not "plus one" because otherwise the "Checkpoint
//      Frame ID" should have been a greater value!

// RTCP Extended Report:
//
//  0                   1                   2                   3
//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |                     SSRC of Report Author                     |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
constexpr int kRtcpExtendedReportHeaderSize = 4;
//
// ...followed by zero or more Blocks:
//
//  0                   1                   2                   3
//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |  Block Type   | Reserved = 0  |       Block Length            |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |           ..."Block Length" words of report data...           |
// +                                                               +
// +                                                               +
constexpr int kRtcpExtendedReportBlockHeaderSize = 4;
//
// Cast Streaming only uses Receiver Reference Time Reports:
// https://tools.ietf.org/html/rfc3611#section-4.4. So, the entire block would
// be:
//
//  0                   1                   2                   3
//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// | Block Type=4  | Reserved = 0  |       Block Length = 2        |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |                         NTP Timestamp                         |
// |                                                               |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
constexpr uint8_t kRtcpReceiverReferenceTimeReportBlockType = 4;
constexpr int kRtcpReceiverReferenceTimeReportBlockSize = 8;

// Cast Picture Loss Indicator Message:
//
//  0                   1                   2                   3
//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |                       SSRC of Receiver                        |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |                        SSRC of Sender                         |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
constexpr int kRtcpPictureLossIndicatorHeaderSize = 8;

}  // namespace cast
}  // namespace openscreen

#endif  // CAST_STREAMING_RTP_DEFINES_H_
