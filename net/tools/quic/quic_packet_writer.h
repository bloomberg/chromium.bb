// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TOOLS_QUIC_QUIC_PACKET_WRITER_H_
#define NET_TOOLS_QUIC_QUIC_PACKET_WRITER_H_

#include "net/base/ip_endpoint.h"

namespace net {

class QuicBlockedWriterInterface;

// An interface between writers and the entity managing the
// socket (in our case the QuicDispatcher).  This allows the Dispatcher to
// control writes, and manage any writers who end up write blocked.
class QuicPacketWriter {
 public:
  virtual ~QuicPacketWriter() {}

  virtual int WritePacket(const char* buffer, size_t buf_len,
                          const net::IPAddressNumber& self_address,
                          const net::IPEndPoint& peer_address,
                          QuicBlockedWriterInterface* blocked_writer,
                          int* error) = 0;
};

}  // namespace net

#endif  // NET_TOOLS_QUIC_QUIC_PACKET_WRITER_H_
