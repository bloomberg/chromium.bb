// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/renderer/task_queue_throttler.h"

#include <stddef.h>

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/test/simple_test_tick_clock.h"
#include "cc/test/ordered_simple_task_runner.h"
#include "platform/scheduler/base/task_queue_impl.h"
#include "platform/scheduler/base/test_time_source.h"
#include "platform/scheduler/child/scheduler_tqm_delegate_for_test.h"
#include "platform/scheduler/renderer/budget_pool.h"
#include "platform/scheduler/renderer/cpu_time_budget_pool.h"
#include "platform/scheduler/renderer/renderer_scheduler_impl.h"
#include "platform/scheduler/renderer/wake_up_budget_pool.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace scheduler {

class BudgetPoolTest : public ::testing::Test {
 public:
  BudgetPoolTest() {}
  ~BudgetPoolTest() override {}

  void SetUp() override {
    clock_.reset(new base::SimpleTestTickClock());
    clock_->Advance(base::TimeDelta::FromMicroseconds(5000));
    mock_task_runner_ =
        make_scoped_refptr(new cc::OrderedSimpleTaskRunner(clock_.get(), true));
    delegate_ = SchedulerTqmDelegateForTest::Create(
        mock_task_runner_, base::MakeUnique<TestTimeSource>(clock_.get()));
    scheduler_.reset(new RendererSchedulerImpl(delegate_));
    task_queue_throttler_ = scheduler_->task_queue_throttler();
    start_time_ = clock_->NowTicks();
  }

  void TearDown() override {
    scheduler_->Shutdown();
    scheduler_.reset();
  }

  base::TimeTicks MillisecondsAfterStart(int milliseconds) {
    return start_time_ + base::TimeDelta::FromMilliseconds(milliseconds);
  }

  base::TimeTicks SecondsAfterStart(int seconds) {
    return start_time_ + base::TimeDelta::FromSeconds(seconds);
  }

 protected:
  std::unique_ptr<base::SimpleTestTickClock> clock_;
  scoped_refptr<cc::OrderedSimpleTaskRunner> mock_task_runner_;
  scoped_refptr<SchedulerTqmDelegate> delegate_;
  std::unique_ptr<RendererSchedulerImpl> scheduler_;
  TaskQueueThrottler* task_queue_throttler_;  // NOT OWNED
  base::TimeTicks start_time_;

  DISALLOW_COPY_AND_ASSIGN(BudgetPoolTest);
};

TEST_F(BudgetPoolTest, CPUTimeBudgetPool) {
  CPUTimeBudgetPool* pool =
      task_queue_throttler_->CreateCPUTimeBudgetPool("test");

  pool->SetTimeBudgetRecoveryRate(SecondsAfterStart(0), 0.1);

  EXPECT_TRUE(pool->CanRunTasksAt(SecondsAfterStart(0), false));
  EXPECT_EQ(SecondsAfterStart(0),
            pool->GetNextAllowedRunTime(SecondsAfterStart(0)));

  // Run an expensive task and make sure that we're throttled.
  pool->RecordTaskRunTime(nullptr, SecondsAfterStart(0),
                          MillisecondsAfterStart(100));

  EXPECT_FALSE(pool->CanRunTasksAt(MillisecondsAfterStart(500), false));
  EXPECT_EQ(MillisecondsAfterStart(1000),
            pool->GetNextAllowedRunTime(SecondsAfterStart(0)));
  EXPECT_TRUE(pool->CanRunTasksAt(MillisecondsAfterStart(1000), false));

  // Run a cheap task and make sure that it doesn't affect anything.
  EXPECT_TRUE(pool->CanRunTasksAt(MillisecondsAfterStart(2000), false));
  pool->RecordTaskRunTime(nullptr, MillisecondsAfterStart(2000),
                          MillisecondsAfterStart(2020));
  EXPECT_TRUE(pool->CanRunTasksAt(MillisecondsAfterStart(2020), false));
  EXPECT_EQ(MillisecondsAfterStart(2020),
            pool->GetNextAllowedRunTime(SecondsAfterStart(0)));

  pool->Close();
}

TEST_F(BudgetPoolTest, CPUTimeBudgetPoolMinBudgetLevelToRun) {
  CPUTimeBudgetPool* pool =
      task_queue_throttler_->CreateCPUTimeBudgetPool("test");

  pool->SetMinBudgetLevelToRun(SecondsAfterStart(0),
                               base::TimeDelta::FromMilliseconds(10));
  pool->SetTimeBudgetRecoveryRate(SecondsAfterStart(0), 0.1);

  EXPECT_TRUE(pool->CanRunTasksAt(SecondsAfterStart(0), false));
  EXPECT_EQ(SecondsAfterStart(0),
            pool->GetNextAllowedRunTime(SecondsAfterStart(0)));

  pool->RecordTaskRunTime(nullptr, SecondsAfterStart(0),
                          MillisecondsAfterStart(10));
  EXPECT_FALSE(pool->CanRunTasksAt(MillisecondsAfterStart(15), false));
  EXPECT_FALSE(pool->CanRunTasksAt(MillisecondsAfterStart(150), false));
  // We need to wait extra 100ms to get budget of 10ms.
  EXPECT_EQ(MillisecondsAfterStart(200),
            pool->GetNextAllowedRunTime(SecondsAfterStart(0)));

  pool->RecordTaskRunTime(nullptr, MillisecondsAfterStart(200),
                          MillisecondsAfterStart(205));
  // We can run when budget is non-negative even when it less than 10ms.
  EXPECT_EQ(MillisecondsAfterStart(205),
            pool->GetNextAllowedRunTime(SecondsAfterStart(0)));

  pool->RecordTaskRunTime(nullptr, MillisecondsAfterStart(205),
                          MillisecondsAfterStart(215));
  EXPECT_EQ(MillisecondsAfterStart(350),
            pool->GetNextAllowedRunTime(SecondsAfterStart(0)));
}

TEST_F(BudgetPoolTest, WakeUpBudgetPool) {
  WakeUpBudgetPool* pool =
      task_queue_throttler_->CreateWakeUpBudgetPool("test");

  scoped_refptr<TaskQueue> queue =
      scheduler_->NewTimerTaskQueue(TaskQueue::QueueType::TEST);

  pool->SetWakeUpRate(0.1);
  pool->SetWakeUpDuration(base::TimeDelta::FromMilliseconds(10));

  // Can't run tasks until a wake-up.
  EXPECT_FALSE(pool->CanRunTasksAt(MillisecondsAfterStart(0), false));
  EXPECT_FALSE(pool->CanRunTasksAt(MillisecondsAfterStart(5), false));
  EXPECT_FALSE(pool->CanRunTasksAt(MillisecondsAfterStart(9), false));
  EXPECT_FALSE(pool->CanRunTasksAt(MillisecondsAfterStart(10), false));
  EXPECT_FALSE(pool->CanRunTasksAt(MillisecondsAfterStart(11), false));

  pool->OnWakeUp(MillisecondsAfterStart(0));

  EXPECT_TRUE(pool->CanRunTasksAt(MillisecondsAfterStart(0), false));
  EXPECT_TRUE(pool->CanRunTasksAt(MillisecondsAfterStart(5), false));
  EXPECT_TRUE(pool->CanRunTasksAt(MillisecondsAfterStart(9), false));
  EXPECT_FALSE(pool->CanRunTasksAt(MillisecondsAfterStart(10), false));
  EXPECT_FALSE(pool->CanRunTasksAt(MillisecondsAfterStart(11), false));

  // GetNextAllowedRunTime should return the desired time when in the
  // wakeup window and return the next wakeup otherwise.
  EXPECT_EQ(start_time_, pool->GetNextAllowedRunTime(start_time_));
  EXPECT_EQ(base::TimeTicks() + base::TimeDelta::FromSeconds(10),
            pool->GetNextAllowedRunTime(MillisecondsAfterStart(15)));

  pool->RecordTaskRunTime(queue.get(), MillisecondsAfterStart(5),
                          MillisecondsAfterStart(7));

  // Make sure that nothing changes after a task inside wakeup window.
  EXPECT_TRUE(pool->CanRunTasksAt(MillisecondsAfterStart(0), false));
  EXPECT_TRUE(pool->CanRunTasksAt(MillisecondsAfterStart(5), false));
  EXPECT_TRUE(pool->CanRunTasksAt(MillisecondsAfterStart(9), false));
  EXPECT_FALSE(pool->CanRunTasksAt(MillisecondsAfterStart(10), false));
  EXPECT_FALSE(pool->CanRunTasksAt(MillisecondsAfterStart(11), false));
  EXPECT_EQ(start_time_, pool->GetNextAllowedRunTime(start_time_));
  EXPECT_EQ(base::TimeTicks() + base::TimeDelta::FromSeconds(10),
            pool->GetNextAllowedRunTime(MillisecondsAfterStart(15)));

  pool->OnWakeUp(MillisecondsAfterStart(12005));
  pool->RecordTaskRunTime(queue.get(), MillisecondsAfterStart(12005),
                          MillisecondsAfterStart(12007));

  EXPECT_TRUE(pool->CanRunTasksAt(MillisecondsAfterStart(12005), false));
  EXPECT_TRUE(pool->CanRunTasksAt(MillisecondsAfterStart(12007), false));
  EXPECT_TRUE(pool->CanRunTasksAt(MillisecondsAfterStart(12014), false));
  EXPECT_FALSE(pool->CanRunTasksAt(MillisecondsAfterStart(12015), false));
  EXPECT_FALSE(pool->CanRunTasksAt(MillisecondsAfterStart(12016), false));
  EXPECT_EQ(base::TimeTicks() + base::TimeDelta::FromSeconds(20),
            pool->GetNextAllowedRunTime(SecondsAfterStart(13)));
}

}  // namespace scheduler
}  // namespace blink
