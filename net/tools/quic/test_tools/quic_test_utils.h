// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Common utilities for Quic tests

#ifndef NET_TOOLS_QUIC_TEST_TOOLS_QUIC_TEST_UTILS_H_
#define NET_TOOLS_QUIC_TEST_TOOLS_QUIC_TEST_UTILS_H_

#include <string>

#include "base/strings/string_piece.h"
#include "net/quic/quic_connection.h"
#include "net/quic/quic_packet_writer.h"
#include "net/quic/quic_session.h"
#include "net/spdy/spdy_framer.h"
#include "net/tools/quic/quic_server_session.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace net {

class EpollServer;
class IPEndPoint;

namespace tools {
namespace test {

static const QuicConnectionId kTestConnectionId = 42;
static const int kTestPort = 123;
static const uint32 kInitialFlowControlWindowForTest = 32 * 1024;  // 32 KB

// Simple random number generator used to compute random numbers suitable
// for pseudo-randomly dropping packets in tests.  It works by computing
// the sha1 hash of the current seed, and using the first 64 bits as
// the next random number, and the next seed.
class SimpleRandom {
 public:
  SimpleRandom() : seed_(0) {}

  // Returns a random number in the range [0, kuint64max].
  uint64 RandUint64();

  void set_seed(uint64 seed) { seed_ = seed; }

 private:
  uint64 seed_;
};

class MockConnection : public QuicConnection {
 public:
  // Uses a MockHelper, ConnectionId of 42, and 127.0.0.1:123.
  explicit MockConnection(bool is_server);

  // Uses a MockHelper, ConnectionId of 42.
  MockConnection(IPEndPoint address, bool is_server);

  // Uses a MockHelper, and 127.0.0.1:123
  MockConnection(QuicConnectionId connection_id, bool is_server);

  // Uses a Mock helper, ConnectionId of 42, and 127.0.0.1:123.
  MockConnection(bool is_server, const QuicVersionVector& supported_versions);

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
  MOCK_METHOD2(SendConnectionClosePacket, void(QuicErrorCode error,
                                               const std::string& details));
  MOCK_METHOD3(SendRstStream, void(QuicStreamId id,
                                   QuicRstStreamErrorCode error,
                                   QuicStreamOffset bytes_written));
  MOCK_METHOD3(SendGoAway, void(QuicErrorCode error,
                                QuicStreamId last_good_stream_id,
                                const std::string& reason));
  MOCK_METHOD1(SendBlocked, void(QuicStreamId id));
  MOCK_METHOD2(SendWindowUpdate, void(QuicStreamId id,
                                      QuicStreamOffset byte_offset));
  MOCK_METHOD0(OnCanWrite, void());
  MOCK_CONST_METHOD0(HasPendingWrites, bool());

  void ReallyProcessUdpPacket(const IPEndPoint& self_address,
                              const IPEndPoint& peer_address,
                              const QuicEncryptedPacket& packet) {
    return QuicConnection::ProcessUdpPacket(self_address, peer_address, packet);
  }

  virtual bool OnProtocolVersionMismatch(QuicVersion version) { return false; }

 private:
  scoped_ptr<QuicPacketWriter> writer_;
  scoped_ptr<QuicConnectionHelperInterface> helper_;

  DISALLOW_COPY_AND_ASSIGN(MockConnection);
};

class TestSession : public QuicSession {
 public:
  TestSession(QuicConnection* connection, const QuicConfig& config);
  virtual ~TestSession();

  MOCK_METHOD1(CreateIncomingDataStream, QuicDataStream*(QuicStreamId id));
  MOCK_METHOD0(CreateOutgoingDataStream, QuicDataStream*());

  void SetCryptoStream(QuicCryptoStream* stream);

  virtual QuicCryptoStream* GetCryptoStream() OVERRIDE;

 private:
  QuicCryptoStream* crypto_stream_;
  DISALLOW_COPY_AND_ASSIGN(TestSession);
};

class MockPacketWriter : public QuicPacketWriter {
 public:
  MockPacketWriter();
  virtual ~MockPacketWriter();

  MOCK_METHOD4(WritePacket,
               WriteResult(const char* buffer,
                           size_t buf_len,
                           const IPAddressNumber& self_address,
                           const IPEndPoint& peer_address));
  MOCK_CONST_METHOD0(IsWriteBlockedDataBuffered, bool());
  MOCK_CONST_METHOD0(IsWriteBlocked, bool());
  MOCK_METHOD0(SetWritable, void());
};

class MockQuicServerSessionVisitor : public QuicServerSessionVisitor {
 public:
  MockQuicServerSessionVisitor();
  virtual ~MockQuicServerSessionVisitor();
  MOCK_METHOD2(OnConnectionClosed, void(QuicConnectionId connection_id,
                                        QuicErrorCode error));
  MOCK_METHOD1(OnWriteBlocked, void(QuicBlockedWriterInterface* writer));
};

class MockAckNotifierDelegate : public QuicAckNotifier::DelegateInterface {
 public:
  MockAckNotifierDelegate();

  MOCK_METHOD4(OnAckNotification, void(int num_original_packets,
                                       int num_original_bytes,
                                       int num_retransmitted_packets,
                                       int num_retransmitted_bytes));

 protected:
  // Object is ref counted.
  virtual ~MockAckNotifierDelegate();
};

}  // namespace test
}  // namespace tools
}  // namespace net

#endif  // NET_TOOLS_QUIC_TEST_TOOLS_QUIC_TEST_UTILS_H_
