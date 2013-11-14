// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/quic_default_packet_writer.h"

#include "net/tools/quic/quic_socket_utils.h"

namespace net {
namespace tools {

QuicDefaultPacketWriter::QuicDefaultPacketWriter(int fd) : fd_(fd) {}

QuicDefaultPacketWriter::~QuicDefaultPacketWriter() {}

WriteResult QuicDefaultPacketWriter::WritePacket(
    const char* buffer, size_t buf_len,
    const net::IPAddressNumber& self_address,
    const net::IPEndPoint& peer_address,
    QuicBlockedWriterInterface* blocked_writer) {
  return QuicSocketUtils::WritePacket(fd_, buffer, buf_len,
                                      self_address, peer_address);
}

bool QuicDefaultPacketWriter::IsWriteBlockedDataBuffered() const {
  return false;
}

}  // namespace tools
}  // namespace net
