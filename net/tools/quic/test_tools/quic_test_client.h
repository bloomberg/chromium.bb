// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_TEST_TOOLS_QUIC_CLIENT_H_
#define NET_QUIC_TEST_TOOLS_QUIC_CLIENT_H_

#include <string>

#include "base/basictypes.h"
#include "net/quic/quic_framer.h"
#include "net/quic/quic_packet_creator.h"
#include "net/quic/quic_protocol.h"
#include "net/tools/quic/quic_client.h"

namespace net {

namespace test {

class HTTPMessage;

// A toy QUIC client used for testing.
class QuicTestClient :  public ReliableQuicStream::Visitor {
 public:
  QuicTestClient(IPEndPoint server_address, const string& server_hostname);
  virtual ~QuicTestClient();

  // Clears any outstanding state and sends a simple GET of 'uri' to the
  // server.  Returns 0 if the request failed and no bytes were written.
  ssize_t SendRequest(const string& uri);
  ssize_t SendMessage(const HTTPMessage& message);

  string SendCustomSynchronousRequest(const HTTPMessage& message);
  string SendSynchronousRequest(const string& uri);

  // Wraps data in a quic packet and sends it.
  ssize_t SendData(string data, bool last_data);

  QuicPacketCreator::Options* options() { return client_.options(); }

  const BalsaHeaders *response_headers() const {return &headers_;}

  void WaitForResponse();

  void Connect();
  void ResetConnection();
  void Disconnect();
  IPEndPoint LocalSocketAddress() const;
  void ClearPerRequestState();
  void WaitForInitialResponse();
  ssize_t Send(const void *buffer, size_t size);
  int response_size() const;
  size_t bytes_read() const;
  size_t bytes_written() const;

  // From ReliableQuicStream::Visitor
  virtual void OnClose(ReliableQuicStream* stream) OVERRIDE;

  void SetNextStreamId(QuicStreamId id);

  // Returns NULL if the maximum number of streams have already been created.
  QuicReliableClientStream* GetOrCreateStream();

  QuicErrorCode stream_error() { return stream_error_; }
  QuicErrorCode connection_error() { return connection_error_; }

  QuicClient* client() { return &client_; }

  const string& response_body() {return response_;}
  bool connected() const;

 private:
  IPEndPoint server_address_;
  IPEndPoint client_address_;
  QuicClient client_;  // The actual client
  QuicReliableClientStream* stream_;

  QuicErrorCode stream_error_;
  QuicErrorCode connection_error_;

  BalsaHeaders headers_;
  string response_;
  uint64 bytes_read_;
  uint64 bytes_written_;
  // True if the client has never connected before.  The client will
  // auto-connect exactly once before sending data.  If something causes a
  // connection reset, it will not automatically reconnect.
  bool never_connected_;
};

}  // namespace test

}  // namespace net

#endif  // NET_QUIC_TEST_TOOLS_QUIC_CLIENT_H_
