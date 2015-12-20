// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/quic_client_session.h"

#include <vector>

#include "net/base/ip_endpoint.h"
#include "net/quic/crypto/aes_128_gcm_12_encrypter.h"
#include "net/quic/quic_flags.h"
#include "net/quic/test_tools/crypto_test_utils.h"
#include "net/quic/test_tools/quic_connection_peer.h"
#include "net/quic/test_tools/quic_packet_creator_peer.h"
#include "net/quic/test_tools/quic_spdy_session_peer.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "net/tools/quic/quic_spdy_client_stream.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::test::ConstructEncryptedPacket;
using net::test::ConstructMisFramedEncryptedPacket;
using net::test::CryptoTestUtils;
using net::test::DefaultQuicConfig;
using net::test::MockConnection;
using net::test::MockConnectionHelper;
using net::test::PacketSavingConnection;
using net::test::QuicConnectionPeer;
using net::test::QuicPacketCreatorPeer;
using net::test::QuicSpdySessionPeer;
using net::test::SupportedVersions;
using net::test::TestPeerIPAddress;
using net::test::ValueRestore;
using net::test::kTestPort;
using testing::AnyNumber;
using testing::Invoke;
using testing::Truly;
using testing::_;

namespace net {
namespace tools {
namespace test {
namespace {

const char kServerHostname[] = "test.example.com";
const uint16_t kPort = 80;

class ToolsQuicClientSessionTest
    : public ::testing::TestWithParam<QuicVersion> {
 protected:
  ToolsQuicClientSessionTest()
      : crypto_config_(CryptoTestUtils::ProofVerifierForTesting()) {
    Initialize();
    // Advance the time, because timers do not like uninitialized times.
    connection_->AdvanceTime(QuicTime::Delta::FromSeconds(1));
  }

  void Initialize() {
    session_.reset();
    connection_ = new PacketSavingConnection(&helper_, Perspective::IS_CLIENT,
                                             SupportedVersions(GetParam()));
    session_.reset(new QuicClientSession(
        DefaultQuicConfig(), connection_,
        QuicServerId(kServerHostname, kPort, PRIVACY_MODE_DISABLED),
        &crypto_config_));
    session_->Initialize();
  }

  void CompleteCryptoHandshake() {
    session_->CryptoConnect();
    QuicCryptoClientStream* stream =
        static_cast<QuicCryptoClientStream*>(session_->GetCryptoStream());
    CryptoTestUtils::FakeServerOptions options;
    CryptoTestUtils::HandshakeWithFakeServer(&helper_, connection_, stream,
                                             options);
  }

  QuicCryptoClientConfig crypto_config_;
  MockConnectionHelper helper_;
  PacketSavingConnection* connection_;
  scoped_ptr<QuicClientSession> session_;
};

INSTANTIATE_TEST_CASE_P(Tests,
                        ToolsQuicClientSessionTest,
                        ::testing::ValuesIn(QuicSupportedVersions()));

TEST_P(ToolsQuicClientSessionTest, CryptoConnect) {
  CompleteCryptoHandshake();
}

TEST_P(ToolsQuicClientSessionTest, NoEncryptionAfterInitialEncryption) {
  ValueRestore<bool> old_flag(&FLAGS_quic_block_unencrypted_writes, true);
  // Complete a handshake in order to prime the crypto config for 0-RTT.
  CompleteCryptoHandshake();

  // Now create a second session using the same crypto config.
  Initialize();

  // Starting the handshake should move immediately to encryption
  // established and will allow streams to be created.
  session_->CryptoConnect();
  EXPECT_TRUE(session_->IsEncryptionEstablished());
  QuicSpdyClientStream* stream =
      session_->CreateOutgoingDynamicStream(kDefaultPriority);
  DCHECK_NE(kCryptoStreamId, stream->id());
  EXPECT_TRUE(stream != nullptr);

  // Process an "inchoate" REJ from the server which will cause
  // an inchoate CHLO to be sent and will leave the encryption level
  // at NONE.
  CryptoHandshakeMessage rej;
  CryptoTestUtils::FillInDummyReject(&rej, /* stateless */ false);
  EXPECT_TRUE(session_->IsEncryptionEstablished());
  session_->GetCryptoStream()->OnHandshakeMessage(rej);
  EXPECT_FALSE(session_->IsEncryptionEstablished());
  EXPECT_EQ(ENCRYPTION_NONE,
            QuicPacketCreatorPeer::GetEncryptionLevel(
                QuicConnectionPeer::GetPacketCreator(connection_)));
  // Verify that no new streams may be created.
  EXPECT_TRUE(session_->CreateOutgoingDynamicStream(kDefaultPriority) ==
              nullptr);
  // Verify that no data may be send on existing streams.
  char data[] = "hello world";
  struct iovec iov = {data, arraysize(data)};
  QuicIOVector iovector(&iov, 1, iov.iov_len);
  QuicConsumedData consumed = session_->WritevData(
      stream->id(), iovector, 0, false, MAY_FEC_PROTECT, nullptr);
  EXPECT_FALSE(consumed.fin_consumed);
  EXPECT_EQ(0u, consumed.bytes_consumed);
}

TEST_P(ToolsQuicClientSessionTest, MaxNumStreamsWithNoFinOrRst) {
  EXPECT_CALL(*connection_, SendRstStream(_, _, _)).Times(AnyNumber());

  session_->config()->SetMaxStreamsPerConnection(1, 1);

  // Initialize crypto before the client session will create a stream.
  CompleteCryptoHandshake();

  QuicSpdyClientStream* stream =
      session_->CreateOutgoingDynamicStream(kDefaultPriority);
  ASSERT_TRUE(stream);
  EXPECT_FALSE(session_->CreateOutgoingDynamicStream(kDefaultPriority));

  // Close the stream, but without having received a FIN or a RST_STREAM
  // and check that a new one can not be created.
  session_->CloseStream(stream->id());
  EXPECT_EQ(1u, session_->GetNumOpenOutgoingStreams());

  stream = session_->CreateOutgoingDynamicStream(kDefaultPriority);
  EXPECT_FALSE(stream);
}

TEST_P(ToolsQuicClientSessionTest, MaxNumStreamsWithRst) {
  EXPECT_CALL(*connection_, SendRstStream(_, _, _)).Times(AnyNumber());

  session_->config()->SetMaxStreamsPerConnection(1, 1);

  // Initialize crypto before the client session will create a stream.
  CompleteCryptoHandshake();

  QuicSpdyClientStream* stream =
      session_->CreateOutgoingDynamicStream(kDefaultPriority);
  ASSERT_TRUE(stream);
  EXPECT_FALSE(session_->CreateOutgoingDynamicStream(kDefaultPriority));

  // Close the stream and receive an RST frame to remove the unfinished stream
  session_->CloseStream(stream->id());
  session_->OnRstStream(QuicRstStreamFrame(
      stream->id(), AdjustErrorForVersion(QUIC_RST_ACKNOWLEDGEMENT, GetParam()),
      0));
  // Check that a new one can be created.
  EXPECT_EQ(0u, session_->GetNumOpenOutgoingStreams());
  stream = session_->CreateOutgoingDynamicStream(kDefaultPriority);
  EXPECT_TRUE(stream);
}

TEST_P(ToolsQuicClientSessionTest, GoAwayReceived) {
  CompleteCryptoHandshake();

  // After receiving a GoAway, I should no longer be able to create outgoing
  // streams.
  session_->connection()->OnGoAwayFrame(
      QuicGoAwayFrame(QUIC_PEER_GOING_AWAY, 1u, "Going away."));
  EXPECT_EQ(nullptr, session_->CreateOutgoingDynamicStream(kDefaultPriority));
}

TEST_P(ToolsQuicClientSessionTest, SetFecProtectionFromConfig) {
  ValueRestore<bool> old_flag(&FLAGS_enable_quic_fec, true);

  // Set FEC config in client's connection options.
  QuicTagVector copt;
  copt.push_back(kFHDR);
  session_->config()->SetConnectionOptionsToSend(copt);

  // Doing the handshake should set up FEC config correctly.
  CompleteCryptoHandshake();

  // Verify that headers stream is always protected and data streams are
  // optionally protected.
  EXPECT_EQ(
      FEC_PROTECT_ALWAYS,
      QuicSpdySessionPeer::GetHeadersStream(session_.get())->fec_policy());
  QuicSpdyClientStream* stream =
      session_->CreateOutgoingDynamicStream(kDefaultPriority);
  ASSERT_TRUE(stream);
  EXPECT_EQ(FEC_PROTECT_OPTIONAL, stream->fec_policy());
}

static bool CheckForDecryptionError(QuicFramer* framer) {
  return framer->error() == QUIC_DECRYPTION_FAILURE;
}

// Regression test for b/17206611.
TEST_P(ToolsQuicClientSessionTest, InvalidPacketReceived) {
  IPEndPoint server_address(TestPeerIPAddress(), kTestPort);
  IPEndPoint client_address(TestPeerIPAddress(), kTestPort);

  EXPECT_CALL(*connection_, ProcessUdpPacket(server_address, client_address, _))
      .WillRepeatedly(Invoke(static_cast<MockConnection*>(connection_),
                             &MockConnection::ReallyProcessUdpPacket));
  EXPECT_CALL(*connection_, OnCanWrite()).Times(AnyNumber());
  EXPECT_CALL(*connection_, OnError(_)).Times(1);

  // Verify that empty packets don't close the connection.
  QuicEncryptedPacket zero_length_packet(nullptr, 0, false);
  EXPECT_CALL(*connection_, SendConnectionCloseWithDetails(_, _)).Times(0);
  session_->connection()->ProcessUdpPacket(client_address, server_address,
                                           zero_length_packet);

  // Verifiy that small, invalid packets don't close the connection.
  char buf[2] = {0x00, 0x01};
  QuicEncryptedPacket valid_packet(buf, 2, false);
  // Close connection shouldn't be called.
  EXPECT_CALL(*connection_, SendConnectionCloseWithDetails(_, _)).Times(0);
  session_->connection()->ProcessUdpPacket(client_address, server_address,
                                           valid_packet);

  // Verify that a non-decryptable packet doesn't close the connection.
  QuicConnectionId connection_id = session_->connection()->connection_id();
  scoped_ptr<QuicEncryptedPacket> packet(
      ConstructEncryptedPacket(connection_id, false, false, 100, "data"));
  // Change the last byte of the encrypted data.
  *(const_cast<char*>(packet->data() + packet->length() - 1)) += 1;
  EXPECT_CALL(*connection_, SendConnectionCloseWithDetails(_, _)).Times(0);
  EXPECT_CALL(*connection_, OnError(Truly(CheckForDecryptionError))).Times(1);
  session_->connection()->ProcessUdpPacket(client_address, server_address,
                                           *packet);
}

// A packet with invalid framing should cause a connection to be closed.
TEST_P(ToolsQuicClientSessionTest, InvalidFramedPacketReceived) {
  IPEndPoint server_address(TestPeerIPAddress(), kTestPort);
  IPEndPoint client_address(TestPeerIPAddress(), kTestPort);

  EXPECT_CALL(*connection_, ProcessUdpPacket(server_address, client_address, _))
      .WillRepeatedly(Invoke(static_cast<MockConnection*>(connection_),
                             &MockConnection::ReallyProcessUdpPacket));
  EXPECT_CALL(*connection_, OnError(_)).Times(1);

  // Verify that a decryptable packet with bad frames does close the connection.
  QuicConnectionId connection_id = session_->connection()->connection_id();
  scoped_ptr<QuicEncryptedPacket> packet(ConstructMisFramedEncryptedPacket(
      connection_id, false, false, 100, "data", PACKET_8BYTE_CONNECTION_ID,
      PACKET_6BYTE_PACKET_NUMBER, nullptr));
  EXPECT_CALL(*connection_, SendConnectionCloseWithDetails(_, _)).Times(1);
  session_->connection()->ProcessUdpPacket(client_address, server_address,
                                           *packet);
}

}  // namespace
}  // namespace test
}  // namespace tools
}  // namespace net
