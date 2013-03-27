// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TOOLS_QUIC_QUIC_SPDY_SERVER_STREAM_H_
#define NET_TOOLS_QUIC_QUIC_SPDY_SERVER_STREAM_H_

#include <string>

#include "net/base/io_buffer.h"
#include "net/tools/quic/quic_reliable_server_stream.h"

namespace net {

// All this does right now is aggregate data, and on fin, send a cached
// response.
class QuicSession;

class QuicSpdyServerStream : public QuicReliableServerStream {
 public:
  QuicSpdyServerStream(QuicStreamId id, QuicSession* session);
  virtual ~QuicSpdyServerStream();

  // ReliableQuicStream implementation called by the session when there's
  // data for us.
  virtual uint32 ProcessData(const char* data, uint32 data_len) OVERRIDE;

  virtual void SendHeaders(const BalsaHeaders& response_headers) OVERRIDE;

  int ParseRequestHeaders();

 protected:
  virtual void TerminateFromPeer(bool half_close) OVERRIDE;

  // Buffer into which response header data is read.
  scoped_refptr<GrowableIOBuffer> read_buf_;
  bool request_headers_received_;
};

}  // namespace net

#endif  // NET_TOOLS_QUIC_QUIC_SPDY_SERVER_STREAM_H_
