// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_RTP_WRITER_H_
#define REMOTING_PROTOCOL_RTP_WRITER_H_

#include "net/socket/socket.h"
#include "remoting/protocol/buffered_socket_writer.h"
#include "remoting/protocol/rtp_utils.h"

namespace remoting {

class CompoundBuffer;

namespace protocol {

class RtpWriter {
 public:
  RtpWriter(base::MessageLoopProxy* message_loop);
  virtual ~RtpWriter();

  // Initializes the writer. Must be called on the thread the socket
  // belongs to.
  void Init(net::Socket* socket);
  void Close();

  // Sends next packet. The packet is mutated by
  void SendPacket(uint32 timestamp, bool marker,
                  const Vp8Descriptor& vp8_descriptor,
                  const CompoundBuffer& payload);

  // Returns number of packets queued in the buffer.
  int GetPendingPackets();

 private:
  uint32 last_packet_number_;

  scoped_refptr<BufferedDatagramWriter> buffered_rtp_writer_;
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_RTP_WRITER_H_
