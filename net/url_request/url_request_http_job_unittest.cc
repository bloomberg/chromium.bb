// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_http_job.h"

#include <stdint.h>

#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/test/histogram_tester.h"
#include "net/base/auth.h"
#include "net/base/request_priority.h"
#include "net/cookies/cookie_store_test_helpers.h"
#include "net/http/http_transaction_factory.h"
#include "net/http/http_transaction_test_util.h"
#include "net/log/net_log_event_type.h"
#include "net/log/test_net_log.h"
#include "net/log/test_net_log_entry.h"
#include "net/log/test_net_log_util.h"
#include "net/net_features.h"
#include "net/socket/socket_test_util.h"
#include "net/test/cert_test_util.h"
#include "net/test/gtest_util.h"
#include "net/test/test_data_directory.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_job_factory_impl.h"
#include "net/url_request/url_request_status.h"
#include "net/url_request/url_request_test_util.h"
#include "net/websockets/websocket_handshake_stream_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/url_constants.h"

#if defined(OS_ANDROID)
#include "base/android/build_info.h"
#include "base/android/jni_android.h"
#include "jni/AndroidNetworkLibraryTestUtil_jni.h"
#endif

using net::test::IsError;
using net::test::IsOk;

namespace net {

namespace {

using ::testing::Return;

const char kSimpleGetMockWrite[] =
    "GET / HTTP/1.1\r\n"
    "Host: www.example.com\r\n"
    "Connection: keep-alive\r\n"
    "User-Agent:\r\n"
    "Accept-Encoding: gzip, deflate\r\n"
    "Accept-Language: en-us,fr\r\n\r\n";

const char kSimpleHeadMockWrite[] =
    "HEAD / HTTP/1.1\r\n"
    "Host: www.example.com\r\n"
    "Connection: keep-alive\r\n"
    "User-Agent:\r\n"
    "Accept-Encoding: gzip, deflate\r\n"
    "Accept-Language: en-us,fr\r\n\r\n";

// Inherit from URLRequestHttpJob to expose the priority and some
// other hidden functions.
class TestURLRequestHttpJob : public URLRequestHttpJob {
 public:
  explicit TestURLRequestHttpJob(URLRequest* request)
      : URLRequestHttpJob(request,
                          request->context()->network_delegate(),
                          request->context()->http_user_agent_settings()),
        use_null_source_stream_(false) {}

  ~TestURLRequestHttpJob() override {}

  // URLRequestJob implementation:
  std::unique_ptr<SourceStream> SetUpSourceStream() override {
    if (use_null_source_stream_)
      return nullptr;
    return URLRequestHttpJob::SetUpSourceStream();
  }

  void set_use_null_source_stream(bool use_null_source_stream) {
    use_null_source_stream_ = use_null_source_stream;
  }

  using URLRequestHttpJob::SetPriority;
  using URLRequestHttpJob::Start;
  using URLRequestHttpJob::Kill;
  using URLRequestHttpJob::priority;

 private:
  bool use_null_source_stream_;

  DISALLOW_COPY_AND_ASSIGN(TestURLRequestHttpJob);
};

class URLRequestHttpJobSetUpSourceTest : public ::testing::Test {
 public:
  URLRequestHttpJobSetUpSourceTest() : context_(true) {
    test_job_interceptor_ = new TestJobInterceptor();
    EXPECT_TRUE(test_job_factory_.SetProtocolHandler(
        url::kHttpScheme, base::WrapUnique(test_job_interceptor_)));
    context_.set_job_factory(&test_job_factory_);
    context_.set_client_socket_factory(&socket_factory_);
    context_.Init();
  }

 protected:
  MockClientSocketFactory socket_factory_;
  // |test_job_interceptor_| is owned by |test_job_factory_|.
  TestJobInterceptor* test_job_interceptor_;
  URLRequestJobFactoryImpl test_job_factory_;

  TestURLRequestContext context_;
  TestDelegate delegate_;
};

// Tests that if SetUpSourceStream() returns nullptr, the request fails.
TEST_F(URLRequestHttpJobSetUpSourceTest, SetUpSourceFails) {
  MockWrite writes[] = {MockWrite(kSimpleGetMockWrite)};
  MockRead reads[] = {MockRead("HTTP/1.1 200 OK\r\n"
                               "Content-Length: 12\r\n\r\n"),
                      MockRead("Test Content")};

  StaticSocketDataProvider socket_data(reads, arraysize(reads), writes,
                                       arraysize(writes));
  socket_factory_.AddSocketDataProvider(&socket_data);

  std::unique_ptr<URLRequest> request =
      context_.CreateRequest(GURL("http://www.example.com"), DEFAULT_PRIORITY,
                             &delegate_, TRAFFIC_ANNOTATION_FOR_TESTS);
  auto job = std::make_unique<TestURLRequestHttpJob>(request.get());
  job->set_use_null_source_stream(true);
  test_job_interceptor_->set_main_intercept_job(std::move(job));
  request->Start();

  base::RunLoop().Run();
  EXPECT_EQ(ERR_CONTENT_DECODING_INIT_FAILED, delegate_.request_status());
}

// Tests that if there is an unknown content-encoding type, the raw response
// body is passed through.
TEST_F(URLRequestHttpJobSetUpSourceTest, UnknownEncoding) {
  MockWrite writes[] = {MockWrite(kSimpleGetMockWrite)};
  MockRead reads[] = {MockRead("HTTP/1.1 200 OK\r\n"
                               "Content-Encoding: foo, gzip\r\n"
                               "Content-Length: 12\r\n\r\n"),
                      MockRead("Test Content")};

  StaticSocketDataProvider socket_data(reads, arraysize(reads), writes,
                                       arraysize(writes));
  socket_factory_.AddSocketDataProvider(&socket_data);

  std::unique_ptr<URLRequest> request =
      context_.CreateRequest(GURL("http://www.example.com"), DEFAULT_PRIORITY,
                             &delegate_, TRAFFIC_ANNOTATION_FOR_TESTS);
  auto job = std::make_unique<TestURLRequestHttpJob>(request.get());
  test_job_interceptor_->set_main_intercept_job(std::move(job));
  request->Start();

  base::RunLoop().Run();
  EXPECT_EQ(OK, delegate_.request_status());
  EXPECT_EQ("Test Content", delegate_.data_received());
}

class URLRequestHttpJobWithProxy {
 public:
  explicit URLRequestHttpJobWithProxy(
      std::unique_ptr<ProxyService> proxy_service)
      : proxy_service_(std::move(proxy_service)),
        context_(new TestURLRequestContext(true)) {
    context_->set_client_socket_factory(&socket_factory_);
    context_->set_network_delegate(&network_delegate_);
    context_->set_proxy_service(proxy_service_.get());
    context_->Init();
  }

  MockClientSocketFactory socket_factory_;
  TestNetworkDelegate network_delegate_;
  std::unique_ptr<ProxyService> proxy_service_;
  std::unique_ptr<TestURLRequestContext> context_;

 private:
  DISALLOW_COPY_AND_ASSIGN(URLRequestHttpJobWithProxy);
};

// Tests that when proxy is not used, the proxy server is set correctly on the
// URLRequest.
TEST(URLRequestHttpJobWithProxy, TestFailureWithoutProxy) {
  URLRequestHttpJobWithProxy http_job_with_proxy(nullptr);

  MockWrite writes[] = {MockWrite(kSimpleGetMockWrite)};
  MockRead reads[] = {MockRead(SYNCHRONOUS, ERR_CONNECTION_RESET)};

  StaticSocketDataProvider socket_data(reads, arraysize(reads), writes,
                                       arraysize(writes));
  http_job_with_proxy.socket_factory_.AddSocketDataProvider(&socket_data);

  TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      http_job_with_proxy.context_->CreateRequest(
          GURL("http://www.example.com"), DEFAULT_PRIORITY, &delegate,
          TRAFFIC_ANNOTATION_FOR_TESTS);

  request->Start();
  ASSERT_TRUE(request->is_pending());
  base::RunLoop().Run();

  EXPECT_THAT(delegate.request_status(), IsError(ERR_CONNECTION_RESET));
  EXPECT_EQ(ProxyServer::Direct(), request->proxy_server());
  EXPECT_FALSE(request->was_fetched_via_proxy());
  EXPECT_EQ(0, request->received_response_content_length());
  EXPECT_EQ(CountWriteBytes(writes, arraysize(writes)),
            request->GetTotalSentBytes());
  EXPECT_EQ(CountReadBytes(reads, arraysize(reads)),
            request->GetTotalReceivedBytes());
  EXPECT_EQ(CountWriteBytes(writes, arraysize(writes)),
            http_job_with_proxy.network_delegate_.total_network_bytes_sent());
  EXPECT_EQ(
      CountReadBytes(reads, arraysize(reads)),
      http_job_with_proxy.network_delegate_.total_network_bytes_received());
}

// Tests that when one proxy is in use and the connection to the proxy server
// fails, the proxy server is still set correctly on the URLRequest.
TEST(URLRequestHttpJobWithProxy, TestSuccessfulWithOneProxy) {
  const char kSimpleProxyGetMockWrite[] =
      "GET http://www.example.com/ HTTP/1.1\r\n"
      "Host: www.example.com\r\n"
      "Proxy-Connection: keep-alive\r\n"
      "User-Agent:\r\n"
      "Accept-Encoding: gzip, deflate\r\n"
      "Accept-Language: en-us,fr\r\n\r\n";

  const ProxyServer proxy_server =
      ProxyServer::FromURI("http://origin.net:80", ProxyServer::SCHEME_HTTP);

  std::unique_ptr<ProxyService> proxy_service =
      ProxyService::CreateFixedFromPacResult(proxy_server.ToPacString());

  MockWrite writes[] = {MockWrite(kSimpleProxyGetMockWrite)};
  MockRead reads[] = {MockRead(SYNCHRONOUS, ERR_CONNECTION_RESET)};

  StaticSocketDataProvider socket_data(reads, arraysize(reads), writes,
                                       arraysize(writes));

  URLRequestHttpJobWithProxy http_job_with_proxy(std::move(proxy_service));
  http_job_with_proxy.socket_factory_.AddSocketDataProvider(&socket_data);

  TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      http_job_with_proxy.context_->CreateRequest(
          GURL("http://www.example.com"), DEFAULT_PRIORITY, &delegate,
          TRAFFIC_ANNOTATION_FOR_TESTS);

  request->Start();
  ASSERT_TRUE(request->is_pending());
  base::RunLoop().Run();

  EXPECT_THAT(delegate.request_status(), IsError(ERR_CONNECTION_RESET));
  // When request fails due to proxy connection errors, the proxy server should
  // still be set on the |request|.
  EXPECT_EQ(proxy_server, request->proxy_server());
  EXPECT_FALSE(request->was_fetched_via_proxy());
  EXPECT_EQ(0, request->received_response_content_length());
  EXPECT_EQ(CountWriteBytes(writes, arraysize(writes)),
            request->GetTotalSentBytes());
  EXPECT_EQ(0, request->GetTotalReceivedBytes());
  EXPECT_EQ(CountWriteBytes(writes, arraysize(writes)),
            http_job_with_proxy.network_delegate_.total_network_bytes_sent());
  EXPECT_EQ(
      0, http_job_with_proxy.network_delegate_.total_network_bytes_received());
}

// Tests that when two proxies are in use and the connection to the first proxy
// server fails, the proxy server is set correctly on the URLRequest.
TEST(URLRequestHttpJobWithProxy,
     TestContentLengthSuccessfulRequestWithTwoProxies) {
  const ProxyServer proxy_server =
      ProxyServer::FromURI("http://origin.net:80", ProxyServer::SCHEME_HTTP);

  // Connection to |proxy_server| would fail. Request should be fetched over
  // DIRECT.
  std::unique_ptr<ProxyService> proxy_service =
      ProxyService::CreateFixedFromPacResult(
          proxy_server.ToPacString() + "; " +
          ProxyServer::Direct().ToPacString());

  MockWrite writes[] = {MockWrite(kSimpleGetMockWrite)};
  MockRead reads[] = {MockRead("HTTP/1.1 200 OK\r\n"
                               "Content-Length: 12\r\n\r\n"),
                      MockRead("Test Content"), MockRead(ASYNC, OK)};

  MockConnect mock_connect_1(SYNCHRONOUS, ERR_CONNECTION_RESET);
  StaticSocketDataProvider connect_data_1;
  connect_data_1.set_connect_data(mock_connect_1);

  StaticSocketDataProvider socket_data(reads, arraysize(reads), writes,
                                       arraysize(writes));

  URLRequestHttpJobWithProxy http_job_with_proxy(std::move(proxy_service));
  http_job_with_proxy.socket_factory_.AddSocketDataProvider(&connect_data_1);
  http_job_with_proxy.socket_factory_.AddSocketDataProvider(&socket_data);

  TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      http_job_with_proxy.context_->CreateRequest(
          GURL("http://www.example.com"), DEFAULT_PRIORITY, &delegate,
          TRAFFIC_ANNOTATION_FOR_TESTS);

  request->Start();
  ASSERT_TRUE(request->is_pending());
  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(delegate.request_status(), IsOk());
  EXPECT_EQ(ProxyServer::Direct(), request->proxy_server());
  EXPECT_FALSE(request->was_fetched_via_proxy());
  EXPECT_EQ(12, request->received_response_content_length());
  EXPECT_EQ(CountWriteBytes(writes, arraysize(writes)),
            request->GetTotalSentBytes());
  EXPECT_EQ(CountReadBytes(reads, arraysize(reads)),
            request->GetTotalReceivedBytes());
  EXPECT_EQ(CountWriteBytes(writes, arraysize(writes)),
            http_job_with_proxy.network_delegate_.total_network_bytes_sent());
  EXPECT_EQ(
      CountReadBytes(reads, arraysize(reads)),
      http_job_with_proxy.network_delegate_.total_network_bytes_received());
}

class URLRequestHttpJobTest : public ::testing::Test {
 protected:
  URLRequestHttpJobTest() : context_(true) {
    context_.set_http_transaction_factory(&network_layer_);

    // The |test_job_factory_| takes ownership of the interceptor.
    test_job_interceptor_ = new TestJobInterceptor();
    EXPECT_TRUE(test_job_factory_.SetProtocolHandler(
        url::kHttpScheme, base::WrapUnique(test_job_interceptor_)));
    context_.set_job_factory(&test_job_factory_);
    context_.set_net_log(&net_log_);
    context_.Init();

    req_ =
        context_.CreateRequest(GURL("http://www.example.com"), DEFAULT_PRIORITY,
                               &delegate_, TRAFFIC_ANNOTATION_FOR_TESTS);
  }

  MockNetworkLayer network_layer_;

  // |test_job_interceptor_| is owned by |test_job_factory_|.
  TestJobInterceptor* test_job_interceptor_;
  URLRequestJobFactoryImpl test_job_factory_;

  TestURLRequestContext context_;
  TestDelegate delegate_;
  TestNetLog net_log_;
  std::unique_ptr<URLRequest> req_;
};

class URLRequestHttpJobWithMockSocketsTest : public ::testing::Test {
 protected:
  URLRequestHttpJobWithMockSocketsTest()
      : context_(new TestURLRequestContext(true)) {
    context_->set_client_socket_factory(&socket_factory_);
    context_->set_network_delegate(&network_delegate_);
    context_->Init();
  }

  MockClientSocketFactory socket_factory_;
  TestNetworkDelegate network_delegate_;
  std::unique_ptr<TestURLRequestContext> context_;
};

TEST_F(URLRequestHttpJobWithMockSocketsTest,
       TestContentLengthSuccessfulRequest) {
  MockWrite writes[] = {MockWrite(kSimpleGetMockWrite)};
  MockRead reads[] = {MockRead("HTTP/1.1 200 OK\r\n"
                               "Content-Length: 12\r\n\r\n"),
                      MockRead("Test Content")};

  StaticSocketDataProvider socket_data(reads, arraysize(reads), writes,
                                       arraysize(writes));
  socket_factory_.AddSocketDataProvider(&socket_data);

  TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context_->CreateRequest(GURL("http://www.example.com"), DEFAULT_PRIORITY,
                              &delegate, TRAFFIC_ANNOTATION_FOR_TESTS);

  request->Start();
  ASSERT_TRUE(request->is_pending());
  base::RunLoop().Run();

  EXPECT_THAT(delegate.request_status(), IsOk());
  EXPECT_EQ(12, request->received_response_content_length());
  EXPECT_EQ(CountWriteBytes(writes, arraysize(writes)),
            request->GetTotalSentBytes());
  EXPECT_EQ(CountReadBytes(reads, arraysize(reads)),
            request->GetTotalReceivedBytes());
  EXPECT_EQ(CountWriteBytes(writes, arraysize(writes)),
            network_delegate_.total_network_bytes_sent());
  EXPECT_EQ(CountReadBytes(reads, arraysize(reads)),
            network_delegate_.total_network_bytes_received());
}

// Tests a successful HEAD request.
TEST_F(URLRequestHttpJobWithMockSocketsTest, TestSuccessfulHead) {
  MockWrite writes[] = {MockWrite(kSimpleHeadMockWrite)};
  MockRead reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n"
               "Content-Length: 0\r\n\r\n")};

  StaticSocketDataProvider socket_data(reads, arraysize(reads), writes,
                                       arraysize(writes));
  socket_factory_.AddSocketDataProvider(&socket_data);

  TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context_->CreateRequest(GURL("http://www.example.com"), DEFAULT_PRIORITY,
                              &delegate, TRAFFIC_ANNOTATION_FOR_TESTS);

  request->set_method("HEAD");
  request->Start();
  ASSERT_TRUE(request->is_pending());
  base::RunLoop().Run();

  EXPECT_THAT(delegate.request_status(), IsOk());
  EXPECT_EQ(0, request->received_response_content_length());
  EXPECT_EQ(CountWriteBytes(writes, arraysize(writes)),
            request->GetTotalSentBytes());
  EXPECT_EQ(CountReadBytes(reads, arraysize(reads)),
            request->GetTotalReceivedBytes());
  EXPECT_EQ(CountWriteBytes(writes, arraysize(writes)),
            network_delegate_.total_network_bytes_sent());
  EXPECT_EQ(CountReadBytes(reads, arraysize(reads)),
            network_delegate_.total_network_bytes_received());
}

// Similar to above test but tests that even if response body is there in the
// HEAD response stream, it should not be read due to HttpStreamParser's logic.
TEST_F(URLRequestHttpJobWithMockSocketsTest, TestSuccessfulHeadWithContent) {
  MockWrite writes[] = {MockWrite(kSimpleHeadMockWrite)};
  MockRead reads[] = {MockRead("HTTP/1.1 200 OK\r\n"
                               "Content-Length: 12\r\n\r\n"),
                      MockRead("Test Content")};

  StaticSocketDataProvider socket_data(reads, arraysize(reads), writes,
                                       arraysize(writes));
  socket_factory_.AddSocketDataProvider(&socket_data);

  TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context_->CreateRequest(GURL("http://www.example.com"), DEFAULT_PRIORITY,
                              &delegate, TRAFFIC_ANNOTATION_FOR_TESTS);

  request->set_method("HEAD");
  request->Start();
  ASSERT_TRUE(request->is_pending());
  base::RunLoop().Run();

  EXPECT_THAT(delegate.request_status(), IsOk());
  EXPECT_EQ(0, request->received_response_content_length());
  EXPECT_EQ(CountWriteBytes(writes, arraysize(writes)),
            request->GetTotalSentBytes());
  EXPECT_EQ(CountReadBytes(reads, arraysize(reads)) - 12,
            request->GetTotalReceivedBytes());
  EXPECT_EQ(CountWriteBytes(writes, arraysize(writes)),
            network_delegate_.total_network_bytes_sent());
  EXPECT_EQ(CountReadBytes(reads, arraysize(reads)) - 12,
            network_delegate_.total_network_bytes_received());
}

TEST_F(URLRequestHttpJobWithMockSocketsTest, TestSuccessfulCachedHeadRequest) {
  // Cache the response.
  {
    MockWrite writes[] = {MockWrite(kSimpleGetMockWrite)};
    MockRead reads[] = {MockRead("HTTP/1.1 200 OK\r\n"
                                 "Content-Length: 12\r\n\r\n"),
                        MockRead("Test Content")};

    StaticSocketDataProvider socket_data(reads, arraysize(reads), writes,
                                         arraysize(writes));
    socket_factory_.AddSocketDataProvider(&socket_data);

    TestDelegate delegate;
    std::unique_ptr<URLRequest> request = context_->CreateRequest(
        GURL("http://www.example.com"), DEFAULT_PRIORITY, &delegate,
        TRAFFIC_ANNOTATION_FOR_TESTS);

    request->Start();
    ASSERT_TRUE(request->is_pending());
    base::RunLoop().Run();

    EXPECT_THAT(delegate.request_status(), IsOk());
    EXPECT_EQ(12, request->received_response_content_length());
    EXPECT_EQ(CountWriteBytes(writes, arraysize(writes)),
              request->GetTotalSentBytes());
    EXPECT_EQ(CountReadBytes(reads, arraysize(reads)),
              request->GetTotalReceivedBytes());
    EXPECT_EQ(CountWriteBytes(writes, arraysize(writes)),
              network_delegate_.total_network_bytes_sent());
    EXPECT_EQ(CountReadBytes(reads, arraysize(reads)),
              network_delegate_.total_network_bytes_received());
  }

  // Send a HEAD request for the cached response.
  {
    MockWrite writes[] = {MockWrite(kSimpleHeadMockWrite)};
    MockRead reads[] = {
        MockRead("HTTP/1.1 200 OK\r\n"
                 "Content-Length: 0\r\n\r\n")};

    StaticSocketDataProvider socket_data(reads, arraysize(reads), writes,
                                         arraysize(writes));
    socket_factory_.AddSocketDataProvider(&socket_data);

    TestDelegate delegate;
    std::unique_ptr<URLRequest> request = context_->CreateRequest(
        GURL("http://www.example.com"), DEFAULT_PRIORITY, &delegate,
        TRAFFIC_ANNOTATION_FOR_TESTS);

    // Use the cached version.
    request->SetLoadFlags(LOAD_SKIP_CACHE_VALIDATION);
    request->set_method("HEAD");
    request->Start();
    ASSERT_TRUE(request->is_pending());
    base::RunLoop().Run();

    EXPECT_THAT(delegate.request_status(), IsOk());
    EXPECT_EQ(0, request->received_response_content_length());
    EXPECT_EQ(0, request->GetTotalSentBytes());
    EXPECT_EQ(0, request->GetTotalReceivedBytes());
  }
}

TEST_F(URLRequestHttpJobWithMockSocketsTest,
       TestContentLengthSuccessfulHttp09Request) {
  MockWrite writes[] = {MockWrite(kSimpleGetMockWrite)};
  MockRead reads[] = {MockRead("Test Content"),
                      MockRead(net::SYNCHRONOUS, net::OK)};

  StaticSocketDataProvider socket_data(reads, arraysize(reads), nullptr, 0);
  socket_factory_.AddSocketDataProvider(&socket_data);

  TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context_->CreateRequest(GURL("http://www.example.com"), DEFAULT_PRIORITY,
                              &delegate, TRAFFIC_ANNOTATION_FOR_TESTS);

  request->Start();
  ASSERT_TRUE(request->is_pending());
  base::RunLoop().Run();

  EXPECT_THAT(delegate.request_status(), IsOk());
  EXPECT_EQ(12, request->received_response_content_length());
  EXPECT_EQ(CountWriteBytes(writes, arraysize(writes)),
            request->GetTotalSentBytes());
  EXPECT_EQ(CountReadBytes(reads, arraysize(reads)),
            request->GetTotalReceivedBytes());
  EXPECT_EQ(CountWriteBytes(writes, arraysize(writes)),
            network_delegate_.total_network_bytes_sent());
  EXPECT_EQ(CountReadBytes(reads, arraysize(reads)),
            network_delegate_.total_network_bytes_received());
}

TEST_F(URLRequestHttpJobWithMockSocketsTest, TestContentLengthFailedRequest) {
  MockWrite writes[] = {MockWrite(kSimpleGetMockWrite)};
  MockRead reads[] = {MockRead("HTTP/1.1 200 OK\r\n"
                               "Content-Length: 20\r\n\r\n"),
                      MockRead("Test Content"),
                      MockRead(net::SYNCHRONOUS, net::ERR_FAILED)};

  StaticSocketDataProvider socket_data(reads, arraysize(reads), writes,
                                       arraysize(writes));
  socket_factory_.AddSocketDataProvider(&socket_data);

  TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context_->CreateRequest(GURL("http://www.example.com"), DEFAULT_PRIORITY,
                              &delegate, TRAFFIC_ANNOTATION_FOR_TESTS);

  request->Start();
  ASSERT_TRUE(request->is_pending());
  base::RunLoop().Run();

  EXPECT_THAT(delegate.request_status(), IsError(ERR_FAILED));
  EXPECT_EQ(12, request->received_response_content_length());
  EXPECT_EQ(CountWriteBytes(writes, arraysize(writes)),
            request->GetTotalSentBytes());
  EXPECT_EQ(CountReadBytes(reads, arraysize(reads)),
            request->GetTotalReceivedBytes());
  EXPECT_EQ(CountWriteBytes(writes, arraysize(writes)),
            network_delegate_.total_network_bytes_sent());
  EXPECT_EQ(CountReadBytes(reads, arraysize(reads)),
            network_delegate_.total_network_bytes_received());
}

TEST_F(URLRequestHttpJobWithMockSocketsTest,
       TestContentLengthCancelledRequest) {
  MockWrite writes[] = {MockWrite(kSimpleGetMockWrite)};
  MockRead reads[] = {MockRead("HTTP/1.1 200 OK\r\n"
                               "Content-Length: 20\r\n\r\n"),
                      MockRead("Test Content"),
                      MockRead(net::SYNCHRONOUS, net::ERR_IO_PENDING)};

  StaticSocketDataProvider socket_data(reads, arraysize(reads), writes,
                                       arraysize(writes));
  socket_factory_.AddSocketDataProvider(&socket_data);

  TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context_->CreateRequest(GURL("http://www.example.com"), DEFAULT_PRIORITY,
                              &delegate, TRAFFIC_ANNOTATION_FOR_TESTS);

  delegate.set_cancel_in_received_data(true);
  request->Start();
  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(delegate.request_status(), IsError(ERR_ABORTED));
  EXPECT_EQ(12, request->received_response_content_length());
  EXPECT_EQ(CountWriteBytes(writes, arraysize(writes)),
            request->GetTotalSentBytes());
  EXPECT_EQ(CountReadBytes(reads, arraysize(reads)),
            request->GetTotalReceivedBytes());
  EXPECT_EQ(CountWriteBytes(writes, arraysize(writes)),
            network_delegate_.total_network_bytes_sent());
  EXPECT_EQ(CountReadBytes(reads, arraysize(reads)),
            network_delegate_.total_network_bytes_received());
}

TEST_F(URLRequestHttpJobWithMockSocketsTest,
       TestRawHeaderSizeSuccessfullRequest) {
  MockWrite writes[] = {MockWrite(kSimpleGetMockWrite)};

  const std::string& response_header =
      "HTTP/1.1 200 OK\r\n"
      "Content-Length: 12\r\n\r\n";
  const std::string& content_data = "Test Content";

  MockRead reads[] = {MockRead(response_header.c_str()),
                      MockRead(content_data.c_str()),
                      MockRead(net::SYNCHRONOUS, net::OK)};

  StaticSocketDataProvider socket_data(reads, arraysize(reads), writes,
                                       arraysize(writes));
  socket_factory_.AddSocketDataProvider(&socket_data);

  TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context_->CreateRequest(GURL("http://www.example.com"), DEFAULT_PRIORITY,
                              &delegate, TRAFFIC_ANNOTATION_FOR_TESTS);

  request->Start();
  ASSERT_TRUE(request->is_pending());
  base::RunLoop().Run();

  EXPECT_EQ(net::OK, request->status().error());
  EXPECT_EQ(static_cast<int>(content_data.size()),
            request->received_response_content_length());
  EXPECT_EQ(static_cast<int>(response_header.size()),
            request->raw_header_size());
  EXPECT_EQ(CountReadBytes(reads, arraysize(reads)),
            request->GetTotalReceivedBytes());
}

TEST_F(URLRequestHttpJobWithMockSocketsTest,
       TestRawHeaderSizeSuccessfull100ContinueRequest) {
  MockWrite writes[] = {MockWrite(kSimpleGetMockWrite)};

  const std::string& continue_header = "HTTP/1.1 100 Continue\r\n\r\n";
  const std::string& response_header =
      "HTTP/1.1 200 OK\r\n"
      "Content-Length: 12\r\n\r\n";
  const std::string& content_data = "Test Content";

  MockRead reads[] = {
      MockRead(continue_header.c_str()), MockRead(response_header.c_str()),
      MockRead(content_data.c_str()), MockRead(net::SYNCHRONOUS, net::OK)};

  StaticSocketDataProvider socket_data(reads, arraysize(reads), writes,
                                       arraysize(writes));
  socket_factory_.AddSocketDataProvider(&socket_data);

  TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context_->CreateRequest(GURL("http://www.example.com"), DEFAULT_PRIORITY,
                              &delegate, TRAFFIC_ANNOTATION_FOR_TESTS);

  request->Start();
  ASSERT_TRUE(request->is_pending());
  base::RunLoop().Run();

  EXPECT_EQ(net::OK, request->status().error());
  EXPECT_EQ(static_cast<int>(content_data.size()),
            request->received_response_content_length());
  EXPECT_EQ(static_cast<int>(continue_header.size() + response_header.size()),
            request->raw_header_size());
  EXPECT_EQ(CountReadBytes(reads, arraysize(reads)),
            request->GetTotalReceivedBytes());
}

TEST_F(URLRequestHttpJobWithMockSocketsTest,
       TestRawHeaderSizeFailureTruncatedHeaders) {
  MockWrite writes[] = {MockWrite(kSimpleGetMockWrite)};
  MockRead reads[] = {MockRead("HTTP/1.0 200 OK\r\n"
                               "Content-Len"),
                      MockRead(net::SYNCHRONOUS, net::OK)};

  StaticSocketDataProvider socket_data(reads, arraysize(reads), writes,
                                       arraysize(writes));
  socket_factory_.AddSocketDataProvider(&socket_data);

  TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context_->CreateRequest(GURL("http://www.example.com"), DEFAULT_PRIORITY,
                              &delegate, TRAFFIC_ANNOTATION_FOR_TESTS);

  delegate.set_cancel_in_response_started(true);
  request->Start();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(ERR_ABORTED, request->status().error());
  EXPECT_EQ(0, request->received_response_content_length());
  EXPECT_EQ(28, request->raw_header_size());
  EXPECT_EQ(CountReadBytes(reads, arraysize(reads)),
            request->GetTotalReceivedBytes());
}

TEST_F(URLRequestHttpJobWithMockSocketsTest,
       TestRawHeaderSizeSuccessfullContinuiousRead) {
  MockWrite writes[] = {MockWrite(kSimpleGetMockWrite)};
  const std::string& header_data =
      "HTTP/1.1 200 OK\r\n"
      "Content-Length: 12\r\n\r\n";
  const std::string& content_data = "Test Content";
  std::string single_read_content = header_data;
  single_read_content.append(content_data);
  MockRead reads[] = {MockRead(single_read_content.c_str())};

  StaticSocketDataProvider socket_data(reads, arraysize(reads), writes,
                                       arraysize(writes));
  socket_factory_.AddSocketDataProvider(&socket_data);

  TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context_->CreateRequest(GURL("http://www.example.com"), DEFAULT_PRIORITY,
                              &delegate, TRAFFIC_ANNOTATION_FOR_TESTS);

  request->Start();
  base::RunLoop().Run();

  EXPECT_EQ(net::OK, request->status().error());
  EXPECT_EQ(static_cast<int>(content_data.size()),
            request->received_response_content_length());
  EXPECT_EQ(static_cast<int>(header_data.size()), request->raw_header_size());
  EXPECT_EQ(CountReadBytes(reads, arraysize(reads)),
            request->GetTotalReceivedBytes());
}

TEST_F(URLRequestHttpJobWithMockSocketsTest,
       TestNetworkBytesRedirectedRequest) {
  MockWrite redirect_writes[] = {
      MockWrite("GET / HTTP/1.1\r\n"
                "Host: www.redirect.com\r\n"
                "Connection: keep-alive\r\n"
                "User-Agent:\r\n"
                "Accept-Encoding: gzip, deflate\r\n"
                "Accept-Language: en-us,fr\r\n\r\n")};

  MockRead redirect_reads[] = {
      MockRead("HTTP/1.1 302 Found\r\n"
               "Location: http://www.example.com\r\n\r\n"),
  };
  StaticSocketDataProvider redirect_socket_data(
      redirect_reads, arraysize(redirect_reads), redirect_writes,
      arraysize(redirect_writes));
  socket_factory_.AddSocketDataProvider(&redirect_socket_data);

  MockWrite final_writes[] = {MockWrite(kSimpleGetMockWrite)};
  MockRead final_reads[] = {MockRead("HTTP/1.1 200 OK\r\n"
                                     "Content-Length: 12\r\n\r\n"),
                            MockRead("Test Content")};
  StaticSocketDataProvider final_socket_data(
      final_reads, arraysize(final_reads), final_writes,
      arraysize(final_writes));
  socket_factory_.AddSocketDataProvider(&final_socket_data);

  TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context_->CreateRequest(GURL("http://www.redirect.com"), DEFAULT_PRIORITY,
                              &delegate, TRAFFIC_ANNOTATION_FOR_TESTS);

  request->Start();
  ASSERT_TRUE(request->is_pending());
  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(delegate.request_status(), IsOk());
  EXPECT_EQ(12, request->received_response_content_length());
  // Should not include the redirect.
  EXPECT_EQ(CountWriteBytes(final_writes, arraysize(final_writes)),
            request->GetTotalSentBytes());
  EXPECT_EQ(CountReadBytes(final_reads, arraysize(final_reads)),
            request->GetTotalReceivedBytes());
  // Should include the redirect as well as the final response.
  EXPECT_EQ(CountWriteBytes(redirect_writes, arraysize(redirect_writes)) +
                CountWriteBytes(final_writes, arraysize(final_writes)),
            network_delegate_.total_network_bytes_sent());
  EXPECT_EQ(CountReadBytes(redirect_reads, arraysize(redirect_reads)) +
                CountReadBytes(final_reads, arraysize(final_reads)),
            network_delegate_.total_network_bytes_received());
}

TEST_F(URLRequestHttpJobWithMockSocketsTest,
       TestNetworkBytesCancelledAfterHeaders) {
  MockWrite writes[] = {MockWrite(kSimpleGetMockWrite)};
  MockRead reads[] = {MockRead("HTTP/1.1 200 OK\r\n\r\n")};
  StaticSocketDataProvider socket_data(reads, arraysize(reads), writes,
                                       arraysize(writes));
  socket_factory_.AddSocketDataProvider(&socket_data);

  TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context_->CreateRequest(GURL("http://www.example.com"), DEFAULT_PRIORITY,
                              &delegate, TRAFFIC_ANNOTATION_FOR_TESTS);

  delegate.set_cancel_in_response_started(true);
  request->Start();
  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(delegate.request_status(), IsError(ERR_ABORTED));
  EXPECT_EQ(0, request->received_response_content_length());
  EXPECT_EQ(CountWriteBytes(writes, arraysize(writes)),
            request->GetTotalSentBytes());
  EXPECT_EQ(CountReadBytes(reads, arraysize(reads)),
            request->GetTotalReceivedBytes());
  EXPECT_EQ(CountWriteBytes(writes, arraysize(writes)),
            network_delegate_.total_network_bytes_sent());
  EXPECT_EQ(CountReadBytes(reads, arraysize(reads)),
            network_delegate_.total_network_bytes_received());
}

TEST_F(URLRequestHttpJobWithMockSocketsTest,
       TestNetworkBytesCancelledImmediately) {
  StaticSocketDataProvider socket_data(nullptr, 0, nullptr, 0);
  socket_factory_.AddSocketDataProvider(&socket_data);

  TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context_->CreateRequest(GURL("http://www.example.com"), DEFAULT_PRIORITY,
                              &delegate, TRAFFIC_ANNOTATION_FOR_TESTS);

  request->Start();
  request->Cancel();
  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(delegate.request_status(), IsError(ERR_ABORTED));
  EXPECT_EQ(0, request->received_response_content_length());
  EXPECT_EQ(0, request->GetTotalSentBytes());
  EXPECT_EQ(0, request->GetTotalReceivedBytes());
  EXPECT_EQ(0, network_delegate_.total_network_bytes_received());
}

TEST_F(URLRequestHttpJobWithMockSocketsTest, TestHttpTimeToFirstByte) {
  base::HistogramTester histograms;
  MockWrite writes[] = {MockWrite(kSimpleGetMockWrite)};
  MockRead reads[] = {MockRead("HTTP/1.1 200 OK\r\n"
                               "Content-Length: 12\r\n\r\n"),
                      MockRead("Test Content")};

  StaticSocketDataProvider socket_data(reads, arraysize(reads), writes,
                                       arraysize(writes));
  socket_factory_.AddSocketDataProvider(&socket_data);

  TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context_->CreateRequest(GURL("http://www.example.com"), DEFAULT_PRIORITY,
                              &delegate, TRAFFIC_ANNOTATION_FOR_TESTS);
  histograms.ExpectTotalCount("Net.HttpTimeToFirstByte", 0);

  request->Start();
  base::RunLoop().Run();

  EXPECT_THAT(delegate.request_status(), IsOk());
  histograms.ExpectTotalCount("Net.HttpTimeToFirstByte", 1);
}

TEST_F(URLRequestHttpJobWithMockSocketsTest,
       TestHttpTimeToFirstByteForCancelledTask) {
  base::HistogramTester histograms;
  MockWrite writes[] = {MockWrite(kSimpleGetMockWrite)};
  MockRead reads[] = {MockRead("HTTP/1.1 200 OK\r\n"
                               "Content-Length: 12\r\n\r\n"),
                      MockRead("Test Content")};

  StaticSocketDataProvider socket_data(reads, arraysize(reads), writes,
                                       arraysize(writes));
  socket_factory_.AddSocketDataProvider(&socket_data);

  TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context_->CreateRequest(GURL("http://www.example.com"), DEFAULT_PRIORITY,
                              &delegate, TRAFFIC_ANNOTATION_FOR_TESTS);

  request->Start();
  request->Cancel();
  base::RunLoop().Run();

  EXPECT_THAT(delegate.request_status(), IsError(ERR_ABORTED));
  histograms.ExpectTotalCount("Net.HttpTimeToFirstByte", 0);
}

TEST_F(URLRequestHttpJobWithMockSocketsTest,
       TestHttpJobSuccessPriorityKeyedTotalTime) {
  base::HistogramTester histograms;

  for (int priority = 0; priority < net::NUM_PRIORITIES; ++priority) {
    for (int request_index = 0; request_index <= priority; ++request_index) {
      MockWrite writes[] = {MockWrite(kSimpleGetMockWrite)};
      MockRead reads[] = {MockRead("HTTP/1.1 200 OK\r\n"
                                   "Content-Length: 12\r\n\r\n"),
                          MockRead("Test Content")};

      StaticSocketDataProvider socket_data(reads, arraysize(reads), writes,
                                           arraysize(writes));
      socket_factory_.AddSocketDataProvider(&socket_data);

      TestDelegate delegate;
      std::unique_ptr<URLRequest> request =
          context_->CreateRequest(GURL("http://www.example.com/"),
                                  static_cast<net::RequestPriority>(priority),
                                  &delegate, TRAFFIC_ANNOTATION_FOR_TESTS);

      request->Start();
      base::RunLoop().Run();
      EXPECT_THAT(delegate.request_status(), IsOk());
    }
  }

  for (int priority = 0; priority < net::NUM_PRIORITIES; ++priority) {
    histograms.ExpectTotalCount(
        "Net.HttpJob.TotalTimeSuccess.Priority" + base::IntToString(priority),
        priority + 1);
  }
}

TEST_F(URLRequestHttpJobTest, TestCancelWhileReadingCookies) {
  DelayedCookieMonster cookie_monster;
  TestURLRequestContext context(true);
  context.set_cookie_store(&cookie_monster);
  context.Init();

  TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context.CreateRequest(GURL("http://www.example.com"), DEFAULT_PRIORITY,
                            &delegate, TRAFFIC_ANNOTATION_FOR_TESTS);

  request->Start();
  request->Cancel();
  base::RunLoop().Run();

  EXPECT_THAT(delegate.request_status(), IsError(ERR_ABORTED));
}

// Make sure that SetPriority actually sets the URLRequestHttpJob's
// priority, before start.  Other tests handle the after start case.
TEST_F(URLRequestHttpJobTest, SetPriorityBasic) {
  auto job = std::make_unique<TestURLRequestHttpJob>(req_.get());
  EXPECT_EQ(DEFAULT_PRIORITY, job->priority());

  job->SetPriority(LOWEST);
  EXPECT_EQ(LOWEST, job->priority());

  job->SetPriority(LOW);
  EXPECT_EQ(LOW, job->priority());
}

// Make sure that URLRequestHttpJob passes on its priority to its
// transaction on start.
TEST_F(URLRequestHttpJobTest, SetTransactionPriorityOnStart) {
  test_job_interceptor_->set_main_intercept_job(
      std::make_unique<TestURLRequestHttpJob>(req_.get()));
  req_->SetPriority(LOW);

  EXPECT_FALSE(network_layer_.last_transaction());

  req_->Start();

  ASSERT_TRUE(network_layer_.last_transaction());
  EXPECT_EQ(LOW, network_layer_.last_transaction()->priority());
}

// Make sure that URLRequestHttpJob passes on its priority updates to
// its transaction.
TEST_F(URLRequestHttpJobTest, SetTransactionPriority) {
  test_job_interceptor_->set_main_intercept_job(
      std::make_unique<TestURLRequestHttpJob>(req_.get()));
  req_->SetPriority(LOW);
  req_->Start();
  ASSERT_TRUE(network_layer_.last_transaction());
  EXPECT_EQ(LOW, network_layer_.last_transaction()->priority());

  req_->SetPriority(HIGHEST);
  EXPECT_EQ(HIGHEST, network_layer_.last_transaction()->priority());
}

TEST_F(URLRequestHttpJobTest, HSTSInternalRedirectTest) {
  // Setup HSTS state.
  context_.transport_security_state()->AddHSTS(
      "upgrade.test", base::Time::Now() + base::TimeDelta::FromSeconds(10),
      true);
  ASSERT_TRUE(
      context_.transport_security_state()->ShouldUpgradeToSSL("upgrade.test"));
  ASSERT_FALSE(context_.transport_security_state()->ShouldUpgradeToSSL(
      "no-upgrade.test"));

  struct TestCase {
    const char* url;
    bool upgrade_expected;
    const char* url_expected;
  } cases[] = {
    {"http://upgrade.test/", true, "https://upgrade.test/"},
    {"http://upgrade.test:123/", true, "https://upgrade.test:123/"},
    {"http://no-upgrade.test/", false, "http://no-upgrade.test/"},
    {"http://no-upgrade.test:123/", false, "http://no-upgrade.test:123/"},
#if BUILDFLAG(ENABLE_WEBSOCKETS)
    {"ws://upgrade.test/", true, "wss://upgrade.test/"},
    {"ws://upgrade.test:123/", true, "wss://upgrade.test:123/"},
    {"ws://no-upgrade.test/", false, "ws://no-upgrade.test/"},
    {"ws://no-upgrade.test:123/", false, "ws://no-upgrade.test:123/"},
#endif  // BUILDFLAG(ENABLE_WEBSOCKETS)
  };

  for (const auto& test : cases) {
    SCOPED_TRACE(test.url);
    TestDelegate d;
    TestNetworkDelegate network_delegate;
    std::unique_ptr<URLRequest> r(context_.CreateRequest(
        GURL(test.url), DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));

    net_log_.Clear();
    r->Start();
    base::RunLoop().Run();

    if (test.upgrade_expected) {
      net::TestNetLogEntry::List entries;
      net_log_.GetEntries(&entries);
      int redirects = 0;
      for (const auto& entry : entries) {
        if (entry.type == net::NetLogEventType::URL_REQUEST_REDIRECT_JOB) {
          redirects++;
          std::string value;
          EXPECT_TRUE(entry.GetStringValue("reason", &value));
          EXPECT_EQ("HSTS", value);
        }
      }
      EXPECT_EQ(1, redirects);
      EXPECT_EQ(1, d.received_redirect_count());
      EXPECT_EQ(2u, r->url_chain().size());
    } else {
      EXPECT_EQ(0, d.received_redirect_count());
      EXPECT_EQ(1u, r->url_chain().size());
    }
    EXPECT_EQ(GURL(test.url_expected), r->url());
  }
}

class URLRequestHttpJobWithBrotliSupportTest : public ::testing::Test {
 protected:
  URLRequestHttpJobWithBrotliSupportTest()
      : context_(new TestURLRequestContext(true)) {
    auto params = std::make_unique<HttpNetworkSession::Params>();
    context_->set_enable_brotli(true);
    context_->set_http_network_session_params(std::move(params));
    context_->set_client_socket_factory(&socket_factory_);
    context_->Init();
  }

  MockClientSocketFactory socket_factory_;
  std::unique_ptr<TestURLRequestContext> context_;
};

TEST_F(URLRequestHttpJobWithBrotliSupportTest, NoBrotliAdvertisementOverHttp) {
  MockWrite writes[] = {MockWrite(kSimpleGetMockWrite)};
  MockRead reads[] = {MockRead("HTTP/1.1 200 OK\r\n"
                               "Content-Length: 12\r\n\r\n"),
                      MockRead("Test Content")};
  StaticSocketDataProvider socket_data(reads, arraysize(reads), writes,
                                       arraysize(writes));
  socket_factory_.AddSocketDataProvider(&socket_data);

  TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context_->CreateRequest(GURL("http://www.example.com"), DEFAULT_PRIORITY,
                              &delegate, TRAFFIC_ANNOTATION_FOR_TESTS);
  request->Start();
  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(delegate.request_status(), IsOk());
  EXPECT_EQ(12, request->received_response_content_length());
  EXPECT_EQ(CountWriteBytes(writes, arraysize(writes)),
            request->GetTotalSentBytes());
  EXPECT_EQ(CountReadBytes(reads, arraysize(reads)),
            request->GetTotalReceivedBytes());
}

TEST_F(URLRequestHttpJobWithBrotliSupportTest, BrotliAdvertisement) {
  net::SSLSocketDataProvider ssl_socket_data_provider(net::ASYNC, net::OK);
  ssl_socket_data_provider.next_proto = kProtoHTTP11;
  ssl_socket_data_provider.cert =
      ImportCertFromFile(GetTestCertsDirectory(), "unittest.selfsigned.der");
  socket_factory_.AddSSLSocketDataProvider(&ssl_socket_data_provider);

  MockWrite writes[] = {
      MockWrite("GET / HTTP/1.1\r\n"
                "Host: www.example.com\r\n"
                "Connection: keep-alive\r\n"
                "User-Agent:\r\n"
                "Accept-Encoding: gzip, deflate, br\r\n"
                "Accept-Language: en-us,fr\r\n\r\n")};
  MockRead reads[] = {MockRead("HTTP/1.1 200 OK\r\n"
                               "Content-Length: 12\r\n\r\n"),
                      MockRead("Test Content")};
  StaticSocketDataProvider socket_data(reads, arraysize(reads), writes,
                                       arraysize(writes));
  socket_factory_.AddSocketDataProvider(&socket_data);

  TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context_->CreateRequest(GURL("https://www.example.com"), DEFAULT_PRIORITY,
                              &delegate, TRAFFIC_ANNOTATION_FOR_TESTS);
  request->Start();
  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(delegate.request_status(), IsOk());
  EXPECT_EQ(12, request->received_response_content_length());
  EXPECT_EQ(CountWriteBytes(writes, arraysize(writes)),
            request->GetTotalSentBytes());
  EXPECT_EQ(CountReadBytes(reads, arraysize(reads)),
            request->GetTotalReceivedBytes());
}

#if defined(OS_ANDROID)
TEST_F(URLRequestHttpJobTest, AndroidCleartextPermittedTest) {
  context_.set_check_cleartext_permitted(true);

  struct TestCase {
    const char* url;
    bool cleartext_permitted;
    bool should_block;
  } cases[] = {
      {"http://blocked.test/", true, false},
      {"https://blocked.test/", true, false},
      {"http://blocked.test/", false, true},
      {"https://blocked.test/", false, false},
  };

  for (const TestCase& test : cases) {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_AndroidNetworkLibraryTestUtil_setUpSecurityPolicyForTesting(
        env, test.cleartext_permitted);

    TestDelegate delegate;
    std::unique_ptr<URLRequest> request =
        context_.CreateRequest(GURL(test.url), DEFAULT_PRIORITY, &delegate,
                               TRAFFIC_ANNOTATION_FOR_TESTS);
    request->Start();
    base::RunLoop().Run();

    int sdk_int = base::android::BuildInfo::GetInstance()->sdk_int();
    bool expect_blocked = (sdk_int >= base::android::SDK_VERSION_MARSHMALLOW &&
                           test.should_block);
    if (expect_blocked) {
      EXPECT_THAT(delegate.request_status(),
                  IsError(ERR_CLEARTEXT_NOT_PERMITTED));
    } else {
      // Should fail since there's no test server running
      EXPECT_THAT(delegate.request_status(), IsError(ERR_FAILED));
    }
  }
}
#endif

// This base class just serves to set up some things before the TestURLRequest
// constructor is called.
class URLRequestHttpJobWebSocketTestBase : public ::testing::Test {
 protected:
  URLRequestHttpJobWebSocketTestBase() : socket_data_(nullptr, 0, nullptr, 0),
                                         context_(true) {
    // A Network Delegate is required for the WebSocketHandshakeStreamBase
    // object to be passed on to the HttpNetworkTransaction.
    context_.set_network_delegate(&network_delegate_);

    // Attempting to create real ClientSocketHandles is not going to work out so
    // well. Set up a fake socket factory.
    socket_factory_.AddSocketDataProvider(&socket_data_);
    context_.set_client_socket_factory(&socket_factory_);
    context_.Init();
  }

  StaticSocketDataProvider socket_data_;
  TestNetworkDelegate network_delegate_;
  MockClientSocketFactory socket_factory_;
  TestURLRequestContext context_;
};

class URLRequestHttpJobWebSocketTest
    : public URLRequestHttpJobWebSocketTestBase {
 protected:
  URLRequestHttpJobWebSocketTest()
      : req_(context_.CreateRequest(GURL("ws://www.example.com"),
                                    DEFAULT_PRIORITY,
                                    &delegate_,
                                    TRAFFIC_ANNOTATION_FOR_TESTS)) {}

  TestDelegate delegate_;
  std::unique_ptr<URLRequest> req_;
};

class MockCreateHelper : public WebSocketHandshakeStreamBase::CreateHelper {
 public:
  // GoogleMock does not appear to play nicely with move-only types like
  // std::unique_ptr, so this forwarding method acts as a workaround.
  std::unique_ptr<WebSocketHandshakeStreamBase> CreateBasicStream(
      std::unique_ptr<ClientSocketHandle> connection,
      bool using_proxy) override {
    // Discard the arguments since we don't need them anyway.
    return base::WrapUnique(CreateBasicStreamMock());
  }

  MOCK_METHOD0(CreateBasicStreamMock,
               WebSocketHandshakeStreamBase*());
};

#if BUILDFLAG(ENABLE_WEBSOCKETS)

class FakeWebSocketHandshakeStream : public WebSocketHandshakeStreamBase {
 public:
  FakeWebSocketHandshakeStream() : initialize_stream_was_called_(false) {}

  bool initialize_stream_was_called() const {
    return initialize_stream_was_called_;
  }

  // Fake implementation of HttpStreamBase methods.
  int InitializeStream(const HttpRequestInfo* request_info,
                       RequestPriority priority,
                       const NetLogWithSource& net_log,
                       const CompletionCallback& callback) override {
    initialize_stream_was_called_ = true;
    return ERR_IO_PENDING;
  }

  int SendRequest(const HttpRequestHeaders& request_headers,
                  HttpResponseInfo* response,
                  const CompletionCallback& callback) override {
    return ERR_IO_PENDING;
  }

  int ReadResponseHeaders(const CompletionCallback& callback) override {
    return ERR_IO_PENDING;
  }

  int ReadResponseBody(IOBuffer* buf,
                       int buf_len,
                       const CompletionCallback& callback) override {
    return ERR_IO_PENDING;
  }

  void Close(bool not_reusable) override {}

  bool IsResponseBodyComplete() const override { return false; }

  bool IsConnectionReused() const override { return false; }
  void SetConnectionReused() override {}

  bool CanReuseConnection() const override { return false; }

  int64_t GetTotalReceivedBytes() const override { return 0; }
  int64_t GetTotalSentBytes() const override { return 0; }

  bool GetLoadTimingInfo(LoadTimingInfo* load_timing_info) const override {
    return false;
  }

  bool GetAlternativeService(
      AlternativeService* alternative_service) const override {
    return false;
  }

  void GetSSLInfo(SSLInfo* ssl_info) override {}

  void GetSSLCertRequestInfo(SSLCertRequestInfo* cert_request_info) override {}

  bool GetRemoteEndpoint(IPEndPoint* endpoint) override { return false; }

  Error GetTokenBindingSignature(crypto::ECPrivateKey* key,
                                 TokenBindingType tb_type,
                                 std::vector<uint8_t>* out) override {
    ADD_FAILURE();
    return ERR_NOT_IMPLEMENTED;
  }

  void Drain(HttpNetworkSession* session) override {}

  void PopulateNetErrorDetails(NetErrorDetails* details) override { return; }

  void SetPriority(RequestPriority priority) override {}

  HttpStream* RenewStreamForAuth() override { return nullptr; }

  // Fake implementation of WebSocketHandshakeStreamBase method(s)
  std::unique_ptr<WebSocketStream> Upgrade() override {
    return std::unique_ptr<WebSocketStream>();
  }

 private:
  bool initialize_stream_was_called_;
};

TEST_F(URLRequestHttpJobWebSocketTest, RejectedWithoutCreateHelper) {
  req_->Start();
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(delegate_.request_status(), IsError(ERR_DISALLOWED_URL_SCHEME));
}

TEST_F(URLRequestHttpJobWebSocketTest, CreateHelperPassedThrough) {
  std::unique_ptr<MockCreateHelper> create_helper =
      std::make_unique<::testing::StrictMock<MockCreateHelper>>();
  FakeWebSocketHandshakeStream* fake_handshake_stream(
      new FakeWebSocketHandshakeStream);
  // Ownership of fake_handshake_stream is transferred when CreateBasicStream()
  // is called.
  EXPECT_CALL(*create_helper, CreateBasicStreamMock())
      .WillOnce(Return(fake_handshake_stream));
  req_->SetUserData(WebSocketHandshakeStreamBase::CreateHelper::DataKey(),
                    std::move(create_helper));
  req_->SetLoadFlags(LOAD_DISABLE_CACHE);
  req_->Start();
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(delegate_.request_status(), IsError(ERR_IO_PENDING));
  EXPECT_TRUE(fake_handshake_stream->initialize_stream_was_called());
}

#endif  // BUILDFLAG(ENABLE_WEBSOCKETS)

}  // namespace

}  // namespace net
