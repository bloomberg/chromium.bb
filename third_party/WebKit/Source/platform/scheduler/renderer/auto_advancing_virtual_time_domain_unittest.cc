// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/renderer/auto_advancing_virtual_time_domain.h"

#include <memory>
#include "base/test/simple_test_tick_clock.h"
#include "components/viz/test/ordered_simple_task_runner.h"
#include "platform/scheduler/base/task_queue_manager.h"
#include "platform/scheduler/base/test_task_time_observer.h"
#include "platform/scheduler/base/test_time_source.h"
#include "platform/scheduler/child/scheduler_tqm_delegate_for_test.h"
#include "platform/scheduler/child/worker_scheduler_helper.h"
#include "platform/scheduler/test/test_task_queue.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace scheduler {
// Namespace to avoid symbol collisions in jumbo builds.
namespace auto_advancing_virtual_time_domain_unittest {

class AutoAdvancingVirtualTimeDomainTest : public ::testing::Test {
 public:
  AutoAdvancingVirtualTimeDomainTest() {}
  ~AutoAdvancingVirtualTimeDomainTest() override {}

  void SetUp() override {
    clock_.reset(new base::SimpleTestTickClock());
    clock_->Advance(base::TimeDelta::FromMicroseconds(5000));

    test_time_source_.reset(new TestTimeSource(clock_.get()));
    mock_task_runner_ =
        base::MakeRefCounted<cc::OrderedSimpleTaskRunner>(clock_.get(), false);
    main_task_runner_ = SchedulerTqmDelegateForTest::Create(
        mock_task_runner_, std::make_unique<TestTimeSource>(clock_.get()));

    scheduler_helper_.reset(new WorkerSchedulerHelper(main_task_runner_));

    scheduler_helper_->AddTaskTimeObserver(&test_task_time_observer_);
    task_queue_ = scheduler_helper_->DefaultWorkerTaskQueue();
    initial_time_ = clock_->NowTicks();
    auto_advancing_time_domain_.reset(new AutoAdvancingVirtualTimeDomain(
        initial_time_, scheduler_helper_.get()));
    scheduler_helper_->RegisterTimeDomain(auto_advancing_time_domain_.get());
    task_queue_->SetTimeDomain(auto_advancing_time_domain_.get());
  }

  void TearDown() override {
    task_queue_->UnregisterTaskQueue();
    scheduler_helper_->UnregisterTimeDomain(auto_advancing_time_domain_.get());
  }

  base::TimeTicks initial_time_;
  std::unique_ptr<base::SimpleTestTickClock> clock_;
  std::unique_ptr<TestTimeSource> test_time_source_;
  scoped_refptr<cc::OrderedSimpleTaskRunner> mock_task_runner_;
  scoped_refptr<SchedulerTqmDelegate> main_task_runner_;
  std::unique_ptr<WorkerSchedulerHelper> scheduler_helper_;
  scoped_refptr<TaskQueue> task_queue_;
  std::unique_ptr<AutoAdvancingVirtualTimeDomain> auto_advancing_time_domain_;
  TestTaskTimeObserver test_task_time_observer_;
};

namespace {
void NopTask(bool* task_run) {
  *task_run = true;
}

class MockObserver : public AutoAdvancingVirtualTimeDomain::Observer {
 public:
  MOCK_METHOD0(OnVirtualTimeAdvanced, void());
};

}  // namesapce

TEST_F(AutoAdvancingVirtualTimeDomainTest, VirtualTimeAdvances) {
  MockObserver mock_observer;
  auto_advancing_time_domain_->SetObserver(&mock_observer);

  base::TimeDelta delay = base::TimeDelta::FromMilliseconds(10);
  bool task_run = false;
  task_queue_->PostDelayedTask(FROM_HERE, base::Bind(NopTask, &task_run),
                               delay);

  EXPECT_CALL(mock_observer, OnVirtualTimeAdvanced());
  mock_task_runner_->RunUntilIdle();

  EXPECT_EQ(initial_time_, clock_->NowTicks());
  EXPECT_EQ(initial_time_ + delay,
            auto_advancing_time_domain_->CreateLazyNow().Now());
  EXPECT_TRUE(task_run);

  auto_advancing_time_domain_->SetObserver(nullptr);
}

TEST_F(AutoAdvancingVirtualTimeDomainTest, VirtualTimeDoesNotAdvance) {
  MockObserver mock_observer;
  auto_advancing_time_domain_->SetObserver(&mock_observer);

  base::TimeDelta delay = base::TimeDelta::FromMilliseconds(10);
  bool task_run = false;
  task_queue_->PostDelayedTask(FROM_HERE, base::Bind(NopTask, &task_run),
                               delay);

  auto_advancing_time_domain_->SetCanAdvanceVirtualTime(false);

  EXPECT_CALL(mock_observer, OnVirtualTimeAdvanced()).Times(0);
  mock_task_runner_->RunUntilIdle();

  EXPECT_EQ(initial_time_, clock_->NowTicks());
  EXPECT_EQ(initial_time_, auto_advancing_time_domain_->CreateLazyNow().Now());
  EXPECT_FALSE(task_run);

  auto_advancing_time_domain_->SetObserver(nullptr);
}

namespace {
void RepostingTask(scoped_refptr<TaskQueue> task_queue,
                   int max_count,
                   int* count) {
  if (++(*count) >= max_count)
    return;

  task_queue->PostTask(
      FROM_HERE, base::Bind(&RepostingTask, task_queue, max_count, count));
}

void DelayedTask(int* count_in, int* count_out) {
  *count_out = *count_in;
}

}  // namespace

TEST_F(AutoAdvancingVirtualTimeDomainTest,
       MaxVirtualTimeTaskStarvationCountOneHundred) {
  auto_advancing_time_domain_->SetCanAdvanceVirtualTime(true);
  auto_advancing_time_domain_->SetMaxVirtualTimeTaskStarvationCount(100);

  int count = 0;
  int delayed_task_run_at_count = 0;
  RepostingTask(task_queue_, 1000, &count);
  task_queue_->PostDelayedTask(
      FROM_HERE, base::Bind(DelayedTask, &count, &delayed_task_run_at_count),
      base::TimeDelta::FromMilliseconds(10));

  mock_task_runner_->RunUntilIdle();

  EXPECT_EQ(1000, count);
  EXPECT_EQ(102, delayed_task_run_at_count);
}

TEST_F(AutoAdvancingVirtualTimeDomainTest,
       MaxVirtualTimeTaskStarvationCountZero) {
  auto_advancing_time_domain_->SetCanAdvanceVirtualTime(true);
  auto_advancing_time_domain_->SetMaxVirtualTimeTaskStarvationCount(0);

  int count = 0;
  int delayed_task_run_at_count = 0;
  RepostingTask(task_queue_, 1000, &count);
  task_queue_->PostDelayedTask(
      FROM_HERE, base::Bind(DelayedTask, &count, &delayed_task_run_at_count),
      base::TimeDelta::FromMilliseconds(10));

  mock_task_runner_->RunUntilIdle();

  EXPECT_EQ(1000, count);
  // If the initial count had been higher, the delayed task could have been
  // arbitrarily delayed.
  EXPECT_EQ(1000, delayed_task_run_at_count);
}

TEST_F(AutoAdvancingVirtualTimeDomainTest, TaskStarvationCountIncrements) {
  auto_advancing_time_domain_->SetMaxVirtualTimeTaskStarvationCount(100);
  EXPECT_EQ(0, auto_advancing_time_domain_->task_starvation_count());
  base::PendingTask fake_task(FROM_HERE, base::OnceClosure());
  auto_advancing_time_domain_->DidProcessTask(fake_task);
  EXPECT_EQ(1, auto_advancing_time_domain_->task_starvation_count());
}

TEST_F(AutoAdvancingVirtualTimeDomainTest, TaskStarvationCountNotIncrements) {
  auto_advancing_time_domain_->SetMaxVirtualTimeTaskStarvationCount(0);
  EXPECT_EQ(0, auto_advancing_time_domain_->task_starvation_count());
  base::PendingTask fake_task(FROM_HERE, base::OnceClosure());
  auto_advancing_time_domain_->DidProcessTask(fake_task);
  EXPECT_EQ(0, auto_advancing_time_domain_->task_starvation_count());
}

TEST_F(AutoAdvancingVirtualTimeDomainTest, TaskStarvationCountResets) {
  auto_advancing_time_domain_->SetMaxVirtualTimeTaskStarvationCount(100);
  base::PendingTask fake_task(FROM_HERE, base::OnceClosure());
  auto_advancing_time_domain_->DidProcessTask(fake_task);
  EXPECT_EQ(1, auto_advancing_time_domain_->task_starvation_count());
  auto_advancing_time_domain_->SetMaxVirtualTimeTaskStarvationCount(0);
  EXPECT_EQ(0, auto_advancing_time_domain_->task_starvation_count());
}

}  // namespace auto_advancing_virtual_time_domain_unittest
}  // namespace scheduler
}  // namespace blink
