// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/resource_fetcher.h"

#include "base/callback.h"
#include "base/message_loop.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURLResponse.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "webkit/glue/unittest_test_server.h"
#include "webkit/tools/test_shell/simple_resource_loader_bridge.h"
#include "webkit/tools/test_shell/test_shell_test.h"

#if defined(TOOLKIT_USES_GTK)
#include <gtk/gtk.h>
#endif

using WebKit::WebFrame;
using WebKit::WebURLResponse;
using webkit_glue::ResourceFetcher;
using webkit_glue::ResourceFetcherWithTimeout;

namespace {

class ResourceFetcherTests : public TestShellTest {
 protected:
  UnittestTestServer test_server_;
};

static const int kMaxWaitTimeMs = 5000;
static const int kWaitIntervalMs = 100;

class FetcherDelegate {
 public:
  FetcherDelegate()
      : timer_id_(0), completed_(false), time_elapsed_ms_(0) {
    // Start a repeating timer waiting for the download to complete.  The
    // callback has to be a static function, so we hold on to our instance.
    FetcherDelegate::instance_ = this;
    CreateTimer(kWaitIntervalMs);
  }

  ResourceFetcher::Callback* NewCallback() {
    return ::NewCallback(this, &FetcherDelegate::OnURLFetchComplete);
  }

  virtual void OnURLFetchComplete(const WebURLResponse& response,
                                  const std::string& data) {
    response_ = response;
    data_ = data;
    completed_ = true;
    DestroyTimer();
    MessageLoop::current()->Quit();
  }

  bool completed() const { return completed_; }
  bool timed_out() const { return time_elapsed_ms_ > kMaxWaitTimeMs; }

  int time_elapsed_ms() const { return time_elapsed_ms_; }
  std::string data() const { return data_; }
  const WebURLResponse& response() const { return response_; }

  // Wait for the request to complete or timeout.  We use a loop here b/c the
  // testing infrastructure (test_shell) can generate spurious calls to the
  // MessageLoop's Quit method.
  void WaitForResponse() {
    while (!completed() && !timed_out())
      MessageLoop::current()->Run();
  }

  void CreateTimer(int interval) {
#if defined(OS_WIN)
    timer_id_ = ::SetTimer(NULL, NULL, interval,
                           &FetcherDelegate::TimerCallback);
#elif defined(TOOLKIT_USES_GTK)
    timer_id_ = g_timeout_add(interval, &FetcherDelegate::TimerCallback, NULL);
#elif defined(OS_MACOSX)
    // CFAbsoluteTime is in seconds and |interval| is in ms, so make sure we
    // keep the units correct.
    CFTimeInterval interval_in_seconds = static_cast<double>(interval) / 1000.0;
    CFAbsoluteTime fire_date =
        CFAbsoluteTimeGetCurrent() + interval_in_seconds;
    timer_id_ = CFRunLoopTimerCreate(NULL, fire_date, interval_in_seconds, 0,
                                     0, FetcherDelegate::TimerCallback, NULL);
    CFRunLoopAddTimer(CFRunLoopGetCurrent(), timer_id_, kCFRunLoopCommonModes);
#endif
  }

  void DestroyTimer() {
#if defined(OS_WIN)
    ::KillTimer(NULL, timer_id_);
#elif defined(TOOLKIT_USES_GTK)
    g_source_remove(timer_id_);
#elif defined(OS_MACOSX)
    CFRunLoopRemoveTimer(CFRunLoopGetCurrent(), timer_id_,
                         kCFRunLoopCommonModes);
    CFRelease(timer_id_);
#endif
  }

#if defined(OS_WIN)
  // Static timer callback, just passes through to instance version.
  static VOID CALLBACK TimerCallback(HWND hwnd, UINT msg, UINT_PTR timer_id,
                                     DWORD ms) {
    instance_->TimerFired();
  }
#elif defined(TOOLKIT_USES_GTK)
  static gboolean TimerCallback(gpointer data) {
    instance_->TimerFired();
    return true;
  }
#elif defined(OS_MACOSX)
  static void TimerCallback(CFRunLoopTimerRef timer, void* info) {
    instance_->TimerFired();
  }
#endif

  void TimerFired() {
    ASSERT_FALSE(completed_);

    if (timed_out()) {
      DestroyTimer();
      MessageLoop::current()->Quit();
      FAIL() << "fetch timed out";
      return;
    }

    time_elapsed_ms_ += kWaitIntervalMs;
  }

  static FetcherDelegate* instance_;

 private:
#if defined(OS_WIN)
  UINT_PTR timer_id_;
#elif defined(TOOLKIT_USES_GTK)
  guint timer_id_;
#elif defined(OS_MACOSX)
  CFRunLoopTimerRef timer_id_;
#endif
  bool completed_;
  int time_elapsed_ms_;
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
      url, frame, delegate->NewCallback()));

  delegate->WaitForResponse();

  ASSERT_TRUE(delegate->completed());
  EXPECT_EQ(delegate->response().httpStatusCode(), 200);
  std::string text = delegate->data();
  EXPECT_TRUE(text.find("What is this page?") != std::string::npos);

  // Test 404 response.
  url = test_server_.GetURL("files/thisfiledoesntexist.html");
  delegate.reset(new FetcherDelegate);
  fetcher.reset(new ResourceFetcher(url, frame, delegate->NewCallback()));

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
      url, frame, delegate->NewCallback()));

  delegate->WaitForResponse();

  // When we fail, we still call the Delegate callback but we pass in empty
  // values.
  EXPECT_TRUE(delegate->completed());
  EXPECT_TRUE(delegate->response().isNull());
  EXPECT_EQ(delegate->data(), std::string());
  EXPECT_TRUE(delegate->time_elapsed_ms() < kMaxWaitTimeMs);
}

TEST_F(ResourceFetcherTests, ResourceFetcherTimeout) {
  ASSERT_TRUE(test_server_.Start());

  WebFrame* frame = test_shell_->webView()->mainFrame();

  // Grab a page that takes at least 1 sec to respond, but set the fetcher to
  // timeout in 0 sec.
  GURL url(test_server_.GetURL("slow?1"));
  scoped_ptr<FetcherDelegate> delegate(new FetcherDelegate);
  scoped_ptr<ResourceFetcher> fetcher(new ResourceFetcherWithTimeout(
      url, frame, 0, delegate->NewCallback()));

  delegate->WaitForResponse();

  // When we timeout, we still call the Delegate callback but we pass in empty
  // values.
  EXPECT_TRUE(delegate->completed());
  EXPECT_TRUE(delegate->response().isNull());
  EXPECT_EQ(delegate->data(), std::string());
  EXPECT_TRUE(delegate->time_elapsed_ms() < kMaxWaitTimeMs);
}

class EvilFetcherDelegate : public FetcherDelegate {
 public:
  void SetFetcher(ResourceFetcher* fetcher) {
    fetcher_.reset(fetcher);
  }

  void OnURLFetchComplete(const WebURLResponse& response,
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
      url, frame, 0, delegate->NewCallback()));
  delegate->SetFetcher(fetcher.release());

  delegate->WaitForResponse();
}

}  // namespace
