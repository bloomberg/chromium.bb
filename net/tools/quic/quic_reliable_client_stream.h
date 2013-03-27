// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TOOLS_QUIC_QUIC_RELIABLE_CLIENT_STREAM_H_
#define NET_TOOLS_QUIC_QUIC_RELIABLE_CLIENT_STREAM_H_

#include <sys/types.h>
#include <string>

#include "base/string_piece.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/reliable_quic_stream.h"
#include "net/tools/flip_server/balsa_frame.h"
#include "net/tools/flip_server/balsa_headers.h"

namespace net {

class QuicClientSession;
class QuicSession;

// A base class for spdy/http client streams which handles the concept
// of sending and receiving headers and bodies.
class QuicReliableClientStream : public ReliableQuicStream {
 public:
  QuicReliableClientStream(QuicStreamId id, QuicSession* session)
      : ReliableQuicStream(id, session) {
  }

  // Serializes the headers and body, sends it to the server, and
  // returns the number of bytes sent.
  virtual ssize_t SendRequest(const BalsaHeaders& headers,
                              base::StringPiece body,
                              bool fin) = 0;
  // Sends body data to the server and returns the number of bytes sent.
  virtual ssize_t SendBody(const std::string& data, bool fin);

  // Override the base class to close the Write side as soon as we get a
  // response.
  // SPDY/HTTP do not support bidirectional streaming.
  virtual bool OnStreamFrame(const QuicStreamFrame& frame) OVERRIDE;

  // Returns the response data.
  const std::string& data() { return data_; }

  // Returns whatever headers have been received for this stream.
  const BalsaHeaders& headers() { return headers_; }

  bool closed() { return closed_; }

 protected:
  std::string* mutable_data() { return &data_; }
  BalsaHeaders* mutable_headers() { return &headers_; }

 private:
  BalsaHeaders headers_;
  std::string data_;
  bool closed_;

  DISALLOW_COPY_AND_ASSIGN(QuicReliableClientStream);
};

}  // namespace net

#endif  // NET_TOOLS_QUIC_QUIC_RELIABLE_CLIENT_STREAM_H_
