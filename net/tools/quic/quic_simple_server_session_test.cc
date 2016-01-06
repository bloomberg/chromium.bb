// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/quic_simple_server_session.h"

#include "base/macros.h"
#include "net/quic/crypto/quic_crypto_server_config.h"
#include "net/quic/crypto/quic_random.h"
#include "net/quic/proto/cached_network_parameters.pb.h"
#include "net/quic/quic_connection.h"
#include "net/quic/quic_crypto_server_stream.h"
#include "net/quic/quic_utils.h"
#include "net/quic/test_tools/crypto_test_utils.h"
#include "net/quic/test_tools/quic_config_peer.h"
#include "net/quic/test_tools/quic_connection_peer.h"
#include "net/quic/test_tools/quic_sent_packet_manager_peer.h"
#include "net/quic/test_tools/quic_session_peer.h"
#include "net/quic/test_tools/quic_spdy_session_peer.h"
#include "net/quic/test_tools/quic_spdy_stream_peer.h"
#include "net/quic/test_tools/quic_sustained_bandwidth_recorder_peer.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "net/test/gtest_util.h"
#include "net/tools/quic/quic_simple_server_stream.h"
#include "net/tools/quic/test_tools/mock_quic_server_session_visitor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using __gnu_cxx::vector;
using net::test::CryptoTestUtils;
using net::test::MockConnection;
using net::test::MockConnectionHelper;
using net::test::QuicConfigPeer;
using net::test::QuicConnectionPeer;
using net::test::QuicSpdyStreamPeer;
using net::test::QuicSentPacketManagerPeer;
using net::test::QuicSessionPeer;
using net::test::QuicSpdySessionPeer;
using net::test::QuicSustainedBandwidthRecorderPeer;
using net::test::SupportedVersions;
using net::test::ValueRestore;
using net::test::kClientDataStreamId1;
using net::test::kClientDataStreamId2;
using net::test::kClientDataStreamId3;
using net::test::kInitialSessionFlowControlWindowForTest;
using net::test::kInitialStreamFlowControlWindowForTest;
using std::string;
using testing::StrictMock;
using testing::_;

namespace net {
namespace tools {
namespace test {

class QuicSimpleServerSessionPeer {
 public:
  static void SetCryptoStream(QuicSimpleServerSession* s,
                              QuicCryptoServerStream* crypto_stream) {
    s->crypto_stream_.reset(crypto_stream);
    s->static_streams()[kCryptoStreamId] = crypto_stream;
  }

  static QuicSpdyStream* CreateIncomingDynamicStream(QuicSimpleServerSession* s,
                                                     QuicStreamId id) {
    return s->CreateIncomingDynamicStream(id);
  }

  static QuicSimpleServerStream* CreateOutgoingDynamicStream(
      QuicSimpleServerSession* s,
      SpdyPriority priority) {
    return s->CreateOutgoingDynamicStream(priority);
  }
};

namespace {

const size_t kMaxStreamsForTest = 10;

class QuicSimpleServerSessionTest
    : public ::testing::TestWithParam<QuicVersion> {
 protected:
  QuicSimpleServerSessionTest()
      : crypto_config_(QuicCryptoServerConfig::TESTING,
                       QuicRandom::GetInstance(),
                       CryptoTestUtils::ProofSourceForTesting()) {
    config_.SetMaxStreamsPerConnection(kMaxStreamsForTest, kMaxStreamsForTest);
    config_.SetInitialStreamFlowControlWindowToSend(
        kInitialStreamFlowControlWindowForTest);
    config_.SetInitialSessionFlowControlWindowToSend(
        kInitialSessionFlowControlWindowForTest);

    connection_ = new StrictMock<MockConnection>(
        &helper_, Perspective::IS_SERVER, SupportedVersions(GetParam()));
    session_.reset(new QuicSimpleServerSession(config_, connection_, &owner_,
                                               &crypto_config_));
    MockClock clock;
    handshake_message_.reset(crypto_config_.AddDefaultConfig(
        QuicRandom::GetInstance(), &clock,
        QuicCryptoServerConfig::ConfigOptions()));
    session_->Initialize();
    visitor_ = QuicConnectionPeer::GetVisitor(connection_);
  }

  StrictMock<MockQuicServerSessionVisitor> owner_;
  MockConnectionHelper helper_;
  StrictMock<MockConnection>* connection_;
  QuicConfig config_;
  QuicCryptoServerConfig crypto_config_;
  scoped_ptr<QuicSimpleServerSession> session_;
  scoped_ptr<CryptoHandshakeMessage> handshake_message_;
  QuicConnectionVisitorInterface* visitor_;
};

INSTANTIATE_TEST_CASE_P(Tests,
                        QuicSimpleServerSessionTest,
                        ::testing::ValuesIn(QuicSupportedVersions()));

TEST_P(QuicSimpleServerSessionTest, CloseStreamDueToReset) {
  // Open a stream, then reset it.
  // Send two bytes of payload to open it.
  QuicStreamFrame data1(kClientDataStreamId1, false, 0, StringPiece("HT"));
  session_->OnStreamFrame(data1);
  EXPECT_EQ(1u, session_->GetNumOpenIncomingStreams());

  // Send a reset (and expect the peer to send a RST in response).
  QuicRstStreamFrame rst1(kClientDataStreamId1, QUIC_ERROR_PROCESSING_STREAM,
                          0);
  EXPECT_CALL(*connection_,
              SendRstStream(kClientDataStreamId1, QUIC_RST_ACKNOWLEDGEMENT, 0));
  visitor_->OnRstStream(rst1);
  EXPECT_EQ(0u, session_->GetNumOpenIncomingStreams());

  // Send the same two bytes of payload in a new packet.
  visitor_->OnStreamFrame(data1);

  // The stream should not be re-opened.
  EXPECT_EQ(0u, session_->GetNumOpenIncomingStreams());
  EXPECT_TRUE(connection_->connected());
}

TEST_P(QuicSimpleServerSessionTest, NeverOpenStreamDueToReset) {
  // Send a reset (and expect the peer to send a RST in response).
  QuicRstStreamFrame rst1(kClientDataStreamId1, QUIC_ERROR_PROCESSING_STREAM,
                          0);
  EXPECT_CALL(*connection_,
              SendRstStream(kClientDataStreamId1, QUIC_RST_ACKNOWLEDGEMENT, 0));
  visitor_->OnRstStream(rst1);
  EXPECT_EQ(0u, session_->GetNumOpenIncomingStreams());

  // Send two bytes of payload.
  QuicStreamFrame data1(kClientDataStreamId1, false, 0, StringPiece("HT"));
  visitor_->OnStreamFrame(data1);

  // The stream should never be opened, now that the reset is received.
  EXPECT_EQ(0u, session_->GetNumOpenIncomingStreams());
  EXPECT_TRUE(connection_->connected());
}

TEST_P(QuicSimpleServerSessionTest, AcceptClosedStream) {
  // Send (empty) compressed headers followed by two bytes of data.
  QuicStreamFrame frame1(kClientDataStreamId1, false, 0,
                         StringPiece("\1\0\0\0\0\0\0\0HT"));
  QuicStreamFrame frame2(kClientDataStreamId2, false, 0,
                         StringPiece("\2\0\0\0\0\0\0\0HT"));
  visitor_->OnStreamFrame(frame1);
  visitor_->OnStreamFrame(frame2);
  EXPECT_EQ(2u, session_->GetNumOpenIncomingStreams());

  // Send a reset (and expect the peer to send a RST in response).
  QuicRstStreamFrame rst(kClientDataStreamId1, QUIC_ERROR_PROCESSING_STREAM, 0);
  EXPECT_CALL(*connection_,
              SendRstStream(kClientDataStreamId1, QUIC_RST_ACKNOWLEDGEMENT, 0));
  visitor_->OnRstStream(rst);

  // If we were tracking, we'd probably want to reject this because it's data
  // past the reset point of stream 3.  As it's a closed stream we just drop the
  // data on the floor, but accept the packet because it has data for stream 5.
  QuicStreamFrame frame3(kClientDataStreamId1, false, 2, StringPiece("TP"));
  QuicStreamFrame frame4(kClientDataStreamId2, false, 2, StringPiece("TP"));
  visitor_->OnStreamFrame(frame3);
  visitor_->OnStreamFrame(frame4);
  // The stream should never be opened, now that the reset is received.
  EXPECT_EQ(1u, session_->GetNumOpenIncomingStreams());
  EXPECT_TRUE(connection_->connected());
}

TEST_P(QuicSimpleServerSessionTest, CreateIncomingDynamicStreamDisconnected) {
  // Tests that incoming stream creation fails when connection is not connected.
  size_t initial_num_open_stream = session_->GetNumOpenIncomingStreams();
  QuicConnectionPeer::CloseConnection(connection_);
  EXPECT_DFATAL(QuicSimpleServerSessionPeer::CreateIncomingDynamicStream(
                    session_.get(), kClientDataStreamId1),
                "ShouldCreateIncomingDynamicStream called when disconnected");
  EXPECT_EQ(initial_num_open_stream, session_->GetNumOpenIncomingStreams());
}

TEST_P(QuicSimpleServerSessionTest, CreateEvenIncomingDynamicStream) {
  // Tests that incoming stream creation fails when given stream id is even.
  size_t initial_num_open_stream = session_->GetNumOpenIncomingStreams();
  EXPECT_CALL(*connection_, SendConnectionCloseWithDetails(
                                QUIC_INVALID_STREAM_ID,
                                "Client created even numbered stream"));
  QuicSimpleServerSessionPeer::CreateIncomingDynamicStream(session_.get(), 2);
  EXPECT_EQ(initial_num_open_stream, session_->GetNumOpenIncomingStreams());
}

TEST_P(QuicSimpleServerSessionTest, CreateIncomingDynamicStream) {
  std::unique_ptr<QuicSpdyStream> stream(
      QuicSimpleServerSessionPeer::CreateIncomingDynamicStream(
          session_.get(), kClientDataStreamId1));
  EXPECT_NE(nullptr, stream);
  EXPECT_EQ(kClientDataStreamId1, stream->id());
}

class MockQuicCryptoServerStream : public QuicCryptoServerStream {
 public:
  explicit MockQuicCryptoServerStream(
      const QuicCryptoServerConfig* crypto_config,
      QuicSession* session)
      : QuicCryptoServerStream(crypto_config, session) {}
  ~MockQuicCryptoServerStream() override {}

  MOCK_METHOD1(SendServerConfigUpdate,
               void(const CachedNetworkParameters* cached_network_parameters));

  void set_encryption_established(bool has_established) {
    encryption_established_ = has_established;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockQuicCryptoServerStream);
};

TEST_P(QuicSimpleServerSessionTest, CreateOutgoingDynamicStreamDisconnected) {
  // Tests that outgoing stream creation fails when connection is not connected.
  size_t initial_num_open_stream = session_->GetNumOpenOutgoingStreams();
  QuicConnectionPeer::CloseConnection(connection_);
  EXPECT_DFATAL(QuicSimpleServerSessionPeer::CreateOutgoingDynamicStream(
                    session_.get(), kDefaultPriority),
                "ShouldCreateOutgoingDynamicStream called when disconnected");

  EXPECT_EQ(initial_num_open_stream, session_->GetNumOpenOutgoingStreams());
}

TEST_P(QuicSimpleServerSessionTest, CreateOutgoingDynamicStreamUnencrypted) {
  // Tests that outgoing stream creation fails when encryption has not yet been
  // established.
  size_t initial_num_open_stream = session_->GetNumOpenOutgoingStreams();
  EXPECT_DFATAL(QuicSimpleServerSessionPeer::CreateOutgoingDynamicStream(
                    session_.get(), kDefaultPriority),
                "Encryption not established so no outgoing stream created.");
  EXPECT_EQ(initial_num_open_stream, session_->GetNumOpenOutgoingStreams());
}

TEST_P(QuicSimpleServerSessionTest, CreateOutgoingDynamicStreamUptoLimit) {
  // Tests that outgoing stream creation should not be affected by existing
  // incoming stream and vice-versa. But when reaching the limit of max outgoing
  // stream allowed, creation should fail.

  // Receive some data to initiate a incoming stream which should not effect
  // creating outgoing streams.
  QuicStreamFrame data1(kClientDataStreamId1, false, 0, StringPiece("HT"));
  session_->OnStreamFrame(data1);
  EXPECT_EQ(1u, session_->GetNumOpenIncomingStreams());
  EXPECT_EQ(0u, session_->GetNumOpenOutgoingStreams());

  // Assume encryption already established.
  MockQuicCryptoServerStream* crypto_stream =
      new MockQuicCryptoServerStream(&crypto_config_, session_.get());
  crypto_stream->set_encryption_established(true);
  QuicSimpleServerSessionPeer::SetCryptoStream(session_.get(), crypto_stream);

  // Create push streams till reaching the upper limit of allowed open streams.
  for (size_t i = 0; i < kMaxStreamsForTest; ++i) {
    QuicSpdyStream* created_stream =
        QuicSimpleServerSessionPeer::CreateOutgoingDynamicStream(
            session_.get(), kDefaultPriority);
    EXPECT_EQ(2 * (i + 1), created_stream->id());
    EXPECT_EQ(i + 1, session_->GetNumOpenOutgoingStreams());
  }

  // Continuing creating push stream would fail.
  EXPECT_EQ(nullptr, QuicSimpleServerSessionPeer::CreateOutgoingDynamicStream(
                         session_.get(), kDefaultPriority));
  EXPECT_EQ(kMaxStreamsForTest, session_->GetNumOpenOutgoingStreams());

  // Create peer initiated stream should have no problem.
  QuicStreamFrame data2(kClientDataStreamId2, false, 0, StringPiece("HT"));
  session_->OnStreamFrame(data2);
  EXPECT_EQ(2u, session_->GetNumOpenIncomingStreams());
}

}  // namespace
}  // namespace test
}  // namespace tools
}  // namespace net
