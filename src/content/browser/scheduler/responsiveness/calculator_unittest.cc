// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/scheduler/responsiveness/calculator.h"

#include "build/build_config.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace responsiveness {

using JankType = Calculator::JankType;
using ::testing::_;

namespace {
// Copied from calculator.cc.
constexpr int kMeasurementIntervalInMs = 30 * 1000;
constexpr int kJankThresholdInMs = 100;

class FakeCalculator : public Calculator {
 public:
  MOCK_METHOD3(EmitResponsiveness,
               void(JankType jank_type,
                    size_t janky_slices,
                    bool was_process_suspended));

  using Calculator::GetLastCalculationTime;
};

}  // namespace

class ResponsivenessCalculatorTest : public testing::Test {
 public:
  void SetUp() override {
    calculator_ = std::make_unique<testing::StrictMock<FakeCalculator>>();
    last_calculation_time_ = calculator_->GetLastCalculationTime();
#if defined(OS_ANDROID)
    base::android::ApplicationStatusListener::NotifyApplicationStateChange(
        base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES);
    base::RunLoop().RunUntilIdle();
#endif
  }

  void AddEventUI(int queue_time_in_ms,
                  int execution_start_time_in_ms,
                  int execution_finish_time_in_ms) {
    calculator_->TaskOrEventFinishedOnUIThread(
        last_calculation_time_ +
            base::TimeDelta::FromMilliseconds(queue_time_in_ms),
        last_calculation_time_ +
            base::TimeDelta::FromMilliseconds(execution_start_time_in_ms),
        last_calculation_time_ +
            base::TimeDelta::FromMilliseconds(execution_finish_time_in_ms));
  }

  void AddEventIO(int queue_time_in_ms,
                  int execution_start_time_in_ms,
                  int execution_finish_time_in_ms) {
    calculator_->TaskOrEventFinishedOnIOThread(
        last_calculation_time_ +
            base::TimeDelta::FromMilliseconds(queue_time_in_ms),
        last_calculation_time_ +
            base::TimeDelta::FromMilliseconds(execution_start_time_in_ms),
        last_calculation_time_ +
            base::TimeDelta::FromMilliseconds(execution_finish_time_in_ms));
  }

  void TriggerCalculation() {
    AddEventUI(kMeasurementIntervalInMs + 1, kMeasurementIntervalInMs + 1,
               kMeasurementIntervalInMs + 1);
    last_calculation_time_ = calculator_->GetLastCalculationTime();
  }

 protected:
  // This member sets up BrowserThread::IO and BrowserThread::UI. It must be the
  // first member, as other members may depend on these abstractions.
  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<FakeCalculator> calculator_;
  base::TimeTicks last_calculation_time_;
};

#define EXPECT_EXECUTION_JANKY_SLICES(num_slices) \
  EXPECT_CALL(*calculator_,                       \
              EmitResponsiveness(JankType::kExecution, num_slices, false));
#define EXPECT_QUEUE_AND_EXECUTION_JANKY_SLICES(num_slices)                  \
  EXPECT_CALL(*calculator_, EmitResponsiveness(JankType::kQueueAndExecution, \
                                               num_slices, false));

// A single event executing slightly longer than kJankThresholdInMs.
TEST_F(ResponsivenessCalculatorTest, ShortExecutionJank) {
  constexpr int kQueueTime = 35;
  constexpr int kStartTime = 40;
  constexpr int kFinishTime = kStartTime + kJankThresholdInMs + 5;

  AddEventUI(kQueueTime, kStartTime, kFinishTime);
  EXPECT_EXECUTION_JANKY_SLICES(1u);
  EXPECT_QUEUE_AND_EXECUTION_JANKY_SLICES(1u);
  TriggerCalculation();
}

// A single event queued slightly longer than kJankThresholdInMs.
TEST_F(ResponsivenessCalculatorTest, ShortQueueJank) {
  constexpr int kQueueTime = 35;
  constexpr int kStartTime = kQueueTime + kJankThresholdInMs + 5;
  constexpr int kFinishTime = kStartTime + 5;

  AddEventUI(kQueueTime, kStartTime, kFinishTime);
  EXPECT_EXECUTION_JANKY_SLICES(0u);
  EXPECT_QUEUE_AND_EXECUTION_JANKY_SLICES(1u);
  TriggerCalculation();
}

// A single event whose queuing and execution time together take longer than
// kJankThresholdInMs.
TEST_F(ResponsivenessCalculatorTest, ShortCombinedQueueAndExecutionJank) {
  constexpr int kQueueTime = 35;
  constexpr int kStartTime = kQueueTime + (kJankThresholdInMs / 2);
  constexpr int kFinishTime = kStartTime + (kJankThresholdInMs / 2) + 1;

  AddEventUI(kQueueTime, kStartTime, kFinishTime);
  EXPECT_EXECUTION_JANKY_SLICES(0u);
  EXPECT_QUEUE_AND_EXECUTION_JANKY_SLICES(1u);
  TriggerCalculation();
}

// A single event executing slightly longer than 10 * kJankThresholdInMs.
TEST_F(ResponsivenessCalculatorTest, LongExecutionJank) {
  constexpr int kQueueTime = 35;
  constexpr int kStartTime = 40;
  constexpr int kFinishTime = kStartTime + 10 * kJankThresholdInMs + 5;

  AddEventUI(kQueueTime, kStartTime, kFinishTime);
  EXPECT_EXECUTION_JANKY_SLICES(10u);
  EXPECT_QUEUE_AND_EXECUTION_JANKY_SLICES(10u);
  TriggerCalculation();
}

// A single event executing slightly longer than 10 * kJankThresholdInMs.
TEST_F(ResponsivenessCalculatorTest, LongQueueJank) {
  constexpr int kQueueTime = 35;
  constexpr int kStartTime = kQueueTime + 10 * kJankThresholdInMs + 5;
  constexpr int kFinishTime = kStartTime + 5;

  AddEventUI(kQueueTime, kStartTime, kFinishTime);
  EXPECT_EXECUTION_JANKY_SLICES(0u);
  EXPECT_QUEUE_AND_EXECUTION_JANKY_SLICES(10u);
  TriggerCalculation();
}

// Events that execute in less than 100ms do not jank, regardless of start time.
TEST_F(ResponsivenessCalculatorTest, NoExecutionJank) {
  int base_time = 30;
  for (int i = 0; i < kJankThresholdInMs; ++i) {
    AddEventUI(base_time, base_time, base_time + i);
  }

  base_time += kJankThresholdInMs;
  for (int i = 0; i < kJankThresholdInMs; ++i) {
    AddEventUI(base_time + i, base_time + i, base_time + 2 * i);
  }

  EXPECT_EXECUTION_JANKY_SLICES(0u);
  EXPECT_QUEUE_AND_EXECUTION_JANKY_SLICES(0u);
  TriggerCalculation();
}

// Events that are queued and execute in less than 100ms do not jank, regardless
// of start time.
TEST_F(ResponsivenessCalculatorTest, NoQueueJank) {
  int base_time = 30;
  for (int i = 0; i < kJankThresholdInMs; ++i) {
    AddEventUI(base_time, base_time + i, base_time + i);
  }

  base_time += kJankThresholdInMs;
  for (int i = 0; i < kJankThresholdInMs; ++i) {
    AddEventUI(base_time + i, base_time + 2 * i, base_time + 2 * i);
  }

  EXPECT_EXECUTION_JANKY_SLICES(0u);
  EXPECT_QUEUE_AND_EXECUTION_JANKY_SLICES(0u);
  TriggerCalculation();
}

// 10 execution jank events, but very closely overlapping. Time slices are
// discretized and fixed, e.g. [0 100] [100 200] [200 300]. In this test, the
// events all start in the [0 100] slice and end in the [100 200] slice. All of
// them end up marking the [100 200] slice as janky.
TEST_F(ResponsivenessCalculatorTest, OverlappingExecutionJank) {
  int base_time = 30;
  for (int i = 0; i < 10; ++i) {
    const int queue_time = base_time;
    const int start_time = base_time;
    const int finish_time = start_time + kJankThresholdInMs + i;
    AddEventUI(queue_time, start_time, finish_time);
  }

  EXPECT_EXECUTION_JANKY_SLICES(1u);
  EXPECT_QUEUE_AND_EXECUTION_JANKY_SLICES(1u);
  TriggerCalculation();
}

// 10 queue jank events, but very closely overlapping. Time slices are
// discretized and fixed, e.g. [0 100] [100 200] [200 300]. In this test, the
// events are all queued in the [0 100] slice and start executing in the [100
// 200] slice. All of them end up marking the [100 200] slice as janky.
TEST_F(ResponsivenessCalculatorTest, OverlappingQueueJank) {
  int base_time = 30;
  for (int i = 0; i < 10; ++i) {
    const int queue_time = base_time;
    const int start_time = base_time + kJankThresholdInMs + i;
    const int finish_time = start_time + 1;
    AddEventUI(queue_time, start_time, finish_time);
  }

  EXPECT_EXECUTION_JANKY_SLICES(0u);
  EXPECT_QUEUE_AND_EXECUTION_JANKY_SLICES(1u);
  TriggerCalculation();
}

// UI thread has 3 execution jank events on slices 1, 2, 3
// IO thread has 3 execution jank events on slices 3, 4, 5,
// There should be a total of 5 jank events.
TEST_F(ResponsivenessCalculatorTest, OverlappingExecutionJankMultipleThreads) {
  int base_time = 105;
  for (int i = 0; i < 3; ++i) {
    const int queue_time = base_time + i * kJankThresholdInMs;
    const int start_time = queue_time;
    const int finish_time = start_time + kJankThresholdInMs + 10;
    AddEventUI(queue_time, start_time, finish_time);
  }

  base_time = 305;
  for (int i = 0; i < 3; ++i) {
    const int queue_time = base_time + i * kJankThresholdInMs;
    const int start_time = queue_time;
    const int finish_time = start_time + kJankThresholdInMs + 10;
    AddEventIO(queue_time, start_time, finish_time);
  }

  EXPECT_EXECUTION_JANKY_SLICES(5u);
  EXPECT_QUEUE_AND_EXECUTION_JANKY_SLICES(5u);
  TriggerCalculation();
}

// UI thread has 3 queue jank events on slices 1, 2, 3
// IO thread has 3 queue jank events on slices 3, 4, 5,
// There should be a total of 5 jank events.
TEST_F(ResponsivenessCalculatorTest, OverlappingQueueJankMultipleThreads) {
  int base_time = 105;
  for (int i = 0; i < 3; ++i) {
    const int queue_time = base_time + i * kJankThresholdInMs;
    const int start_time = queue_time + kJankThresholdInMs + 10;
    const int finish_time = start_time;
    AddEventUI(queue_time, start_time, finish_time);
  }

  base_time = 305;
  for (int i = 0; i < 3; ++i) {
    const int queue_time = base_time + i * kJankThresholdInMs;
    const int start_time = queue_time + kJankThresholdInMs + 10;
    const int finish_time = start_time;
    AddEventIO(queue_time, start_time, finish_time);
  }

  EXPECT_EXECUTION_JANKY_SLICES(0u);
  EXPECT_QUEUE_AND_EXECUTION_JANKY_SLICES(5u);
  TriggerCalculation();
}

// Three execution janks, each of length 2, separated by some shorter events.
TEST_F(ResponsivenessCalculatorTest, SeparatedExecutionJanks) {
  int base_time = 105;

  for (int i = 0; i < 3; ++i) {
    {
      const int queue_time = base_time;
      const int start_time = base_time;
      const int finish_time = base_time + 1;
      AddEventUI(queue_time, start_time, finish_time);
    }
    {
      const int queue_time = base_time;
      const int start_time = base_time;
      const int finish_time = base_time + 2 * kJankThresholdInMs + 1;
      AddEventUI(queue_time, start_time, finish_time);
    }
    base_time += 10 * kJankThresholdInMs;
  }

  EXPECT_EXECUTION_JANKY_SLICES(6u);
  EXPECT_QUEUE_AND_EXECUTION_JANKY_SLICES(6u);
  TriggerCalculation();
}

// Three queue janks, each of length 2, separated by some shorter events.
TEST_F(ResponsivenessCalculatorTest, SeparatedQueueJanks) {
  int base_time = 105;

  for (int i = 0; i < 3; ++i) {
    {
      const int queue_time = base_time;
      const int start_time = base_time + 1;
      const int finish_time = start_time;
      AddEventUI(queue_time, start_time, finish_time);
    }
    {
      const int queue_time = base_time;
      const int start_time = base_time + 2 * kJankThresholdInMs + 1;
      const int finish_time = start_time;
      AddEventUI(queue_time, start_time, finish_time);
    }
    base_time += 10 * kJankThresholdInMs;
  }

  EXPECT_EXECUTION_JANKY_SLICES(0u);
  EXPECT_QUEUE_AND_EXECUTION_JANKY_SLICES(6u);
  TriggerCalculation();
}

TEST_F(ResponsivenessCalculatorTest, MultipleTrigger) {
  int base_time = 105;

  // 3 Janks, then trigger, then repeat.
  for (int i = 0; i < 10; ++i) {
    for (int j = 0; j < 3; ++j) {
      AddEventUI(base_time, base_time, base_time + 3 * kJankThresholdInMs + 1);
      base_time += 3 * kJankThresholdInMs;
    }

    EXPECT_EXECUTION_JANKY_SLICES(9u);
    EXPECT_QUEUE_AND_EXECUTION_JANKY_SLICES(9u);
    TriggerCalculation();
    testing::Mock::VerifyAndClear(calculator_.get());
  }
}

// A long delay means that the machine likely went to sleep.
TEST_F(ResponsivenessCalculatorTest, LongDelay) {
  int base_time = 105;
  AddEventUI(base_time, base_time, base_time + 3 * kJankThresholdInMs + 1);
  base_time += 10 * kMeasurementIntervalInMs;
  AddEventUI(base_time, base_time, base_time + 1);

  EXPECT_CALL(*calculator_, EmitResponsiveness(_, _, _)).Times(0);
}

// A long event means that the machine likely went to sleep.
TEST_F(ResponsivenessCalculatorTest, LongEvent) {
  int base_time = 105;
  AddEventUI(base_time, base_time, base_time + 10 * kMeasurementIntervalInMs);

  EXPECT_CALL(*calculator_, EmitResponsiveness(_, _, _)).Times(0);
}

#if defined(OS_ANDROID)
// Metric should not be recorded when application is in background.
TEST_F(ResponsivenessCalculatorTest, ApplicationInBackground) {
  constexpr int kQueueTime = 35;
  constexpr int kStartTime = 40;
  constexpr int kFinishTime = kStartTime + kJankThresholdInMs + 5;
  AddEventUI(kQueueTime, kStartTime, kFinishTime);

  base::android::ApplicationStatusListener::NotifyApplicationStateChange(
      base::android::APPLICATION_STATE_HAS_STOPPED_ACTIVITIES);
  base::RunLoop().RunUntilIdle();

  AddEventUI(kQueueTime, kStartTime + 1, kFinishTime + 1);

  EXPECT_CALL(*calculator_, EmitResponsiveness(_, _, _)).Times(0);
  TriggerCalculation();
}
#endif

// The suspended state must be passed to EmitResponsiveness(...).
// A single event executing slightly longer than 10 * kJankThresholdInMs.
TEST_F(ResponsivenessCalculatorTest, JankWithPowerSuspend) {
  constexpr int kQueueTime = 35;
  constexpr int kStartTime = 40;
  constexpr int kFinishTime = kStartTime + 10 * kJankThresholdInMs + 5;

  AddEventUI(kQueueTime, kStartTime, kFinishTime);
  EXPECT_CALL(*calculator_,
              EmitResponsiveness(JankType::kExecution, 10, false));
  EXPECT_CALL(*calculator_,
              EmitResponsiveness(JankType::kQueueAndExecution, 10, false));
  TriggerCalculation();

  calculator_->SetProcessSuspended(true);
  AddEventUI(kQueueTime, kStartTime, kFinishTime);
  EXPECT_CALL(*calculator_, EmitResponsiveness(JankType::kExecution, 10, true));
  EXPECT_CALL(*calculator_,
              EmitResponsiveness(JankType::kQueueAndExecution, 10, true));
  TriggerCalculation();

  calculator_->SetProcessSuspended(false);
  AddEventUI(kQueueTime, kStartTime, kFinishTime);
  EXPECT_CALL(*calculator_, EmitResponsiveness(JankType::kExecution, 10, true));
  EXPECT_CALL(*calculator_,
              EmitResponsiveness(JankType::kQueueAndExecution, 10, true));
  TriggerCalculation();

  calculator_->SetProcessSuspended(true);
  AddEventUI(kQueueTime, kStartTime, kFinishTime);
  calculator_->SetProcessSuspended(false);
  EXPECT_CALL(*calculator_, EmitResponsiveness(JankType::kExecution, 10, true));
  EXPECT_CALL(*calculator_,
              EmitResponsiveness(JankType::kQueueAndExecution, 10, true));
  TriggerCalculation();

  // The whole slice must be flagged as containing suspend/resume events.
  calculator_->SetProcessSuspended(true);
  calculator_->SetProcessSuspended(false);
  AddEventUI(kQueueTime, kStartTime, kFinishTime);
  EXPECT_CALL(*calculator_, EmitResponsiveness(JankType::kExecution, 10, true));
  EXPECT_CALL(*calculator_,
              EmitResponsiveness(JankType::kQueueAndExecution, 10, true));
  TriggerCalculation();

  AddEventUI(kQueueTime, kStartTime, kFinishTime);
  EXPECT_CALL(*calculator_,
              EmitResponsiveness(JankType::kExecution, 10, false));
  EXPECT_CALL(*calculator_,
              EmitResponsiveness(JankType::kQueueAndExecution, 10, false));
  TriggerCalculation();
}

// An event execution that crosses a measurement interval boundary should count
// towards both measurement intervals.
TEST_F(ResponsivenessCalculatorTest, ExecutionCrossesBoundary) {
  // Dummy event so that Calculator doesn't think the process is suspended.
  {
    const int kTime = 0.5 * kMeasurementIntervalInMs;
    AddEventUI(kTime, kTime, kTime);
  }

  // The event goes from [29801, 30150]. It should count as 1 jank in the first
  // measurement interval and 2 in the second.
  {
    EXPECT_EXECUTION_JANKY_SLICES(1u);
    EXPECT_QUEUE_AND_EXECUTION_JANKY_SLICES(1u);
    const int queue_time =
        kMeasurementIntervalInMs - 2 * kJankThresholdInMs + 1;
    const int start_time = queue_time;
    const int finish_time = kMeasurementIntervalInMs + 1.5 * kJankThresholdInMs;
    AddEventUI(queue_time, start_time, finish_time);
  }

  // Dummy event so that Calculator doesn't think the process is suspended.
  {
    const int kTime = 1.5 * kMeasurementIntervalInMs;
    AddEventUI(kTime, kTime, kTime);
  }

  // Trigger another calculation.
  EXPECT_EXECUTION_JANKY_SLICES(2u);
  EXPECT_QUEUE_AND_EXECUTION_JANKY_SLICES(2u);

  const int kTime = 2 * kMeasurementIntervalInMs + 1;
  AddEventUI(kTime, kTime, kTime);
}

// An event queuing that crosses a measurement interval boundary should count
// towards both measurement intervals.
TEST_F(ResponsivenessCalculatorTest, QueuingCrossesBoundary) {
  // Dummy event so that Calculator doesn't think the process is suspended.
  {
    const int kTime = 0.5 * kMeasurementIntervalInMs;
    AddEventUI(kTime, kTime, kTime);
  }

  // The event goes from [29801, 30150]. It should count as 1 jank in the first
  // measurement interval and 2 in the second.
  {
    EXPECT_EXECUTION_JANKY_SLICES(0u);
    EXPECT_QUEUE_AND_EXECUTION_JANKY_SLICES(1u);
    const int queue_time =
        kMeasurementIntervalInMs - 2 * kJankThresholdInMs + 1;
    const int start_time = kMeasurementIntervalInMs + 1.5 * kJankThresholdInMs;
    const int finish_time = start_time;
    AddEventUI(queue_time, start_time, finish_time);
  }

  // Dummy event so that Calculator doesn't think the process is suspended.
  {
    const int kTime = 1.5 * kMeasurementIntervalInMs;
    AddEventUI(kTime, kTime, kTime);
  }

  // Trigger another calculation.
  EXPECT_EXECUTION_JANKY_SLICES(0u);
  EXPECT_QUEUE_AND_EXECUTION_JANKY_SLICES(2u);

  const int kTime = 2 * kMeasurementIntervalInMs + 1;
  AddEventUI(kTime, kTime, kTime);
}

// Events may not be ordered by start or end time.
TEST_F(ResponsivenessCalculatorTest, UnorderedEvents) {
  // We add the following tasks:
  //   [100, 100, 250]
  //   [150, 150, 300]
  //   [50, 50, 200]
  //   [50, 50, 390] <- A
  //
  //   [1100, 1250, 1251]
  //   [1150, 1300, 1301]
  //   [1050, 1200, 1201]
  //   [1050, 1390, 1391] <- B
  //
  // The execution jank in A subsumes all other execution janks. The queue jank
  // in B subsumes all other queue janks.
  AddEventUI(100, 100, 250);
  AddEventUI(150, 150, 300);
  AddEventUI(50, 50, 200);
  AddEventUI(50, 50, 390);

  AddEventUI(1100, 1250, 1251);
  AddEventUI(1150, 1300, 1301);
  AddEventUI(1050, 1200, 1201);
  AddEventUI(1050, 1390, 1391);

  EXPECT_EXECUTION_JANKY_SLICES(3u);
  EXPECT_QUEUE_AND_EXECUTION_JANKY_SLICES(6u);
  TriggerCalculation();
}

}  // namespace responsiveness
}  // namespace content
