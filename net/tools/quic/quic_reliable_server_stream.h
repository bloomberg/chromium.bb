// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TOOLS_QUIC_QUIC_RELIABLE_SERVER_STREAM_H_
#define NET_TOOLS_QUIC_QUIC_RELIABLE_SERVER_STREAM_H_

#include <string>

#include "net/quic/quic_protocol.h"
#include "net/quic/reliable_quic_stream.h"
#include "net/tools/flip_server/balsa_headers.h"

namespace net {

namespace test {
class QuicReliableServerStreamPeer;
}  // namespace test

class QuicSession;

// A base class for spdy/http server streams which handles the concept
// of sending and receiving headers and bodies.
class QuicReliableServerStream : public ReliableQuicStream {
 public:
  QuicReliableServerStream(QuicStreamId id, QuicSession* session);
  virtual ~QuicReliableServerStream() {}

  // Subclasses should process and frame data when this is called, returning
  // how many bytes are processed.
  virtual uint32 ProcessData(const char* data, uint32 data_len) = 0;
  // Subclasses should implement this to serialize headers in a
  // protocol-specific manner, and send it out to the client.
  virtual void SendHeaders(const BalsaHeaders& response_headers) = 0;

  // Sends a basic 200 response using SendHeaders for the headers and WriteData
  // for the body.
  void SendResponse();
  // Sends a basic 500 response using SendHeaders for the headers and WriteData
  // for the body
  void SendErrorResponse();
  // Make sure that as soon as we start writing data, we stop reading.
  virtual QuicConsumedData WriteData(base::StringPiece data, bool fin) OVERRIDE;

  // Returns whatever headers have been received for this stream.
  const BalsaHeaders& headers() { return headers_; }

  const string& body() { return body_; }
 protected:
  BalsaHeaders* mutable_headers() { return &headers_; }
  string* mutable_body() { return &body_; }

 private:
  friend class test::QuicReliableServerStreamPeer;

  BalsaHeaders headers_;
  string body_;
};

}  // namespace net

#endif  // NET_TOOLS_QUIC_QUIC_RELIABLE_SERVER_STREAM_H_
