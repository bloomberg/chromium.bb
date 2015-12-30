// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_job.h"

#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "net/base/request_priority.h"
#include "net/http/http_transaction_test_util.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

// Data encoded in kBrotliHelloData.
const char kBrotliDecodedHelloData[] = "hello, world!\n";
// kBrotliDecodedHelloData encoded with brotli.
const char kBrotliHelloData[] =
    "\033\015\0\0\244\024\102\152\020\111\152\072\235\126\034";

// This is a header that signals the end of the data.
const char kGzipData[] = "\x1f\x08b\x08\0\0\0\0\0\0\3\3\0\0\0\0\0\0\0\0";
const char kGzipDataWithName[] =
    "\x1f\x08b\x08\x08\0\0\0\0\0\0name\0\3\0\0\0\0\0\0\0\0";
// Gzip data that contains the word hello with a newline character.
const char kGzipHelloData[] =
    "\x1f\x8b\x08\x08\x46\x7d\x4e\x56\x00\x03\x67\x7a\x69\x70\x2e\x74\x78\x74"
    "\x00\xcb\x48\xcd\xc9\xc9\xe7\x02\x00\x20\x30\x3a\x36\x06\x00\x00\x00";

void GZipServer(const HttpRequestInfo* request,
                std::string* response_status,
                std::string* response_headers,
                std::string* response_data) {
  response_data->assign(kGzipData, sizeof(kGzipData));
}

void GZipHelloServer(const HttpRequestInfo* request,
                     std::string* response_status,
                     std::string* response_headers,
                     std::string* response_data) {
  response_data->assign(kGzipHelloData, sizeof(kGzipHelloData));
}

void BigGZipServer(const HttpRequestInfo* request,
                   std::string* response_status,
                   std::string* response_headers,
                   std::string* response_data) {
  response_data->assign(kGzipDataWithName, sizeof(kGzipDataWithName));
  response_data->insert(10, 64 * 1024, 'a');
}

void BrotliHelloServer(const HttpRequestInfo* request,
                       std::string* response_status,
                       std::string* response_headers,
                       std::string* response_data) {
  response_data->assign(kBrotliHelloData, sizeof(kBrotliHelloData) - 1);
}

const MockTransaction kGZip_Transaction = {
    "http://www.google.com/gzyp",
    "GET",
    base::Time(),
    "",
    LOAD_NORMAL,
    "HTTP/1.1 200 OK",
    "Cache-Control: max-age=10000\n"
    "Content-Encoding: gzip\n"
    "Content-Length: 30\n",  // Intentionally wrong.
    base::Time(),
    "",
    TEST_MODE_NORMAL,
    &GZipServer,
    nullptr,
    0,
    0,
    OK,
};

const MockTransaction kGzip_Slow_Transaction = {
    "http://www.google.com/gzyp", "GET", base::Time(), "", LOAD_NORMAL,
    "HTTP/1.1 200 OK",
    "Cache-Control: max-age=10000\n"
    "Content-Encoding: gzip\n",
    base::Time(), "", TEST_MODE_SLOW_READ, &GZipHelloServer, nullptr, 0, 0, OK,
};

const MockTransaction kRedirect_Transaction = {
    "http://www.google.com/redirect",
    "GET",
    base::Time(),
    "",
    LOAD_NORMAL,
    "HTTP/1.1 302 Found",
    "Cache-Control: max-age=10000\n"
    "Location: http://www.google.com/destination\n"
    "Content-Length: 5\n",
    base::Time(),
    "hello",
    TEST_MODE_NORMAL,
    nullptr,
    nullptr,
    0,
    0,
    OK,
};

const MockTransaction kEmptyBodyGzip_Transaction = {
    "http://www.google.com/empty_body",
    "GET",
    base::Time(),
    "",
    LOAD_NORMAL,
    "HTTP/1.1 200 OK",
    "Content-Encoding: gzip\n",
    base::Time(),
    "",
    TEST_MODE_NORMAL,
    nullptr,
    nullptr,
    0,
    0,
    OK,
};

const MockTransaction kBrotli_Slow_Transaction = {
    "http://www.google.com/brotli", "GET", base::Time(), "", LOAD_NORMAL,
    "HTTP/1.1 200 OK",
    "Cache-Control: max-age=10000\n"
    "Content-Encoding: br\n",
    base::Time(), "", TEST_MODE_SLOW_READ, &BrotliHelloServer, nullptr, 0, 0,
    OK,
};

}  // namespace

TEST(URLRequestJob, TransactionNotifiedWhenDone) {
  MockNetworkLayer network_layer;
  TestURLRequestContext context;
  context.set_http_transaction_factory(&network_layer);

  TestDelegate d;
  scoped_ptr<URLRequest> req(
      context.CreateRequest(GURL(kGZip_Transaction.url), DEFAULT_PRIORITY, &d));
  AddMockTransaction(&kGZip_Transaction);

  req->set_method("GET");
  req->Start();

  base::MessageLoop::current()->Run();

  EXPECT_TRUE(network_layer.done_reading_called());

  RemoveMockTransaction(&kGZip_Transaction);
}

TEST(URLRequestJob, SyncTransactionNotifiedWhenDone) {
  MockNetworkLayer network_layer;
  TestURLRequestContext context;
  context.set_http_transaction_factory(&network_layer);

  TestDelegate d;
  scoped_ptr<URLRequest> req(
      context.CreateRequest(GURL(kGZip_Transaction.url), DEFAULT_PRIORITY, &d));
  MockTransaction transaction(kGZip_Transaction);
  transaction.test_mode = TEST_MODE_SYNC_ALL;
  AddMockTransaction(&transaction);

  req->set_method("GET");
  req->Start();

  base::RunLoop().Run();

  EXPECT_TRUE(network_layer.done_reading_called());

  RemoveMockTransaction(&transaction);
}

// Tests processing a large gzip header one byte at a time.
TEST(URLRequestJob, SyncSlowTransaction) {
  MockNetworkLayer network_layer;
  TestURLRequestContext context;
  context.set_http_transaction_factory(&network_layer);

  TestDelegate d;
  scoped_ptr<URLRequest> req(
      context.CreateRequest(GURL(kGZip_Transaction.url), DEFAULT_PRIORITY, &d));
  MockTransaction transaction(kGZip_Transaction);
  transaction.test_mode = TEST_MODE_SYNC_ALL | TEST_MODE_SLOW_READ;
  transaction.handler = &BigGZipServer;
  AddMockTransaction(&transaction);

  req->set_method("GET");
  req->Start();

  base::RunLoop().Run();

  EXPECT_TRUE(network_layer.done_reading_called());

  RemoveMockTransaction(&transaction);
}

TEST(URLRequestJob, RedirectTransactionNotifiedWhenDone) {
  MockNetworkLayer network_layer;
  TestURLRequestContext context;
  context.set_http_transaction_factory(&network_layer);

  TestDelegate d;
  scoped_ptr<URLRequest> req(context.CreateRequest(
      GURL(kRedirect_Transaction.url), DEFAULT_PRIORITY, &d));
  AddMockTransaction(&kRedirect_Transaction);

  req->set_method("GET");
  req->Start();

  base::RunLoop().Run();

  EXPECT_TRUE(network_layer.done_reading_called());

  RemoveMockTransaction(&kRedirect_Transaction);
}

TEST(URLRequestJob, TransactionNotCachedWhenNetworkDelegateRedirects) {
  MockNetworkLayer network_layer;
  TestNetworkDelegate network_delegate;
  network_delegate.set_redirect_on_headers_received_url(GURL("http://foo"));
  TestURLRequestContext context;
  context.set_http_transaction_factory(&network_layer);
  context.set_network_delegate(&network_delegate);

  TestDelegate d;
  scoped_ptr<URLRequest> req(
      context.CreateRequest(GURL(kGZip_Transaction.url), DEFAULT_PRIORITY, &d));
  AddMockTransaction(&kGZip_Transaction);

  req->set_method("GET");
  req->Start();

  base::RunLoop().Run();

  EXPECT_TRUE(network_layer.stop_caching_called());

  RemoveMockTransaction(&kGZip_Transaction);
}

// Makes sure that ReadRawDataComplete correctly updates request status before
// calling ReadFilteredData.
// Regression test for crbug.com/553300.
TEST(URLRequestJob, EmptyBodySkipFilter) {
  MockNetworkLayer network_layer;
  TestURLRequestContext context;
  context.set_http_transaction_factory(&network_layer);

  TestDelegate d;
  scoped_ptr<URLRequest> req(context.CreateRequest(
      GURL(kEmptyBodyGzip_Transaction.url), DEFAULT_PRIORITY, &d));
  AddMockTransaction(&kEmptyBodyGzip_Transaction);

  req->set_method("GET");
  req->Start();

  base::MessageLoop::current()->Run();

  EXPECT_FALSE(d.request_failed());
  EXPECT_EQ(200, req->GetResponseCode());
  EXPECT_TRUE(d.data_received().empty());
  EXPECT_TRUE(network_layer.done_reading_called());

  RemoveMockTransaction(&kEmptyBodyGzip_Transaction);
}

// Regression test for crbug.com/553300.
TEST(URLRequestJob, SlowFilterRead) {
  MockNetworkLayer network_layer;
  TestURLRequestContext context;
  context.set_http_transaction_factory(&network_layer);

  TestDelegate d;
  scoped_ptr<URLRequest> req(context.CreateRequest(
      GURL(kGzip_Slow_Transaction.url), DEFAULT_PRIORITY, &d));
  AddMockTransaction(&kGzip_Slow_Transaction);

  req->set_method("GET");
  req->Start();

  base::MessageLoop::current()->Run();

  EXPECT_FALSE(d.request_failed());
  EXPECT_EQ(200, req->GetResponseCode());
  EXPECT_EQ("hello\n", d.data_received());
  EXPECT_TRUE(network_layer.done_reading_called());

  RemoveMockTransaction(&kGzip_Slow_Transaction);
}

TEST(URLRequestJob, SlowBrotliRead) {
  MockNetworkLayer network_layer;
  TestURLRequestContext context;
  context.set_http_transaction_factory(&network_layer);

  TestDelegate d;
  scoped_ptr<URLRequest> req(context.CreateRequest(
      GURL(kBrotli_Slow_Transaction.url), DEFAULT_PRIORITY, &d));
  AddMockTransaction(&kBrotli_Slow_Transaction);

  req->set_method("GET");
  req->Start();

  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(d.request_failed());
  EXPECT_EQ(200, req->GetResponseCode());
  EXPECT_EQ(kBrotliDecodedHelloData, d.data_received());
  EXPECT_TRUE(network_layer.done_reading_called());

  RemoveMockTransaction(&kBrotli_Slow_Transaction);
}

}  // namespace net
