// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/quic_client_session.h"

#include <vector>

#include "net/base/ip_endpoint.h"
#include "net/quic/crypto/aes_128_gcm_12_encrypter.h"
#include "net/quic/test_tools/crypto_test_utils.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "net/tools/quic/quic_reliable_client_stream.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using net::test::CryptoTestUtils;
using net::test::PacketSavingConnection;

namespace net {
namespace tools {
namespace test {
namespace {

const char kServerHostname[] = "www.example.com";

class ToolsQuicClientSessionTest : public ::testing::Test {
 protected:
  ToolsQuicClientSessionTest()
      : guid_(1),
        connection_(new PacketSavingConnection(guid_, IPEndPoint(), false)) {
    crypto_config_.SetDefaults();
    session_.reset(new QuicClientSession(kServerHostname, QuicConfig(),
                                         connection_, &crypto_config_));
    session_->config()->SetDefaults();
  }

  void CompleteCryptoHandshake() {
    ASSERT_TRUE(session_->CryptoConnect());
    CryptoTestUtils::HandshakeWithFakeServer(
        connection_, session_->GetCryptoStream());
  }

  QuicGuid guid_;
  PacketSavingConnection* connection_;
  scoped_ptr<QuicClientSession> session_;
  QuicCryptoClientConfig crypto_config_;
};

TEST_F(ToolsQuicClientSessionTest, CryptoConnect) {
  if (!Aes128Gcm12Encrypter::IsSupported()) {
    LOG(INFO) << "AES GCM not supported. Test skipped.";
    return;
  }
  CompleteCryptoHandshake();
}

TEST_F(ToolsQuicClientSessionTest, MaxNumStreams) {
  session_->config()->set_max_streams_per_connection(1, 1);
  if (!Aes128Gcm12Encrypter::IsSupported()) {
    LOG(INFO) << "AES GCM not supported. Test skipped.";
    return;
  }
  // FLAGS_max_streams_per_connection = 1;
  // Initialize crypto before the client session will create a stream.
  CompleteCryptoHandshake();

  QuicReliableClientStream* stream =
      session_->CreateOutgoingReliableStream();
  ASSERT_TRUE(stream);
  EXPECT_FALSE(session_->CreateOutgoingReliableStream());

  // Close a stream and ensure I can now open a new one.
  session_->CloseStream(stream->id());
  stream = session_->CreateOutgoingReliableStream();
  EXPECT_TRUE(stream);
}

TEST_F(ToolsQuicClientSessionTest, GoAwayReceived) {
  if (!Aes128Gcm12Encrypter::IsSupported()) {
    LOG(INFO) << "AES GCM not supported. Test skipped.";
    return;
  }

  CompleteCryptoHandshake();

  // After receiving a GoAway, I should no longer be able to create outgoing
  // streams.
  session_->OnGoAway(QuicGoAwayFrame(QUIC_PEER_GOING_AWAY, 1u, "Going away."));
  EXPECT_EQ(NULL, session_->CreateOutgoingReliableStream());
}

}  // namespace
}  // namespace test
}  // namespace tools
}  // namespace net
