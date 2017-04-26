// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/renderer/auto_advancing_virtual_time_domain.h"

#include "base/memory/ptr_util.h"
#include "base/test/simple_test_tick_clock.h"
#include "cc/test/ordered_simple_task_runner.h"
#include "platform/scheduler/base/task_queue_manager.h"
#include "platform/scheduler/base/test_task_time_observer.h"
#include "platform/scheduler/base/test_time_source.h"
#include "platform/scheduler/child/scheduler_tqm_delegate_for_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace scheduler {

class AutoAdvancingVirtualTimeDomainTest : public testing::Test {
 public:
  AutoAdvancingVirtualTimeDomainTest() {}
  ~AutoAdvancingVirtualTimeDomainTest() override {}

  void SetUp() override {
    clock_.reset(new base::SimpleTestTickClock());
    clock_->Advance(base::TimeDelta::FromMicroseconds(5000));

    test_time_source_.reset(new TestTimeSource(clock_.get()));
    mock_task_runner_ = make_scoped_refptr(
        new cc::OrderedSimpleTaskRunner(clock_.get(), false));
    main_task_runner_ = SchedulerTqmDelegateForTest::Create(
        mock_task_runner_, base::MakeUnique<TestTimeSource>(clock_.get()));

    manager_ = base::MakeUnique<TaskQueueManager>(main_task_runner_);
    manager_->AddTaskTimeObserver(&test_task_time_observer_);
    task_queue_ =
        manager_->NewTaskQueue(TaskQueue::Spec(TaskQueue::QueueType::TEST));
    initial_time_ = clock_->NowTicks();
    auto_advancing_time_domain_.reset(
        new AutoAdvancingVirtualTimeDomain(initial_time_));
    manager_->RegisterTimeDomain(auto_advancing_time_domain_.get());
    task_queue_->SetTimeDomain(auto_advancing_time_domain_.get());
  }

  void TearDown() override {
    task_queue_->UnregisterTaskQueue();
    manager_->UnregisterTimeDomain(auto_advancing_time_domain_.get());
  }

  base::TimeTicks initial_time_;
  std::unique_ptr<base::SimpleTestTickClock> clock_;
  std::unique_ptr<TestTimeSource> test_time_source_;
  scoped_refptr<cc::OrderedSimpleTaskRunner> mock_task_runner_;
  scoped_refptr<SchedulerTqmDelegate> main_task_runner_;
  std::unique_ptr<TaskQueueManager> manager_;
  scoped_refptr<TaskQueue> task_queue_;
  std::unique_ptr<AutoAdvancingVirtualTimeDomain> auto_advancing_time_domain_;
  TestTaskTimeObserver test_task_time_observer_;
};

namespace {
void NopTask(bool* task_run) {
  *task_run = true;
}
}  // namesapce

TEST_F(AutoAdvancingVirtualTimeDomainTest, VirtualTimeAdvances) {
  base::TimeDelta delay = base::TimeDelta::FromMilliseconds(10);
  bool task_run = false;
  task_queue_->PostDelayedTask(FROM_HERE, base::Bind(NopTask, &task_run),
                               delay);

  mock_task_runner_->RunUntilIdle();

  EXPECT_EQ(initial_time_, clock_->NowTicks());
  EXPECT_EQ(initial_time_ + delay,
            auto_advancing_time_domain_->CreateLazyNow().Now());
  EXPECT_TRUE(task_run);
}

TEST_F(AutoAdvancingVirtualTimeDomainTest, VirtualTimeDoesNotAdvance) {
  base::TimeDelta delay = base::TimeDelta::FromMilliseconds(10);
  bool task_run = false;
  task_queue_->PostDelayedTask(FROM_HERE, base::Bind(NopTask, &task_run),
                               delay);

  auto_advancing_time_domain_->SetCanAdvanceVirtualTime(false);

  mock_task_runner_->RunUntilIdle();

  EXPECT_EQ(initial_time_, clock_->NowTicks());
  EXPECT_EQ(initial_time_, auto_advancing_time_domain_->CreateLazyNow().Now());
  EXPECT_FALSE(task_run);
}

}  // namespace scheduler
}  // namespace blink
