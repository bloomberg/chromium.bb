// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUIC_CORE_FRAMES_QUIC_STREAM_FRAME_H_
#define NET_THIRD_PARTY_QUIC_CORE_FRAMES_QUIC_STREAM_FRAME_H_

#include <memory>
#include <ostream>

#include "net/third_party/quic/core/quic_buffer_allocator.h"
#include "net/third_party/quic/core/quic_types.h"
#include "net/third_party/quic/platform/api/quic_export.h"
#include "net/third_party/quic/platform/api/quic_string_piece.h"

namespace quic {

struct QUIC_EXPORT_PRIVATE QuicStreamFrame {
  QuicStreamFrame();
  QuicStreamFrame(QuicStreamId stream_id,
                  bool fin,
                  QuicStreamOffset offset,
                  QuicStringPiece data);
  QuicStreamFrame(QuicStreamId stream_id,
                  bool fin,
                  QuicStreamOffset offset,
                  QuicPacketLength data_length);
  ~QuicStreamFrame();

  friend QUIC_EXPORT_PRIVATE std::ostream& operator<<(std::ostream& os,
                                                      const QuicStreamFrame& s);

  QuicStreamId stream_id;
  bool fin;
  QuicPacketLength data_length;
  const char* data_buffer;
  QuicStreamOffset offset;  // Location of this data in the stream.

  QuicStreamFrame(QuicStreamId stream_id,
                  bool fin,
                  QuicStreamOffset offset,
                  const char* data_buffer,
                  QuicPacketLength data_length);

 private:
  DISALLOW_COPY_AND_ASSIGN(QuicStreamFrame);
};
static_assert(sizeof(QuicStreamFrame) <= 64,
              "Keep the QuicStreamFrame size to a cacheline.");

}  // namespace quic

#endif  // NET_THIRD_PARTY_QUIC_CORE_FRAMES_QUIC_STREAM_FRAME_H_
