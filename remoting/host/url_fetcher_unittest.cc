// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/url_fetcher.h"

#include "base/message_loop.h"
#include "base/threading/thread.h"
#include "net/test/test_server.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_status.h"
#include "remoting/host/url_request_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

class UrlFetcherTest : public testing::Test  {
 public:
  UrlFetcherTest()
      : test_server_(
          net::TestServer::TYPE_HTTPS,
          net::TestServer::kLocalhost,
          FilePath(FILE_PATH_LITERAL("net/data/url_request_unittest"))),
        io_thread_("TestIOThread"),
        file_thread_("TestFileThread") {
  }

 protected:
  void SetUp() OVERRIDE {
    ASSERT_TRUE(io_thread_.StartWithOptions(
        base::Thread::Options(MessageLoop::TYPE_IO, 0)));
    ASSERT_TRUE(file_thread_.StartWithOptions(
        base::Thread::Options(MessageLoop::TYPE_IO, 0)));
    context_getter_ = new URLRequestContextGetter(
        message_loop_.message_loop_proxy(),
        io_thread_.message_loop_proxy(),
        static_cast<MessageLoopForIO*>(file_thread_.message_loop()));
    ASSERT_TRUE(test_server_.Start());
  }

 protected:
  void OnDone(const net::URLRequestStatus& status,
              int response_code,
              const std::string& response) {
    ASSERT_EQ(MessageLoop::current(), &message_loop_);
    status_ = status;
    response_code_ = response_code;
    response_ = response;
    message_loop_.PostTask(FROM_HERE, MessageLoop::QuitClosure());
  }

  net::TestServer test_server_;
  MessageLoopForUI message_loop_;
  base::Thread io_thread_;
  base::Thread file_thread_;
  scoped_refptr<URLRequestContextGetter> context_getter_;
  net::URLRequestStatus status_;
  std::string response_;
  int response_code_;
};

TEST_F(UrlFetcherTest, TestGet) {
  UrlFetcher fetcher(test_server_.GetURL("default"), UrlFetcher::GET);
  fetcher.SetRequestContext(context_getter_);
  fetcher.Start(base::Bind(&UrlFetcherTest_TestGet_Test::OnDone,
                           base::Unretained(this)));
  message_loop_.Run();
  EXPECT_EQ(net::URLRequestStatus::SUCCESS, status_.status());
  EXPECT_EQ("Default response given for path: /default", response_);
  EXPECT_EQ(200, response_code_);
}

TEST_F(UrlFetcherTest, TestPost) {
  const char kTestQueryData[] = "123qwe123qwe";
  UrlFetcher fetcher(test_server_.GetURL("echo"), UrlFetcher::POST);
  fetcher.SetRequestContext(context_getter_);
  fetcher.SetUploadData("text/html", kTestQueryData);
  fetcher.Start(base::Bind(&UrlFetcherTest_TestPost_Test::OnDone,
                           base::Unretained(this)));
  message_loop_.Run();
  EXPECT_EQ(net::URLRequestStatus::SUCCESS, status_.status());
  EXPECT_EQ(kTestQueryData, response_);
  EXPECT_EQ(200, response_code_);
}

TEST_F(UrlFetcherTest, TestFailed) {
  UrlFetcher fetcher(test_server_.GetURL("auth-basic"), UrlFetcher::GET);
  fetcher.SetRequestContext(context_getter_);
  fetcher.Start(base::Bind(&UrlFetcherTest_TestFailed_Test::OnDone,
                           base::Unretained(this)));
  message_loop_.Run();
  EXPECT_EQ(net::URLRequestStatus::SUCCESS, status_.status());
  EXPECT_EQ(401, response_code_);
}

}  // namespace remoting
