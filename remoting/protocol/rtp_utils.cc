// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/rtp_utils.h"

#include "base/logging.h"
#include "third_party/libjingle/source/talk/base/byteorder.h"

using talk_base::GetBE16;
using talk_base::GetBE32;
using talk_base::SetBE16;
using talk_base::SetBE32;

namespace remoting {
namespace protocol {

namespace {
const int kRtpBaseHeaderSize = 12;
const uint8 kRtpVersionNumber = 2;
const int kRtpMaxSources = 16;
const int kBytesPerCSRC = 4;
const int kRtcpBaseHeaderSize = 4;
const int kRtcpReceiverReportSize = 28;
const int kRtcpReceiverReportTotalSize =
    kRtcpBaseHeaderSize + kRtcpReceiverReportSize;
const int kRtcpReceiverReportPacketType = 201;
}  // namespace

static inline uint8 ExtractBits(uint8 byte, int bits_count, int shift) {
  return (byte >> shift) & ((1 << bits_count) - 1);
}

// RTP Header format:
//
//  0                   1                   2                   3
//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |V=2|P|X|  CC   |M|     PT      |       sequence number         |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |                           timestamp                           |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |           synchronization source (SSRC) identifier            |
// +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
// |            contributing source (CSRC) identifiers             |
// |                             ....                              |
// +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
//
// On the diagram above order of bytes and order of bits within each
// byte are big-endian. So bits 0 and 7 are the most and the least
// significant bits in the first byte, bit 8 is the most significant
// bit in the second byte, etc.

int GetRtpHeaderSize(const RtpHeader& header) {
  DCHECK_GE(header.sources, 0);
  DCHECK_LT(header.sources, kRtpMaxSources);
  return kRtpBaseHeaderSize + header.sources * kBytesPerCSRC;
}

void PackRtpHeader(const RtpHeader& header, uint8* buffer, int buffer_size) {
  DCHECK_LT(header.sources, kRtpMaxSources);
  DCHECK_LT(header.payload_type, 1 << 7);
  CHECK_GE(buffer_size, GetRtpHeaderSize(header));

  buffer[0] = (kRtpVersionNumber << 6) |
      ((uint8)header.padding << 5) |
      ((uint8)header.extension << 4) |
      header.sources;
  buffer[1] = ((uint8)header.marker << 7) |
      header.payload_type;
  SetBE16(buffer + 2, header.sequence_number);
  SetBE32(buffer + 4, header.timestamp);
  SetBE32(buffer + 8, header.sync_source_id);

  for (int i = 0; i < header.sources; i++) {
    SetBE32(buffer + i * 4 + 12, header.source_id[i]);
  }
}

int UnpackRtpHeader(const uint8* buffer, int buffer_size, RtpHeader* header) {
  if (buffer_size < kRtpBaseHeaderSize) {
    return -1;
  }

  int version = ExtractBits(buffer[0], 2, 6);
  if (version != kRtpVersionNumber) {
    return -1;
  }

  header->padding = ExtractBits(buffer[0], 1, 5) != 0;
  header->extension = ExtractBits(buffer[0], 1, 4) != 0;
  header->sources = ExtractBits(buffer[0], 4, 0);

  header->marker = ExtractBits(buffer[1], 1, 7) != 0;
  header->payload_type = ExtractBits(buffer[1], 7, 0);

  header->sequence_number = GetBE16(buffer + 2);
  header->timestamp = GetBE32(buffer + 4);
  header->sync_source_id = GetBE32(buffer + 8);

  DCHECK_LT(header->sources, 16);

  if (buffer_size < GetRtpHeaderSize(*header)) {
    return -1;
  }
  for (int i = 0; i < header->sources; i++) {
    header->source_id[i] = GetBE32(buffer + i * 4 + 12);
  }

  return GetRtpHeaderSize(*header);
}

// RTCP receiver report:
//
// 0                   1                   2                   3
// 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |V=2|P|   RC=1  |     PT=201    |             length            |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |                         SSRC of sender                        |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |                      SSRC of the reportee                     |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// | fraction lost |       cumulative number of packets lost       |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |           extended highest sequence number received           |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |                      interarrival jitter                      |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |                         last SR (LSR)                         |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |                   delay since last SR (DLSR)                  |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+


int GetRtcpReceiverReportSize(const RtcpReceiverReport& report) {
  return kRtcpReceiverReportTotalSize;
}

void PackRtcpReceiverReport(const RtcpReceiverReport& report,
                            uint8* buffer, int buffer_size) {
  CHECK_GE(buffer_size, GetRtcpReceiverReportSize(report));

  buffer[0] = (kRtpVersionNumber << 6) |
      1 /* RC=1 */;
  buffer[1] = kRtcpReceiverReportPacketType;
  SetBE16(buffer + 2, kRtcpReceiverReportSize);

  SetBE32(buffer + 4, report.receiver_ssrc);
  SetBE32(buffer + 8, report.sender_ssrc);
  SetBE32(buffer + 12, report.total_lost_packets & 0xFFFFFF);
  buffer[12] = report.loss_fraction;
  SetBE32(buffer + 16, report.last_sequence_number);
  SetBE32(buffer + 20, report.jitter);
  SetBE32(buffer + 24, report.last_sender_report_timestamp);
  SetBE32(buffer + 28, report.last_sender_report_delay);
}

int UnpackRtcpReceiverReport(const uint8* buffer, int buffer_size,
                             RtcpReceiverReport* report) {
  if (buffer_size < kRtcpReceiverReportTotalSize) {
    return -1;
  }

  int version = ExtractBits(buffer[0], 2, 6);
  if (version != kRtpVersionNumber) {
    return -1;
  }

  int report_count = ExtractBits(buffer[0], 5, 0);
  if (report_count != 1) {
    // Received RTCP packet with more than one report.  This isn't
    // supported in the current implementation.
    return -1;
  }

  int packet_type = buffer[1];
  if (packet_type != kRtcpReceiverReportPacketType) {
    // The packet isn't receiver report.
    return -1;
  }

  int report_size = GetBE16(buffer + 2);
  if (report_size != kRtcpReceiverReportSize) {
    // Invalid size of the report.
    return -1;
  }

  report->receiver_ssrc = GetBE32(buffer + 4);
  report->sender_ssrc = GetBE32(buffer + 8);
  report->loss_fraction = buffer[12];
  report->total_lost_packets = GetBE32(buffer + 12) & 0xFFFFFF;
  report->last_sequence_number = GetBE32(buffer + 16);
  report->jitter = GetBE32(buffer + 20);
  report->last_sender_report_timestamp = GetBE32(buffer + 24);
  report->last_sender_report_delay = GetBE32(buffer + 28);

  return kRtcpReceiverReportTotalSize;
}

// VP8 Payload Descriptor format:
//
//  0                   1                   2                   3
//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// | RSV |I|N|FI |B|         PictureID (integer #bytes)            |
// +-+-+-+-+-+-+-+-+                                               |
// :                                                               :
// |               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |               : (VP8 data or VP8 payload header; byte aligned)|
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//
// On the diagram above order of bytes and order of bits within each
// byte are big-endian. So bits 0 and 7 are the most and the least
// significant bits in the first byte, bit 8 is the most significant
// bit in the second byte, etc.
//
// RSV: 3 bits
// Bits reserved for future use. MUST be equal to zero and MUST be
// ignored by the receiver.
//
// I: 1 bit
// PictureID present. When set to one, a PictureID is provided after
// the first byte of the payload descriptor. When set to zero, the
// PictureID is omitted, and the one-byte payload descriptor is
// immediately followed by the VP8 payload.
//
// N: 1 bit
// Non-reference frame. When set to one, the frame can be discarded
// without affecting any other future or past frames.
//
// FI: 2 bits
// Fragmentation information field. This field contains information
// about the fragmentation of VP8 payloads carried in the RTP
// packet. The four different values are listed below.
//
// FI   Fragmentation status
//   00 The RTP packet contains no fragmented VP8 partitions. The
//      payload is one or several complete partitions.
//   01 The RTP packet contains the first part of a fragmented
//      partition. The fragment must be placed in its own RTP packet.
//   10 The RTP packet contains a fragment that is neither the first nor
//      the last part of a fragmented partition. The fragment must be
//      placed in its own RTP packet.
//   11 The RTP packet contains the last part of a fragmented
//      partition. The fragment must be placed in its own RTP packet.
//
// B: 1 bit
// Beginning VP8 frame. When set to 1 this signals that a new VP8
// frame starts in this RTP packet.
//
// PictureID: Multiple of 8 bits
// This is a running index of the frames. The field is present only if
// the I bit is equal to one. The most significant bit of each byte is
// an extension flag. The 7 following bits carry (parts of) the
// PictureID. If the extension flag is one, the PictureID continues in
// the next byte. If the extension flag is zero, the 7 remaining bits
// are the last (and least significant) bits in the PictureID. The
// sender may choose any number of bytes for the PictureID. The
// PictureID SHALL start on a random number, and SHALL wrap after
// reaching the maximum ID as chosen by the application

int GetVp8DescriptorSize(const Vp8Descriptor& descriptor) {
  if (descriptor.picture_id == kuint32max)
    return 1;
  int result = 2;
  // We need 1 byte per each 7 bits in picture_id.
  uint32 picture_id = descriptor.picture_id >> 7;
  while (picture_id > 0) {
    picture_id >>= 7;
    ++result;
  }
  return result;
}

void PackVp8Descriptor(const Vp8Descriptor& descriptor, uint8* buffer,
                       int buffer_size) {
  CHECK_GT(buffer_size, 0);

  buffer[0] =
      ((uint8)(descriptor.picture_id != kuint32max) << 4) |
      ((uint8)descriptor.non_reference_frame << 3) |
      (descriptor.fragmentation_info << 1) |
      ((uint8)descriptor.frame_beginning);

  uint32 picture_id = descriptor.picture_id;
  if (picture_id == kuint32max)
    return;

  int pos = 1;
  while (picture_id > 0) {
    CHECK_LT(pos, buffer_size);
    buffer[pos] = picture_id & 0x7F;
    picture_id >>= 7;

    // Set the extension bit if neccessary.
    if (picture_id > 0)
      buffer[pos] |= 0x80;
    ++pos;
  }
}

int UnpackVp8Descriptor(const uint8* buffer, int buffer_size,
                        Vp8Descriptor* descriptor) {
  if (buffer_size <= 0)
    return -1;

  bool picture_id_present = ExtractBits(buffer[0], 1, 4) != 0;
  descriptor->non_reference_frame = ExtractBits(buffer[0], 1, 3) != 0;
  descriptor->fragmentation_info = ExtractBits(buffer[0], 2, 1);
  descriptor->frame_beginning = ExtractBits(buffer[0], 1, 0) != 0;

  // Return here if we don't need to decode PictureID.
  if (!picture_id_present) {
    descriptor->picture_id = kuint32max;
    return 1;
  }

  // Decode PictureID.
  bool extension = true;
  int pos = 1;
  descriptor->picture_id = 0;
  while (extension) {
    if (pos >= buffer_size)
      return -1;

    descriptor->picture_id |= buffer[pos] & 0x7F;
    extension = (buffer[pos] & 0x80) != 0;
    pos += 1;
  }
  return pos;
}

}  // namespace protocol
}  // namespace remoting
