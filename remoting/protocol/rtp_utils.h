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

// Unpacks RTP header and stores unpacked values in |header|. If the header
// is not valid returns -1, otherwise returns size of the header.
int UnpackRtpHeader(const uint8* buffer, int buffer_size, RtpHeader* header);

int GetVp8DescriptorSize(const Vp8Descriptor& descriptor);

void PackVp8Descriptor(const Vp8Descriptor& descriptor, uint8* buffer,
                       int buffer_size);

int UnpackVp8Descriptor(const uint8* buffer, int buffer_size,
                        Vp8Descriptor* descriptor);

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_RTP_UTILS_H_
