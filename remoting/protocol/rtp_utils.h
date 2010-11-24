// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_RTP_UTILS_H_
#define REMOTING_PROTOCOL_RTP_UTILS_H_

#include "base/basictypes.h"

namespace remoting {
namespace protocol {

struct RtpHeader {
  RtpHeader()
      : padding(false),
        extension(false),
        sources(0),
        marker(false),
        payload_type(0),
        sequence_number(0),
        timestamp(0),
        sync_source_id(0) {
  }

  // RTP version is always set to 2.
  // version = 2
  bool padding;
  bool extension;
  uint8 sources;
  bool marker;
  uint8 payload_type;
  uint16 sequence_number;
  uint32 timestamp;
  uint32 sync_source_id;
  uint32 source_id[15];
};

struct RtcpReceiverReport {
  RtcpReceiverReport()
      : receiver_ssrc(0),
        sender_ssrc(0),
        loss_fraction(0),
        total_lost_packets(0),
        last_sequence_number(0),
        jitter(0),
        last_sender_report_timestamp(0),
        last_sender_report_delay(0) {
  }

  uint32 receiver_ssrc;
  uint32 sender_ssrc;
  uint8 loss_fraction;  // 8bit fixed point value in the interval [0..1].
  uint32 total_lost_packets;
  uint32 last_sequence_number;
  uint32 jitter;
  uint32 last_sender_report_timestamp;
  uint32 last_sender_report_delay;
};

// Vp8Descriptor struct used to store values of the VP8 RTP descriptor
// fields. Meaning of each field is documented in the RTP Payload
// Format for VP8 spec: http://www.webmproject.org/code/specs/rtp/ .
struct Vp8Descriptor {
  enum FragmentationInfo {
    NOT_FRAGMENTED = 0,
    FIRST_FRAGMENT = 1,
    MIDDLE_FRAGMENT = 2,
    LAST_FRAGMENT = 3,
  };

  Vp8Descriptor()
      : non_reference_frame(false),
        fragmentation_info(NOT_FRAGMENTED),
        frame_beginning(false),
        picture_id(kuint32max) {
  }

  bool non_reference_frame;
  uint8 fragmentation_info;
  bool frame_beginning;

  // PictureID is considered to be absent if |picture_id| is set to kuint32max.
  uint32 picture_id;
};

// Returns size of RTP header for the specified number of sources.
int GetRtpHeaderSize(const RtpHeader& header);

// Packs RTP header into the buffer.
void PackRtpHeader(const RtpHeader& header, uint8* buffer, int buffer_size);

// Unpacks RTP header and stores unpacked values in |header|.
int UnpackRtpHeader(const uint8* buffer, int buffer_size, RtpHeader* header);

// Three following functions below are used to pack and unpack RTCP
// Receiver Report packets. They implement only subset of RTCP that is
// useful for chromoting.  Particularly there are following
// limitations:
//
//  1. Only one report per packet. There is always only one sender and
//     only one receiver in chromotocol session, so we never need to
//     have more than one report per packet.
//  2. No RTCP Sender Report. Sender Reports are useful for streams
//     synchronization (e.g. audio/video syncronization), but it is
//     not needed for screencasts.

// Returns size of RTCP Receiver Report packet.
int GetRtcpReceiverReportSize(const RtcpReceiverReport& report);

// Packs RTCP Receiver Report into the |buffer|.
void PackRtcpReceiverReport(const RtcpReceiverReport& report,
                            uint8* buffer, int buffer_size);

// Unpack RTCP Receiver Report packet. If the packet is invalid
// returns -1, othewise returns size of the data that was read from
// the packet.
int UnpackRtcpReceiverReport(const uint8* buffer, int buffer_size,
                             RtcpReceiverReport* report);

// Returns size of VP8 Payload Descriptor.
int GetVp8DescriptorSize(const Vp8Descriptor& descriptor);

// Packs VP8 Payload Descriptor into the |buffer|.
void PackVp8Descriptor(const Vp8Descriptor& descriptor, uint8* buffer,
                       int buffer_size);

// Unpacks VP8 Payload Descriptor. If the descriptor is not valid
// returns -1, otherwise returns size of the descriptor.
int UnpackVp8Descriptor(const uint8* buffer, int buffer_size,
                        Vp8Descriptor* descriptor);

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_RTP_UTILS_H_
