// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "core/testing/sim/SimRequest.h"
#include "core/testing/sim/SimTest.h"
#include "platform/scheduler/public/page_scheduler.h"
#include "platform/testing/UnitTestHelpers.h"
#include "public/platform/Platform.h"
#include "public/platform/TaskType.h"
#include "public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "public/web/WebLocalFrame.h"
#include "public/web/WebScriptExecutionCallback.h"
#include "public/web/WebScriptSource.h"
#include "public/web/WebView.h"

namespace blink {
namespace virtual_time_test {

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

class VirtualTimeTest : public SimTest {
 protected:
  void SetUp() override {
    SimTest::SetUp();
    WebView().Scheduler()->EnableVirtualTime();
  }

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

  void StopVirtualTimeAndExitRunLoop() {
    WebView().Scheduler()->SetVirtualTimePolicy(
        PageScheduler::VirtualTimePolicy::kPause);
    test::ExitRunLoop();
  }

  // Some task queues may have repeating v8 tasks that run forever so we impose
  // a hard (virtual) time limit.
  void RunTasksForPeriod(double delay_ms) {
    scheduler::GetSingleThreadTaskRunnerForTesting()->PostDelayedTask(
        FROM_HERE,
        WTF::Bind(&VirtualTimeTest::StopVirtualTimeAndExitRunLoop,
                  WTF::Unretained(this)),
        TimeDelta::FromMillisecondsD(delay_ms));
    test::EnterRunLoop();
  }
};

// http://crbug.com/633321
#if defined(OS_ANDROID)
#define MAYBE_DOMTimersFireInExpectedOrder DISABLED_DOMTimersFireInExpectedOrder
#else
#define MAYBE_DOMTimersFireInExpectedOrder DOMTimersFireInExpectedOrder
#endif
TEST_F(VirtualTimeTest, MAYBE_DOMTimersFireInExpectedOrder) {
  WebView().Scheduler()->EnableVirtualTime();
  WebView().Scheduler()->SetVirtualTimePolicy(
      PageScheduler::VirtualTimePolicy::kAdvance);

  ExecuteJavaScript(
      "var run_order = [];"
      "function timerFn(delay, value) {"
      "  setTimeout(function() { run_order.push(value); }, delay);"
      "};"
      "var one_minute = 60 * 1000;"
      "timerFn(one_minute * 4, 'a');"
      "timerFn(one_minute * 2, 'b');"
      "timerFn(one_minute, 'c');");

  // Normally the JS runs pretty much instantly but the timer callbacks will
  // take 4 mins to fire, but thanks to timer fast forwarding we can make them
  // fire immediatly.

  RunTasksForPeriod(60 * 1000 * 4);
  EXPECT_EQ("c, b, a", ExecuteJavaScript("run_order.join(', ')"));
}

// http://crbug.com/633321
#if defined(OS_ANDROID)
#define MAYBE_SetInterval DISABLED_SetInterval
#else
#define MAYBE_SetInterval SetInterval
#endif
TEST_F(VirtualTimeTest, MAYBE_SetInterval) {
  WebView().Scheduler()->EnableVirtualTime();
  WebView().Scheduler()->SetVirtualTimePolicy(
      PageScheduler::VirtualTimePolicy::kAdvance);

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

  RunTasksForPeriod(10001);

  EXPECT_EQ("9, timer, 8, 7, 6, 5, 4, 3, 2, 1, 0",
            ExecuteJavaScript("run_order.join(', ')"));
}

// http://crbug.com/633321
#if defined(OS_ANDROID)
#define MAYBE_AllowVirtualTimeToAdvance DISABLED_AllowVirtualTimeToAdvance
#else
#define MAYBE_AllowVirtualTimeToAdvance AllowVirtualTimeToAdvance
#endif
TEST_F(VirtualTimeTest, MAYBE_AllowVirtualTimeToAdvance) {
  WebView().Scheduler()->SetVirtualTimePolicy(
      PageScheduler::VirtualTimePolicy::kPause);

  ExecuteJavaScript(
      "var run_order = [];"
      "timerFn = function(delay, value) {"
      "  setTimeout(function() { run_order.push(value); }, delay);"
      "};"
      "timerFn(100, 'a');"
      "timerFn(10, 'b');"
      "timerFn(1, 'c');");

  test::RunPendingTasks();
  EXPECT_EQ("", ExecuteJavaScript("run_order.join(', ')"));

  WebView().Scheduler()->SetVirtualTimePolicy(
      PageScheduler::VirtualTimePolicy::kAdvance);
  RunTasksForPeriod(1000);

  EXPECT_EQ("c, b, a", ExecuteJavaScript("run_order.join(', ')"));
}

// http://crbug.com/633321
#if defined(OS_ANDROID)
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
      PageScheduler::VirtualTimePolicy::kDeterministicLoading);

  EXPECT_TRUE(WebView().Scheduler()->VirtualTimeAllowedToAdvance());

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

  // The loading events are delayed for 10 virtual ms after they have run, we
  // let tasks run for a little while to ensure we don't get any asserts on
  // teardown as a result.
  RunTasksForPeriod(10);
}

// http://crbug.com/633321
#if defined(OS_ANDROID)
#define MAYBE_DOMTimersSuspended DISABLED_DOMTimersSuspended
#else
#define MAYBE_DOMTimersSuspended DOMTimersSuspended
#endif
TEST_F(VirtualTimeTest, MAYBE_DOMTimersSuspended) {
  WebView().Scheduler()->EnableVirtualTime();
  WebView().Scheduler()->SetVirtualTimePolicy(
      PageScheduler::VirtualTimePolicy::kAdvance);

  // Schedule normal DOM timers to run at 1s and 1.001s in the future.
  ExecuteJavaScript(
      "var run_order = [];"
      "setTimeout(() => { run_order.push(1); }, 1000);"
      "setTimeout(() => { run_order.push(2); }, 1001);");

  scoped_refptr<base::SingleThreadTaskRunner> runner =
      Window().GetExecutionContext()->GetTaskRunner(TaskType::kJavascriptTimer);

  // Schedule a task to suspend virtual time at the same point in time.
  runner->PostDelayedTask(FROM_HERE,
                          WTF::Bind(
                              [](PageScheduler* scheduler) {
                                scheduler->SetVirtualTimePolicy(
                                    PageScheduler::VirtualTimePolicy::kPause);
                              },
                              WTF::Unretained(WebView().Scheduler())),
                          TimeDelta::FromMilliseconds(1000));

  // ALso schedule a third timer for the same point in time.
  ExecuteJavaScript("setTimeout(() => { run_order.push(2); }, 1000);");

  // The second DOM timer shouldn't have run because the virtual time budget
  // expired.
  test::RunPendingTasks();
  EXPECT_EQ("1, 2", ExecuteJavaScript("run_order.join(', ')"));
}

#undef MAYBE_DOMTimersFireInExpectedOrder
#undef MAYBE_SetInterval
#undef MAYBE_AllowVirtualTimeToAdvance
#undef MAYBE_VirtualTimeNotAllowedToAdvanceWhileResourcesLoading
#undef MAYBE_DOMTimersSuspended
}  // namespace virtual_time_test
}  // namespace blink
