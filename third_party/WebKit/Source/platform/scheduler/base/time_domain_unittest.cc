// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/base/time_domain.h"

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/test/simple_test_tick_clock.h"
#include "cc/test/ordered_simple_task_runner.h"
#include "platform/scheduler/base/task_queue_impl.h"
#include "platform/scheduler/base/task_queue_manager.h"
#include "platform/scheduler/base/task_queue_manager_delegate_for_test.h"
#include "platform/scheduler/base/test_time_source.h"
#include "platform/scheduler/base/work_queue.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::_;
using testing::AnyNumber;
using testing::Mock;

namespace blink {
namespace scheduler {

class MockTimeDomain : public TimeDomain {
 public:
  explicit MockTimeDomain(TimeDomain::Observer* observer)
      : TimeDomain(observer),
        now_(base::TimeTicks() + base::TimeDelta::FromSeconds(1)) {}

  ~MockTimeDomain() override {}

  using TimeDomain::CancelDelayedWork;
  using TimeDomain::NextScheduledRunTime;
  using TimeDomain::NextScheduledTaskQueue;
  using TimeDomain::OnQueueHasImmediateWork;
  using TimeDomain::ScheduleDelayedWork;
  using TimeDomain::UnregisterQueue;
  using TimeDomain::WakeupReadyDelayedQueues;

  // TimeSource implementation:
  LazyNow CreateLazyNow() const override { return LazyNow(now_); }
  base::TimeTicks Now() const override { return now_; }

  void AsValueIntoInternal(
      base::trace_event::TracedValue* state) const override {}

  base::Optional<base::TimeDelta> DelayTillNextTask(
      LazyNow* lazy_now) override {
    return base::Optional<base::TimeDelta>();
  }
  const char* GetName() const override { return "Test"; }
  void OnRegisterWithTaskQueueManager(
      TaskQueueManager* task_queue_manager) override {}

  MOCK_METHOD2(RequestWakeupAt,
               void(base::TimeTicks now, base::TimeTicks run_time));

  MOCK_METHOD1(CancelWakeupAt, void(base::TimeTicks run_time));

  void SetNow(base::TimeTicks now) { now_ = now; }

 private:
  base::TimeTicks now_;

  DISALLOW_COPY_AND_ASSIGN(MockTimeDomain);
};

class TimeDomainTest : public testing::Test {
 public:
  void SetUp() final {
    time_domain_ = base::WrapUnique(CreateMockTimeDomain());
    task_queue_ = make_scoped_refptr(
        new internal::TaskQueueImpl(nullptr, time_domain_.get(),
                                    TaskQueue::Spec(TaskQueue::QueueType::TEST),
                                    "test.category", "test.category"));
  }

  void TearDown() final {
    if (task_queue_)
      task_queue_->UnregisterTaskQueue();
  }

  virtual MockTimeDomain* CreateMockTimeDomain() {
    return new MockTimeDomain(nullptr);
  }

  std::unique_ptr<MockTimeDomain> time_domain_;
  scoped_refptr<internal::TaskQueueImpl> task_queue_;
};

TEST_F(TimeDomainTest, ScheduleDelayedWork) {
  base::TimeDelta delay = base::TimeDelta::FromMilliseconds(10);
  base::TimeTicks delayed_runtime = time_domain_->Now() + delay;
  EXPECT_CALL(*time_domain_.get(), RequestWakeupAt(_, delayed_runtime));
  base::TimeTicks now = time_domain_->Now();
  time_domain_->ScheduleDelayedWork(task_queue_.get(), {now + delay, 0}, now);

  base::TimeTicks next_scheduled_runtime;
  EXPECT_TRUE(time_domain_->NextScheduledRunTime(&next_scheduled_runtime));
  EXPECT_EQ(delayed_runtime, next_scheduled_runtime);

  TaskQueue* next_task_queue;
  EXPECT_TRUE(time_domain_->NextScheduledTaskQueue(&next_task_queue));
  EXPECT_EQ(task_queue_.get(), next_task_queue);
  Mock::VerifyAndClearExpectations(time_domain_.get());

  EXPECT_CALL(*time_domain_.get(), CancelWakeupAt(_)).Times(AnyNumber());
}

TEST_F(TimeDomainTest, ScheduleDelayedWorkSupersedesPreviousWakeup) {
  base::TimeDelta delay1 = base::TimeDelta::FromMilliseconds(10);
  base::TimeDelta delay2 = base::TimeDelta::FromMilliseconds(100);
  base::TimeTicks delayed_runtime1 = time_domain_->Now() + delay1;
  base::TimeTicks delayed_runtime2 = time_domain_->Now() + delay2;
  EXPECT_CALL(*time_domain_.get(), RequestWakeupAt(_, delayed_runtime1));
  base::TimeTicks now = time_domain_->Now();
  time_domain_->ScheduleDelayedWork(task_queue_.get(), {delayed_runtime1, 0},
                                    now);

  base::TimeTicks next_scheduled_runtime;
  EXPECT_TRUE(time_domain_->NextScheduledRunTime(&next_scheduled_runtime));
  EXPECT_EQ(delayed_runtime1, next_scheduled_runtime);

  Mock::VerifyAndClearExpectations(time_domain_.get());

  // Now scheduler a later wakeup, which should replace the previously requested
  // one.
  EXPECT_CALL(*time_domain_.get(), RequestWakeupAt(_, delayed_runtime2));
  time_domain_->ScheduleDelayedWork(task_queue_.get(), {delayed_runtime2, 0},
                                    now);

  EXPECT_TRUE(time_domain_->NextScheduledRunTime(&next_scheduled_runtime));
  EXPECT_EQ(delayed_runtime2, next_scheduled_runtime);
  Mock::VerifyAndClearExpectations(time_domain_.get());

  EXPECT_CALL(*time_domain_.get(), CancelWakeupAt(_)).Times(AnyNumber());
}

TEST_F(TimeDomainTest, RequestWakeupAt_OnlyCalledForEarlierTasks) {
  scoped_refptr<internal::TaskQueueImpl> task_queue2 = make_scoped_refptr(
      new internal::TaskQueueImpl(nullptr, time_domain_.get(),
                                  TaskQueue::Spec(TaskQueue::QueueType::TEST),
                                  "test.category", "test.category"));

  scoped_refptr<internal::TaskQueueImpl> task_queue3 = make_scoped_refptr(
      new internal::TaskQueueImpl(nullptr, time_domain_.get(),
                                  TaskQueue::Spec(TaskQueue::QueueType::TEST),
                                  "test.category", "test.category"));

  scoped_refptr<internal::TaskQueueImpl> task_queue4 = make_scoped_refptr(
      new internal::TaskQueueImpl(nullptr, time_domain_.get(),
                                  TaskQueue::Spec(TaskQueue::QueueType::TEST),
                                  "test.category", "test.category"));

  base::TimeDelta delay1 = base::TimeDelta::FromMilliseconds(10);
  base::TimeDelta delay2 = base::TimeDelta::FromMilliseconds(20);
  base::TimeDelta delay3 = base::TimeDelta::FromMilliseconds(30);
  base::TimeDelta delay4 = base::TimeDelta::FromMilliseconds(1);

  // RequestWakeupAt should always be called if there are no other wakeups.
  base::TimeTicks now = time_domain_->Now();
  EXPECT_CALL(*time_domain_.get(), RequestWakeupAt(_, now + delay1));
  time_domain_->ScheduleDelayedWork(task_queue_.get(), {now + delay1, 0}, now);

  Mock::VerifyAndClearExpectations(time_domain_.get());

  // RequestWakeupAt should not be called when scheduling later tasks.
  EXPECT_CALL(*time_domain_.get(), RequestWakeupAt(_, _)).Times(0);
  time_domain_->ScheduleDelayedWork(task_queue2.get(), {now + delay2, 0}, now);
  time_domain_->ScheduleDelayedWork(task_queue3.get(), {now + delay3, 0}, now);

  // RequestWakeupAt should be called when scheduling earlier tasks.
  Mock::VerifyAndClearExpectations(time_domain_.get());
  EXPECT_CALL(*time_domain_.get(), RequestWakeupAt(_, now + delay4));
  time_domain_->ScheduleDelayedWork(task_queue4.get(), {now + delay4, 0}, now);

  Mock::VerifyAndClearExpectations(time_domain_.get());

  EXPECT_CALL(*time_domain_.get(), RequestWakeupAt(_, _));
  EXPECT_CALL(*time_domain_.get(), CancelWakeupAt(_)).Times(2);
  task_queue2->UnregisterTaskQueue();
  task_queue3->UnregisterTaskQueue();
  task_queue4->UnregisterTaskQueue();
}

TEST_F(TimeDomainTest, UnregisterQueue) {
  scoped_refptr<internal::TaskQueueImpl> task_queue2_ = make_scoped_refptr(
      new internal::TaskQueueImpl(nullptr, time_domain_.get(),
                                  TaskQueue::Spec(TaskQueue::QueueType::TEST),
                                  "test.category", "test.category"));

  base::TimeTicks now = time_domain_->Now();
  base::TimeTicks wakeup1 = now + base::TimeDelta::FromMilliseconds(10);
  EXPECT_CALL(*time_domain_.get(), RequestWakeupAt(_, wakeup1)).Times(1);
  time_domain_->ScheduleDelayedWork(task_queue_.get(), {wakeup1, 0}, now);
  base::TimeTicks wakeup2 = now + base::TimeDelta::FromMilliseconds(100);
  time_domain_->ScheduleDelayedWork(task_queue2_.get(), {wakeup2, 0}, now);

  TaskQueue* next_task_queue;
  EXPECT_TRUE(time_domain_->NextScheduledTaskQueue(&next_task_queue));
  EXPECT_EQ(task_queue_.get(), next_task_queue);

  testing::Mock::VerifyAndClearExpectations(time_domain_.get());

  EXPECT_CALL(*time_domain_.get(), CancelWakeupAt(wakeup1)).Times(1);
  EXPECT_CALL(*time_domain_.get(), RequestWakeupAt(_, wakeup2)).Times(1);

  time_domain_->UnregisterQueue(task_queue_.get());
  task_queue_ = scoped_refptr<internal::TaskQueueImpl>();
  EXPECT_TRUE(time_domain_->NextScheduledTaskQueue(&next_task_queue));
  EXPECT_EQ(task_queue2_.get(), next_task_queue);

  testing::Mock::VerifyAndClearExpectations(time_domain_.get());

  EXPECT_CALL(*time_domain_.get(), CancelWakeupAt(wakeup2)).Times(1);

  time_domain_->UnregisterQueue(task_queue2_.get());
  EXPECT_FALSE(time_domain_->NextScheduledTaskQueue(&next_task_queue));
}

TEST_F(TimeDomainTest, WakeupReadyDelayedQueues) {
  base::TimeDelta delay = base::TimeDelta::FromMilliseconds(50);
  base::TimeTicks now = time_domain_->Now();
  base::TimeTicks delayed_runtime = now + delay;
  EXPECT_CALL(*time_domain_.get(), RequestWakeupAt(_, delayed_runtime));
  time_domain_->ScheduleDelayedWork(task_queue_.get(), {delayed_runtime, 0},
                                    now);

  base::TimeTicks next_run_time;
  ASSERT_TRUE(time_domain_->NextScheduledRunTime(&next_run_time));
  EXPECT_EQ(delayed_runtime, next_run_time);

  LazyNow lazy_now = time_domain_->CreateLazyNow();
  time_domain_->WakeupReadyDelayedQueues(&lazy_now);
  ASSERT_TRUE(time_domain_->NextScheduledRunTime(&next_run_time));
  EXPECT_EQ(delayed_runtime, next_run_time);

  time_domain_->SetNow(delayed_runtime);
  lazy_now = time_domain_->CreateLazyNow();
  time_domain_->WakeupReadyDelayedQueues(&lazy_now);
  ASSERT_FALSE(time_domain_->NextScheduledRunTime(&next_run_time));
}

TEST_F(TimeDomainTest, WakeupReadyDelayedQueuesWithIdenticalRuntimes) {
  int sequence_num = 0;
  base::TimeDelta delay = base::TimeDelta::FromMilliseconds(50);
  base::TimeTicks now = time_domain_->Now();
  base::TimeTicks delayed_runtime = now + delay;
  EXPECT_CALL(*time_domain_.get(), RequestWakeupAt(_, delayed_runtime));
  EXPECT_CALL(*time_domain_.get(), CancelWakeupAt(delayed_runtime));

  scoped_refptr<internal::TaskQueueImpl> task_queue2 = make_scoped_refptr(
      new internal::TaskQueueImpl(nullptr, time_domain_.get(),
                                  TaskQueue::Spec(TaskQueue::QueueType::TEST),
                                  "test.category", "test.category"));

  time_domain_->ScheduleDelayedWork(task_queue2.get(),
                                    {delayed_runtime, ++sequence_num}, now);
  time_domain_->ScheduleDelayedWork(task_queue_.get(),
                                    {delayed_runtime, ++sequence_num}, now);

  LazyNow lazy_now = time_domain_->CreateLazyNow();
  time_domain_->WakeupReadyDelayedQueues(&lazy_now);

  // The second task queue should wake up first since it has a lower sequence
  // number.
  TaskQueue* next_task_queue;
  EXPECT_TRUE(time_domain_->NextScheduledTaskQueue(&next_task_queue));
  EXPECT_EQ(task_queue2.get(), next_task_queue);

  task_queue2->UnregisterTaskQueue();
}

TEST_F(TimeDomainTest, CancelDelayedWork) {
  base::TimeTicks now = time_domain_->Now();
  base::TimeTicks run_time = now + base::TimeDelta::FromMilliseconds(20);

  EXPECT_CALL(*time_domain_.get(), RequestWakeupAt(_, run_time));
  time_domain_->ScheduleDelayedWork(task_queue_.get(), {run_time, 0}, now);

  TaskQueue* next_task_queue;
  EXPECT_TRUE(time_domain_->NextScheduledTaskQueue(&next_task_queue));
  EXPECT_EQ(task_queue_.get(), next_task_queue);

  EXPECT_CALL(*time_domain_.get(), CancelWakeupAt(run_time));
  time_domain_->CancelDelayedWork(task_queue_.get());
  EXPECT_FALSE(time_domain_->NextScheduledTaskQueue(&next_task_queue));
}

TEST_F(TimeDomainTest, CancelDelayedWork_TwoQueues) {
  scoped_refptr<internal::TaskQueueImpl> task_queue2 = make_scoped_refptr(
      new internal::TaskQueueImpl(nullptr, time_domain_.get(),
                                  TaskQueue::Spec(TaskQueue::QueueType::TEST),
                                  "test.category", "test.category"));

  base::TimeTicks now = time_domain_->Now();
  base::TimeTicks run_time1 = now + base::TimeDelta::FromMilliseconds(20);
  base::TimeTicks run_time2 = now + base::TimeDelta::FromMilliseconds(40);
  EXPECT_CALL(*time_domain_.get(), RequestWakeupAt(_, run_time1));
  time_domain_->ScheduleDelayedWork(task_queue_.get(), {run_time1, 0}, now);
  Mock::VerifyAndClearExpectations(time_domain_.get());

  EXPECT_CALL(*time_domain_.get(), RequestWakeupAt(_, _)).Times(0);
  time_domain_->ScheduleDelayedWork(task_queue2.get(), {run_time2, 0}, now);
  Mock::VerifyAndClearExpectations(time_domain_.get());

  TaskQueue* next_task_queue;
  EXPECT_TRUE(time_domain_->NextScheduledTaskQueue(&next_task_queue));
  EXPECT_EQ(task_queue_.get(), next_task_queue);

  base::TimeTicks next_run_time;
  ASSERT_TRUE(time_domain_->NextScheduledRunTime(&next_run_time));
  EXPECT_EQ(run_time1, next_run_time);

  EXPECT_CALL(*time_domain_.get(), CancelWakeupAt(run_time1));
  EXPECT_CALL(*time_domain_.get(), RequestWakeupAt(_, run_time2));
  time_domain_->CancelDelayedWork(task_queue_.get());
  EXPECT_TRUE(time_domain_->NextScheduledTaskQueue(&next_task_queue));
  EXPECT_EQ(task_queue2.get(), next_task_queue);

  ASSERT_TRUE(time_domain_->NextScheduledRunTime(&next_run_time));
  EXPECT_EQ(run_time2, next_run_time);

  Mock::VerifyAndClearExpectations(time_domain_.get());
  EXPECT_CALL(*time_domain_.get(), RequestWakeupAt(_, _)).Times(AnyNumber());
  EXPECT_CALL(*time_domain_.get(), CancelWakeupAt(_)).Times(AnyNumber());

  // Tidy up.
  task_queue2->UnregisterTaskQueue();
}

namespace {
class MockObserver : public TimeDomain::Observer {
 public:
  ~MockObserver() override {}

  MOCK_METHOD1(OnTimeDomainHasImmediateWork, void(TaskQueue*));
  MOCK_METHOD1(OnTimeDomainHasDelayedWork, void(TaskQueue*));
};
}  // namespace

class TimeDomainWithObserverTest : public TimeDomainTest {
 public:
  MockTimeDomain* CreateMockTimeDomain() override {
    observer_.reset(new MockObserver());
    return new MockTimeDomain(observer_.get());
  }

  std::unique_ptr<MockObserver> observer_;
};

TEST_F(TimeDomainWithObserverTest, OnTimeDomainHasImmediateWork) {
  EXPECT_CALL(*observer_, OnTimeDomainHasImmediateWork(task_queue_.get()));
  time_domain_->OnQueueHasImmediateWork(task_queue_.get());
}

TEST_F(TimeDomainWithObserverTest, OnTimeDomainHasDelayedWork) {
  EXPECT_CALL(*observer_, OnTimeDomainHasDelayedWork(task_queue_.get()));
  EXPECT_CALL(*time_domain_.get(), RequestWakeupAt(_, _));
  base::TimeTicks now = time_domain_->Now();
  time_domain_->ScheduleDelayedWork(
      task_queue_.get(), {now + base::TimeDelta::FromMilliseconds(10), 0}, now);
}

}  // namespace scheduler
}  // namespace blink
