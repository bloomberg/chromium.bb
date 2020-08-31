// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_transport_client.h"

#include <memory>

#include "base/threading/thread_task_runner_handle.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/dns/mock_host_resolver.h"
#include "net/proxy_resolution/configured_proxy_resolution_service.h"
#include "net/test/test_data_directory.h"
#include "net/test/test_with_task_environment.h"
#include "net/third_party/quiche/src/quic/test_tools/crypto_test_utils.h"
#include "net/tools/quic/quic_transport_simple_server.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace test {
namespace {

class MockVisitor : public QuicTransportClient::Visitor {
 public:
  MOCK_METHOD0(OnConnected, void());
  MOCK_METHOD0(OnConnectionFailed, void());
  MOCK_METHOD0(OnClosed, void());
  MOCK_METHOD0(OnError, void());

  MOCK_METHOD0(OnIncomingBidirectionalStreamAvailable, void());
  MOCK_METHOD0(OnIncomingUnidirectionalStreamAvailable, void());
  MOCK_METHOD1(OnDatagramReceived, void(base::StringPiece));
  MOCK_METHOD0(OnCanCreateNewOutgoingBidirectionalStream, void());
  MOCK_METHOD0(OnCanCreateNewOutgoingUnidirectionalStream, void());
};

class QuicTransportEndToEndTest : public TestWithTaskEnvironment {
 public:
  QuicTransportEndToEndTest() {
    quic::QuicEnableVersion(QuicTransportClient::kQuicVersionForOriginTrial);
    origin_ = url::Origin::Create(GURL{"https://example.org"});
    isolation_key_ = NetworkIsolationKey(origin_, origin_);

    URLRequestContextBuilder builder;
    builder.set_proxy_resolution_service(
        ConfiguredProxyResolutionService::CreateDirect());

    auto cert_verifier = std::make_unique<MockCertVerifier>();
    cert_verifier->set_default_result(OK);
    builder.SetCertVerifier(std::move(cert_verifier));

    auto host_resolver = std::make_unique<MockHostResolver>();
    host_resolver->rules()->AddRule("test.example.com", "127.0.0.1");
    builder.set_host_resolver(std::move(host_resolver));

    auto quic_context = std::make_unique<QuicContext>();
    quic_context->params()->supported_versions.clear();
    // This is required to bypass the check that only allows known certificate
    // roots in QUIC.
    quic_context->params()->origins_to_force_quic_on.insert(
        HostPortPair("test.example.com", 0));
    builder.set_quic_context(std::move(quic_context));

    context_ = builder.Build();

    // By default, quit on error instead of waiting for RunLoop() to time out.
    ON_CALL(visitor_, OnConnectionFailed()).WillByDefault([this]() {
      LOG(INFO) << "Connection failed: " << client_->error();
      run_loop_->Quit();
    });
    ON_CALL(visitor_, OnError()).WillByDefault([this]() {
      LOG(INFO) << "Connection error: " << client_->error();
      run_loop_->Quit();
    });

    StartServer();
  }

  GURL GetURL(const std::string& suffix) {
    return GURL{quiche::QuicheStrCat(
        "quic-transport://test.example.com:", port_, suffix)};
  }

  void StartServer() {
    server_ = std::make_unique<QuicTransportSimpleServer>(
        /* port */ 0, std::vector<url::Origin>({origin_}),
        quic::test::crypto_test_utils::ProofSourceForTesting());
    ASSERT_EQ(EXIT_SUCCESS, server_->Start());
    port_ = server_->server_address().port();
  }

  void Run() {
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

  auto StopRunning() {
    return [this]() { run_loop_->Quit(); };
  }

 protected:
  QuicFlagSaver flags_;  // Save/restore all QUIC flag values.
  std::unique_ptr<URLRequestContext> context_;
  std::unique_ptr<QuicTransportClient> client_;
  MockVisitor visitor_;
  std::unique_ptr<QuicTransportSimpleServer> server_;
  std::unique_ptr<base::RunLoop> run_loop_;

  int port_ = 0;
  url::Origin origin_;
  NetworkIsolationKey isolation_key_;
};

TEST_F(QuicTransportEndToEndTest, Connect) {
  client_ = std::make_unique<QuicTransportClient>(
      GetURL("/discard"), origin_, &visitor_, isolation_key_, context_.get());
  client_->Connect();
  EXPECT_CALL(visitor_, OnConnected()).WillOnce(StopRunning());
  Run();
  ASSERT_TRUE(client_->session() != nullptr);
  EXPECT_TRUE(client_->session()->IsSessionReady());
}

TEST_F(QuicTransportEndToEndTest, EchoUnidirectionalStream) {
  client_ = std::make_unique<QuicTransportClient>(
      GetURL("/echo"), origin_, &visitor_, isolation_key_, context_.get());
  client_->Connect();
  EXPECT_CALL(visitor_, OnConnected()).WillOnce(StopRunning());
  Run();

  quic::QuicTransportClientSession* session = client_->session();
  ASSERT_TRUE(session != nullptr);
  ASSERT_TRUE(session->CanOpenNextOutgoingUnidirectionalStream());
  quic::QuicTransportStream* stream_out =
      session->OpenOutgoingUnidirectionalStream();
  EXPECT_TRUE(stream_out->Write("test"));
  EXPECT_TRUE(stream_out->SendFin());

  EXPECT_CALL(visitor_, OnIncomingUnidirectionalStreamAvailable())
      .WillOnce(StopRunning());
  Run();

  quic::QuicTransportStream* stream_in =
      session->AcceptIncomingUnidirectionalStream();
  ASSERT_TRUE(stream_in != nullptr);
  std::string data;
  stream_in->Read(&data);
  EXPECT_EQ("test", data);
}

}  // namespace
}  // namespace test
}  // namespace net
