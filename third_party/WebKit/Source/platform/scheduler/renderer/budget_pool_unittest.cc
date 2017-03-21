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
#include "platform/scheduler/renderer/renderer_scheduler_impl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace scheduler {

class BudgetPoolTest : public testing::Test {
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
  }

  void TearDown() override {
    scheduler_->Shutdown();
    scheduler_.reset();
  }

 protected:
  std::unique_ptr<base::SimpleTestTickClock> clock_;
  scoped_refptr<cc::OrderedSimpleTaskRunner> mock_task_runner_;
  scoped_refptr<SchedulerTqmDelegate> delegate_;
  std::unique_ptr<RendererSchedulerImpl> scheduler_;
  TaskQueueThrottler* task_queue_throttler_;  // NOT OWNED

  DISALLOW_COPY_AND_ASSIGN(BudgetPoolTest);
};

TEST_F(BudgetPoolTest, CPUTimeBudgetPool) {
  CPUTimeBudgetPool* pool =
      task_queue_throttler_->CreateCPUTimeBudgetPool("test");

  base::TimeTicks time_zero = clock_->NowTicks();

  pool->SetTimeBudgetRecoveryRate(time_zero, 0.1);

  EXPECT_TRUE(pool->HasEnoughBudgetToRun(time_zero));
  EXPECT_EQ(time_zero, pool->GetNextAllowedRunTime());

  // Run an expensive task and make sure that we're throttled.
  pool->RecordTaskRunTime(time_zero,
                          time_zero + base::TimeDelta::FromMilliseconds(100));

  EXPECT_FALSE(pool->HasEnoughBudgetToRun(
      time_zero + base::TimeDelta::FromMilliseconds(500)));
  EXPECT_EQ(time_zero + base::TimeDelta::FromMilliseconds(1000),
            pool->GetNextAllowedRunTime());
  EXPECT_TRUE(pool->HasEnoughBudgetToRun(
      time_zero + base::TimeDelta::FromMilliseconds(1000)));

  // Run a cheap task and make sure that it doesn't affect anything.
  EXPECT_TRUE(pool->HasEnoughBudgetToRun(
      time_zero + base::TimeDelta::FromMilliseconds(2000)));
  pool->RecordTaskRunTime(time_zero + base::TimeDelta::FromMilliseconds(2000),
                          time_zero + base::TimeDelta::FromMilliseconds(2020));
  EXPECT_TRUE(pool->HasEnoughBudgetToRun(
      time_zero + base::TimeDelta::FromMilliseconds(2020)));
  EXPECT_EQ(time_zero + base::TimeDelta::FromMilliseconds(2020),
            pool->GetNextAllowedRunTime());

  pool->Close();
}

TEST_F(BudgetPoolTest, CPUTimeBudgetPoolMinBudgetLevelToRun) {
  CPUTimeBudgetPool* pool =
      task_queue_throttler_->CreateCPUTimeBudgetPool("test");

  base::TimeTicks time_zero = clock_->NowTicks();

  pool->SetMinBudgetLevelToRun(time_zero,
                               base::TimeDelta::FromMilliseconds(10));
  pool->SetTimeBudgetRecoveryRate(time_zero, 0.1);

  EXPECT_TRUE(pool->HasEnoughBudgetToRun(time_zero));
  EXPECT_EQ(time_zero, pool->GetNextAllowedRunTime());

  pool->RecordTaskRunTime(time_zero,
                          time_zero + base::TimeDelta::FromMilliseconds(10));
  EXPECT_FALSE(pool->HasEnoughBudgetToRun(
      time_zero + base::TimeDelta::FromMilliseconds(15)));
  EXPECT_FALSE(pool->HasEnoughBudgetToRun(
      time_zero + base::TimeDelta::FromMilliseconds(150)));
  // We need to wait extra 100ms to get budget of 10ms.
  EXPECT_EQ(time_zero + base::TimeDelta::FromMilliseconds(200),
            pool->GetNextAllowedRunTime());

  pool->RecordTaskRunTime(time_zero + base::TimeDelta::FromMilliseconds(200),
                          time_zero + base::TimeDelta::FromMilliseconds(205));
  // We can run when budget is non-negative even when it less than 10ms.
  EXPECT_EQ(time_zero + base::TimeDelta::FromMilliseconds(205),
            pool->GetNextAllowedRunTime());

  pool->RecordTaskRunTime(time_zero + base::TimeDelta::FromMilliseconds(205),
                          time_zero + base::TimeDelta::FromMilliseconds(215));
  EXPECT_EQ(time_zero + base::TimeDelta::FromMilliseconds(350),
            pool->GetNextAllowedRunTime());
}

}  // namespace scheduler
}  // namespace blink
