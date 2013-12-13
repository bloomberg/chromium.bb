// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_TEST_TOOLS_QUIC_TEST_CLIENT_H_
#define NET_QUIC_TEST_TOOLS_QUIC_TEST_CLIENT_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "net/quic/quic_framer.h"
#include "net/quic/quic_packet_creator.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/test_tools/quic_test_writer.h"
#include "net/tools/quic/quic_client.h"

namespace net {

class ProofVerifier;

namespace tools {

namespace test {

class HTTPMessage;

// A toy QUIC client used for testing.
class QuicTestClient :  public QuicDataStream::Visitor {
 public:
  QuicTestClient(IPEndPoint server_address, const string& server_hostname,
                 const QuicVersionVector& supported_versions);
  QuicTestClient(IPEndPoint server_address,
                 const string& server_hostname,
                 bool secure,
                 const QuicVersionVector& supported_versions);
  QuicTestClient(IPEndPoint server_address,
                 const string& server_hostname,
                 bool secure,
                 const QuicConfig& config,
                 const QuicVersionVector& supported_versions);

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

  void WaitForResponse();

  void Connect();
  void ResetConnection();
  void Disconnect();
  IPEndPoint LocalSocketAddress() const;
  void ClearPerRequestState();
  void WaitForResponseForMs(int timeout_ms);
  void WaitForInitialResponseForMs(int timeout_ms);
  ssize_t Send(const void *buffer, size_t size);
  bool response_complete() const { return response_complete_; }
  bool response_headers_complete() const;
  const BalsaHeaders* response_headers() const;
  int response_size() const;
  int response_header_size() const { return response_header_size_; }
  int response_body_size() const { return response_body_size_; }
  size_t bytes_read() const;
  size_t bytes_written() const;
  bool buffer_body() const { return buffer_body_; }
  void set_buffer_body(bool buffer_body) { buffer_body_ = buffer_body; }

  // From QuicDataStream::Visitor
  virtual void OnClose(QuicDataStream* stream) OVERRIDE;

  // Configures client_ to take ownership of and use the writer.
  // Must be called before initial connect.
  void UseWriter(net::test::QuicTestWriter* writer);
  // If the given GUID is nonzero, configures client_ to use a specific GUID
  // instead of a random one.
  void UseGuid(QuicGuid guid);

  // Returns NULL if the maximum number of streams have already been created.
  QuicSpdyClientStream* GetOrCreateStream();

  QuicRstStreamErrorCode stream_error() { return stream_error_; }
  QuicErrorCode connection_error() { return client()->session()->error(); }

  QuicClient* client() { return client_.get(); }

  // cert_common_name returns the common name value of the server's certificate,
  // or the empty string if no certificate was presented.
  const string& cert_common_name() const;

  const string& response_body() {return response_;}
  bool connected() const;

  void set_auto_reconnect(bool reconnect) { auto_reconnect_ = reconnect; }

  void set_priority(QuicPriority priority) { priority_ = priority; }

  void WaitForWriteToFlush();

 private:
  void Initialize(IPEndPoint address, const string& hostname, bool secure);

  IPEndPoint server_address_;
  IPEndPoint client_address_;
  scoped_ptr<QuicClient> client_;  // The actual client
  QuicSpdyClientStream* stream_;

  QuicRstStreamErrorCode stream_error_;

  bool response_complete_;
  bool response_headers_complete_;
  BalsaHeaders headers_;
  QuicPriority priority_;
  string response_;
  uint64 bytes_read_;
  uint64 bytes_written_;
  // The number of uncompressed HTTP header bytes received.
  int response_header_size_;
  // The number of HTTP body bytes received.
  int response_body_size_;
  // True if we tried to connect already since the last call to Disconnect().
  bool connect_attempted_;
  bool secure_;
  // The client will auto-connect exactly once before sending data.  If
  // something causes a connection reset, it will not automatically reconnect
  // unless auto_reconnect_ is true.
  bool auto_reconnect_;
  // Should we buffer the response body? Defaults to true.
  bool buffer_body_;

  // proof_verifier_ points to a RecordingProofVerifier that is owned by
  // client_.
  ProofVerifier* proof_verifier_;
};

}  // namespace test

}  // namespace tools
}  // namespace net

#endif  // NET_QUIC_TEST_TOOLS_QUIC_TEST_CLIENT_H_
