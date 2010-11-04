// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_RTP_UTILS_H_
#define REMOTING_PROTOCOL_RTP_UTILS_H_

#include "base/basictypes.h"

namespace remoting {
namespace protocol {

struct RtpHeader {
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

// Returns size of RTP header for the specified number of sources.
int GetRtpHeaderSize(int sources);

// Packs RTP header into the buffer.
void PackRtpHeader(uint8* buffer, int buffer_size,
                   const RtpHeader& header);

// Unpacks RTP header and stores unpacked values in |header|. If the header
// is not valid returns -1, otherwise returns size of the header.
int UnpackRtpHeader(const uint8* buffer, int buffer_size,
                    RtpHeader* header);

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_RTP_UTILS_H_
