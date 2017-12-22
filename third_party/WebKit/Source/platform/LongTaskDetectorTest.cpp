// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/LongTaskDetector.h"

#include "platform/CrossThreadFunctional.h"
#include "platform/WebTaskRunner.h"
#include "platform/testing/TestingPlatformSupportWithMockScheduler.h"
#include "platform/wtf/Time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {
class TestLongTaskObserver :
    // This has to be garbage collected since LongTaskObserver uses
    // GarbageCollectedMixin.
    public GarbageCollectedFinalized<TestLongTaskObserver>,
    public LongTaskObserver {
  USING_GARBAGE_COLLECTED_MIXIN(TestLongTaskObserver);

 public:
  double last_long_task_start = 0.0;
  double last_long_task_end = 0.0;

  // LongTaskObserver implementation.
  void OnLongTaskDetected(double start_time, double end_time) override {
    last_long_task_start = start_time;
    last_long_task_end = end_time;
  }
};  // Anonymous namespace

}  // namespace
class LongTaskDetectorTest : public ::testing::Test {
 public:
  // Public because it's executed on a task queue.
  void DummyTaskWithDuration(double duration_seconds) {
    dummy_task_start_time_ = CurrentTimeTicksInSeconds();
    platform_->AdvanceClockSeconds(duration_seconds);
    dummy_task_end_time_ = CurrentTimeTicksInSeconds();
  }

 protected:
  void SetUp() {
    // For some reason, platform needs to run for non-zero seconds before we
    // start posting tasks to it. Otherwise TaskTimeObservers don't get notified
    // of tasks.
    platform_->RunForPeriodSeconds(1);
  }
  double DummyTaskStartTime() { return dummy_task_start_time_; }

  double DummyTaskEndTime() { return dummy_task_end_time_; }

  void SimulateTask(double duration_seconds) {
    PostCrossThreadTask(
        *platform_->CurrentThread()->GetWebTaskRunner(), FROM_HERE,
        CrossThreadBind(&LongTaskDetectorTest::DummyTaskWithDuration,
                        CrossThreadUnretained(this), duration_seconds));
    platform_->RunUntilIdle();
  }

  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform_;

 private:
  double dummy_task_start_time_ = 0.0;
  double dummy_task_end_time_ = 0.0;
};

TEST_F(LongTaskDetectorTest, DeliversLongTaskNotificationOnlyWhenRegistered) {
  TestLongTaskObserver* long_task_observer = new TestLongTaskObserver();
  SimulateTask(LongTaskDetector::kLongTaskThresholdSeconds + 0.01);
  EXPECT_EQ(long_task_observer->last_long_task_end, 0.0);

  LongTaskDetector::Instance().RegisterObserver(long_task_observer);
  SimulateTask(LongTaskDetector::kLongTaskThresholdSeconds + 0.01);
  double long_task_end_when_registered = DummyTaskEndTime();
  EXPECT_EQ(long_task_observer->last_long_task_start, DummyTaskStartTime());
  EXPECT_EQ(long_task_observer->last_long_task_end,
            long_task_end_when_registered);

  LongTaskDetector::Instance().UnregisterObserver(long_task_observer);
  SimulateTask(LongTaskDetector::kLongTaskThresholdSeconds + 0.01);
  // Check that we have a long task after unregistering observer.
  ASSERT_FALSE(long_task_end_when_registered == DummyTaskEndTime());
  EXPECT_EQ(long_task_observer->last_long_task_end,
            long_task_end_when_registered);
}

TEST_F(LongTaskDetectorTest, DoesNotGetNotifiedOfShortTasks) {
  TestLongTaskObserver* long_task_observer = new TestLongTaskObserver();
  LongTaskDetector::Instance().RegisterObserver(long_task_observer);
  SimulateTask(LongTaskDetector::kLongTaskThresholdSeconds - 0.01);
  EXPECT_EQ(long_task_observer->last_long_task_end, 0.0);

  SimulateTask(LongTaskDetector::kLongTaskThresholdSeconds + 0.01);
  EXPECT_EQ(long_task_observer->last_long_task_end, DummyTaskEndTime());
}
}  // namespace blink
