// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/proxy_resolving_client_socket.h"

#include <string>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "net/base/test_completion_callback.h"
#include "net/dns/mock_host_resolver.h"
#include "net/proxy_resolution/mock_proxy_resolver.h"
#include "net/proxy_resolution/proxy_config_service_fixed.h"
#include "net/proxy_resolution/proxy_service.h"
#include "net/socket/client_socket_pool_manager.h"
#include "net/socket/socket_test_util.h"
#include "net/test/gtest_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/proxy_resolving_client_socket_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

namespace {

class TestURLRequestContextWithProxy : public net::TestURLRequestContext {
 public:
  explicit TestURLRequestContextWithProxy(const std::string& pac_result)
      : TestURLRequestContext(true) {
    context_storage_.set_proxy_resolution_service(
        net::ProxyResolutionService::CreateFixedFromPacResult(pac_result));
    // net::MockHostResolver maps all hosts to localhost.
    auto host_resolver = std::make_unique<net::MockHostResolver>();
    context_storage_.set_host_resolver(std::move(host_resolver));
    Init();
  }
  ~TestURLRequestContextWithProxy() override {}
};

}  // namespace

class ProxyResolvingClientSocketTest : public testing::Test {
 protected:
  ProxyResolvingClientSocketTest()
      : context_with_proxy_("PROXY bad:99; PROXY maybe:80; DIRECT") {}

  ~ProxyResolvingClientSocketTest() override {}

  void TearDown() override {
    // Clear out any messages posted by ProxyResolvingClientSocket's
    // destructor.
    base::RunLoop().RunUntilIdle();
  }

  base::test::ScopedTaskEnvironment scoped_task_environment_;
  TestURLRequestContextWithProxy context_with_proxy_;
};

// Tests that the global socket pool limit
// (ClientSocketPoolManager::max_sockets_per_group) doesn't apply to this
// type of sockets.
TEST_F(ProxyResolvingClientSocketTest, SocketLimitNotApply) {
  const int kNumSockets = net::ClientSocketPoolManager::max_sockets_per_group(
                              net::HttpNetworkSession::NORMAL_SOCKET_POOL) +
                          10;
  const GURL kDestination("https://example.com:443");
  net::MockClientSocketFactory socket_factory;
  net::MockWrite writes[] = {
      net::MockWrite("CONNECT example.com:443 HTTP/1.1\r\n"
                     "Host: example.com:443\r\n"
                     "Proxy-Connection: keep-alive\r\n\r\n")};
  net::MockRead reads[] = {net::MockRead("HTTP/1.1 200 Success\r\n\r\n")};
  std::vector<std::unique_ptr<net::StaticSocketDataProvider>> socket_data;
  for (int i = 0; i < kNumSockets; ++i) {
    socket_data.push_back(std::make_unique<net::StaticSocketDataProvider>(
        reads, arraysize(reads), writes, arraysize(writes)));
    socket_data[i]->set_connect_data(net::MockConnect(net::ASYNC, net::OK));
    socket_factory.AddSocketDataProvider(socket_data[i].get());
  }

  ProxyResolvingClientSocketFactory proxy_resolving_socket_factory(
      &socket_factory, &context_with_proxy_);
  std::vector<std::unique_ptr<ProxyResolvingClientSocket>> sockets;
  for (int i = 0; i < kNumSockets; ++i) {
    std::unique_ptr<ProxyResolvingClientSocket> socket =
        proxy_resolving_socket_factory.CreateSocket(net::SSLConfig(),
                                                    kDestination);
    net::TestCompletionCallback callback;
    int status = socket->Connect(callback.callback());
    EXPECT_THAT(callback.GetResult(status), net::test::IsOk());
    sockets.push_back(std::move(socket));
  }
  for (int i = 0; i < kNumSockets; ++i) {
    EXPECT_TRUE(socket_data[i]->AllReadDataConsumed());
    EXPECT_TRUE(socket_data[i]->AllWriteDataConsumed());
  }
}

TEST_F(ProxyResolvingClientSocketTest, ConnectError) {
  const struct TestData {
    // Whether the error is encountered synchronously as opposed to
    // asynchronously.
    bool is_error_sync;
    // Whether it is using a direct connection as opposed to a proxy connection.
    bool is_direct;
  } kTestCases[] = {
      {true, true}, {true, false}, {false, true}, {false, false},
  };
  const GURL kDestination("https://example.com:443");
  for (auto test : kTestCases) {
    std::unique_ptr<net::URLRequestContext> context;
    if (test.is_direct) {
      context = std::make_unique<TestURLRequestContextWithProxy>("DIRECT");
    } else {
      context = std::make_unique<TestURLRequestContextWithProxy>(
          "PROXY myproxy.com:89");
    }
    net::MockClientSocketFactory socket_factory;
    net::StaticSocketDataProvider socket_data;
    socket_data.set_connect_data(net::MockConnect(
        test.is_error_sync ? net::SYNCHRONOUS : net::ASYNC, net::ERR_FAILED));
    socket_factory.AddSocketDataProvider(&socket_data);

    ProxyResolvingClientSocketFactory proxy_resolving_socket_factory(
        &socket_factory, context.get());
    std::unique_ptr<ProxyResolvingClientSocket> socket =
        proxy_resolving_socket_factory.CreateSocket(net::SSLConfig(),
                                                    kDestination);
    net::TestCompletionCallback callback;
    int status = socket->Connect(callback.callback());
    EXPECT_EQ(net::ERR_IO_PENDING, status);
    status = callback.WaitForResult();
    if (test.is_direct) {
      EXPECT_EQ(net::ERR_FAILED, status);
    } else {
      EXPECT_EQ(net::ERR_PROXY_CONNECTION_FAILED, status);
    }
    EXPECT_TRUE(socket_data.AllReadDataConsumed());
    EXPECT_TRUE(socket_data.AllWriteDataConsumed());
  }
}

// Tests that the connection is established to the proxy.
TEST_F(ProxyResolvingClientSocketTest, ConnectToProxy) {
  const GURL kDestination("https://example.com:443");
  // Use a different port than that of |kDestination|.
  const int kProxyPort = 8009;
  const int kDirectPort = 443;
  for (bool is_direct : {true, false}) {
    net::MockClientSocketFactory socket_factory;
    std::unique_ptr<net::URLRequestContext> context;
    if (is_direct) {
      context = std::make_unique<TestURLRequestContextWithProxy>("DIRECT");
    } else {
      context = std::make_unique<TestURLRequestContextWithProxy>(
          base::StringPrintf("PROXY myproxy.com:%d", kProxyPort));
    }
    net::MockRead reads[] = {net::MockRead("HTTP/1.1 200 Success\r\n\r\n")};
    net::MockWrite writes[] = {
        net::MockWrite("CONNECT example.com:443 HTTP/1.1\r\n"
                       "Host: example.com:443\r\n"
                       "Proxy-Connection: keep-alive\r\n\r\n")};
    net::StaticSocketDataProvider socket_data(reads, arraysize(reads), writes,
                                              arraysize(writes));
    net::IPEndPoint remote_addr(net::IPAddress(127, 0, 0, 1),
                                is_direct ? kDirectPort : kProxyPort);
    socket_data.set_connect_data(
        net::MockConnect(net::ASYNC, net::OK, remote_addr));
    socket_factory.AddSocketDataProvider(&socket_data);

    ProxyResolvingClientSocketFactory proxy_resolving_socket_factory(
        &socket_factory, context.get());
    std::unique_ptr<ProxyResolvingClientSocket> socket =
        proxy_resolving_socket_factory.CreateSocket(net::SSLConfig(),
                                                    kDestination);
    net::TestCompletionCallback callback;
    int status = socket->Connect(callback.callback());
    EXPECT_EQ(net::ERR_IO_PENDING, status);
    status = callback.WaitForResult();
    EXPECT_EQ(net::OK, status);
    net::IPEndPoint actual_remote_addr;
    status = socket->GetPeerAddress(&actual_remote_addr);
    if (!is_direct) {
      // ProxyResolvingClientSocket::GetPeerAddress() hides the ip of the
      // proxy, so call private member to make sure address is correct.
      EXPECT_EQ(net::ERR_NAME_NOT_RESOLVED, status);
      status =
          socket->transport_->socket()->GetPeerAddress(&actual_remote_addr);
    }
    EXPECT_EQ(net::OK, status);
    EXPECT_EQ(remote_addr.ToString(), actual_remote_addr.ToString());
  }
}

// Tests that connection itself is successful but an error occurred during
// Read()/Write().
TEST_F(ProxyResolvingClientSocketTest, ReadWriteErrors) {
  const GURL kDestination("http://example.com:80");
  const struct TestData {
    // Whether there is a read error as opposed to a write error.
    bool is_read_error;
    // Whether the error is encountered synchronously as opposed to
    // asynchronously.
    bool is_error_sync;
    // Whether it is using a direct connection as opposed to a proxy connection.
    bool is_direct;
  } kTestCases[] = {
      {true, true, true},   {true, true, false},   {false, true, true},
      {false, true, false}, {true, false, true},   {true, false, false},
      {false, false, true}, {false, false, false},
  };
  // Use a different port than that of |kDestination|.
  const int kProxyPort = 8009;
  const int kDirectPort = 80;
  for (auto test : kTestCases) {
    std::unique_ptr<net::URLRequestContext> context;
    if (test.is_direct) {
      context = std::make_unique<TestURLRequestContextWithProxy>("DIRECT");
    } else {
      context = std::make_unique<TestURLRequestContextWithProxy>(
          base::StringPrintf("PROXY myproxy.com:%d", kProxyPort));
    }
    std::vector<net::MockWrite> writes;
    std::vector<net::MockRead> reads;
    if (!test.is_direct) {
      writes.push_back(
          net::MockWrite("CONNECT example.com:80 HTTP/1.1\r\n"
                         "Host: example.com:80\r\n"
                         "Proxy-Connection: keep-alive\r\n\r\n"));
      reads.push_back(net::MockRead("HTTP/1.1 200 Success\r\n\r\n"));
    }
    if (test.is_read_error) {
      reads.emplace_back(test.is_error_sync ? net::SYNCHRONOUS : net::ASYNC,
                         net::ERR_FAILED);
    } else {
      writes.emplace_back(test.is_error_sync ? net::SYNCHRONOUS : net::ASYNC,
                          net::ERR_FAILED);
    }
    net::StaticSocketDataProvider socket_data(reads.data(), reads.size(),
                                              writes.data(), writes.size());
    net::IPEndPoint remote_addr(net::IPAddress(127, 0, 0, 1),
                                test.is_direct ? kDirectPort : kProxyPort);
    socket_data.set_connect_data(
        net::MockConnect(net::ASYNC, net::OK, remote_addr));
    net::MockClientSocketFactory socket_factory;
    socket_factory.AddSocketDataProvider(&socket_data);
    ProxyResolvingClientSocketFactory proxy_resolving_socket_factory(
        &socket_factory, context.get());
    std::unique_ptr<ProxyResolvingClientSocket> socket =
        proxy_resolving_socket_factory.CreateSocket(net::SSLConfig(),
                                                    kDestination);
    net::TestCompletionCallback callback;
    int status = socket->Connect(callback.callback());
    EXPECT_EQ(net::ERR_IO_PENDING, status);
    status = callback.WaitForResult();
    EXPECT_EQ(net::OK, status);
    net::IPEndPoint actual_remote_addr;
    status = socket->GetPeerAddress(&actual_remote_addr);
    if (!test.is_direct) {
      // ProxyResolvingClientSocket::GetPeerAddress() hides the ip of the
      // proxy, so call private member to make sure address is correct.
      EXPECT_EQ(net::ERR_NAME_NOT_RESOLVED, status);
      status =
          socket->transport_->socket()->GetPeerAddress(&actual_remote_addr);
    }
    EXPECT_EQ(net::OK, status);
    EXPECT_EQ(remote_addr.ToString(), actual_remote_addr.ToString());

    net::TestCompletionCallback read_write_callback;
    int read_write_result;
    std::string test_data_string("test data");
    scoped_refptr<net::IOBuffer> read_buffer(new net::IOBufferWithSize(10));
    scoped_refptr<net::IOBuffer> write_buffer(
        new net::StringIOBuffer(test_data_string));
    if (test.is_read_error) {
      read_write_result =
          socket->Read(read_buffer.get(), 10, read_write_callback.callback());
    } else {
      read_write_result = socket->Write(
          write_buffer.get(), test_data_string.size(),
          read_write_callback.callback(), TRAFFIC_ANNOTATION_FOR_TESTS);
    }
    if (read_write_result == net::ERR_IO_PENDING) {
      EXPECT_TRUE(!test.is_error_sync);
      read_write_result = read_write_callback.WaitForResult();
    }
    EXPECT_EQ(net::ERR_FAILED, read_write_result);
    EXPECT_TRUE(socket_data.AllReadDataConsumed());
    EXPECT_TRUE(socket_data.AllWriteDataConsumed());
  }
}

TEST_F(ProxyResolvingClientSocketTest, ReportsBadProxies) {
  const GURL kDestination("https://example.com:443");
  net::MockClientSocketFactory socket_factory;

  net::StaticSocketDataProvider socket_data1;
  socket_data1.set_connect_data(
      net::MockConnect(net::ASYNC, net::ERR_ADDRESS_UNREACHABLE));
  socket_factory.AddSocketDataProvider(&socket_data1);

  net::MockRead reads[] = {net::MockRead("HTTP/1.1 200 Success\r\n\r\n")};
  net::MockWrite writes[] = {
      net::MockWrite("CONNECT example.com:443 HTTP/1.1\r\n"
                     "Host: example.com:443\r\n"
                     "Proxy-Connection: keep-alive\r\n\r\n")};
  net::StaticSocketDataProvider socket_data2(reads, arraysize(reads), writes,
                                             arraysize(writes));
  socket_data2.set_connect_data(net::MockConnect(net::ASYNC, net::OK));
  socket_factory.AddSocketDataProvider(&socket_data2);

  ProxyResolvingClientSocketFactory proxy_resolving_socket_factory(
      &socket_factory, &context_with_proxy_);
  std::unique_ptr<ProxyResolvingClientSocket> socket =
      proxy_resolving_socket_factory.CreateSocket(net::SSLConfig(),
                                                  kDestination);
  net::TestCompletionCallback callback;
  int status = socket->Connect(callback.callback());
  EXPECT_EQ(net::ERR_IO_PENDING, status);
  status = callback.WaitForResult();
  EXPECT_EQ(net::OK, status);

  const net::ProxyRetryInfoMap& retry_info =
      context_with_proxy_.proxy_resolution_service()->proxy_retry_info();

  EXPECT_EQ(1u, retry_info.size());
  net::ProxyRetryInfoMap::const_iterator iter = retry_info.find("bad:99");
  EXPECT_TRUE(iter != retry_info.end());
}

TEST_F(ProxyResolvingClientSocketTest, ReusesHTTPAuthCache_Lookup) {
  net::MockClientSocketFactory socket_factory;
  const GURL kDestination("https://example.com:443");

  // Initial connect without credentials. The server responds with a 407.
  net::MockWrite kConnectWrites1[] = {
      net::MockWrite("CONNECT example.com:443 HTTP/1.1\r\n"
                     "Host: example.com:443\r\n"
                     "Proxy-Connection: keep-alive\r\n"
                     "\r\n")};
  net::MockRead kConnectReads1[] = {
      net::MockRead("HTTP/1.1 407 Proxy Authentication Required\r\n"
                    "Proxy-Authenticate: Basic realm=\"test_realm\"\r\n"
                    "\r\n")};

  // Second connect attempt includes credentials.
  net::MockWrite kConnectWrites2[] = {
      net::MockWrite("CONNECT example.com:443 HTTP/1.1\r\n"
                     "Host: example.com:443\r\n"
                     "Proxy-Connection: keep-alive\r\n"
                     "Proxy-Authorization: Basic dXNlcjpwYXNzd29yZA==\r\n"
                     "\r\n")};
  net::MockRead kConnectReads2[] = {
      net::MockRead("HTTP/1.1 200 Success\r\n\r\n")};

  net::StaticSocketDataProvider kSocketData1(
      kConnectReads1, arraysize(kConnectReads1), kConnectWrites1,
      arraysize(kConnectWrites1));
  socket_factory.AddSocketDataProvider(&kSocketData1);

  net::StaticSocketDataProvider kSocketData2(
      kConnectReads2, arraysize(kConnectReads2), kConnectWrites2,
      arraysize(kConnectWrites2));
  socket_factory.AddSocketDataProvider(&kSocketData2);

  net::HttpAuthCache* auth_cache =
      context_with_proxy_.http_transaction_factory()
          ->GetSession()
          ->http_auth_cache();

  // We are adding these credentials at an empty path so that it won't be picked
  // up by the preemptive authentication step and will only be picked up via
  // origin + realm + scheme lookup.
  auth_cache->Add(GURL("http://bad:99"), "test_realm",
                  net::HttpAuth::AUTH_SCHEME_BASIC,
                  "Basic realm=\"test_realm\"",
                  net::AuthCredentials(base::ASCIIToUTF16("user"),
                                       base::ASCIIToUTF16("password")),
                  std::string());

  ProxyResolvingClientSocketFactory proxy_resolving_socket_factory(
      &socket_factory, &context_with_proxy_);
  std::unique_ptr<ProxyResolvingClientSocket> socket =
      proxy_resolving_socket_factory.CreateSocket(net::SSLConfig(),
                                                  kDestination);
  net::TestCompletionCallback callback;
  int status = socket->Connect(callback.callback());
  EXPECT_THAT(callback.GetResult(status), net::test::IsOk());
}

// Make sure that if HttpAuthCache is updated e.g through normal URLRequests,
// ProxyResolvingClientSocketFactory uses the latest cache for creating new
// sockets.
TEST_F(ProxyResolvingClientSocketTest, FactoryUsesLatestHTTPAuthCache) {
  net::MockClientSocketFactory socket_factory;
  ProxyResolvingClientSocketFactory proxy_resolving_socket_factory(
      &socket_factory, &context_with_proxy_);

  // After creating |socket_factory|, updates the auth cache with credentials.
  // New socket connections should pick up this change.
  net::HttpAuthCache* auth_cache =
      context_with_proxy_.http_transaction_factory()
          ->GetSession()
          ->http_auth_cache();

  // We are adding these credentials at an empty path so that it won't be picked
  // up by the preemptive authentication step and will only be picked up via
  // origin + realm + scheme lookup.
  auth_cache->Add(GURL("http://bad:99"), "test_realm",
                  net::HttpAuth::AUTH_SCHEME_BASIC,
                  "Basic realm=\"test_realm\"",
                  net::AuthCredentials(base::ASCIIToUTF16("user"),
                                       base::ASCIIToUTF16("password")),
                  std::string());

  const GURL kDestination("https://example.com:443");

  // Initial connect without credentials. The server responds with a 407.
  net::MockWrite kConnectWrites[] = {
      net::MockWrite("CONNECT example.com:443 HTTP/1.1\r\n"
                     "Host: example.com:443\r\n"
                     "Proxy-Connection: keep-alive\r\n"
                     "\r\n"),
      net::MockWrite("CONNECT example.com:443 HTTP/1.1\r\n"
                     "Host: example.com:443\r\n"
                     "Proxy-Connection: keep-alive\r\n"
                     "Proxy-Authorization: Basic dXNlcjpwYXNzd29yZA==\r\n"
                     "\r\n")};
  net::MockRead kConnectReads[] = {
      net::MockRead("HTTP/1.1 407 Proxy Authentication Required\r\n"
                    "Proxy-Authenticate: Basic realm=\"test_realm\"\r\n"
                    "Proxy-Connection: keep-alive\r\n"
                    "Content-Length: 0\r\n"
                    "\r\n"),
      net::MockRead("HTTP/1.1 200 Success\r\n\r\n")};

  net::StaticSocketDataProvider kSocketData(
      kConnectReads, arraysize(kConnectReads), kConnectWrites,
      arraysize(kConnectWrites));
  socket_factory.AddSocketDataProvider(&kSocketData);

  std::unique_ptr<ProxyResolvingClientSocket> socket =
      proxy_resolving_socket_factory.CreateSocket(net::SSLConfig(),
                                                  kDestination);
  net::TestCompletionCallback callback;
  int status = socket->Connect(callback.callback());
  EXPECT_THAT(callback.GetResult(status), net::test::IsOk());
}

TEST_F(ProxyResolvingClientSocketTest, ReusesHTTPAuthCache_Preemptive) {
  net::MockClientSocketFactory socket_factory;
  const GURL kDestination("https://example.com:443");

  // Initial connect uses preemptive credentials. That is all.
  net::MockWrite kConnectWrites[] = {
      net::MockWrite("CONNECT example.com:443 HTTP/1.1\r\n"
                     "Host: example.com:443\r\n"
                     "Proxy-Connection: keep-alive\r\n"
                     "Proxy-Authorization: Basic dXNlcjpwYXNzd29yZA==\r\n"
                     "\r\n")};
  net::MockRead kConnectReads[] = {
      net::MockRead("HTTP/1.1 200 Success\r\n\r\n")};

  net::StaticSocketDataProvider kSocketData(
      kConnectReads, arraysize(kConnectReads), kConnectWrites,
      arraysize(kConnectWrites));
  socket_factory.AddSocketDataProvider(&kSocketData);

  net::HttpAuthCache* auth_cache =
      context_with_proxy_.http_transaction_factory()
          ->GetSession()
          ->http_auth_cache();

  auth_cache->Add(GURL("http://bad:99"), "test_realm",
                  net::HttpAuth::AUTH_SCHEME_BASIC,
                  "Basic realm=\"test_realm\"",
                  net::AuthCredentials(base::ASCIIToUTF16("user"),
                                       base::ASCIIToUTF16("password")),
                  "/");

  ProxyResolvingClientSocketFactory proxy_resolving_socket_factory(
      &socket_factory, &context_with_proxy_);
  std::unique_ptr<ProxyResolvingClientSocket> socket =
      proxy_resolving_socket_factory.CreateSocket(net::SSLConfig(),
                                                  kDestination);

  net::TestCompletionCallback callback;
  int status = socket->Connect(callback.callback());
  EXPECT_THAT(callback.GetResult(status), net::test::IsOk());
}

TEST_F(ProxyResolvingClientSocketTest, ReusesHTTPAuthCache_NoCredentials) {
  net::MockClientSocketFactory socket_factory;
  const GURL kDestination("https://example.com:443");

  // Initial connect uses preemptive credentials. That is all.
  net::MockWrite kConnectWrites[] = {
      net::MockWrite("CONNECT example.com:443 HTTP/1.1\r\n"
                     "Host: example.com:443\r\n"
                     "Proxy-Connection: keep-alive\r\n"
                     "\r\n")};
  net::MockRead kConnectReads[] = {
      net::MockRead("HTTP/1.1 407 Proxy Authentication Required\r\n"
                    "Proxy-Authenticate: Basic realm=\"test_realm\"\r\n"
                    "\r\n")};

  net::StaticSocketDataProvider kSocketData(
      kConnectReads, arraysize(kConnectReads), kConnectWrites,
      arraysize(kConnectWrites));
  socket_factory.AddSocketDataProvider(&kSocketData);

  ProxyResolvingClientSocketFactory proxy_resolving_socket_factory(
      &socket_factory, &context_with_proxy_);
  std::unique_ptr<ProxyResolvingClientSocket> socket =
      proxy_resolving_socket_factory.CreateSocket(net::SSLConfig(),
                                                  kDestination);

  net::TestCompletionCallback callback;
  int status = socket->Connect(callback.callback());
  EXPECT_THAT(callback.GetResult(status), net::ERR_PROXY_AUTH_REQUESTED);
}

// Make sure that url is sanitized before it is disclosed to the proxy.
TEST_F(ProxyResolvingClientSocketTest, URLSanitized) {
  GURL url("http://username:password@www.example.com:79/?ref#hash#hash");

  auto context = std::make_unique<net::TestURLRequestContext>(true);
  net::ProxyConfig proxy_config;
  proxy_config.set_pac_url(GURL("http://foopy/proxy.pac"));
  proxy_config.set_pac_mandatory(true);
  net::MockAsyncProxyResolver resolver;
  auto proxy_resolver_factory =
      std::make_unique<net::MockAsyncProxyResolverFactory>(false);
  net::MockAsyncProxyResolverFactory* proxy_resolver_factory_raw =
      proxy_resolver_factory.get();
  net::ProxyResolutionService service(
      std::make_unique<net::ProxyConfigServiceFixed>(proxy_config),
      std::move(proxy_resolver_factory), nullptr);
  context->set_proxy_resolution_service(&service);
  context->Init();

  ProxyResolvingClientSocketFactory proxy_resolving_socket_factory(
      nullptr, context.get());
  std::unique_ptr<ProxyResolvingClientSocket> socket =
      proxy_resolving_socket_factory.CreateSocket(net::SSLConfig(), url);
  net::TestCompletionCallback callback;
  int status = socket->Connect(callback.callback());
  EXPECT_EQ(net::ERR_IO_PENDING, status);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1u, proxy_resolver_factory_raw->pending_requests().size());
  EXPECT_EQ(
      GURL("http://foopy/proxy.pac"),
      proxy_resolver_factory_raw->pending_requests()[0]->script_data()->url());
  proxy_resolver_factory_raw->pending_requests()[0]->CompleteNowWithForwarder(
      net::OK, &resolver);
  ASSERT_EQ(1u, resolver.pending_jobs().size());
  // The URL should have been simplified, stripping the username/password/hash.
  EXPECT_EQ(GURL("http://www.example.com:79/?ref"),
            resolver.pending_jobs()[0]->url());
}

class ReconsiderProxyAfterErrorTest
    : public testing::Test,
      public testing::WithParamInterface<::testing::tuple<bool, int>> {
 public:
  ReconsiderProxyAfterErrorTest()
      : context_with_proxy_(
            "HTTPS badproxy:99; HTTPS badfallbackproxy:98; DIRECT") {}

  ~ReconsiderProxyAfterErrorTest() override {}

  base::test::ScopedTaskEnvironment scoped_task_environment_;
  TestURLRequestContextWithProxy context_with_proxy_;
};

// List of errors that are used in the proxy resolution tests.
const int kProxyTestMockErrors[] = {
    net::ERR_PROXY_CONNECTION_FAILED, net::ERR_NAME_NOT_RESOLVED,
    net::ERR_ADDRESS_UNREACHABLE,     net::ERR_CONNECTION_CLOSED,
    net::ERR_CONNECTION_RESET,        net::ERR_CONNECTION_REFUSED,
    net::ERR_CONNECTION_ABORTED,      net::ERR_TUNNEL_CONNECTION_FAILED,
    net::ERR_SOCKS_CONNECTION_FAILED, net::ERR_TIMED_OUT,
    // Errors that should be retried but are currently not.
    // net::ERR_CONNECTION_TIMED_OUT,
    // net::ERR_PROXY_CERTIFICATE_INVALID,
    // net::ERR_QUIC_PROTOCOL_ERROR,
    // net::ERR_QUIC_HANDSHAKE_FAILED,
    // net::ERR_SSL_PROTOCOL_ERROR,
    // net::ERR_MSG_TOO_BIG,
};

INSTANTIATE_TEST_CASE_P(
    /* no prefix */,
    ReconsiderProxyAfterErrorTest,
    testing::Combine(testing::Bool(), testing::ValuesIn(kProxyTestMockErrors)));

TEST_P(ReconsiderProxyAfterErrorTest, ReconsiderProxyAfterError) {
  net::IoMode io_mode =
      ::testing::get<0>(GetParam()) ? net::SYNCHRONOUS : net::ASYNC;
  const int mock_error = ::testing::get<1>(GetParam());

  // Before starting the test, verify that there are no proxies marked as bad.
  ASSERT_TRUE(context_with_proxy_.proxy_resolution_service()
                  ->proxy_retry_info()
                  .empty())
      << mock_error;

  net::MockClientSocketFactory socket_factory;
  // Connect to first broken proxy.
  net::StaticSocketDataProvider data1;
  data1.set_connect_data(net::MockConnect(io_mode, mock_error));
  socket_factory.AddSocketDataProvider(&data1);

  // Connect to second broken proxy.
  net::StaticSocketDataProvider data2;
  data2.set_connect_data(net::MockConnect(io_mode, mock_error));
  socket_factory.AddSocketDataProvider(&data2);

  // Connect using direct.
  net::StaticSocketDataProvider data3;
  data3.set_connect_data(net::MockConnect(io_mode, net::OK));
  socket_factory.AddSocketDataProvider(&data3);

  const GURL kDestination("https://example.com:443");
  ProxyResolvingClientSocketFactory proxy_resolving_socket_factory(
      &socket_factory, &context_with_proxy_);
  std::unique_ptr<ProxyResolvingClientSocket> socket =
      proxy_resolving_socket_factory.CreateSocket(net::SSLConfig(),
                                                  kDestination);
  net::TestCompletionCallback callback;
  int status = socket->Connect(callback.callback());
  EXPECT_EQ(net::ERR_IO_PENDING, status);
  status = callback.WaitForResult();
  EXPECT_EQ(net::OK, status);

  const net::ProxyRetryInfoMap& retry_info =
      context_with_proxy_.proxy_resolution_service()->proxy_retry_info();
  EXPECT_EQ(2u, retry_info.size()) << mock_error;
  EXPECT_NE(retry_info.end(), retry_info.find("https://badproxy:99"));
  EXPECT_NE(retry_info.end(), retry_info.find("https://badfallbackproxy:98"));
}

}  // namespace network
