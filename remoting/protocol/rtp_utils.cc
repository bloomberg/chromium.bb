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

namespace {
const int kRtpBaseHeaderSize = 12;
const uint8 kRtpVersionNumber = 2;
const int kRtpMaxSources = 16;
}  // namespace

int GetRtpHeaderSize(int sources) {
  DCHECK_GE(sources, 0);
  DCHECK_LT(sources, kRtpMaxSources);
  return kRtpBaseHeaderSize + sources * 4;
}

void PackRtpHeader(uint8* buffer, int buffer_size,
                   const RtpHeader& header) {
  DCHECK_LT(header.sources, kRtpMaxSources);
  DCHECK_LT(header.payload_type, 1 << 7);
  CHECK_GT(buffer_size, GetRtpHeaderSize(header.sources));

  buffer[0] = (kRtpVersionNumber << 6) +
      ((uint8)header.padding << 5) +
      ((uint8)header.extension << 4) +
      header.sources;
  buffer[1] = ((uint8)header.marker << 7) +
      header.payload_type;
  SetBE16(buffer + 2, header.sequence_number);
  SetBE32(buffer + 4, header.timestamp);
  SetBE32(buffer + 8, header.sync_source_id);

  for (int i = 0; i < header.sources; i++) {
    SetBE32(buffer + i * 4 + 12, header.source_id[i]);
  }
}

static inline uint8 ExtractBits(uint8 byte, int bits_count, int shift) {
  return (byte >> shift) & ((1 << bits_count) - 1);
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

  if (buffer_size < GetRtpHeaderSize(header->sources)) {
    return -1;
  }
  for (int i = 0; i < header->sources; i++) {
    header->source_id[i] = GetBE32(buffer + i * 4 + 12);
  }

  return GetRtpHeaderSize(header->sources);
}

}  // namespace remoting
