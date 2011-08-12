// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_RTCP_WRITER_H_
#define REMOTING_PROTOCOL_RTCP_WRITER_H_

#include "net/socket/socket.h"

namespace base {
class MessageLoopProxy;
}  // namespace base

namespace remoting {

class CompoundBuffer;

namespace protocol {

class BufferedDatagramWriter;
struct RtcpReceiverReport;

class RtcpWriter {
 public:
  RtcpWriter(base::MessageLoopProxy* message_loop);
  virtual ~RtcpWriter();

  // Initializes the writer. Must be called on the thread the socket
  // belongs to.
  void Init(net::Socket* socket);
  void Close();

  // Sends next packet. The packet is mutated by
  void SendReport(const RtcpReceiverReport& report);

  // Returns number of packets queued in the buffer.
  int GetPendingPackets();

 private:
  scoped_refptr<BufferedDatagramWriter> buffered_rtcp_writer_;

  DISALLOW_COPY_AND_ASSIGN(RtcpWriter);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_RTCP_WRITER_H_
