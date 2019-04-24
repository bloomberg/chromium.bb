// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUIC_CORE_FRAMES_QUIC_CRYPTO_FRAME_H_
#define NET_THIRD_PARTY_QUIC_CORE_FRAMES_QUIC_CRYPTO_FRAME_H_

#include <memory>
#include <ostream>

#include "net/third_party/quic/core/quic_buffer_allocator.h"
#include "net/third_party/quic/core/quic_types.h"
#include "net/third_party/quic/platform/api/quic_export.h"
#include "net/third_party/quic/platform/api/quic_string_piece.h"

namespace quic {

struct QUIC_EXPORT_PRIVATE QuicCryptoFrame {
  QuicCryptoFrame();
  QuicCryptoFrame(EncryptionLevel level,
                  QuicStreamOffset offset,
                  QuicPacketLength data_length);
  QuicCryptoFrame(EncryptionLevel level,
                  QuicStreamOffset offset,
                  QuicStringPiece data);
  ~QuicCryptoFrame();

  friend QUIC_EXPORT_PRIVATE std::ostream& operator<<(std::ostream& os,
                                                      const QuicCryptoFrame& s);

  // When writing a crypto frame to a packet, the packet must be encrypted at
  // |level|. When a crypto frame is read, the encryption level of the packet it
  // was received in is put in |level|.
  EncryptionLevel level;
  QuicPacketLength data_length;
  // When reading, |data_buffer| points to the data that was received in the
  // frame. |data_buffer| is not used when writing.
  const char* data_buffer;
  QuicStreamOffset offset;  // Location of this data in the stream.

  QuicCryptoFrame(EncryptionLevel level,
                  QuicStreamOffset offset,
                  const char* data_buffer,
                  QuicPacketLength data_length);
};
static_assert(sizeof(QuicCryptoFrame) <= 64,
              "Keep the QuicCryptoFrame size to a cacheline.");

}  // namespace quic

#endif  // NET_THIRD_PARTY_QUIC_CORE_FRAMES_QUIC_CRYPTO_FRAME_H_
