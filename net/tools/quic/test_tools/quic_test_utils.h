// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TOOLS_QUIC_TEST_TOOLS_QUIC_TEST_UTILS_H_
#define NET_TOOLS_QUIC_TEST_TOOLS_QUIC_TEST_UTILS_H_

#include <string>

#include "base/strings/string_piece.h"
#include "net/quic/quic_connection.h"
#include "net/quic/quic_session.h"
#include "net/quic/quic_spdy_decompressor.h"
#include "net/spdy/spdy_framer.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace net {

class EpollServer;
class IPEndPoint;

namespace tools {
namespace test {

std::string SerializeUncompressedHeaders(const SpdyHeaderBlock& headers);

class MockConnection : public QuicConnection {
 public:
  // Uses a QuicConnectionHelper created with fd and eps.
  MockConnection(QuicGuid guid,
                 IPEndPoint address,
                 int fd,
                 EpollServer* eps,
                 bool is_server);
  // Uses a MockHelper.
  MockConnection(QuicGuid guid, IPEndPoint address, bool is_server);
  MockConnection(QuicGuid guid,
                 IPEndPoint address,
                 QuicConnectionHelperInterface* helper, bool is_server);
  virtual ~MockConnection();

  // If the constructor that uses a MockHelper has been used then this method
  // will advance the time of the MockClock.
  void AdvanceTime(QuicTime::Delta delta);

  MOCK_METHOD3(ProcessUdpPacket, void(const IPEndPoint& self_address,
                                      const IPEndPoint& peer_address,
                                      const QuicEncryptedPacket& packet));
  MOCK_METHOD1(SendConnectionClose, void(QuicErrorCode error));
  MOCK_METHOD2(SendConnectionCloseWithDetails, void(
      QuicErrorCode error,
      const std::string& details));
  MOCK_METHOD2(SendRstStream, void(QuicStreamId id,
                                   QuicRstStreamErrorCode error));
  MOCK_METHOD3(SendGoAway, void(QuicErrorCode error,
                                QuicStreamId last_good_stream_id,
                                const std::string& reason));
  MOCK_METHOD0(OnCanWrite, bool());

  void ReallyProcessUdpPacket(const IPEndPoint& self_address,
                              const IPEndPoint& peer_address,
                              const QuicEncryptedPacket& packet) {
    return QuicConnection::ProcessUdpPacket(self_address, peer_address, packet);
  }

  virtual bool OnProtocolVersionMismatch(QuicVersion version) { return false; }

 private:
  const bool has_mock_helper_;

  DISALLOW_COPY_AND_ASSIGN(MockConnection);
};

class TestDecompressorVisitor : public QuicSpdyDecompressor::Visitor {
 public:
  virtual ~TestDecompressorVisitor() {}
  virtual bool OnDecompressedData(base::StringPiece data) OVERRIDE;
  virtual void OnDecompressionError() OVERRIDE;

  std::string data() { return data_; }
  bool error() { return error_; }

 private:
  std::string data_;
  bool error_;
};

class TestSession : public QuicSession {
 public:
  TestSession(QuicConnection* connection,
              const QuicConfig& config,
              bool is_server);
  virtual ~TestSession();

  MOCK_METHOD1(CreateIncomingReliableStream,
               ReliableQuicStream*(QuicStreamId id));
  MOCK_METHOD0(CreateOutgoingReliableStream, ReliableQuicStream*());

  void SetCryptoStream(QuicCryptoStream* stream);

  virtual QuicCryptoStream* GetCryptoStream();

 private:
  QuicCryptoStream* crypto_stream_;
  DISALLOW_COPY_AND_ASSIGN(TestSession);
};

}  // namespace test
}  // namespace tools
}  // namespace net

#endif  // NET_TOOLS_QUIC_TEST_TOOLS_QUIC_TEST_UTILS_H_
