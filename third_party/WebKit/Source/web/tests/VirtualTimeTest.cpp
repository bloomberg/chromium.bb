// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "core/dom/TaskRunnerHelper.h"
#include "platform/scheduler/renderer/web_view_scheduler.h"
#include "platform/testing/UnitTestHelpers.h"
#include "public/platform/Platform.h"
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
  const String Result() const { return result_; }

 private:
  void Completed(const WebVector<v8::Local<v8::Value>>& values) override {
    if (!values.IsEmpty() && !values[0].IsEmpty() && values[0]->IsString()) {
      result_ = ToCoreString(v8::Local<v8::String>::Cast(values[0]));
    }
  }

  String result_;
};
}  // namespace

class VirtualTimeTest : public SimTest {
 protected:
  String ExecuteJavaScript(String script_source) {
    ScriptExecutionCallbackHelper callback_helper;
    WebView()
        .MainFrame()
        ->ToWebLocalFrame()
        ->RequestExecuteScriptAndReturnValue(
            WebScriptSource(WebString(script_source)), false, &callback_helper);
    return callback_helper.Result();
  }

  void TearDown() override {
    // The SimTest destructor calls runPendingTasks. This is a problem because
    // if there are any repeating tasks, advancing virtual time will cause the
    // runloop to busy loop. Disabling virtual time here fixes that.
    WebView().Scheduler()->DisableVirtualTimeForTesting();
  }
};

namespace {
void QuitRunLoop() {
  base::MessageLoop::current()->QuitNow();
}

// Some task queues may have repeating v8 tasks that run forever so we impose a
// hard time limit.
void RunTasksForPeriod(double delay_ms) {
  Platform::Current()->CurrentThread()->GetWebTaskRunner()->PostDelayedTask(
      BLINK_FROM_HERE, WTF::Bind(&QuitRunLoop),
      TimeDelta::FromMillisecondsD(delay_ms));
  testing::EnterRunLoop();
}
}

// http://crbug.com/633321
#if OS(ANDROID)
#define MAYBE_DOMTimersFireInExpectedOrder DISABLED_DOMTimersFireInExpectedOrder
#else
#define MAYBE_DOMTimersFireInExpectedOrder DOMTimersFireInExpectedOrder
#endif
TEST_F(VirtualTimeTest, MAYBE_DOMTimersFireInExpectedOrder) {
  WebView().Scheduler()->EnableVirtualTime();

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

  testing::RunPendingTasks();
  EXPECT_EQ("c, b, a", ExecuteJavaScript("run_order.join(', ')"));
}

// http://crbug.com/633321
#if OS(ANDROID)
#define MAYBE_SetInterval DISABLED_SetInterval
#else
#define MAYBE_SetInterval SetInterval
#endif
TEST_F(VirtualTimeTest, MAYBE_SetInterval) {
  WebView().Scheduler()->EnableVirtualTime();

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

  RunTasksForPeriod(12000);

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
  WebView().Scheduler()->EnableVirtualTime();
  WebView().Scheduler()->SetVirtualTimePolicy(
      WebViewScheduler::VirtualTimePolicy::PAUSE);

  ExecuteJavaScript(
      "var run_order = [];"
      "timerFn = function(delay, value) {"
      "  setTimeout(function() { run_order.push(value); }, delay);"
      "};"
      "timerFn(100, 'a');"
      "timerFn(10, 'b');"
      "timerFn(1, 'c');");

  testing::RunPendingTasks();
  EXPECT_EQ("", ExecuteJavaScript("run_order.join(', ')"));

  WebView().Scheduler()->SetVirtualTimePolicy(
      WebViewScheduler::VirtualTimePolicy::ADVANCE);
  RunTasksForPeriod(1000);

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
  WebView().Scheduler()->EnableVirtualTime();
  WebView().Scheduler()->SetVirtualTimePolicy(
      WebViewScheduler::VirtualTimePolicy::DETERMINISTIC_LOADING);

  // To ensure determinism virtual time is not allowed to advance until we have
  // seen at least one load.
  EXPECT_FALSE(WebView().Scheduler()->VirtualTimeAllowedToAdvance());

  SimRequest main_resource("https://example.com/test.html", "text/html");
  SimRequest css_resource("https://example.com/test.css", "text/css");

  // Loading, virtual time should not advance.
  LoadURL("https://example.com/test.html");
  EXPECT_FALSE(WebView().Scheduler()->VirtualTimeAllowedToAdvance());

  main_resource.Start();

  // Still Loading, virtual time should not advance.
  main_resource.Write("<!DOCTYPE html><link rel=stylesheet href=test.css>");
  EXPECT_FALSE(WebView().Scheduler()->VirtualTimeAllowedToAdvance());

  // Still Loading, virtual time should not advance.
  css_resource.Start();
  css_resource.Write("a { color: red; }");
  EXPECT_FALSE(WebView().Scheduler()->VirtualTimeAllowedToAdvance());

  // Still Loading, virtual time should not advance.
  css_resource.Finish();
  EXPECT_FALSE(WebView().Scheduler()->VirtualTimeAllowedToAdvance());

  // Still Loading, virtual time should not advance.
  main_resource.Write("<body>");
  EXPECT_FALSE(WebView().Scheduler()->VirtualTimeAllowedToAdvance());

  // Finished loading, virtual time should be able to advance.
  main_resource.Finish();
  EXPECT_TRUE(WebView().Scheduler()->VirtualTimeAllowedToAdvance());
}

// http://crbug.com/633321
#if OS(ANDROID)
#define MAYBE_DOMTimersSuspended DISABLED_DOMTimersSuspended
#else
#define MAYBE_DOMTimersSuspended DOMTimersSuspended
#endif
TEST_F(VirtualTimeTest, MAYBE_DOMTimersSuspended) {
  WebView().Scheduler()->EnableVirtualTime();

  // Schedule a normal DOM timer to run at 1s in the future.
  ExecuteJavaScript(
      "var run_order = [];"
      "setTimeout(() => { run_order.push(1); }, 1000);");

  RefPtr<WebTaskRunner> runner =
      TaskRunnerHelper::Get(TaskType::kTimer, Window().GetExecutionContext());

  // Schedule a task to suspend virtual time at the same point in time.
  runner->PostDelayedTask(BLINK_FROM_HERE,
                          WTF::Bind(
                              [](WebViewScheduler* scheduler) {
                                scheduler->SetVirtualTimePolicy(
                                    WebViewScheduler::VirtualTimePolicy::PAUSE);
                              },
                              WTF::Unretained(WebView().Scheduler())),
                          TimeDelta::FromMilliseconds(1000));

  // ALso schedule a second timer for the same point in time.
  ExecuteJavaScript("setTimeout(() => { run_order.push(2); }, 1000);");

  // The second DOM timer shouldn't have run because pausing virtual time also
  // atomically pauses DOM timers.
  testing::RunPendingTasks();
  EXPECT_EQ("1", ExecuteJavaScript("run_order.join(', ')"));
}

}  // namespace blink
