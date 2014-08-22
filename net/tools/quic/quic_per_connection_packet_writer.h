// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TOOLS_QUIC_QUIC_PER_CONNECTION_PACKET_WRITER_H_
#define NET_TOOLS_QUIC_QUIC_PER_CONNECTION_PACKET_WRITER_H_

#include "net/quic/quic_connection.h"
#include "net/quic/quic_packet_writer.h"

namespace net {

namespace tools {

// A connection-specific packet writer that wraps a shared writer and keeps a
// reference to the connection.
class QuicPerConnectionPacketWriter : public QuicPacketWriter {
 public:
  // Does not take ownership of |shared_writer| or |connection|.
  QuicPerConnectionPacketWriter(QuicPacketWriter* shared_writer,
                                QuicConnection* connection);
  virtual ~QuicPerConnectionPacketWriter();

  QuicPacketWriter* shared_writer() const { return shared_writer_; }
  QuicConnection* connection() const { return connection_; }

  // Default implementation of the QuicPacketWriter interface: Passes everything
  // to |shared_writer_|.
  virtual WriteResult WritePacket(const char* buffer,
                                  size_t buf_len,
                                  const IPAddressNumber& self_address,
                                  const IPEndPoint& peer_address) OVERRIDE;
  virtual bool IsWriteBlockedDataBuffered() const OVERRIDE;
  virtual bool IsWriteBlocked() const OVERRIDE;
  virtual void SetWritable() OVERRIDE;

 private:
  QuicPacketWriter* shared_writer_;  // Not owned.
  QuicConnection* connection_;  // Not owned.

  DISALLOW_COPY_AND_ASSIGN(QuicPerConnectionPacketWriter);
};

}  // namespace tools

}  // namespace net

#endif  // NET_TOOLS_QUIC_QUIC_PER_CONNECTION_PACKET_WRITER_H_
