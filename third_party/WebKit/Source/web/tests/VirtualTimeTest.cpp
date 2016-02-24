// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/testing/UnitTestHelpers.h"
#include "public/platform/Platform.h"
#include "public/platform/WebViewScheduler.h"
#include "public/web/WebLocalFrame.h"
#include "public/web/WebScriptExecutionCallback.h"
#include "public/web/WebScriptSource.h"
#include "public/web/WebView.h"
#include "web/tests/sim/SimTest.h"

namespace blink {

namespace {
class ScriptExecutionCallbackHelper : public WebScriptExecutionCallback {
public:
    const String result() const { return m_result; }

private:
    void completed(const WebVector<v8::Local<v8::Value>>& values) override
    {
        if (!values.isEmpty() && !values[0].IsEmpty() && values[0]->IsString()) {
            m_result = toCoreString(v8::Local<v8::String>::Cast(values[0]));
        }
    }

    String m_result;
};
} // namespace

class VirtualTimeTest : public SimTest {
protected:
    String ExecuteJavaScript(String scriptSource)
    {
        ScriptExecutionCallbackHelper callbackHelper;
        webView().mainFrame()->toWebLocalFrame()->requestExecuteScriptAndReturnValue(
            WebScriptSource(WebString(scriptSource)), false, &callbackHelper);
        testing::runPendingTasks();
        return callbackHelper.result();
    }
};

TEST_F(VirtualTimeTest, DOMTimersFireInExpectedOrder)
{
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

    EXPECT_EQ("c, b, a", ExecuteJavaScript("run_order.join(', ')"));
}

TEST_F(VirtualTimeTest, SetInterval)
{
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

    // If virtual time is not supplied to TimerBase then the setInterval
    // won't fire 10x.
    EXPECT_EQ("9, timer, 8, 7, 6, 5, 4, 3, 2, 1, 0", ExecuteJavaScript("run_order.join(', ')"));
}

TEST_F(VirtualTimeTest, AllowVirtualTimeToAdvance)
{
    webView().scheduler()->enableVirtualTime();
    webView().scheduler()->setAllowVirtualTimeToAdvance(false);

    ExecuteJavaScript(
        "var run_order = [];"
        "timerFn = function(delay, value) {"
        "  setTimeout(function() { run_order.push(value); }, delay);"
        "};"
        "timerFn(100, 'a');"
        "timerFn(10, 'b');"
        "timerFn(1, 'c');");

    EXPECT_EQ("", ExecuteJavaScript("run_order.join(', ')"));

    webView().scheduler()->setAllowVirtualTimeToAdvance(true);
    testing::runPendingTasks();

    EXPECT_EQ("c, b, a", ExecuteJavaScript("run_order.join(', ')"));
}

} // namespace blink
