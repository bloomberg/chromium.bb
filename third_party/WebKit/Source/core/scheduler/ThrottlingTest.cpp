// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code if governed by a BSD-style license that can be
// found in LICENSE file.

#include "core/testing/sim/SimRequest.h"
#include "core/testing/sim/SimTest.h"
#include "platform/scheduler/child/web_scheduler.h"
#include "platform/testing/RuntimeEnabledFeaturesTestHelpers.h"
#include "platform/testing/UnitTestHelpers.h"
#include "public/platform/Platform.h"
#include "public/platform/WebThread.h"
#include "public/platform/scheduler/renderer/renderer_scheduler.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::ElementsAre;

namespace blink {

class DisableBackgroundThrottlingIsRespectedTest : public SimTest {
 public:
  void SetUp() override {
    background_tab_timer_throttling_feature_ =
        WTF::MakeUnique<ScopedBackgroundTabTimerThrottlingForTest>(false);
    SimTest::SetUp();
  }

  void TearDown() { background_tab_timer_throttling_feature_.reset(); }

 private:
  typedef ScopedRuntimeEnabledFeatureForTest<
      RuntimeEnabledFeatures::TimerThrottlingForBackgroundTabsEnabled,
      RuntimeEnabledFeatures::SetTimerThrottlingForBackgroundTabsEnabled>
      ScopedBackgroundTabTimerThrottlingForTest;

  std::unique_ptr<ScopedBackgroundTabTimerThrottlingForTest>
      background_tab_timer_throttling_feature_;
};

TEST_F(DisableBackgroundThrottlingIsRespectedTest,
       DisableBackgroundThrottlingIsRespected) {
  SimRequest main_resource("https://example.com/", "text/html");

  LoadURL("https://example.com/");

  main_resource.Complete(
      "(<script>"
      "  function f(repetitions) {"
      "     if (repetitions == 0) return;"
      "     console.log('called f');"
      "     setTimeout(f, 10, repetitions - 1);"
      "  }"
      "  f(5);"
      "</script>)");

  Platform::Current()
      ->CurrentThread()
      ->Scheduler()
      ->GetRendererSchedulerForTest()
      ->SetRendererBackgrounded(true);

  // Run delayed tasks for 1 second. All tasks should be completed
  // with throttling disabled.
  testing::RunDelayedTasks(1000);

  EXPECT_THAT(ConsoleMessages(), ElementsAre("called f", "called f", "called f",
                                             "called f", "called f"));
}

class BackgroundRendererThrottlingTest : public SimTest {};

TEST_F(BackgroundRendererThrottlingTest,
       DISABLED_BackgroundRenderersAreThrottled) {
  SimRequest main_resource("https://example.com/", "text/html");

  LoadURL("https://example.com/");

  main_resource.Complete(
      "(<script>"
      "  function f(repetitions) {"
      "     if (repetitions == 0) return;"
      "     console.log('called f');"
      "     setTimeout(f, 10, repetitions - 1);"
      "  }"
      "  setTimeout(f, 10, 3);"
      "</script>)");

  Platform::Current()
      ->CurrentThread()
      ->Scheduler()
      ->GetRendererSchedulerForTest()
      ->SetRendererBackgrounded(true);

  // Make sure that we run a task once a second.
  for (int i = 0; i < 3; ++i) {
    testing::RunDelayedTasks(1000);
    EXPECT_THAT(ConsoleMessages(), ElementsAre("called f"));
    ConsoleMessages().clear();
  }
}

}  // namespace blink
