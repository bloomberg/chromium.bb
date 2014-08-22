// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/quic_per_connection_packet_writer.h"

namespace net {

namespace tools {

QuicPerConnectionPacketWriter::QuicPerConnectionPacketWriter(
    QuicPacketWriter* shared_writer,
    QuicConnection* connection)
    : shared_writer_(shared_writer),
      connection_(connection) {
}

QuicPerConnectionPacketWriter::~QuicPerConnectionPacketWriter() {
}

WriteResult QuicPerConnectionPacketWriter::WritePacket(
    const char* buffer,
    size_t buf_len,
    const IPAddressNumber& self_address,
    const IPEndPoint& peer_address) {
  return shared_writer_->WritePacket(buffer,
                                     buf_len,
                                     self_address,
                                     peer_address);
}

bool QuicPerConnectionPacketWriter::IsWriteBlockedDataBuffered() const {
  return shared_writer_->IsWriteBlockedDataBuffered();
}

bool QuicPerConnectionPacketWriter::IsWriteBlocked() const {
  return shared_writer_->IsWriteBlocked();
}

void QuicPerConnectionPacketWriter::SetWritable() {
  shared_writer_->SetWritable();
}

}  // namespace tools

}  // namespace net
