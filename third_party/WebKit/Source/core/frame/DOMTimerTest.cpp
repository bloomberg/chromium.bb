// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/DOMTimer.h"

#include <vector>

#include "bindings/core/v8/ScriptController.h"
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
      m_platform;

  // Expected time between each iterator for setInterval(..., 1) or nested
  // setTimeout(..., 1) are 1, 1, 1, 1, 4, 4, ... as a minumum clamp of 4m
  // is applied from the 5th iteration onwards.
  const std::vector<Matcher<double>> kExpectedTimings = {
      DoubleNear(1., 0.000001), DoubleNear(1., 0.000001),
      DoubleNear(1., 0.000001), DoubleNear(1., 0.000001),
      DoubleNear(4., 0.000001), DoubleNear(4., 0.000001),
  };

  void SetUp() override {
    m_platform->setAutoAdvanceNowToPendingTasks(true);
    // Advance timer manually as RenderingTest expects the time to be non-zero.
    m_platform->advanceClockSeconds(1.);
    RenderingTest::SetUp();
    // Advance timer again as otherwise the time between the first call to
    // setInterval and it running will be off by 5us.
    m_platform->advanceClockSeconds(1);
    document().settings()->setScriptEnabled(true);
  }

  v8::Local<v8::Value> EvalExpression(const char* expr) {
    return document().frame()->script().executeScriptInMainWorldAndReturnValue(
        ScriptSourceCode(expr));
  }

  Vector<double> toDoubleArray(v8::Local<v8::Value> value,
                               v8::HandleScope& scope) {
    NonThrowableExceptionState exceptionState;
    return toImplArray<Vector<double>>(value, 0, scope.GetIsolate(),
                                       exceptionState);
  }

  void ExecuteScriptAndWaitUntilIdle(const char* scriptText) {
    ScriptSourceCode script(scriptText);
    document().frame()->script().executeScriptInMainWorld(script);
    m_platform->runUntilIdle();
  }
};

const char* kSetTimeoutScriptText =
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

  ExecuteScriptAndWaitUntilIdle(kSetTimeoutScriptText);

  auto times(toDoubleArray(EvalExpression("times"), scope));

  EXPECT_THAT(times, ElementsAreArray(kExpectedTimings));
}

const char* kSetIntervalScriptText =
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

  ExecuteScriptAndWaitUntilIdle(kSetIntervalScriptText);

  auto times(toDoubleArray(EvalExpression("times"), scope));

  EXPECT_THAT(times, ElementsAreArray(kExpectedTimings));
}

TEST_F(DOMTimerTest, setInterval_NestingResetsForLaterCalls) {
  v8::HandleScope scope(v8::Isolate::GetCurrent());

  ExecuteScriptAndWaitUntilIdle(kSetIntervalScriptText);

  // Run the setIntervalScript again to verify that the clamp imposed for
  // nesting beyond 4 levels is reset when setInterval is called again in the
  // original scope but after the original setInterval has completed.
  ExecuteScriptAndWaitUntilIdle(kSetIntervalScriptText);

  auto times(toDoubleArray(EvalExpression("times"), scope));

  EXPECT_THAT(times, ElementsAreArray(kExpectedTimings));
}

}  // namespace

}  // namespace blink
