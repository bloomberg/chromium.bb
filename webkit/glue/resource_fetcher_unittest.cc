// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/resource_fetcher.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/message_loop.h"
#include "base/timer.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebURLResponse.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "webkit/glue/unittest_test_server.h"
#include "webkit/tools/test_shell/simple_resource_loader_bridge.h"
#include "webkit/tools/test_shell/test_shell_test.h"

using WebKit::WebFrame;
using WebKit::WebURLRequest;
using WebKit::WebURLResponse;
using webkit_glue::ResourceFetcher;
using webkit_glue::ResourceFetcherWithTimeout;

namespace {

class ResourceFetcherTests : public TestShellTest {
 protected:
  UnittestTestServer test_server_;
};

static const int kMaxWaitTimeMs = 5000;

class FetcherDelegate {
 public:
  FetcherDelegate()
      : completed_(false),
        timed_out_(false) {
    // Start a repeating timer waiting for the download to complete.  The
    // callback has to be a static function, so we hold on to our instance.
    FetcherDelegate::instance_ = this;
    StartTimer();
  }

  virtual ~FetcherDelegate() {}

  ResourceFetcher::Callback NewCallback() {
    return base::Bind(&FetcherDelegate::OnURLFetchComplete,
                      base::Unretained(this));
  }

  virtual void OnURLFetchComplete(const WebURLResponse& response,
                                  const std::string& data) {
    response_ = response;
    data_ = data;
    completed_ = true;
    timer_.Stop();
    MessageLoop::current()->Quit();
  }

  bool completed() const { return completed_; }
  bool timed_out() const { return timed_out_; }

  std::string data() const { return data_; }
  const WebURLResponse& response() const { return response_; }

  // Wait for the request to complete or timeout.  We use a loop here b/c the
  // testing infrastructure (test_shell) can generate spurious calls to the
  // MessageLoop's Quit method.
  void WaitForResponse() {
    while (!completed() && !timed_out())
      MessageLoop::current()->Run();
  }

  void StartTimer() {
    timer_.Start(FROM_HERE,
                 base::TimeDelta::FromMilliseconds(kMaxWaitTimeMs),
                 this,
                 &FetcherDelegate::TimerFired);
  }

  void TimerFired() {
    ASSERT_FALSE(completed_);

    timed_out_ = true;
    MessageLoop::current()->Quit();
    FAIL() << "fetch timed out";
  }

  static FetcherDelegate* instance_;

 private:
  base::OneShotTimer<FetcherDelegate> timer_;
  bool completed_;
  bool timed_out_;
  WebURLResponse response_;
  std::string data_;
};

FetcherDelegate* FetcherDelegate::instance_ = NULL;

// Test a fetch from the test server.
// Flaky, http://crbug.com/51622.
TEST_F(ResourceFetcherTests, FLAKY_ResourceFetcherDownload) {
  ASSERT_TRUE(test_server_.Start());

  WebFrame* frame = test_shell_->webView()->mainFrame();

  GURL url(test_server_.GetURL("files/test_shell/index.html"));
  scoped_ptr<FetcherDelegate> delegate(new FetcherDelegate);
  scoped_ptr<ResourceFetcher> fetcher(new ResourceFetcher(
      url, frame, WebURLRequest::TargetIsMainFrame, delegate->NewCallback()));

  delegate->WaitForResponse();

  ASSERT_TRUE(delegate->completed());
  EXPECT_EQ(delegate->response().httpStatusCode(), 200);
  std::string text = delegate->data();
  EXPECT_TRUE(text.find("What is this page?") != std::string::npos);

  // Test 404 response.
  url = test_server_.GetURL("files/thisfiledoesntexist.html");
  delegate.reset(new FetcherDelegate);
  fetcher.reset(new ResourceFetcher(url, frame,
                                    WebURLRequest::TargetIsMainFrame,
                                    delegate->NewCallback()));

  delegate->WaitForResponse();

  ASSERT_TRUE(delegate->completed());
  EXPECT_EQ(delegate->response().httpStatusCode(), 404);
  EXPECT_TRUE(delegate->data().find("Not Found.") != std::string::npos);
}

// Flaky, http://crbug.com/51622.
TEST_F(ResourceFetcherTests, FLAKY_ResourceFetcherDidFail) {
  ASSERT_TRUE(test_server_.Start());

  WebFrame* frame = test_shell_->webView()->mainFrame();

  // Try to fetch a page on a site that doesn't exist.
  GURL url("http://localhost:1339/doesnotexist");
  scoped_ptr<FetcherDelegate> delegate(new FetcherDelegate);
  scoped_ptr<ResourceFetcher> fetcher(new ResourceFetcher(
      url, frame, WebURLRequest::TargetIsMainFrame, delegate->NewCallback()));

  delegate->WaitForResponse();

  // When we fail, we still call the Delegate callback but we pass in empty
  // values.
  EXPECT_TRUE(delegate->completed());
  EXPECT_TRUE(delegate->response().isNull());
  EXPECT_EQ(delegate->data(), std::string());
  EXPECT_FALSE(delegate->timed_out());
}

TEST_F(ResourceFetcherTests, ResourceFetcherTimeout) {
  ASSERT_TRUE(test_server_.Start());

  WebFrame* frame = test_shell_->webView()->mainFrame();

  // Grab a page that takes at least 1 sec to respond, but set the fetcher to
  // timeout in 0 sec.
  GURL url(test_server_.GetURL("slow?1"));
  scoped_ptr<FetcherDelegate> delegate(new FetcherDelegate);
  scoped_ptr<ResourceFetcher> fetcher(new ResourceFetcherWithTimeout(
      url, frame, WebURLRequest::TargetIsMainFrame,
      0, delegate->NewCallback()));

  delegate->WaitForResponse();

  // When we timeout, we still call the Delegate callback but we pass in empty
  // values.
  EXPECT_TRUE(delegate->completed());
  EXPECT_TRUE(delegate->response().isNull());
  EXPECT_EQ(delegate->data(), std::string());
  EXPECT_FALSE(delegate->timed_out());
}

class EvilFetcherDelegate : public FetcherDelegate {
 public:
  virtual ~EvilFetcherDelegate() {}

  void SetFetcher(ResourceFetcher* fetcher) {
    fetcher_.reset(fetcher);
  }

  virtual void OnURLFetchComplete(const WebURLResponse& response,
                                  const std::string& data) {
    // Destroy the ResourceFetcher here.  We are testing that upon returning
    // to the ResourceFetcher that it does not crash.
    fetcher_.reset();
    FetcherDelegate::OnURLFetchComplete(response, data);
  }

 private:
  scoped_ptr<ResourceFetcher> fetcher_;
};

TEST_F(ResourceFetcherTests, ResourceFetcherDeletedInCallback) {
  ASSERT_TRUE(test_server_.Start());

  WebFrame* frame = test_shell_->webView()->mainFrame();

  // Grab a page that takes at least 1 sec to respond, but set the fetcher to
  // timeout in 0 sec.
  GURL url(test_server_.GetURL("slow?1"));
  scoped_ptr<EvilFetcherDelegate> delegate(new EvilFetcherDelegate);
  scoped_ptr<ResourceFetcher> fetcher(new ResourceFetcherWithTimeout(
      url, frame, WebURLRequest::TargetIsMainFrame,
      0, delegate->NewCallback()));
  delegate->SetFetcher(fetcher.release());

  delegate->WaitForResponse();
  EXPECT_FALSE(delegate->timed_out());
}

}  // namespace
