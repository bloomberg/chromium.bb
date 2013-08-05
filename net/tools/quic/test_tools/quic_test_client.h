// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_TEST_TOOLS_QUIC_CLIENT_H_
#define NET_QUIC_TEST_TOOLS_QUIC_CLIENT_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "net/quic/quic_framer.h"
#include "net/quic/quic_packet_creator.h"
#include "net/quic/quic_protocol.h"
#include "net/tools/quic/quic_client.h"
#include "net/tools/quic/quic_packet_writer.h"

namespace net {

class ProofVerifier;

namespace tools {

namespace test {

// Allows setting a writer for the client's QuicConnectionHelper, to allow
// fine-grained control of writes.
class QuicTestWriter : public QuicPacketWriter {
 public:
  virtual ~QuicTestWriter() {}
  virtual void set_fd(int fd) = 0;
};

class HTTPMessage;

// A toy QUIC client used for testing.
class QuicTestClient :  public ReliableQuicStream::Visitor {
 public:
  QuicTestClient(IPEndPoint server_address, const string& server_hostname,
                 const QuicVersion version);
  QuicTestClient(IPEndPoint server_address,
                 const string& server_hostname,
                 bool secure,
                 const QuicVersion version);
  QuicTestClient(IPEndPoint server_address,
                 const string& server_hostname,
                 bool secure,
                 const QuicConfig& config,
                 const QuicVersion version);

  virtual ~QuicTestClient();

  // ExpectCertificates controls whether the server is expected to provide
  // certificates. The certificates, if any, are not verified, but the common
  // name is recorded and available with |cert_common_name()|.
  void ExpectCertificates(bool on);

  // Clears any outstanding state and sends a simple GET of 'uri' to the
  // server.  Returns 0 if the request failed and no bytes were written.
  ssize_t SendRequest(const string& uri);
  ssize_t SendMessage(const HTTPMessage& message);

  string SendCustomSynchronousRequest(const HTTPMessage& message);
  string SendSynchronousRequest(const string& uri);

  // Wraps data in a quic packet and sends it.
  ssize_t SendData(string data, bool last_data);

  QuicPacketCreator::Options* options() { return client_->options(); }

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

  // Configures client_ to take ownership of and use the writer.
  // Must be called before initial connect.
  void UseWriter(QuicTestWriter* writer);

  // Returns NULL if the maximum number of streams have already been created.
  QuicReliableClientStream* GetOrCreateStream();

  QuicRstStreamErrorCode stream_error() { return stream_error_; }
  QuicErrorCode connection_error() { return client()->session()->error(); }

  QuicClient* client() { return client_.get(); }

  // cert_common_name returns the common name value of the server's certificate,
  // or the empty string if no certificate was presented.
  const string& cert_common_name() const;

  const string& response_body() {return response_;}
  bool connected() const;

  void set_auto_reconnect(bool reconnect) { auto_reconnect_ = reconnect; }

 private:
  void Initialize(IPEndPoint address, const string& hostname, bool secure);

  IPEndPoint server_address_;
  IPEndPoint client_address_;
  scoped_ptr<QuicClient> client_;  // The actual client
  QuicReliableClientStream* stream_;

  QuicRstStreamErrorCode stream_error_;

  BalsaHeaders headers_;
  string response_;
  uint64 bytes_read_;
  uint64 bytes_written_;
  // True if the client has never connected before.  The client will
  // auto-connect exactly once before sending data.  If something causes a
  // connection reset, it will not automatically reconnect.
  bool never_connected_;
  bool secure_;
  // If true, the client will always reconnect if necessary before creating a
  // stream.
  bool auto_reconnect_;

  // proof_verifier_ points to a RecordingProofVerifier that is owned by
  // client_.
  ProofVerifier* proof_verifier_;
};

}  // namespace test

}  // namespace tools
}  // namespace net

#endif  // NET_QUIC_TEST_TOOLS_QUIC_CLIENT_H_
