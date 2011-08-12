// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/rtcp_writer.h"

#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "remoting/base/compound_buffer.h"
#include "remoting/protocol/buffered_socket_writer.h"
#include "remoting/protocol/rtp_utils.h"

namespace remoting {
namespace protocol {

RtcpWriter::RtcpWriter(base::MessageLoopProxy* message_loop)
    : buffered_rtcp_writer_(new BufferedDatagramWriter(message_loop)) {
}

RtcpWriter::~RtcpWriter() {
}

void RtcpWriter::Close() {
  buffered_rtcp_writer_->Close();
}

// Initializes the writer. Must be called on the thread the sockets
// belong to.
void RtcpWriter::Init(net::Socket* socket) {
  buffered_rtcp_writer_->Init(socket, NULL);
}

void RtcpWriter::SendReport(const RtcpReceiverReport& report) {
  int size = GetRtcpReceiverReportSize(report);
  net::IOBufferWithSize* buffer = new net::IOBufferWithSize(size);

  PackRtcpReceiverReport(report, reinterpret_cast<uint8*>(buffer->data()),
                         size);

  buffered_rtcp_writer_->Write(buffer, NULL);
}

}  // namespace protocol
}  // namespace remoting
