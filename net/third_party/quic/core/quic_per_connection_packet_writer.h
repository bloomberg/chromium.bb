// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUIC_CORE_QUIC_PER_CONNECTION_PACKET_WRITER_H_
#define NET_THIRD_PARTY_QUIC_CORE_QUIC_PER_CONNECTION_PACKET_WRITER_H_

#include <cstddef>

#include "base/macros.h"
#include "net/third_party/quic/core/quic_connection.h"
#include "net/third_party/quic/core/quic_packet_writer.h"

namespace quic {

// A connection-specific packet writer that wraps a shared writer.
// TODO(wub): Get rid of this class.
class QuicPerConnectionPacketWriter : public QuicPacketWriter {
 public:
  // Does not take ownership of |shared_writer|.
  explicit QuicPerConnectionPacketWriter(QuicPacketWriter* shared_writer);
  ~QuicPerConnectionPacketWriter() override;

  // Default implementation of the QuicPacketWriter interface: Passes everything
  // to |shared_writer_|.
  WriteResult WritePacket(const char* buffer,
                          size_t buf_len,
                          const QuicIpAddress& self_address,
                          const QuicSocketAddress& peer_address,
                          PerPacketOptions* options) override;
  bool IsWriteBlockedDataBuffered() const override;
  bool IsWriteBlocked() const override;
  void SetWritable() override;
  QuicByteCount GetMaxPacketSize(
      const QuicSocketAddress& peer_address) const override;
  bool SupportsReleaseTime() const override;

  bool IsBatchMode() const override;
  char* GetNextWriteLocation() const override;
  WriteResult Flush() override;

 private:
  QuicPacketWriter* shared_writer_;  // Not owned.

  DISALLOW_COPY_AND_ASSIGN(QuicPerConnectionPacketWriter);
};

}  // namespace quic

#endif  // NET_THIRD_PARTY_QUIC_CORE_QUIC_PER_CONNECTION_PACKET_WRITER_H_
