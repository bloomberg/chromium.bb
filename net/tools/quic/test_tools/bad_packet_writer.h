// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TOOLS_QUIC_TEST_TOOLS_BAD_PACKET_WRITER_H_
#define NET_TOOLS_QUIC_TEST_TOOLS_BAD_PACKET_WRITER_H_

#include "net/tools/quic/quic_packet_writer_wrapper.h"

namespace net {

namespace test {
// This packet writer allows causing packet write error with specified error
// code when writing a particular packet.
class BadPacketWriter : public QuicPacketWriterWrapper {
 public:
  BadPacketWriter(size_t packet_causing_write_error, int error_code);

  ~BadPacketWriter() override;

  WriteResult WritePacket(const char* buffer,
                          size_t buf_len,
                          const QuicIpAddress& self_address,
                          const QuicSocketAddress& peer_address,
                          PerPacketOptions* options) override;

 private:
  size_t packet_causing_write_error_;
  int error_code_;
};

}  // namespace test

}  // namespace net

#endif  // NET_TOOLS_QUIC_TEST_TOOLS_BAD_PACKET_WRITER_H_
