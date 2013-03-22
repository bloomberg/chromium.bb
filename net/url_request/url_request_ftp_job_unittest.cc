// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_ftp_job.h"

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_vector.h"
#include "base/run_loop.h"
#include "googleurl/src/gurl.h"
#include "net/http/http_transaction_unittest.h"
#include "net/proxy/proxy_config_service.h"
#include "net/socket/socket_test_util.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_status.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

class SimpleProxyConfigService : public ProxyConfigService {
 public:
  SimpleProxyConfigService() {
    // Any FTP requests that ever go through HTTP paths are proxied requests.
    config_.proxy_rules().ParseFromString("ftp=localhost");
  }

  virtual void AddObserver(Observer* observer) OVERRIDE {
    observer_ = observer;
  }

  virtual void RemoveObserver(Observer* observer) OVERRIDE {
    if (observer_ == observer) {
      observer_ = NULL;
    }
  }

  virtual ConfigAvailability GetLatestProxyConfig(
      ProxyConfig* config) OVERRIDE {
    *config = config_;
    return CONFIG_VALID;
  }

  void IncrementConfigId() {
    config_.set_id(config_.id() + 1);
    observer_->OnProxyConfigChanged(config_, ProxyConfigService::CONFIG_VALID);
  }

 private:
  ProxyConfig config_;
  Observer* observer_;
};

// Inherit from URLRequestFtpJob to expose the priority and some
// other hidden functions.
class TestURLRequestFtpJob : public URLRequestFtpJob {
 public:
  explicit TestURLRequestFtpJob(URLRequest* request)
      : URLRequestFtpJob(request, NULL,
                         request->context()->ftp_transaction_factory(),
                         request->context()->ftp_auth_cache()) {}

  using URLRequestFtpJob::SetPriority;
  using URLRequestFtpJob::Start;
  using URLRequestFtpJob::Kill;
  using URLRequestFtpJob::priority;

 protected:
  virtual ~TestURLRequestFtpJob() {}
};

// Fixture for priority-related tests. Priority matters when there is
// an HTTP proxy.
class URLRequestFtpJobPriorityTest : public testing::Test {
 protected:
  URLRequestFtpJobPriorityTest()
      : req_(GURL("ftp://ftp.example.com"), &delegate_, &context_, NULL) {
    context_.set_proxy_service(
        new ProxyService(new SimpleProxyConfigService, NULL, NULL));
    context_.set_http_transaction_factory(&network_layer_);
  }

  MockNetworkLayer network_layer_;
  TestURLRequestContext context_;
  TestDelegate delegate_;
  TestURLRequest req_;
};

// Make sure that SetPriority actually sets the URLRequestFtpJob's
// priority, both before and after start.
TEST_F(URLRequestFtpJobPriorityTest, SetPriorityBasic) {
  scoped_refptr<TestURLRequestFtpJob> job(new TestURLRequestFtpJob(&req_));
  EXPECT_EQ(DEFAULT_PRIORITY, job->priority());

  job->SetPriority(LOWEST);
  EXPECT_EQ(LOWEST, job->priority());

  job->SetPriority(LOW);
  EXPECT_EQ(LOW, job->priority());

  job->Start();
  EXPECT_EQ(LOW, job->priority());

  job->SetPriority(MEDIUM);
  EXPECT_EQ(MEDIUM, job->priority());
}

// Make sure that URLRequestFtpJob passes on its priority to its
// transaction on start.
TEST_F(URLRequestFtpJobPriorityTest, SetTransactionPriorityOnStart) {
  scoped_refptr<TestURLRequestFtpJob> job(new TestURLRequestFtpJob(&req_));
  job->SetPriority(LOW);

  EXPECT_FALSE(network_layer_.last_transaction());

  job->Start();

  ASSERT_TRUE(network_layer_.last_transaction());
  EXPECT_EQ(LOW, network_layer_.last_transaction()->priority());
}

// Make sure that URLRequestFtpJob passes on its priority updates to
// its transaction.
TEST_F(URLRequestFtpJobPriorityTest, SetTransactionPriority) {
  scoped_refptr<TestURLRequestFtpJob> job(new TestURLRequestFtpJob(&req_));
  job->SetPriority(LOW);
  job->Start();
  ASSERT_TRUE(network_layer_.last_transaction());
  EXPECT_EQ(LOW, network_layer_.last_transaction()->priority());

  job->SetPriority(HIGHEST);
  EXPECT_EQ(HIGHEST, network_layer_.last_transaction()->priority());
}

// Make sure that URLRequestFtpJob passes on its priority updates to
// newly-created transactions after the first one.
TEST_F(URLRequestFtpJobPriorityTest, SetSubsequentTransactionPriority) {
  scoped_refptr<TestURLRequestFtpJob> job(new TestURLRequestFtpJob(&req_));
  job->Start();

  job->SetPriority(LOW);
  ASSERT_TRUE(network_layer_.last_transaction());
  EXPECT_EQ(LOW, network_layer_.last_transaction()->priority());

  job->Kill();
  network_layer_.ClearLastTransaction();

  // Creates a second transaction.
  job->Start();
  ASSERT_TRUE(network_layer_.last_transaction());
  EXPECT_EQ(LOW, network_layer_.last_transaction()->priority());
}

class FtpTestURLRequestContext : public TestURLRequestContext {
 public:
  FtpTestURLRequestContext(ClientSocketFactory* socket_factory,
                           ProxyService* proxy_service,
                           NetworkDelegate* network_delegate)
      : TestURLRequestContext(true) {
    set_client_socket_factory(socket_factory);
    context_storage_.set_proxy_service(proxy_service);
    set_network_delegate(network_delegate);
    Init();
  }
};

class URLRequestFtpJobTest : public testing::Test {
 public:
  URLRequestFtpJobTest()
      : proxy_service_(new ProxyService(
                           new SimpleProxyConfigService, NULL, NULL)),
        request_context_(&socket_factory_,
                         proxy_service_,
                         &network_delegate_) {
  }

  virtual ~URLRequestFtpJobTest() {
    // Clean up any remaining tasks that mess up unrelated tests.
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }

  void AddSocket(MockRead* reads, size_t reads_size,
                 MockWrite* writes, size_t writes_size) {
    DeterministicSocketData* socket_data = new DeterministicSocketData(
        reads, reads_size, writes, writes_size);
    socket_data->set_connect_data(MockConnect(SYNCHRONOUS, OK));
    socket_data->StopAfter(reads_size + writes_size - 1);
    socket_factory_.AddSocketDataProvider(socket_data);

    socket_data_.push_back(socket_data);
  }

  URLRequestContext* request_context() { return &request_context_; }
  TestNetworkDelegate* network_delegate() { return &network_delegate_; }
  DeterministicSocketData* socket_data(size_t index) {
    return socket_data_[index];
  }

 private:
  ScopedVector<DeterministicSocketData> socket_data_;
  DeterministicMockClientSocketFactory socket_factory_;
  TestNetworkDelegate network_delegate_;

  // Owned by |request_context_|:
  ProxyService* proxy_service_;

  FtpTestURLRequestContext request_context_;
};

TEST_F(URLRequestFtpJobTest, FtpProxyRequest) {
  MockWrite writes[] = {
    MockWrite(ASYNC, 0, "GET ftp://ftp.example.com/ HTTP/1.1\r\n"
              "Host: ftp.example.com\r\n"
              "Proxy-Connection: keep-alive\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(ASYNC, 1, "HTTP/1.1 200 OK\r\n"),
    MockRead(ASYNC, 2, "Content-Length: 9\r\n\r\n"),
    MockRead(ASYNC, 3, "test.html"),
  };

  AddSocket(reads, arraysize(reads), writes, arraysize(writes));

  TestDelegate request_delegate;
  URLRequest url_request(GURL("ftp://ftp.example.com/"),
                         &request_delegate,
                         request_context(),
                         network_delegate());
  url_request.Start();
  ASSERT_TRUE(url_request.is_pending());
  socket_data(0)->RunFor(4);

  EXPECT_TRUE(url_request.status().is_success());
  EXPECT_EQ(1, network_delegate()->completed_requests());
  EXPECT_EQ(0, network_delegate()->error_count());
  EXPECT_FALSE(request_delegate.auth_required_called());
  EXPECT_EQ("test.html", request_delegate.data_received());
}

TEST_F(URLRequestFtpJobTest, FtpProxyRequestNeedAuth) {
  MockWrite writes[] = {
    MockWrite(ASYNC, 0, "GET ftp://ftp.example.com/ HTTP/1.1\r\n"
              "Host: ftp.example.com\r\n"
              "Proxy-Connection: keep-alive\r\n\r\n"),
  };
  MockRead reads[] = {
    // No credentials.
    MockRead(ASYNC, 1, "HTTP/1.1 407 Proxy Authentication Required\r\n"),
    MockRead(ASYNC, 2, "Proxy-Authenticate: Basic "
             "realm=\"MyRealm1\"\r\n"),
    MockRead(ASYNC, 3, "Content-Length: 9\r\n\r\n"),
    MockRead(ASYNC, 4, "test.html"),
  };

  AddSocket(reads, arraysize(reads), writes, arraysize(writes));

  TestDelegate request_delegate;
  URLRequest url_request(GURL("ftp://ftp.example.com/"),
                         &request_delegate,
                         request_context(),
                         network_delegate());
  url_request.Start();
  ASSERT_TRUE(url_request.is_pending());
  socket_data(0)->RunFor(5);

  EXPECT_TRUE(url_request.status().is_success());
  EXPECT_EQ(1, network_delegate()->completed_requests());
  EXPECT_EQ(0, network_delegate()->error_count());
  EXPECT_FALSE(request_delegate.auth_required_called());
  EXPECT_EQ("test.html", request_delegate.data_received());
}

TEST_F(URLRequestFtpJobTest, FtpProxyRequestDoNotSaveCookies) {
  MockWrite writes[] = {
    MockWrite(ASYNC, 0, "GET ftp://ftp.example.com/ HTTP/1.1\r\n"
              "Host: ftp.example.com\r\n"
              "Proxy-Connection: keep-alive\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(ASYNC, 1, "HTTP/1.1 200 OK\r\n"),
    MockRead(ASYNC, 2, "Content-Length: 9\r\n"),
    MockRead(ASYNC, 3, "Set-Cookie: name=value\r\n\r\n"),
    MockRead(ASYNC, 4, "test.html"),
  };

  AddSocket(reads, arraysize(reads), writes, arraysize(writes));

  TestDelegate request_delegate;
  URLRequest url_request(GURL("ftp://ftp.example.com/"),
                         &request_delegate,
                         request_context(),
                         network_delegate());
  url_request.Start();
  ASSERT_TRUE(url_request.is_pending());

  socket_data(0)->RunFor(5);

  EXPECT_TRUE(url_request.status().is_success());
  EXPECT_EQ(1, network_delegate()->completed_requests());
  EXPECT_EQ(0, network_delegate()->error_count());

  // Make sure we do not accept cookies.
  EXPECT_EQ(0, network_delegate()->set_cookie_count());

  EXPECT_FALSE(request_delegate.auth_required_called());
  EXPECT_EQ("test.html", request_delegate.data_received());
}

TEST_F(URLRequestFtpJobTest, FtpProxyRequestDoNotFollowRedirects) {
  MockWrite writes[] = {
    MockWrite(SYNCHRONOUS, 0, "GET ftp://ftp.example.com/ HTTP/1.1\r\n"
              "Host: ftp.example.com\r\n"
              "Proxy-Connection: keep-alive\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(SYNCHRONOUS, 1, "HTTP/1.1 302 Found\r\n"),
    MockRead(ASYNC, 2, "Location: http://other.example.com/\r\n\r\n"),
  };

  AddSocket(reads, arraysize(reads), writes, arraysize(writes));

  TestDelegate request_delegate;
  URLRequest url_request(GURL("ftp://ftp.example.com/"),
                         &request_delegate,
                         request_context(),
                         network_delegate());
  url_request.Start();
  EXPECT_TRUE(url_request.is_pending());

  MessageLoop::current()->RunUntilIdle();

  EXPECT_TRUE(url_request.is_pending());
  EXPECT_EQ(0, request_delegate.response_started_count());
  EXPECT_EQ(0, network_delegate()->error_count());
  ASSERT_TRUE(url_request.status().is_success());

  socket_data(0)->RunFor(1);

  EXPECT_EQ(1, network_delegate()->completed_requests());
  EXPECT_EQ(1, network_delegate()->error_count());
  EXPECT_FALSE(url_request.status().is_success());
  EXPECT_EQ(ERR_UNSAFE_REDIRECT, url_request.status().error());
}

// We should re-use socket for requests using the same scheme, host, and port.
TEST_F(URLRequestFtpJobTest, FtpProxyRequestReuseSocket) {
  MockWrite writes[] = {
    MockWrite(ASYNC, 0, "GET ftp://ftp.example.com/first HTTP/1.1\r\n"
              "Host: ftp.example.com\r\n"
              "Proxy-Connection: keep-alive\r\n\r\n"),
    MockWrite(ASYNC, 4, "GET ftp://ftp.example.com/second HTTP/1.1\r\n"
              "Host: ftp.example.com\r\n"
              "Proxy-Connection: keep-alive\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(ASYNC, 1, "HTTP/1.1 200 OK\r\n"),
    MockRead(ASYNC, 2, "Content-Length: 10\r\n\r\n"),
    MockRead(ASYNC, 3, "test1.html"),
    MockRead(ASYNC, 5, "HTTP/1.1 200 OK\r\n"),
    MockRead(ASYNC, 6, "Content-Length: 10\r\n\r\n"),
    MockRead(ASYNC, 7, "test2.html"),
  };

  AddSocket(reads, arraysize(reads), writes, arraysize(writes));

  TestDelegate request_delegate1;
  URLRequest url_request1(GURL("ftp://ftp.example.com/first"),
                          &request_delegate1,
                          request_context(),
                          network_delegate());
  url_request1.Start();
  ASSERT_TRUE(url_request1.is_pending());
  socket_data(0)->RunFor(4);

  EXPECT_TRUE(url_request1.status().is_success());
  EXPECT_EQ(1, network_delegate()->completed_requests());
  EXPECT_EQ(0, network_delegate()->error_count());
  EXPECT_FALSE(request_delegate1.auth_required_called());
  EXPECT_EQ("test1.html", request_delegate1.data_received());

  TestDelegate request_delegate2;
  URLRequest url_request2(GURL("ftp://ftp.example.com/second"),
                          &request_delegate2,
                          request_context(),
                          network_delegate());
  url_request2.Start();
  ASSERT_TRUE(url_request2.is_pending());
  socket_data(0)->RunFor(4);

  EXPECT_TRUE(url_request2.status().is_success());
  EXPECT_EQ(2, network_delegate()->completed_requests());
  EXPECT_EQ(0, network_delegate()->error_count());
  EXPECT_FALSE(request_delegate2.auth_required_called());
  EXPECT_EQ("test2.html", request_delegate2.data_received());
}

// We should not re-use socket when there are two requests to the same host,
// but one is FTP and the other is HTTP.
TEST_F(URLRequestFtpJobTest, FtpProxyRequestDoNotReuseSocket) {
  MockWrite writes1[] = {
    MockWrite(ASYNC, 0, "GET ftp://ftp.example.com/first HTTP/1.1\r\n"
              "Host: ftp.example.com\r\n"
              "Proxy-Connection: keep-alive\r\n\r\n"),
  };
  MockWrite writes2[] = {
    MockWrite(ASYNC, 0, "GET /second HTTP/1.1\r\n"
              "Host: ftp.example.com\r\n"
              "Connection: keep-alive\r\n"
              "User-Agent:\r\n"
              "Accept-Encoding: gzip,deflate\r\n"
              "Accept-Language: en-us,fr\r\n\r\n"),
  };
  MockRead reads1[] = {
    MockRead(ASYNC, 1, "HTTP/1.1 200 OK\r\n"),
    MockRead(ASYNC, 2, "Content-Length: 10\r\n\r\n"),
    MockRead(ASYNC, 3, "test1.html"),
  };
  MockRead reads2[] = {
    MockRead(ASYNC, 1, "HTTP/1.1 200 OK\r\n"),
    MockRead(ASYNC, 2, "Content-Length: 10\r\n\r\n"),
    MockRead(ASYNC, 3, "test2.html"),
  };

  AddSocket(reads1, arraysize(reads1), writes1, arraysize(writes1));
  AddSocket(reads2, arraysize(reads2), writes2, arraysize(writes2));

  TestDelegate request_delegate1;
  URLRequest url_request1(GURL("ftp://ftp.example.com/first"),
                          &request_delegate1,
                          request_context(),
                          network_delegate());
  url_request1.Start();
  ASSERT_TRUE(url_request1.is_pending());
  socket_data(0)->RunFor(4);

  EXPECT_TRUE(url_request1.status().is_success());
  EXPECT_EQ(1, network_delegate()->completed_requests());
  EXPECT_EQ(0, network_delegate()->error_count());
  EXPECT_FALSE(request_delegate1.auth_required_called());
  EXPECT_EQ("test1.html", request_delegate1.data_received());

  TestDelegate request_delegate2;
  URLRequest url_request2(GURL("http://ftp.example.com/second"),
                          &request_delegate2,
                          request_context(),
                          network_delegate());
  url_request2.Start();
  ASSERT_TRUE(url_request2.is_pending());
  socket_data(1)->RunFor(4);

  EXPECT_TRUE(url_request2.status().is_success());
  EXPECT_EQ(2, network_delegate()->completed_requests());
  EXPECT_EQ(0, network_delegate()->error_count());
  EXPECT_FALSE(request_delegate2.auth_required_called());
  EXPECT_EQ("test2.html", request_delegate2.data_received());
}

}  // namespace

}  // namespace net
