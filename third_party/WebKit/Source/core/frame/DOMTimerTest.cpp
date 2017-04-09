// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/DOMTimer.h"

#include <vector>

#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/V8Binding.h"
#include "core/dom/Document.h"
#include "core/layout/LayoutTestHelper.h"
#include "platform/testing/TestingPlatformSupport.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::DoubleNear;
using testing::ElementsAreArray;
using testing::Matcher;

namespace blink {

namespace {

class DOMTimerTest : public RenderingTest {
 public:
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform_;

  // Expected time between each iterator for setInterval(..., 1) or nested
  // setTimeout(..., 1) are 1, 1, 1, 1, 4, 4, ... as a minumum clamp of 4m
  // is applied from the 5th iteration onwards.
  const std::vector<Matcher<double>> k_expected_timings = {
      DoubleNear(1., 0.000001), DoubleNear(1., 0.000001),
      DoubleNear(1., 0.000001), DoubleNear(1., 0.000001),
      DoubleNear(4., 0.000001), DoubleNear(4., 0.000001),
  };

  void SetUp() override {
    platform_->SetAutoAdvanceNowToPendingTasks(true);
    // Advance timer manually as RenderingTest expects the time to be non-zero.
    platform_->AdvanceClockSeconds(1.);
    RenderingTest::SetUp();
    // Advance timer again as otherwise the time between the first call to
    // setInterval and it running will be off by 5us.
    platform_->AdvanceClockSeconds(1);
    GetDocument().GetSettings()->SetScriptEnabled(true);
  }

  v8::Local<v8::Value> EvalExpression(const char* expr) {
    return GetDocument()
        .GetFrame()
        ->Script()
        .ExecuteScriptInMainWorldAndReturnValue(ScriptSourceCode(expr));
  }

  Vector<double> ToDoubleArray(v8::Local<v8::Value> value,
                               v8::HandleScope& scope) {
    NonThrowableExceptionState exception_state;
    return ToImplArray<Vector<double>>(value, 0, scope.GetIsolate(),
                                       exception_state);
  }

  void ExecuteScriptAndWaitUntilIdle(const char* script_text) {
    ScriptSourceCode script(script_text);
    GetDocument().GetFrame()->Script().ExecuteScriptInMainWorld(script);
    platform_->RunUntilIdle();
  }
};

const char* g_k_set_timeout_script_text =
    "var id;"
    "var last = performance.now();"
    "var times = [];"
    "function nestSetTimeouts() {"
    "  var current = performance.now();"
    "  var elapsed = current - last;"
    "  last = current;"
    "  times.push(elapsed);"
    "  if (times.length < 6) {"
    "    setTimeout(nestSetTimeouts, 1);"
    "  }"
    "}"
    "setTimeout(nestSetTimeouts, 1);";

TEST_F(DOMTimerTest, setTimeout_ClampsAfter4Nestings) {
  v8::HandleScope scope(v8::Isolate::GetCurrent());

  ExecuteScriptAndWaitUntilIdle(g_k_set_timeout_script_text);

  auto times(ToDoubleArray(EvalExpression("times"), scope));

  EXPECT_THAT(times, ElementsAreArray(k_expected_timings));
}

const char* g_k_set_interval_script_text =
    "var last = performance.now();"
    "var times = [];"
    "var id = setInterval(function() {"
    "  var current = performance.now();"
    "  var elapsed = current - last;"
    "  last = current;"
    "  times.push(elapsed);"
    "  if (times.length > 5) {"
    "    clearInterval(id);"
    "  }"
    "}, 1);";

TEST_F(DOMTimerTest, setInterval_ClampsAfter4Iterations) {
  v8::HandleScope scope(v8::Isolate::GetCurrent());

  ExecuteScriptAndWaitUntilIdle(g_k_set_interval_script_text);

  auto times(ToDoubleArray(EvalExpression("times"), scope));

  EXPECT_THAT(times, ElementsAreArray(k_expected_timings));
}

TEST_F(DOMTimerTest, setInterval_NestingResetsForLaterCalls) {
  v8::HandleScope scope(v8::Isolate::GetCurrent());

  ExecuteScriptAndWaitUntilIdle(g_k_set_interval_script_text);

  // Run the setIntervalScript again to verify that the clamp imposed for
  // nesting beyond 4 levels is reset when setInterval is called again in the
  // original scope but after the original setInterval has completed.
  ExecuteScriptAndWaitUntilIdle(g_k_set_interval_script_text);

  auto times(ToDoubleArray(EvalExpression("times"), scope));

  EXPECT_THAT(times, ElementsAreArray(k_expected_timings));
}

}  // namespace

}  // namespace blink
