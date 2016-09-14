// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "net/base/load_timing_info.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/dns/mapped_host_resolver.h"
#include "net/dns/mock_host_resolver.h"
#include "net/quic/chromium/crypto/proof_source_chromium.h"
#include "net/quic/test_tools/crypto_test_utils.h"
#include "net/test/cert_test_util.h"
#include "net/test/gtest_util.h"
#include "net/test/test_data_directory.h"
#include "net/tools/quic/quic_in_memory_cache.h"
#include "net/tools/quic/quic_simple_server.h"
#include "net/tools/quic/test_tools/quic_in_memory_cache_peer.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace net {

namespace {

// This must match the certificate used (quic_test.example.com.crt and
// quic_test.example.com.key.pkcs8).
const int kTestServerPort = 6121;
const char kTestServerHost[] = "test.example.com:6121";
// Used as a simple response from the server.
const char kHelloPath[] = "/hello.txt";
const char kHelloBodyValue[] = "Hello from QUIC Server";
const int kHelloStatus = 200;

class URLRequestQuicTest : public ::testing::Test {
 protected:
  URLRequestQuicTest() : context_(new TestURLRequestContext(true)) {
    StartQuicServer();

    std::unique_ptr<HttpNetworkSession::Params> params(
        new HttpNetworkSession::Params);
    CertVerifyResult verify_result;
    verify_result.verified_cert = ImportCertFromFile(
        GetTestCertsDirectory(), "quic_test.example.com.crt");
    cert_verifier_.AddResultForCertAndHost(verify_result.verified_cert.get(),
                                           "test.example.com", verify_result,
                                           OK);
    verify_result.verified_cert = ImportCertFromFile(
        GetTestCertsDirectory(), "quic_test_ecc.example.com.crt");
    cert_verifier_.AddResultForCertAndHost(verify_result.verified_cert.get(),
                                           "test.example.com", verify_result,
                                           OK);
    // To simplify the test, and avoid the race with the HTTP request, we force
    // QUIC for these requests.
    params->origins_to_force_quic_on.insert(
        HostPortPair::FromString(kTestServerHost));
    params->cert_verifier = &cert_verifier_;
    params->enable_quic = true;
    params->host_resolver = host_resolver_.get();
    context_->set_http_network_session_params(std::move(params));
    context_->set_cert_verifier(&cert_verifier_);
    context_->Init();
  }

  void TearDown() override {
    if (server_)
      server_->Shutdown();
  }

  std::unique_ptr<TestURLRequestContext> context_;

 private:
  void StartQuicServer() {
    // Set up in-memory cache.
    test::QuicInMemoryCachePeer::ResetForTests();
    QuicInMemoryCache::GetInstance()->AddSimpleResponse(
        kTestServerHost, kHelloPath, kHelloStatus, kHelloBodyValue);
    net::QuicConfig config;
    // Set up server certs.
    std::unique_ptr<net::ProofSourceChromium> proof_source(
        new net::ProofSourceChromium());
    base::FilePath directory = GetTestCertsDirectory();
    CHECK(proof_source->Initialize(
        directory.Append(FILE_PATH_LITERAL("quic_test.example.com.crt")),
        directory.Append(FILE_PATH_LITERAL("quic_test.example.com.key.pkcs8")),
        directory.Append(FILE_PATH_LITERAL("quic_test.example.com.key.sct"))));
    server_.reset(new QuicSimpleServer(
        test::CryptoTestUtils::ProofSourceForTesting(), config,
        net::QuicCryptoServerConfig::ConfigOptions(), AllSupportedVersions()));
    int rv = server_->Listen(
        net::IPEndPoint(net::IPAddress::IPv4AllZeros(), kTestServerPort));
    EXPECT_GE(rv, 0) << "Quic server fails to start";

    std::unique_ptr<MockHostResolver> resolver(new MockHostResolver());
    resolver->rules()->AddRule("test.example.com", "127.0.0.1");
    host_resolver_.reset(new MappedHostResolver(std::move(resolver)));
    // Use a mapped host resolver so that request for test.example.com (port 80)
    // reach the server running on localhost.
    std::string map_rule = "MAP test.example.com test.example.com:" +
                           base::IntToString(server_->server_address().port());
    EXPECT_TRUE(host_resolver_->AddRuleFromString(map_rule));
  }

  std::unique_ptr<MappedHostResolver> host_resolver_;
  std::unique_ptr<QuicSimpleServer> server_;
  MockCertVerifier cert_verifier_;
};

// A URLRequest::Delegate that checks LoadTimingInfo when response headers are
// received.
class CheckLoadTimingDelegate : public TestDelegate {
 public:
  CheckLoadTimingDelegate(bool session_reused)
      : session_reused_(session_reused) {}
  void OnResponseStarted(URLRequest* request, int error) override {
    TestDelegate::OnResponseStarted(request, error);
    LoadTimingInfo load_timing_info;
    request->GetLoadTimingInfo(&load_timing_info);
    assertLoadTimingValid(load_timing_info, session_reused_);
  }

 private:
  void assertLoadTimingValid(const LoadTimingInfo& load_timing_info,
                             bool session_reused) {
    EXPECT_EQ(session_reused, load_timing_info.socket_reused);

    // If |session_reused| is true, these fields should all be null, non-null
    // otherwise.
    EXPECT_EQ(session_reused,
              load_timing_info.connect_timing.connect_start.is_null());
    EXPECT_EQ(session_reused,
              load_timing_info.connect_timing.connect_end.is_null());
    EXPECT_EQ(session_reused,
              load_timing_info.connect_timing.ssl_start.is_null());
    EXPECT_EQ(session_reused,
              load_timing_info.connect_timing.ssl_end.is_null());
    EXPECT_EQ(load_timing_info.connect_timing.connect_start,
              load_timing_info.connect_timing.ssl_start);
    EXPECT_EQ(load_timing_info.connect_timing.connect_end,
              load_timing_info.connect_timing.ssl_end);
    EXPECT_EQ(session_reused,
              load_timing_info.connect_timing.dns_start.is_null());
    EXPECT_EQ(session_reused,
              load_timing_info.connect_timing.dns_end.is_null());
  }

  bool session_reused_;

  DISALLOW_COPY_AND_ASSIGN(CheckLoadTimingDelegate);
};

}  // namespace

TEST_F(URLRequestQuicTest, TestGetRequest) {
  CheckLoadTimingDelegate delegate(false);
  std::string url =
      base::StringPrintf("https://%s%s", kTestServerHost, kHelloPath);
  std::unique_ptr<URLRequest> request =
      context_->CreateRequest(GURL(url), DEFAULT_PRIORITY, &delegate);

  request->Start();
  ASSERT_TRUE(request->is_pending());
  base::RunLoop().Run();

  EXPECT_TRUE(request->status().is_success());
  EXPECT_EQ(kHelloBodyValue, delegate.data_received());
}

// Tests that if two requests use the same QUIC session, the second request
// should not have |LoadTimingInfo::connect_timing|.
TEST_F(URLRequestQuicTest, TestTwoRequests) {
  CheckLoadTimingDelegate delegate(false);
  std::string url =
      base::StringPrintf("https://%s%s", kTestServerHost, kHelloPath);
  std::unique_ptr<URLRequest> request =
      context_->CreateRequest(GURL(url), DEFAULT_PRIORITY, &delegate);

  CheckLoadTimingDelegate delegate2(true);
  std::unique_ptr<URLRequest> request2 =
      context_->CreateRequest(GURL(url), DEFAULT_PRIORITY, &delegate2);
  request->Start();
  request2->Start();
  ASSERT_TRUE(request->is_pending());
  ASSERT_TRUE(request2->is_pending());
  base::RunLoop().Run();

  EXPECT_TRUE(request->status().is_success());
  EXPECT_TRUE(request2->status().is_success());
  EXPECT_EQ(kHelloBodyValue, delegate.data_received());
  EXPECT_EQ(kHelloBodyValue, delegate2.data_received());
}

}  // namespace net
