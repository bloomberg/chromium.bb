// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "net/third_party/quiche/src/quic/core/crypto/quic_crypto_server_config.h"
#include "net/third_party/quiche/src/quic/core/crypto/tls_client_connection.h"
#include "net/third_party/quiche/src/quic/core/crypto/tls_server_connection.h"
#include "net/third_party/quiche/src/quic/core/quic_utils.h"
#include "net/third_party/quiche/src/quic/core/tls_client_handshaker.h"
#include "net/third_party/quiche/src/quic/core/tls_server_handshaker.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_test.h"
#include "net/third_party/quiche/src/quic/test_tools/crypto_test_utils.h"
#include "net/third_party/quiche/src/quic/test_tools/fake_proof_source.h"
#include "net/third_party/quiche/src/quic/test_tools/mock_quic_session_visitor.h"
#include "net/third_party/quiche/src/quic/test_tools/quic_test_utils.h"
#include "net/third_party/quiche/src/quic/tools/fake_proof_verifier.h"
#include "net/third_party/quiche/src/common/platform/api/quiche_arraysize.h"
#include "net/third_party/quiche/src/common/platform/api/quiche_string_piece.h"

namespace quic {
namespace test {
namespace {

using ::testing::_;
using ::testing::ElementsAreArray;
using ::testing::Return;

class TestProofVerifier : public ProofVerifier {
 public:
  TestProofVerifier()
      : verifier_(crypto_test_utils::ProofVerifierForTesting()) {}

  QuicAsyncStatus VerifyProof(
      const std::string& hostname,
      const uint16_t port,
      const std::string& server_config,
      QuicTransportVersion quic_version,
      quiche::QuicheStringPiece chlo_hash,
      const std::vector<std::string>& certs,
      const std::string& cert_sct,
      const std::string& signature,
      const ProofVerifyContext* context,
      std::string* error_details,
      std::unique_ptr<ProofVerifyDetails>* details,
      std::unique_ptr<ProofVerifierCallback> callback) override {
    return verifier_->VerifyProof(
        hostname, port, server_config, quic_version, chlo_hash, certs, cert_sct,
        signature, context, error_details, details, std::move(callback));
  }

  QuicAsyncStatus VerifyCertChain(
      const std::string& hostname,
      const uint16_t port,
      const std::vector<std::string>& certs,
      const std::string& ocsp_response,
      const std::string& cert_sct,
      const ProofVerifyContext* context,
      std::string* error_details,
      std::unique_ptr<ProofVerifyDetails>* details,
      std::unique_ptr<ProofVerifierCallback> callback) override {
    if (!active_) {
      return verifier_->VerifyCertChain(hostname, port, certs, ocsp_response,
                                        cert_sct, context, error_details,
                                        details, std::move(callback));
    }
    pending_ops_.push_back(std::make_unique<VerifyChainPendingOp>(
        hostname, port, certs, ocsp_response, cert_sct, context, error_details,
        details, std::move(callback), verifier_.get()));
    return QUIC_PENDING;
  }

  std::unique_ptr<ProofVerifyContext> CreateDefaultContext() override {
    return nullptr;
  }

  void Activate() { active_ = true; }

  size_t NumPendingCallbacks() const { return pending_ops_.size(); }

  void InvokePendingCallback(size_t n) {
    CHECK(NumPendingCallbacks() > n);
    pending_ops_[n]->Run();
    auto it = pending_ops_.begin() + n;
    pending_ops_.erase(it);
  }

 private:
  // Implementation of ProofVerifierCallback that fails if the callback is ever
  // run.
  class FailingProofVerifierCallback : public ProofVerifierCallback {
   public:
    void Run(bool /*ok*/,
             const std::string& /*error_details*/,
             std::unique_ptr<ProofVerifyDetails>* /*details*/) override {
      FAIL();
    }
  };

  class VerifyChainPendingOp {
   public:
    VerifyChainPendingOp(const std::string& hostname,
                         const uint16_t port,
                         const std::vector<std::string>& certs,
                         const std::string& ocsp_response,
                         const std::string& cert_sct,
                         const ProofVerifyContext* context,
                         std::string* error_details,
                         std::unique_ptr<ProofVerifyDetails>* details,
                         std::unique_ptr<ProofVerifierCallback> callback,
                         ProofVerifier* delegate)
        : hostname_(hostname),
          port_(port),
          certs_(certs),
          ocsp_response_(ocsp_response),
          cert_sct_(cert_sct),
          context_(context),
          error_details_(error_details),
          details_(details),
          callback_(std::move(callback)),
          delegate_(delegate) {}

    void Run() {
      // TestProofVerifier depends on crypto_test_utils::ProofVerifierForTesting
      // running synchronously. It passes a FailingProofVerifierCallback and
      // runs the original callback after asserting that the verification ran
      // synchronously.
      QuicAsyncStatus status = delegate_->VerifyCertChain(
          hostname_, port_, certs_, ocsp_response_, cert_sct_, context_,
          error_details_, details_,
          std::make_unique<FailingProofVerifierCallback>());
      ASSERT_NE(status, QUIC_PENDING);
      callback_->Run(status == QUIC_SUCCESS, *error_details_, details_);
    }

   private:
    std::string hostname_;
    const uint16_t port_;
    std::vector<std::string> certs_;
    std::string ocsp_response_;
    std::string cert_sct_;
    const ProofVerifyContext* context_;
    std::string* error_details_;
    std::unique_ptr<ProofVerifyDetails>* details_;
    std::unique_ptr<ProofVerifierCallback> callback_;
    ProofVerifier* delegate_;
  };

  std::unique_ptr<ProofVerifier> verifier_;
  bool active_ = false;
  std::vector<std::unique_ptr<VerifyChainPendingOp>> pending_ops_;
};

class TestQuicCryptoStream : public QuicCryptoStream {
 public:
  explicit TestQuicCryptoStream(QuicSession* session)
      : QuicCryptoStream(session) {}

  ~TestQuicCryptoStream() override = default;

  virtual TlsHandshaker* handshaker() const = 0;

  bool encryption_established() const override {
    return handshaker()->encryption_established();
  }

  bool one_rtt_keys_available() const override {
    return handshaker()->one_rtt_keys_available();
  }

  const QuicCryptoNegotiatedParameters& crypto_negotiated_params()
      const override {
    return handshaker()->crypto_negotiated_params();
  }

  CryptoMessageParser* crypto_message_parser() override {
    return handshaker()->crypto_message_parser();
  }

  void WriteCryptoData(EncryptionLevel level,
                       quiche::QuicheStringPiece data) override {
    pending_writes_.push_back(std::make_pair(std::string(data), level));
  }

  void OnPacketDecrypted(EncryptionLevel /*level*/) override {}
  void OnOneRttPacketAcknowledged() override {}
  void OnHandshakePacketSent() override {}

  HandshakeState GetHandshakeState() const override {
    return handshaker()->GetHandshakeState();
  }

  const std::vector<std::pair<std::string, EncryptionLevel>>& pending_writes() {
    return pending_writes_;
  }

  // Sends the pending frames to |stream| and clears the array of pending
  // writes.
  void SendCryptoMessagesToPeer(QuicCryptoStream* stream) {
    QUIC_LOG(INFO) << "Sending " << pending_writes_.size() << " frames";
    // This is a minimal re-implementation of QuicCryptoStream::OnDataAvailable.
    // It doesn't work to call QuicStream::OnStreamFrame because
    // QuicCryptoStream::OnDataAvailable currently (as an implementation detail)
    // relies on the QuicConnection to know the EncryptionLevel to pass into
    // CryptoMessageParser::ProcessInput. Since the crypto messages in this test
    // never reach the framer or connection and never get encrypted/decrypted,
    // QuicCryptoStream::OnDataAvailable isn't able to call ProcessInput with
    // the correct EncryptionLevel. Instead, that can be short-circuited by
    // directly calling ProcessInput here.
    for (size_t i = 0; i < pending_writes_.size(); ++i) {
      if (!stream->crypto_message_parser()->ProcessInput(
              pending_writes_[i].first, pending_writes_[i].second)) {
        OnUnrecoverableError(stream->crypto_message_parser()->error(),
                             stream->crypto_message_parser()->error_detail());
        break;
      }
    }
    pending_writes_.clear();
  }

 private:
  std::vector<std::pair<std::string, EncryptionLevel>> pending_writes_;
};

class MockProofHandler : public QuicCryptoClientStream::ProofHandler {
 public:
  MockProofHandler() = default;
  ~MockProofHandler() override {}

  MOCK_METHOD(  // NOLINT(build/deprecated)
      void,
      OnProofValid,
      (const QuicCryptoClientConfig::CachedState&),
      (override));
  MOCK_METHOD(  // NOLINT(build/deprecated)
      void,
      OnProofVerifyDetailsAvailable,
      (const ProofVerifyDetails&),
      (override));
};

class TestQuicCryptoClientStream : public TestQuicCryptoStream {
 public:
  explicit TestQuicCryptoClientStream(QuicSession* session)
      : TestQuicCryptoClientStream(session,
                                   QuicServerId("test.example.com", 443),
                                   std::make_unique<TestProofVerifier>()) {}

  TestQuicCryptoClientStream(QuicSession* session,
                             const QuicServerId& server_id,
                             std::unique_ptr<ProofVerifier> proof_verifier)
      : TestQuicCryptoStream(session),
        crypto_config_(std::move(proof_verifier),
                       /*session_cache*/ nullptr),
        handshaker_(new TlsClientHandshaker(
            server_id,
            this,
            session,
            crypto_test_utils::ProofVerifyContextForTesting(),
            &crypto_config_,
            &proof_handler_,
            /*has_application_state = */ false)) {}

  ~TestQuicCryptoClientStream() override = default;

  TlsHandshaker* handshaker() const override { return handshaker_.get(); }
  TlsClientHandshaker* client_handshaker() const { return handshaker_.get(); }
  const MockProofHandler& proof_handler() { return proof_handler_; }
  void OnHandshakeDoneReceived() override {}

  bool CryptoConnect() { return handshaker_->CryptoConnect(); }

  TestProofVerifier* GetTestProofVerifier() const {
    return static_cast<TestProofVerifier*>(crypto_config_.proof_verifier());
  }

 private:
  MockProofHandler proof_handler_;
  QuicCryptoClientConfig crypto_config_;
  std::unique_ptr<TlsClientHandshaker> handshaker_;
};

class TestTlsServerHandshaker : public TlsServerHandshaker {
 public:
  TestTlsServerHandshaker(QuicSession* session,
                          const QuicCryptoServerConfig& crypto_config,
                          TestQuicCryptoStream* test_stream)
      : TlsServerHandshaker(session, crypto_config),
        test_stream_(test_stream) {}

  void WriteCryptoData(EncryptionLevel level,
                       quiche::QuicheStringPiece data) override {
    test_stream_->WriteCryptoData(level, data);
  }

 private:
  TestQuicCryptoStream* test_stream_;
};

class TestQuicCryptoServerStream : public TestQuicCryptoStream {
 public:
  TestQuicCryptoServerStream(QuicSession* session)
      : TestQuicCryptoStream(session),
        crypto_config_(QuicCryptoServerConfig::TESTING,
                       QuicRandom::GetInstance(),
                       std::make_unique<FakeProofSource>(),
                       KeyExchangeSource::Default()),
        handshaker_(
            new TestTlsServerHandshaker(session, crypto_config_, this)) {}

  ~TestQuicCryptoServerStream() override = default;

  void CancelOutstandingCallbacks() {
    handshaker_->CancelOutstandingCallbacks();
  }

  void OnPacketDecrypted(EncryptionLevel level) override {
    handshaker_->OnPacketDecrypted(level);
  }
  void OnHandshakeDoneReceived() override { DCHECK(false); }

  TlsHandshaker* handshaker() const override { return handshaker_.get(); }

  FakeProofSource* GetFakeProofSource() const {
    return static_cast<FakeProofSource*>(crypto_config_.proof_source());
  }

 private:
  QuicCryptoServerConfig crypto_config_;
  std::unique_ptr<TlsServerHandshaker> handshaker_;
};

void ExchangeHandshakeMessages(TestQuicCryptoStream* client,
                               TestQuicCryptoServerStream* server) {
  while (!client->pending_writes().empty() ||
         !server->pending_writes().empty()) {
    client->SendCryptoMessagesToPeer(server);
    server->SendCryptoMessagesToPeer(client);
  }
}

class TlsHandshakerTest : public QuicTestWithParam<ParsedQuicVersion> {
 public:
  TlsHandshakerTest()
      : version_(GetParam()),
        client_conn_(new MockQuicConnection(&conn_helper_,
                                            &alarm_factory_,
                                            Perspective::IS_CLIENT,
                                            {version_})),
        server_conn_(new MockQuicConnection(&conn_helper_,
                                            &alarm_factory_,
                                            Perspective::IS_SERVER,
                                            {version_})),
        client_session_(client_conn_, /*create_mock_crypto_stream=*/false),
        server_session_(server_conn_, /*create_mock_crypto_stream=*/false) {
    client_stream_ = new TestQuicCryptoClientStream(&client_session_);
    client_session_.SetCryptoStream(client_stream_);
    server_stream_ = new TestQuicCryptoServerStream(&server_session_);
    server_session_.SetCryptoStream(server_stream_);
    client_session_.Initialize();
    server_session_.Initialize();
    EXPECT_FALSE(client_stream_->encryption_established());
    EXPECT_FALSE(client_stream_->one_rtt_keys_available());
    EXPECT_FALSE(server_stream_->encryption_established());
    EXPECT_FALSE(server_stream_->one_rtt_keys_available());
    const std::string default_alpn =
        AlpnForVersion(client_session_.connection()->version());
    ON_CALL(client_session_, GetAlpnsToOffer())
        .WillByDefault(Return(std::vector<std::string>({default_alpn})));
    ON_CALL(server_session_, SelectAlpn(_))
        .WillByDefault(
            [default_alpn](
                const std::vector<quiche::QuicheStringPiece>& alpns) {
              return std::find(alpns.begin(), alpns.end(), default_alpn);
            });
  }

  void ExpectHandshakeSuccessful() {
    EXPECT_TRUE(client_stream_->one_rtt_keys_available());
    EXPECT_TRUE(client_stream_->encryption_established());
    EXPECT_TRUE(server_stream_->one_rtt_keys_available());
    EXPECT_TRUE(server_stream_->encryption_established());
    EXPECT_EQ(HANDSHAKE_COMPLETE, client_stream_->GetHandshakeState());
    EXPECT_EQ(HANDSHAKE_CONFIRMED, server_stream_->GetHandshakeState());

    const auto& client_crypto_params =
        client_stream_->crypto_negotiated_params();
    const auto& server_crypto_params =
        server_stream_->crypto_negotiated_params();
    // The TLS params should be filled in on the client.
    EXPECT_NE(0, client_crypto_params.cipher_suite);
    EXPECT_NE(0, client_crypto_params.key_exchange_group);
    EXPECT_NE(0, client_crypto_params.peer_signature_algorithm);

    // The cipher suite and key exchange group should match on the client and
    // server.
    EXPECT_EQ(client_crypto_params.cipher_suite,
              server_crypto_params.cipher_suite);
    EXPECT_EQ(client_crypto_params.key_exchange_group,
              server_crypto_params.key_exchange_group);
    // We don't support client certs on the server (yet), so the server
    // shouldn't have a peer signature algorithm to report.
    EXPECT_EQ(0, server_crypto_params.peer_signature_algorithm);
  }

  ParsedQuicVersion version_;
  MockQuicConnectionHelper conn_helper_;
  MockAlarmFactory alarm_factory_;
  MockQuicConnection* client_conn_;
  MockQuicConnection* server_conn_;
  MockQuicSession client_session_;
  MockQuicSession server_session_;

  TestQuicCryptoClientStream* client_stream_;
  TestQuicCryptoServerStream* server_stream_;
};

std::vector<ParsedQuicVersion> AllSupportedTlsVersions() {
  std::vector<ParsedQuicVersion> tls_versions;
  for (const ParsedQuicVersion& version : AllSupportedVersions()) {
    if (version.handshake_protocol == PROTOCOL_TLS1_3) {
      tls_versions.push_back(version);
    }
  }
  return tls_versions;
}

INSTANTIATE_TEST_SUITE_P(TlsHandshakerTests,
                         TlsHandshakerTest,
                         ::testing::ValuesIn(AllSupportedTlsVersions()),
                         ::testing::PrintToStringParamName());

TEST_P(TlsHandshakerTest, CryptoHandshake) {
  EXPECT_FALSE(client_conn_->IsHandshakeComplete());
  EXPECT_FALSE(server_conn_->IsHandshakeComplete());

  EXPECT_CALL(*client_conn_, CloseConnection(_, _, _)).Times(0);
  EXPECT_CALL(*server_conn_, CloseConnection(_, _, _)).Times(0);
  EXPECT_CALL(client_stream_->proof_handler(), OnProofVerifyDetailsAvailable);
  client_stream_->CryptoConnect();
  ExchangeHandshakeMessages(client_stream_, server_stream_);

  ExpectHandshakeSuccessful();
}

}  // namespace
}  // namespace test
}  // namespace quic
