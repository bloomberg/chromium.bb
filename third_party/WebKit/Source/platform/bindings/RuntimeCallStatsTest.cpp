// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/bindings/RuntimeCallStats.h"

#include "platform/RuntimeEnabledFeatures.h"
#include "platform/wtf/CurrentTime.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

TimeTicks ticks_;

void AdvanceClock(int milliseconds) {
  ticks_ += TimeDelta::FromMilliseconds(milliseconds);
}

RuntimeCallStats::CounterId test_counter_1_id =
    RuntimeCallStats::CounterId::kTestCounter1;
RuntimeCallStats::CounterId test_counter_2_id =
    RuntimeCallStats::CounterId::kTestCounter2;

}  // namespace

class RuntimeCallStatsTest : public ::testing::Test {
 public:
  void SetUp() override {
    // Add one millisecond because RuntimeCallTimer uses |start_ticks_| =
    // TimeTicks() to represent that the timer is not running.
    ticks_ = WTF::TimeTicks() + TimeDelta::FromMilliseconds(1);
    original_time_function_ =
        SetTimeFunctionsForTesting([] { return ticks_.InSeconds(); });
  }

  void TearDown() override {
    SetTimeFunctionsForTesting(original_time_function_);
    features_backup_.Restore();
  }

 private:
  TimeFunction original_time_function_;
  RuntimeEnabledFeatures::Backup features_backup_;
};

TEST_F(RuntimeCallStatsTest, InitialCountShouldBeZero) {
  RuntimeCallCounter counter("counter");
  EXPECT_EQ(0ul, counter.GetCount());
}

TEST_F(RuntimeCallStatsTest, StatsCounterNameIsCorrect) {
  RuntimeCallStats stats;
  EXPECT_STREQ("Blink_TestCounter1",
               stats.GetCounter(test_counter_1_id)->GetName());
}

TEST_F(RuntimeCallStatsTest, TestBindingsCountersForMethods) {
  RuntimeCallStats stats;
  RuntimeCallCounter* method_counter =
      stats.GetCounter(RuntimeCallStats::CounterId::kBindingsMethodTestCounter);
  EXPECT_STREQ("Blink_BindingsMethodTestCounter", method_counter->GetName());
}

TEST_F(RuntimeCallStatsTest, TestBindingsCountersForReadOnlyAttributes) {
  RuntimeCallStats stats;
  RuntimeCallCounter* getter_counter =
      stats.GetCounter(RuntimeCallStats::CounterId::
                           kBindingsReadOnlyAttributeTestCounter_Getter);
  EXPECT_STREQ("Blink_BindingsReadOnlyAttributeTestCounter_Getter",
               getter_counter->GetName());
}

TEST_F(RuntimeCallStatsTest, TestBindingsCountersForAttributes) {
  RuntimeCallStats stats;
  RuntimeCallCounter* getter_counter = stats.GetCounter(
      RuntimeCallStats::CounterId::kBindingsAttributeTestCounter_Getter);
  RuntimeCallCounter* setter_counter = stats.GetCounter(
      RuntimeCallStats::CounterId::kBindingsAttributeTestCounter_Setter);
  EXPECT_STREQ("Blink_BindingsAttributeTestCounter_Getter",
               getter_counter->GetName());
  EXPECT_STREQ("Blink_BindingsAttributeTestCounter_Setter",
               setter_counter->GetName());
}

TEST_F(RuntimeCallStatsTest, CountIsUpdatedAfterLeave) {
  RuntimeCallTimer timer;
  RuntimeCallStats stats;
  RuntimeCallCounter* counter = stats.GetCounter(test_counter_1_id);

  stats.Enter(&timer, test_counter_1_id);
  EXPECT_EQ(0ul, counter->GetCount());
  stats.Leave(&timer);
  EXPECT_EQ(1ul, counter->GetCount());
}

TEST_F(RuntimeCallStatsTest, TimeIsUpdatedAfterLeave) {
  RuntimeCallStats stats;
  RuntimeCallTimer timer;
  RuntimeCallCounter* counter = stats.GetCounter(test_counter_1_id);

  stats.Enter(&timer, test_counter_1_id);
  AdvanceClock(50);
  stats.Leave(&timer);
  EXPECT_EQ(50, counter->GetTime().InMilliseconds());
}

TEST_F(RuntimeCallStatsTest, CountAndTimeAreUpdatedAfterMultipleExecutions) {
  RuntimeCallStats stats;
  RuntimeCallCounter* counter = stats.GetCounter(test_counter_1_id);

  const unsigned func_duration = 20;
  const unsigned loops = 5;

  auto func = [&stats, func_duration]() {
    RuntimeCallTimer timer;
    stats.Enter(&timer, test_counter_1_id);
    AdvanceClock(func_duration);
    stats.Leave(&timer);
  };

  for (unsigned i = 0; i < loops; i++)
    func();

  EXPECT_EQ((uint64_t)loops, counter->GetCount());
  EXPECT_EQ(loops * func_duration, counter->GetTime().InMilliseconds());
}

TEST_F(RuntimeCallStatsTest, NestedTimersTest) {
  RuntimeCallStats stats;
  RuntimeCallCounter* outer_counter = stats.GetCounter(test_counter_1_id);
  RuntimeCallCounter* inner_counter = stats.GetCounter(test_counter_2_id);

  const unsigned inner_func_duration = 50;
  const unsigned outer_func_duration = 20;

  auto inner_func = [&stats, inner_func_duration]() {
    RuntimeCallTimer timer;
    stats.Enter(&timer, test_counter_2_id);
    AdvanceClock(inner_func_duration);
    stats.Leave(&timer);
  };

  auto outer_func = [&stats, &inner_func, outer_func_duration]() {
    RuntimeCallTimer timer;
    stats.Enter(&timer, test_counter_1_id);
    inner_func();
    AdvanceClock(outer_func_duration);
    stats.Leave(&timer);
  };

  outer_func();

  EXPECT_EQ(1ul, outer_counter->GetCount());
  EXPECT_EQ(1ul, inner_counter->GetCount());
  EXPECT_EQ(outer_func_duration, outer_counter->GetTime().InMilliseconds());
  EXPECT_EQ(inner_func_duration, inner_counter->GetTime().InMilliseconds());
}

TEST_F(RuntimeCallStatsTest, RuntimeCallTimerScopeTest) {
  RuntimeCallStats stats;
  RuntimeCallCounter* counter = stats.GetCounter(test_counter_1_id);

  auto func = [&stats]() {
    RuntimeCallTimerScope scope(&stats, test_counter_1_id);
    AdvanceClock(50);
  };

  func();

  EXPECT_EQ(1ul, counter->GetCount());
  EXPECT_EQ(50, counter->GetTime().InMilliseconds());

  func();

  EXPECT_EQ(2ul, counter->GetCount());
  EXPECT_EQ(100, counter->GetTime().InMilliseconds());
}

TEST_F(RuntimeCallStatsTest, RecursiveFunctionWithScopeTest) {
  RuntimeCallStats stats;
  RuntimeCallCounter* counter = stats.GetCounter(test_counter_1_id);

  std::function<void(int)> recursive_func;
  recursive_func = [&stats, &recursive_func](int x) {
    RuntimeCallTimerScope scope(&stats, test_counter_1_id);
    if (x <= 0)
      return;
    AdvanceClock(50);
    recursive_func(x - 1);
  };
  recursive_func(5);

  EXPECT_EQ(6ul, counter->GetCount());
  EXPECT_EQ(250, counter->GetTime().InMilliseconds());
}

TEST_F(RuntimeCallStatsTest, ReuseTimer) {
  RuntimeCallStats stats;
  RuntimeCallTimer timer;
  RuntimeCallCounter* counter1 = stats.GetCounter(test_counter_1_id);
  RuntimeCallCounter* counter2 = stats.GetCounter(test_counter_2_id);

  stats.Enter(&timer, test_counter_1_id);
  AdvanceClock(50);
  stats.Leave(&timer);

  timer.Reset();

  stats.Enter(&timer, test_counter_2_id);
  AdvanceClock(25);
  stats.Leave(&timer);

  EXPECT_EQ(1ul, counter1->GetCount());
  EXPECT_EQ(1ul, counter2->GetCount());
  EXPECT_EQ(50, counter1->GetTime().InMilliseconds());
  EXPECT_EQ(25, counter2->GetTime().InMilliseconds());
}

TEST_F(RuntimeCallStatsTest, ResetCallStats) {
  RuntimeCallStats stats;
  RuntimeCallCounter* counter1 = stats.GetCounter(test_counter_1_id);
  RuntimeCallCounter* counter2 = stats.GetCounter(test_counter_2_id);

  {
    RuntimeCallTimerScope scope1(&stats, test_counter_1_id);
    RuntimeCallTimerScope scope2(&stats, test_counter_2_id);
  }

  EXPECT_EQ(1ul, counter1->GetCount());
  EXPECT_EQ(1ul, counter2->GetCount());

  stats.Reset();

  EXPECT_EQ(0ul, counter1->GetCount());
  EXPECT_EQ(0ul, counter2->GetCount());
}

TEST_F(RuntimeCallStatsTest, TestEnterAndLeaveMacrosWithCallStatsDisabled) {
  RuntimeEnabledFeatures::SetBlinkRuntimeCallStatsEnabled(false);
  RuntimeCallStats stats;
  RuntimeCallCounter* counter = stats.GetCounter(test_counter_1_id);
  RuntimeCallTimer timer;

  RUNTIME_CALL_STATS_ENTER_WITH_RCS(&stats, &timer, test_counter_1_id);
  AdvanceClock(25);
  RUNTIME_CALL_STATS_LEAVE_WITH_RCS(&stats, &timer);

  EXPECT_EQ(0ul, counter->GetCount());
  EXPECT_EQ(0, counter->GetTime().InMilliseconds());
}

TEST_F(RuntimeCallStatsTest, TestEnterAndLeaveMacrosWithCallStatsEnabled) {
  RuntimeEnabledFeatures::SetBlinkRuntimeCallStatsEnabled(true);
  RuntimeCallStats stats;
  RuntimeCallCounter* counter = stats.GetCounter(test_counter_1_id);
  RuntimeCallTimer timer;

  RUNTIME_CALL_STATS_ENTER_WITH_RCS(&stats, &timer, test_counter_1_id);
  AdvanceClock(25);
  RUNTIME_CALL_STATS_LEAVE_WITH_RCS(&stats, &timer);

  EXPECT_EQ(1ul, counter->GetCount());
  EXPECT_EQ(25, counter->GetTime().InMilliseconds());
}

TEST_F(RuntimeCallStatsTest, TestScopeMacroWithCallStatsDisabled) {
  RuntimeEnabledFeatures::SetBlinkRuntimeCallStatsEnabled(false);
  RuntimeCallStats stats;
  RuntimeCallCounter* counter = stats.GetCounter(test_counter_1_id);

  {
    RUNTIME_CALL_TIMER_SCOPE_WITH_RCS(&stats, test_counter_1_id);
    AdvanceClock(25);
  }

  EXPECT_EQ(0ul, counter->GetCount());
  EXPECT_EQ(0, counter->GetTime().InMilliseconds());
}

TEST_F(RuntimeCallStatsTest, TestScopeMacroWithCallStatsEnabled) {
  RuntimeEnabledFeatures::SetBlinkRuntimeCallStatsEnabled(true);
  RuntimeCallStats stats;
  RuntimeCallCounter* counter = stats.GetCounter(test_counter_1_id);

  {
    RUNTIME_CALL_TIMER_SCOPE_WITH_RCS(&stats, test_counter_1_id);
    AdvanceClock(25);
  }

  EXPECT_EQ(1ul, counter->GetCount());
  EXPECT_EQ(25, counter->GetTime().InMilliseconds());
}

TEST_F(RuntimeCallStatsTest, TestScopeWithOptionalMacroWithCallStatsDisabled) {
  RuntimeEnabledFeatures::SetBlinkRuntimeCallStatsEnabled(false);
  RuntimeCallStats stats;
  RuntimeCallCounter* counter = stats.GetCounter(test_counter_1_id);

  {
    Optional<RuntimeCallTimerScope> scope;
    RUNTIME_CALL_TIMER_SCOPE_WITH_OPTIONAL_RCS(scope, &stats,
                                               test_counter_1_id);
    AdvanceClock(25);
  }

  EXPECT_EQ(0ul, counter->GetCount());
  EXPECT_EQ(0, counter->GetTime().InMilliseconds());
}

TEST_F(RuntimeCallStatsTest, TestScopeWithOptionalMacroWithCallStatsEnabled) {
  RuntimeEnabledFeatures::SetBlinkRuntimeCallStatsEnabled(true);
  RuntimeCallStats stats;
  RuntimeCallCounter* counter = stats.GetCounter(test_counter_1_id);

  {
    Optional<RuntimeCallTimerScope> scope;
    RUNTIME_CALL_TIMER_SCOPE_WITH_OPTIONAL_RCS(scope, &stats,
                                               test_counter_1_id);
    AdvanceClock(25);
  }

  EXPECT_EQ(1ul, counter->GetCount());
  EXPECT_EQ(25, counter->GetTime().InMilliseconds());
}

}  // namespace blink
