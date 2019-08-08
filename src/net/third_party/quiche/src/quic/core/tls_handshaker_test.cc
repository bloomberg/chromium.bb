// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "net/third_party/quiche/src/quic/core/quic_utils.h"
#include "net/third_party/quiche/src/quic/core/tls_client_handshaker.h"
#include "net/third_party/quiche/src/quic/core/tls_server_handshaker.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_arraysize.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_ptr_util.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_test.h"
#include "net/third_party/quiche/src/quic/test_tools/crypto_test_utils.h"
#include "net/third_party/quiche/src/quic/test_tools/fake_proof_source.h"
#include "net/third_party/quiche/src/quic/test_tools/mock_quic_session_visitor.h"
#include "net/third_party/quiche/src/quic/test_tools/quic_test_utils.h"

namespace quic {
namespace test {
namespace {

using ::testing::_;

class FakeProofVerifier : public ProofVerifier {
 public:
  FakeProofVerifier()
      : verifier_(crypto_test_utils::ProofVerifierForTesting()) {}

  QuicAsyncStatus VerifyProof(
      const std::string& hostname,
      const uint16_t port,
      const std::string& server_config,
      QuicTransportVersion quic_version,
      QuicStringPiece chlo_hash,
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
      const std::vector<std::string>& certs,
      const std::string& ocsp_response,
      const std::string& cert_sct,
      const ProofVerifyContext* context,
      std::string* error_details,
      std::unique_ptr<ProofVerifyDetails>* details,
      std::unique_ptr<ProofVerifierCallback> callback) override {
    if (!active_) {
      return verifier_->VerifyCertChain(hostname, certs, ocsp_response,
                                        cert_sct, context, error_details,
                                        details, std::move(callback));
    }
    pending_ops_.push_back(QuicMakeUnique<VerifyChainPendingOp>(
        hostname, certs, ocsp_response, cert_sct, context, error_details,
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
    void Run(bool ok,
             const std::string& error_details,
             std::unique_ptr<ProofVerifyDetails>* details) override {
      FAIL();
    }
  };

  class VerifyChainPendingOp {
   public:
    VerifyChainPendingOp(const std::string& hostname,
                         const std::vector<std::string>& certs,
                         const std::string& ocsp_response,
                         const std::string& cert_sct,
                         const ProofVerifyContext* context,
                         std::string* error_details,
                         std::unique_ptr<ProofVerifyDetails>* details,
                         std::unique_ptr<ProofVerifierCallback> callback,
                         ProofVerifier* delegate)
        : hostname_(hostname),
          certs_(certs),
          ocsp_response_(ocsp_response),
          cert_sct_(cert_sct),
          context_(context),
          error_details_(error_details),
          details_(details),
          callback_(std::move(callback)),
          delegate_(delegate) {}

    void Run() {
      // FakeProofVerifier depends on crypto_test_utils::ProofVerifierForTesting
      // running synchronously. It passes a FailingProofVerifierCallback and
      // runs the original callback after asserting that the verification ran
      // synchronously.
      QuicAsyncStatus status = delegate_->VerifyCertChain(
          hostname_, certs_, ocsp_response_, cert_sct_, context_,
          error_details_, details_,
          QuicMakeUnique<FailingProofVerifierCallback>());
      ASSERT_NE(status, QUIC_PENDING);
      callback_->Run(status == QUIC_SUCCESS, *error_details_, details_);
    }

   private:
    std::string hostname_;
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

  bool handshake_confirmed() const override {
    return handshaker()->handshake_confirmed();
  }

  const QuicCryptoNegotiatedParameters& crypto_negotiated_params()
      const override {
    return handshaker()->crypto_negotiated_params();
  }

  CryptoMessageParser* crypto_message_parser() override {
    return handshaker()->crypto_message_parser();
  }

  void WriteCryptoData(EncryptionLevel level, QuicStringPiece data) override {
    pending_writes_.push_back(std::make_pair(std::string(data), level));
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
        CloseConnectionWithDetails(
            stream->crypto_message_parser()->error(),
            stream->crypto_message_parser()->error_detail());
        break;
      }
    }
    pending_writes_.clear();
  }

 private:
  std::vector<std::pair<std::string, EncryptionLevel>> pending_writes_;
};

class TestQuicCryptoClientStream : public TestQuicCryptoStream {
 public:
  explicit TestQuicCryptoClientStream(QuicSession* session)
      : TestQuicCryptoStream(session),
        proof_verifier_(new FakeProofVerifier),
        ssl_ctx_(TlsClientHandshaker::CreateSslCtx()),
        handshaker_(new TlsClientHandshaker(
            this,
            session,
            QuicServerId("test.example.com", 443, false),
            proof_verifier_.get(),
            ssl_ctx_.get(),
            crypto_test_utils::ProofVerifyContextForTesting(),
            "quic-tester")) {}

  ~TestQuicCryptoClientStream() override = default;

  TlsHandshaker* handshaker() const override { return handshaker_.get(); }

  bool CryptoConnect() { return handshaker_->CryptoConnect(); }

  FakeProofVerifier* GetFakeProofVerifier() const {
    return proof_verifier_.get();
  }

 private:
  std::unique_ptr<FakeProofVerifier> proof_verifier_;
  bssl::UniquePtr<SSL_CTX> ssl_ctx_;
  std::unique_ptr<TlsClientHandshaker> handshaker_;
};

class TestQuicCryptoServerStream : public TestQuicCryptoStream {
 public:
  TestQuicCryptoServerStream(QuicSession* session,
                             FakeProofSource* proof_source)
      : TestQuicCryptoStream(session),
        proof_source_(proof_source),
        ssl_ctx_(TlsServerHandshaker::CreateSslCtx()),
        handshaker_(new TlsServerHandshaker(this,
                                            session,
                                            ssl_ctx_.get(),
                                            proof_source_)) {}

  ~TestQuicCryptoServerStream() override = default;

  void CancelOutstandingCallbacks() {
    handshaker_->CancelOutstandingCallbacks();
  }

  TlsHandshaker* handshaker() const override { return handshaker_.get(); }

  FakeProofSource* GetFakeProofSource() const { return proof_source_; }

 private:
  FakeProofSource* proof_source_;
  bssl::UniquePtr<SSL_CTX> ssl_ctx_;
  std::unique_ptr<TlsServerHandshaker> handshaker_;
};

void ExchangeHandshakeMessages(TestQuicCryptoStream* client,
                               TestQuicCryptoStream* server) {
  while (!client->pending_writes().empty() ||
         !server->pending_writes().empty()) {
    client->SendCryptoMessagesToPeer(server);
    server->SendCryptoMessagesToPeer(client);
  }
}

ParsedQuicVersionVector AllTlsSupportedVersions() {
  SetQuicReloadableFlag(quic_enable_version_99, true);
  SetQuicFlag(FLAGS_quic_supports_tls_handshake, true);
  ParsedQuicVersionVector supported_versions;
  for (QuicTransportVersion version : kSupportedTransportVersions) {
    if (!QuicVersionUsesCryptoFrames(version)) {
      // The TLS handshake is only deployable if CRYPTO frames are also used.
      continue;
    }
    supported_versions.push_back(ParsedQuicVersion(PROTOCOL_TLS1_3, version));
  }
  return supported_versions;
}

class TlsHandshakerTest : public QuicTest {
 public:
  TlsHandshakerTest()
      : client_conn_(new MockQuicConnection(&conn_helper_,
                                            &alarm_factory_,
                                            Perspective::IS_CLIENT,
                                            AllTlsSupportedVersions())),
        server_conn_(new MockQuicConnection(&conn_helper_,
                                            &alarm_factory_,
                                            Perspective::IS_SERVER,
                                            AllTlsSupportedVersions())),
        client_session_(client_conn_, /*create_mock_crypto_stream=*/false),
        server_session_(server_conn_, /*create_mock_crypto_stream=*/false) {
    client_stream_ = new TestQuicCryptoClientStream(&client_session_);
    client_session_.SetCryptoStream(client_stream_);
    server_stream_ =
        new TestQuicCryptoServerStream(&server_session_, &proof_source_);
    server_session_.SetCryptoStream(server_stream_);
    client_session_.Initialize();
    server_session_.Initialize();
    EXPECT_FALSE(client_stream_->encryption_established());
    EXPECT_FALSE(client_stream_->handshake_confirmed());
    EXPECT_FALSE(server_stream_->encryption_established());
    EXPECT_FALSE(server_stream_->handshake_confirmed());
  }

  MockQuicConnectionHelper conn_helper_;
  MockAlarmFactory alarm_factory_;
  MockQuicConnection* client_conn_;
  MockQuicConnection* server_conn_;
  MockQuicSession client_session_;
  MockQuicSession server_session_;

  FakeProofSource proof_source_;
  TestQuicCryptoClientStream* client_stream_;
  TestQuicCryptoServerStream* server_stream_;
};

TEST_F(TlsHandshakerTest, CryptoHandshake) {
  EXPECT_CALL(*client_conn_, CloseConnection(_, _, _)).Times(0);
  EXPECT_CALL(*server_conn_, CloseConnection(_, _, _)).Times(0);
  client_stream_->CryptoConnect();
  ExchangeHandshakeMessages(client_stream_, server_stream_);

  EXPECT_TRUE(client_stream_->handshake_confirmed());
  EXPECT_TRUE(client_stream_->encryption_established());
  EXPECT_TRUE(server_stream_->handshake_confirmed());
  EXPECT_TRUE(server_stream_->encryption_established());
}

TEST_F(TlsHandshakerTest, HandshakeWithAsyncProofSource) {
  EXPECT_CALL(*client_conn_, CloseConnection(_, _, _)).Times(0);
  EXPECT_CALL(*server_conn_, CloseConnection(_, _, _)).Times(0);
  // Enable FakeProofSource to capture call to ComputeTlsSignature and run it
  // asynchronously.
  FakeProofSource* proof_source = server_stream_->GetFakeProofSource();
  proof_source->Activate();

  // Start handshake.
  client_stream_->CryptoConnect();
  ExchangeHandshakeMessages(client_stream_, server_stream_);

  ASSERT_EQ(proof_source->NumPendingCallbacks(), 1);
  proof_source->InvokePendingCallback(0);

  ExchangeHandshakeMessages(client_stream_, server_stream_);

  EXPECT_TRUE(client_stream_->handshake_confirmed());
  EXPECT_TRUE(client_stream_->encryption_established());
  EXPECT_TRUE(server_stream_->handshake_confirmed());
  EXPECT_TRUE(server_stream_->encryption_established());
}

TEST_F(TlsHandshakerTest, CancelPendingProofSource) {
  EXPECT_CALL(*client_conn_, CloseConnection(_, _, _)).Times(0);
  EXPECT_CALL(*server_conn_, CloseConnection(_, _, _)).Times(0);
  // Enable FakeProofSource to capture call to ComputeTlsSignature and run it
  // asynchronously.
  FakeProofSource* proof_source = server_stream_->GetFakeProofSource();
  proof_source->Activate();

  // Start handshake.
  client_stream_->CryptoConnect();
  ExchangeHandshakeMessages(client_stream_, server_stream_);

  ASSERT_EQ(proof_source->NumPendingCallbacks(), 1);
  server_stream_ = nullptr;

  proof_source->InvokePendingCallback(0);
}

TEST_F(TlsHandshakerTest, HandshakeWithAsyncProofVerifier) {
  EXPECT_CALL(*client_conn_, CloseConnection(_, _, _)).Times(0);
  EXPECT_CALL(*server_conn_, CloseConnection(_, _, _)).Times(0);
  // Enable FakeProofVerifier to capture call to VerifyCertChain and run it
  // asynchronously.
  FakeProofVerifier* proof_verifier = client_stream_->GetFakeProofVerifier();
  proof_verifier->Activate();

  // Start handshake.
  client_stream_->CryptoConnect();
  ExchangeHandshakeMessages(client_stream_, server_stream_);

  ASSERT_EQ(proof_verifier->NumPendingCallbacks(), 1u);
  proof_verifier->InvokePendingCallback(0);

  ExchangeHandshakeMessages(client_stream_, server_stream_);

  EXPECT_TRUE(client_stream_->handshake_confirmed());
  EXPECT_TRUE(client_stream_->encryption_established());
  EXPECT_TRUE(server_stream_->handshake_confirmed());
  EXPECT_TRUE(server_stream_->encryption_established());
}

TEST_F(TlsHandshakerTest, ClientConnectionClosedOnTlsError) {
  // Have client send ClientHello.
  client_stream_->CryptoConnect();
  EXPECT_CALL(*client_conn_, CloseConnection(QUIC_HANDSHAKE_FAILED, _, _));

  // Send a zero-length ServerHello from server to client.
  char bogus_handshake_message[] = {
      // Handshake struct (RFC 8446 appendix B.3)
      2,        // HandshakeType server_hello
      0, 0, 0,  // uint24 length
  };
  server_stream_->WriteCryptoData(
      ENCRYPTION_INITIAL,
      QuicStringPiece(bogus_handshake_message,
                      QUIC_ARRAYSIZE(bogus_handshake_message)));
  server_stream_->SendCryptoMessagesToPeer(client_stream_);

  EXPECT_FALSE(client_stream_->handshake_confirmed());
}

TEST_F(TlsHandshakerTest, ServerConnectionClosedOnTlsError) {
  EXPECT_CALL(*server_conn_, CloseConnection(QUIC_HANDSHAKE_FAILED, _, _));

  // Send a zero-length ClientHello from client to server.
  char bogus_handshake_message[] = {
      // Handshake struct (RFC 8446 appendix B.3)
      1,        // HandshakeType client_hello
      0, 0, 0,  // uint24 length
  };
  client_stream_->WriteCryptoData(
      ENCRYPTION_INITIAL,
      QuicStringPiece(bogus_handshake_message,
                      QUIC_ARRAYSIZE(bogus_handshake_message)));
  client_stream_->SendCryptoMessagesToPeer(server_stream_);

  EXPECT_FALSE(server_stream_->handshake_confirmed());
}

}  // namespace
}  // namespace test
}  // namespace quic
