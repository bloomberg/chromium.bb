// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/net/network_diagnostics/tls_prober.h"

#include <deque>
#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/chromeos/net/network_diagnostics/fake_host_resolver.h"
#include "chrome/browser/chromeos/net/network_diagnostics/fake_network_context.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "net/ssl/ssl_server_config.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/network/network_service.h"
#include "services/network/test/test_network_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace chromeos {
namespace network_diagnostics {
namespace {

using ProbeExitEnum = TlsProber::ProbeExitEnum;

}  // namespace

class TlsProberWithFakeNetworkContextTest : public ::testing::Test {
 public:
  TlsProberWithFakeNetworkContextTest() = default;
  TlsProberWithFakeNetworkContextTest(
      const TlsProberWithFakeNetworkContextTest&) = delete;
  TlsProberWithFakeNetworkContextTest& operator=(
      const TlsProberWithFakeNetworkContextTest&) = delete;

  void InitializeProberNetworkContext(
      std::deque<FakeHostResolver::DnsResult*> fake_dns_result,
      base::Optional<net::Error> tcp_connect_code,
      base::Optional<net::Error> tls_upgrade_code) {
    fake_network_context_ = std::make_unique<FakeNetworkContext>();
    fake_network_context_->set_fake_dns_results(std::move(fake_dns_result));
    fake_network_context_->SetTCPConnectCode(tcp_connect_code);
    fake_network_context_->SetTLSUpgradeCode(tls_upgrade_code);
  }

  void CreateAndExecuteTlsProber(const GURL& url,
                                 TlsProber::TlsProbeCompleteCallback callback) {
    ASSERT_TRUE(fake_network_context_);
    tls_prober_ = std::make_unique<TlsProber>(
        base::BindRepeating(
            [](network::mojom::NetworkContext* network_context) {
              return network_context;
            },
            static_cast<network::mojom::NetworkContext*>(
                fake_network_context_.get())),
        url, std::move(callback));
  }

  FakeNetworkContext* fake_network_context() {
    return fake_network_context_.get();
  }

 protected:
  const GURL kFakeUrl{"https://www.FAKE_HOST_NAME.com:1234/"};
  const net::IPEndPoint kFakeIPAddress{
      net::IPEndPoint(net::IPAddress::IPv4Localhost(), /*port=*/1234)};

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<FakeNetworkContext> fake_network_context_;
  std::unique_ptr<TlsProber> tls_prober_;
};

TEST_F(TlsProberWithFakeNetworkContextTest,
       SocketConnectedAndUpgradedSuccessfully) {
  auto dns_result = std::make_unique<FakeHostResolver::DnsResult>(
      net::OK, net::ResolveErrorInfo(net::OK),
      net::AddressList(kFakeIPAddress));
  std::deque<FakeHostResolver::DnsResult*> fake_dns_result = {dns_result.get()};
  net::Error tcp_connect_code = net::OK;
  net::Error tls_upgrade_code = net::OK;
  InitializeProberNetworkContext(std::move(fake_dns_result), tcp_connect_code,
                                 tls_upgrade_code);
  int probe_result = -1;
  ProbeExitEnum probe_exit_enum = ProbeExitEnum::kTcpConnectionFailure;
  base::RunLoop run_loop;
  CreateAndExecuteTlsProber(
      kFakeUrl, base::BindOnce(
                    [](int* probe_result, ProbeExitEnum* probe_exit_enum,
                       base::OnceClosure quit_closure, int result,
                       ProbeExitEnum exit_enum) {
                      *probe_result = result;
                      *probe_exit_enum = exit_enum;
                      std::move(quit_closure).Run();
                    },
                    &probe_result, &probe_exit_enum, run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_EQ(net::OK, probe_result);
  EXPECT_EQ(ProbeExitEnum::kSuccess, probe_exit_enum);
}

TEST_F(TlsProberWithFakeNetworkContextTest, FailedDnsLookup) {
  auto dns_result = std::make_unique<FakeHostResolver::DnsResult>(
      net::ERR_NAME_NOT_RESOLVED,
      net::ResolveErrorInfo(net::ERR_NAME_NOT_RESOLVED), net::AddressList());
  std::deque<FakeHostResolver::DnsResult*> fake_dns_result = {dns_result.get()};
  // Neither TCP connect nor TLS upgrade should not be called in this scenario.
  InitializeProberNetworkContext(std::move(fake_dns_result),
                                 /*tcp_connect_code=*/base::nullopt,
                                 /*tls_upgrade_code=*/base::nullopt);
  int probe_result = -1;
  ProbeExitEnum probe_exit_enum = ProbeExitEnum::kSuccess;
  base::RunLoop run_loop;
  CreateAndExecuteTlsProber(
      kFakeUrl, base::BindOnce(
                    [](int* probe_result, ProbeExitEnum* probe_exit_enum,
                       base::OnceClosure quit_closure, int result,
                       ProbeExitEnum exit_enum) {
                      *probe_result = result;
                      *probe_exit_enum = exit_enum;
                      std::move(quit_closure).Run();
                    },
                    &probe_result, &probe_exit_enum, run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_EQ(net::ERR_NAME_NOT_RESOLVED, probe_result);
  EXPECT_EQ(ProbeExitEnum::kDnsFailure, probe_exit_enum);
}

TEST_F(TlsProberWithFakeNetworkContextTest, MojoDisconnectDuringDnsLookup) {
  // Host resolution will not be successful due to Mojo disconnect.
  std::deque<FakeHostResolver::DnsResult*> fake_dns_result = {};
  // Neither TCP connect nor TLS upgrade should not be called in this scenario.
  InitializeProberNetworkContext(std::move(fake_dns_result),
                                 /*tcp_connect_code=*/base::nullopt,
                                 /*tls_upgrade_code=*/base::nullopt);
  fake_network_context()->set_disconnect_during_host_resolution(true);
  int probe_result = -1;
  ProbeExitEnum probe_exit_enum = ProbeExitEnum::kSuccess;
  base::RunLoop run_loop;
  CreateAndExecuteTlsProber(
      kFakeUrl, base::BindOnce(
                    [](int* probe_result, ProbeExitEnum* probe_exit_enum,
                       base::OnceClosure quit_closure, int result,
                       ProbeExitEnum exit_enum) {
                      *probe_result = result;
                      *probe_exit_enum = exit_enum;
                      std::move(quit_closure).Run();
                    },
                    &probe_result, &probe_exit_enum, run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_EQ(net::ERR_NAME_NOT_RESOLVED, probe_result);
  EXPECT_EQ(ProbeExitEnum::kDnsFailure, probe_exit_enum);
}

TEST_F(TlsProberWithFakeNetworkContextTest, FailedTcpConnection) {
  auto dns_result = std::make_unique<FakeHostResolver::DnsResult>(
      net::OK, net::ResolveErrorInfo(net::OK),
      net::AddressList(kFakeIPAddress));
  std::deque<FakeHostResolver::DnsResult*> fake_dns_result = {dns_result.get()};
  net::Error tcp_connect_code = net::ERR_CONNECTION_FAILED;
  // TLS upgrade should not be called in this scenario.
  InitializeProberNetworkContext(std::move(fake_dns_result), tcp_connect_code,
                                 /*tls_upgrade_code=*/base::nullopt);
  int probe_result = -1;
  ProbeExitEnum probe_exit_enum = ProbeExitEnum::kSuccess;
  base::RunLoop run_loop;
  CreateAndExecuteTlsProber(
      kFakeUrl, base::BindOnce(
                    [](int* probe_result, ProbeExitEnum* probe_exit_enum,
                       base::OnceClosure quit_closure, int result,
                       ProbeExitEnum exit_enum) {
                      *probe_result = result;
                      *probe_exit_enum = exit_enum;
                      std::move(quit_closure).Run();
                    },
                    &probe_result, &probe_exit_enum, run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_EQ(net::ERR_CONNECTION_FAILED, probe_result);
  EXPECT_EQ(ProbeExitEnum::kTcpConnectionFailure, probe_exit_enum);
}

TEST_F(TlsProberWithFakeNetworkContextTest, FailedTlsUpgrade) {
  auto dns_result = std::make_unique<FakeHostResolver::DnsResult>(
      net::OK, net::ResolveErrorInfo(net::OK),
      net::AddressList(kFakeIPAddress));
  std::deque<FakeHostResolver::DnsResult*> fake_dns_result = {dns_result.get()};
  net::Error tcp_connect_code = net::OK;
  net::Error tls_upgrade_code = net::ERR_SSL_PROTOCOL_ERROR;
  InitializeProberNetworkContext(std::move(fake_dns_result), tcp_connect_code,
                                 tls_upgrade_code);
  int probe_result = -1;
  ProbeExitEnum probe_exit_enum = ProbeExitEnum::kSuccess;
  base::RunLoop run_loop;
  CreateAndExecuteTlsProber(
      kFakeUrl, base::BindOnce(
                    [](int* probe_result, ProbeExitEnum* probe_exit_enum,
                       base::OnceClosure quit_closure, int result,
                       ProbeExitEnum exit_enum) {
                      *probe_result = result;
                      *probe_exit_enum = exit_enum;
                      std::move(quit_closure).Run();
                    },
                    &probe_result, &probe_exit_enum, run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_EQ(net::ERR_SSL_PROTOCOL_ERROR, probe_result);
  EXPECT_EQ(ProbeExitEnum::kTlsUpgradeFailure, probe_exit_enum);
}

TEST_F(TlsProberWithFakeNetworkContextTest,
       MojoDisconnectedDuringTcpConnectionAttempt) {
  auto dns_result = std::make_unique<FakeHostResolver::DnsResult>(
      net::OK, net::ResolveErrorInfo(net::OK),
      net::AddressList(kFakeIPAddress));
  std::deque<FakeHostResolver::DnsResult*> fake_dns_result = {dns_result.get()};
  // Since the TCP connection is disconnected, no connection codes are needed.
  InitializeProberNetworkContext(std::move(fake_dns_result),
                                 /*tcp_connect_code=*/base::nullopt,
                                 /*tls_upgrade_code=*/base::nullopt);
  fake_network_context()->set_disconnect_during_tcp_connection_attempt(true);
  int probe_result = -1;
  ProbeExitEnum probe_exit_enum = ProbeExitEnum::kSuccess;
  base::RunLoop run_loop;
  CreateAndExecuteTlsProber(
      kFakeUrl, base::BindOnce(
                    [](int* probe_result, ProbeExitEnum* probe_exit_enum,
                       base::OnceClosure quit_closure, int result,
                       ProbeExitEnum exit_enum) {
                      *probe_result = result;
                      *probe_exit_enum = exit_enum;
                      std::move(quit_closure).Run();
                    },
                    &probe_result, &probe_exit_enum, run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_EQ(net::ERR_FAILED, probe_result);
  EXPECT_EQ(ProbeExitEnum::kMojoDisconnectFailure, probe_exit_enum);
}

TEST_F(TlsProberWithFakeNetworkContextTest,
       MojoDisconnectedDuringTlsUpgradeAttempt) {
  auto dns_result = std::make_unique<FakeHostResolver::DnsResult>(
      net::OK, net::ResolveErrorInfo(net::OK),
      net::AddressList(kFakeIPAddress));
  std::deque<FakeHostResolver::DnsResult*> fake_dns_result = {dns_result.get()};
  net::Error tcp_connect_code = net::OK;
  // TLS upgrade attempt will fail due to disconnection. |tls_upgrade_code|
  // is only populated to correctly initialize the FakeNetworkContext instance.
  net::Error tls_upgrade_code = net::OK;
  InitializeProberNetworkContext(std::move(fake_dns_result), tcp_connect_code,
                                 tls_upgrade_code);
  fake_network_context()->set_disconnect_during_tls_upgrade_attempt(true);
  int probe_result = -1;
  ProbeExitEnum probe_exit_enum = ProbeExitEnum::kSuccess;
  base::RunLoop run_loop;
  CreateAndExecuteTlsProber(
      kFakeUrl, base::BindOnce(
                    [](int* probe_result, ProbeExitEnum* probe_exit_enum,
                       base::OnceClosure quit_closure, int result,
                       ProbeExitEnum exit_enum) {
                      *probe_result = result;
                      *probe_exit_enum = exit_enum;
                      std::move(quit_closure).Run();
                    },
                    &probe_result, &probe_exit_enum, run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_EQ(net::ERR_FAILED, probe_result);
  EXPECT_EQ(ProbeExitEnum::kMojoDisconnectFailure, probe_exit_enum);
}

class TlsProberWithRealNetworkContextTest : public ::testing::Test {
 public:
  TlsProberWithRealNetworkContextTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP) {}

  TlsProberWithRealNetworkContextTest(
      const TlsProberWithRealNetworkContextTest&) = delete;
  TlsProberWithRealNetworkContextTest& operator=(
      const TlsProberWithRealNetworkContextTest&) = delete;

  void SetUp() override {
    service_ = network::NetworkService::CreateForTesting();
    service_->Bind(network_service_.BindNewPipeAndPassReceiver());
    auto context_params = network::mojom::NetworkContextParams::New();
    context_params->cert_verifier_params = content::GetCertVerifierParams(
        network::mojom::CertVerifierCreationParams::New());
    network_service_->CreateNetworkContext(
        network_context_.BindNewPipeAndPassReceiver(),
        std::move(context_params));
    test_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    net::SSLServerConfig ssl_config;
    ssl_config.client_cert_type =
        net::SSLServerConfig::ClientCertType::NO_CLIENT_CERT;
    test_server_->SetSSLConfig(net::EmbeddedTestServer::CERT_OK, ssl_config);
    test_server_->RegisterRequestHandler(base::BindRepeating(
        &TlsProberWithRealNetworkContextTest::ReturnResponse));
    ASSERT_TRUE((test_server_handle_ = test_server_->StartAndReturnHandle()));
  }

  void CreateAndExecuteTlsProber(const GURL& url,
                                 TlsProber::TlsProbeCompleteCallback callback) {
    tls_prober_ = std::make_unique<TlsProber>(
        base::BindRepeating(
            [](network::mojom::NetworkContext* network_context) {
              return network_context;
            },
            network_context_.get()),
        url, std::move(callback));
  }

  static std::unique_ptr<net::test_server::HttpResponse> ReturnResponse(
      const net::test_server::HttpRequest& request) {
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_code(net::HTTP_OK);
    response->set_content("");
    response->set_content_type("text/plain");
    return response;
  }

  // Returns the URL containing hostname (127.0.0.1) and a random port used by
  // the test server.
  const GURL& url() { return test_server_->base_url(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<network::NetworkService> service_;
  mojo::Remote<network::mojom::NetworkService> network_service_;
  mojo::Remote<network::mojom::NetworkContext> network_context_;
  std::unique_ptr<net::EmbeddedTestServer> test_server_;
  net::test_server::EmbeddedTestServerHandle test_server_handle_;
  std::unique_ptr<TlsProber> tls_prober_;
};

TEST_F(TlsProberWithRealNetworkContextTest,
       TestSuccessfulProbeUsingRealNetworkContext) {
  int probe_result = -1;
  ProbeExitEnum probe_exit_enum = ProbeExitEnum::kTcpConnectionFailure;
  base::RunLoop run_loop;
  CreateAndExecuteTlsProber(
      url(), base::BindOnce(
                 [](int* probe_result, ProbeExitEnum* probe_exit_enum,
                    base::OnceClosure quit_closure, int result,
                    ProbeExitEnum exit_enum) {
                   *probe_result = result;
                   *probe_exit_enum = exit_enum;
                   std::move(quit_closure).Run();
                 },
                 &probe_result, &probe_exit_enum, run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_EQ(net::OK, probe_result);
  EXPECT_EQ(ProbeExitEnum::kSuccess, probe_exit_enum);
}

}  // namespace network_diagnostics
}  // namespace chromeos
