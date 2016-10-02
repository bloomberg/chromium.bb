// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "platform/testing/UnitTestHelpers.h"
#include "public/platform/Platform.h"
#include "public/platform/WebViewScheduler.h"
#include "public/web/WebLocalFrame.h"
#include "public/web/WebScriptExecutionCallback.h"
#include "public/web/WebScriptSource.h"
#include "public/web/WebView.h"
#include "web/tests/sim/SimRequest.h"
#include "web/tests/sim/SimTest.h"

namespace blink {

namespace {
class ScriptExecutionCallbackHelper : public WebScriptExecutionCallback {
 public:
  const String result() const { return m_result; }

 private:
  void completed(const WebVector<v8::Local<v8::Value>>& values) override {
    if (!values.isEmpty() && !values[0].IsEmpty() && values[0]->IsString()) {
      m_result = toCoreString(v8::Local<v8::String>::Cast(values[0]));
    }
  }

  String m_result;
};
}  // namespace

class VirtualTimeTest : public SimTest {
 protected:
  String ExecuteJavaScript(String scriptSource) {
    ScriptExecutionCallbackHelper callbackHelper;
    webView()
        .mainFrame()
        ->toWebLocalFrame()
        ->requestExecuteScriptAndReturnValue(
            WebScriptSource(WebString(scriptSource)), false, &callbackHelper);
    return callbackHelper.result();
  }

  void TearDown() override {
    // The SimTest destructor calls runPendingTasks. This is a problem because
    // if there are any repeating tasks, advancing virtual time will cause the
    // runloop to busy loop. Pausing virtual time here fixes that.
    webView().scheduler()->setVirtualTimePolicy(
        WebViewScheduler::VirtualTimePolicy::PAUSE);
  }
};

namespace {
void quitRunLoop() {
  base::MessageLoop::current()->QuitNow();
}

// Some task queues may have repeating v8 tasks that run forever so we impose a
// hard time limit.
void runTasksForPeriod(double delayMs) {
  Platform::current()->currentThread()->getWebTaskRunner()->postDelayedTask(
      BLINK_FROM_HERE, WTF::bind(&quitRunLoop), delayMs);
  testing::enterRunLoop();
}
}

// http://crbug.com/633321
#if OS(ANDROID)
#define MAYBE_DOMTimersFireInExpectedOrder DISABLED_DOMTimersFireInExpectedOrder
#else
#define MAYBE_DOMTimersFireInExpectedOrder DOMTimersFireInExpectedOrder
#endif
TEST_F(VirtualTimeTest, MAYBE_DOMTimersFireInExpectedOrder) {
  webView().scheduler()->enableVirtualTime();

  ExecuteJavaScript(
      "var run_order = [];"
      "function timerFn(delay, value) {"
      "  setTimeout(function() { run_order.push(value); }, delay);"
      "};"
      "var one_hour = 60 * 60 * 1000;"
      "timerFn(one_hour * 100, 'a');"
      "timerFn(one_hour * 10, 'b');"
      "timerFn(one_hour, 'c');");

  // Normally the JS runs pretty much instantly but the timer callbacks will
  // take 100h to fire, but thanks to timer fast forwarding we can make them
  // fire immediatly.

  testing::runPendingTasks();
  EXPECT_EQ("c, b, a", ExecuteJavaScript("run_order.join(', ')"));
}

// http://crbug.com/633321
#if OS(ANDROID)
#define MAYBE_SetInterval DISABLED_SetInterval
#else
#define MAYBE_SetInterval SetInterval
#endif
TEST_F(VirtualTimeTest, MAYBE_SetInterval) {
  webView().scheduler()->enableVirtualTime();

  ExecuteJavaScript(
      "var run_order = [];"
      "var count = 10;"
      "var interval_handle = setInterval(function() {"
      "  if (--window.count == 0) {"
      "     clearInterval(interval_handle);"
      "  }"
      "  run_order.push(count);"
      "}, 1000);"
      "setTimeout(function() { run_order.push('timer'); }, 1500);");

  runTasksForPeriod(12000);

  EXPECT_EQ("9, timer, 8, 7, 6, 5, 4, 3, 2, 1, 0",
            ExecuteJavaScript("run_order.join(', ')"));
}

// http://crbug.com/633321
#if OS(ANDROID)
#define MAYBE_AllowVirtualTimeToAdvance DISABLED_AllowVirtualTimeToAdvance
#else
#define MAYBE_AllowVirtualTimeToAdvance AllowVirtualTimeToAdvance
#endif
TEST_F(VirtualTimeTest, MAYBE_AllowVirtualTimeToAdvance) {
  webView().scheduler()->enableVirtualTime();
  webView().scheduler()->setVirtualTimePolicy(
      WebViewScheduler::VirtualTimePolicy::PAUSE);

  ExecuteJavaScript(
      "var run_order = [];"
      "timerFn = function(delay, value) {"
      "  setTimeout(function() { run_order.push(value); }, delay);"
      "};"
      "timerFn(100, 'a');"
      "timerFn(10, 'b');"
      "timerFn(1, 'c');");

  testing::runPendingTasks();
  EXPECT_EQ("", ExecuteJavaScript("run_order.join(', ')"));

  webView().scheduler()->setVirtualTimePolicy(
      WebViewScheduler::VirtualTimePolicy::ADVANCE);
  runTasksForPeriod(1000);

  EXPECT_EQ("c, b, a", ExecuteJavaScript("run_order.join(', ')"));
}

// http://crbug.com/633321
#if OS(ANDROID)
#define MAYBE_VirtualTimeNotAllowedToAdvanceWhileResourcesLoading \
  DISABLED_VirtualTimeNotAllowedToAdvanceWhileResourcesLoading
#else
#define MAYBE_VirtualTimeNotAllowedToAdvanceWhileResourcesLoading \
  VirtualTimeNotAllowedToAdvanceWhileResourcesLoading
#endif
TEST_F(VirtualTimeTest,
       MAYBE_VirtualTimeNotAllowedToAdvanceWhileResourcesLoading) {
  webView().scheduler()->enableVirtualTime();
  webView().scheduler()->setVirtualTimePolicy(
      WebViewScheduler::VirtualTimePolicy::DETERMINISTIC_LOADING);

  // To ensure determinism virtual time is not allowed to advance until we have
  // seen at least one load.
  EXPECT_FALSE(webView().scheduler()->virtualTimeAllowedToAdvance());

  SimRequest mainResource("https://example.com/test.html", "text/html");
  SimRequest cssResource("https://example.com/test.css", "text/css");

  // Loading, virtual time should not advance.
  loadURL("https://example.com/test.html");
  EXPECT_FALSE(webView().scheduler()->virtualTimeAllowedToAdvance());

  mainResource.start();

  // Still Loading, virtual time should not advance.
  mainResource.write("<!DOCTYPE html><link rel=stylesheet href=test.css>");
  EXPECT_FALSE(webView().scheduler()->virtualTimeAllowedToAdvance());

  // Still Loading, virtual time should not advance.
  cssResource.start();
  cssResource.write("a { color: red; }");
  EXPECT_FALSE(webView().scheduler()->virtualTimeAllowedToAdvance());

  // Still Loading, virtual time should not advance.
  cssResource.finish();
  EXPECT_FALSE(webView().scheduler()->virtualTimeAllowedToAdvance());

  // Still Loading, virtual time should not advance.
  mainResource.write("<body>");
  EXPECT_FALSE(webView().scheduler()->virtualTimeAllowedToAdvance());

  // Finished loading, virtual time should be able to advance.
  mainResource.finish();
  EXPECT_TRUE(webView().scheduler()->virtualTimeAllowedToAdvance());
}

}  // namespace blink
