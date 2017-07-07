// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/Timer.h"

#include <memory>
#include <queue>
#include "base/message_loop/message_loop.h"
#include "platform/scheduler/base/task_queue_impl.h"
#include "platform/scheduler/child/web_scheduler.h"
#include "platform/scheduler/child/web_task_runner_impl.h"
#include "platform/scheduler/renderer/renderer_scheduler_impl.h"
#include "platform/scheduler/renderer/web_view_scheduler.h"
#include "platform/testing/TestingPlatformSupport.h"
#include "platform/wtf/CurrentTime.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/RefCounted.h"
#include "public/platform/Platform.h"
#include "public/platform/WebThread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::ElementsAre;

namespace blink {
namespace {

class TimerTest : public ::testing::Test {
 public:
  void SetUp() override {
    run_times_.clear();
    platform_->AdvanceClockSeconds(10.0);
    start_time_ = MonotonicallyIncreasingTime();
  }

  void CountingTask(TimerBase*) {
    run_times_.push_back(MonotonicallyIncreasingTime());
  }

  void RecordNextFireTimeTask(TimerBase* timer) {
    next_fire_times_.push_back(MonotonicallyIncreasingTime() +
                               timer->NextFireInterval());
  }

  void RunUntilDeadline(double deadline) {
    double period = deadline - MonotonicallyIncreasingTime();
    EXPECT_GE(period, 0.0);
    platform_->RunForPeriodSeconds(period);
  }

  // Returns false if there are no pending delayed tasks, otherwise sets |time|
  // to the delay in seconds till the next pending delayed task is scheduled to
  // fire.
  bool TimeTillNextDelayedTask(double* time) const {
    base::TimeTicks next_run_time;
    if (!platform_->GetRendererScheduler()
             ->TimerTaskQueue()
             ->GetTimeDomain()
             ->NextScheduledRunTime(&next_run_time))
      return false;
    *time = (next_run_time - platform_->GetRendererScheduler()
                                 ->TimerTaskQueue()
                                 ->GetTimeDomain()
                                 ->Now())
                .InSecondsF();
    return true;
  }

 protected:
  double start_time_;
  WTF::Vector<double> run_times_;
  WTF::Vector<double> next_fire_times_;
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform_;
};

class OnHeapTimerOwner final
    : public GarbageCollectedFinalized<OnHeapTimerOwner> {
 public:
  class Record final : public RefCounted<Record> {
   public:
    static PassRefPtr<Record> Create() { return AdoptRef(new Record); }

    bool TimerHasFired() const { return timer_has_fired_; }
    bool IsDisposed() const { return is_disposed_; }
    bool OwnerIsDestructed() const { return owner_is_destructed_; }
    void SetTimerHasFired() { timer_has_fired_ = true; }
    void Dispose() { is_disposed_ = true; }
    void SetOwnerIsDestructed() { owner_is_destructed_ = true; }

   private:
    Record() = default;

    bool timer_has_fired_ = false;
    bool is_disposed_ = false;
    bool owner_is_destructed_ = false;
  };

  explicit OnHeapTimerOwner(PassRefPtr<Record> record)
      : timer_(this, &OnHeapTimerOwner::Fired), record_(std::move(record)) {}
  ~OnHeapTimerOwner() { record_->SetOwnerIsDestructed(); }

  void StartOneShot(double interval, const WebTraceLocation& caller) {
    timer_.StartOneShot(interval, caller);
  }

  DEFINE_INLINE_TRACE() {}

 private:
  void Fired(TimerBase*) {
    EXPECT_FALSE(record_->IsDisposed());
    record_->SetTimerHasFired();
  }

  Timer<OnHeapTimerOwner> timer_;
  RefPtr<Record> record_;
};

class GCForbiddenScope final {
 public:
  STACK_ALLOCATED();
  GCForbiddenScope() { ThreadState::Current()->EnterGCForbiddenScope(); }
  ~GCForbiddenScope() { ThreadState::Current()->LeaveGCForbiddenScope(); }
};

TEST_F(TimerTest, StartOneShot_Zero) {
  Timer<TimerTest> timer(this, &TimerTest::CountingTask);
  timer.StartOneShot(0, BLINK_FROM_HERE);

  double run_time;
  EXPECT_FALSE(TimeTillNextDelayedTask(&run_time));

  platform_->RunUntilIdle();
  EXPECT_THAT(run_times_, ElementsAre(start_time_));
}

TEST_F(TimerTest, StartOneShot_ZeroAndCancel) {
  Timer<TimerTest> timer(this, &TimerTest::CountingTask);
  timer.StartOneShot(0, BLINK_FROM_HERE);

  double run_time;
  EXPECT_FALSE(TimeTillNextDelayedTask(&run_time));

  timer.Stop();

  platform_->RunUntilIdle();
  EXPECT_FALSE(run_times_.size());
}

TEST_F(TimerTest, StartOneShot_ZeroAndCancelThenRepost) {
  Timer<TimerTest> timer(this, &TimerTest::CountingTask);
  timer.StartOneShot(0, BLINK_FROM_HERE);

  double run_time;
  EXPECT_FALSE(TimeTillNextDelayedTask(&run_time));

  timer.Stop();

  platform_->RunUntilIdle();
  EXPECT_FALSE(run_times_.size());

  timer.StartOneShot(0, BLINK_FROM_HERE);

  EXPECT_FALSE(TimeTillNextDelayedTask(&run_time));

  platform_->RunUntilIdle();
  EXPECT_THAT(run_times_, ElementsAre(start_time_));
}

TEST_F(TimerTest, StartOneShot_Zero_RepostingAfterRunning) {
  Timer<TimerTest> timer(this, &TimerTest::CountingTask);
  timer.StartOneShot(0, BLINK_FROM_HERE);

  double run_time;
  EXPECT_FALSE(TimeTillNextDelayedTask(&run_time));

  platform_->RunUntilIdle();
  EXPECT_THAT(run_times_, ElementsAre(start_time_));

  timer.StartOneShot(0, BLINK_FROM_HERE);

  EXPECT_FALSE(TimeTillNextDelayedTask(&run_time));

  platform_->RunUntilIdle();
  EXPECT_THAT(run_times_, ElementsAre(start_time_, start_time_));
}

TEST_F(TimerTest, StartOneShot_NonZero) {
  Timer<TimerTest> timer(this, &TimerTest::CountingTask);
  timer.StartOneShot(10.0, BLINK_FROM_HERE);

  double run_time;
  EXPECT_TRUE(TimeTillNextDelayedTask(&run_time));
  EXPECT_FLOAT_EQ(10.0, run_time);

  platform_->RunUntilIdle();
  EXPECT_THAT(run_times_, ElementsAre(start_time_ + 10.0));
}

TEST_F(TimerTest, StartOneShot_NonZeroAndCancel) {
  Timer<TimerTest> timer(this, &TimerTest::CountingTask);
  timer.StartOneShot(10, BLINK_FROM_HERE);

  double run_time;
  EXPECT_TRUE(TimeTillNextDelayedTask(&run_time));
  EXPECT_FLOAT_EQ(10.0, run_time);

  timer.Stop();
  EXPECT_TRUE(TimeTillNextDelayedTask(&run_time));

  platform_->RunUntilIdle();
  EXPECT_FALSE(run_times_.size());
}

TEST_F(TimerTest, StartOneShot_NonZeroAndCancelThenRepost) {
  Timer<TimerTest> timer(this, &TimerTest::CountingTask);
  timer.StartOneShot(10, BLINK_FROM_HERE);

  double run_time;
  EXPECT_TRUE(TimeTillNextDelayedTask(&run_time));
  EXPECT_FLOAT_EQ(10.0, run_time);

  timer.Stop();
  EXPECT_TRUE(TimeTillNextDelayedTask(&run_time));

  platform_->RunUntilIdle();
  EXPECT_FALSE(run_times_.size());

  double second_post_time = MonotonicallyIncreasingTime();
  timer.StartOneShot(10, BLINK_FROM_HERE);

  EXPECT_TRUE(TimeTillNextDelayedTask(&run_time));
  EXPECT_FLOAT_EQ(10.0, run_time);

  platform_->RunUntilIdle();
  EXPECT_THAT(run_times_, ElementsAre(second_post_time + 10.0));
}

TEST_F(TimerTest, StartOneShot_NonZero_RepostingAfterRunning) {
  Timer<TimerTest> timer(this, &TimerTest::CountingTask);
  timer.StartOneShot(10, BLINK_FROM_HERE);

  double run_time;
  EXPECT_TRUE(TimeTillNextDelayedTask(&run_time));
  EXPECT_FLOAT_EQ(10.0, run_time);

  platform_->RunUntilIdle();
  EXPECT_THAT(run_times_, ElementsAre(start_time_ + 10.0));

  timer.StartOneShot(20, BLINK_FROM_HERE);

  EXPECT_TRUE(TimeTillNextDelayedTask(&run_time));
  EXPECT_FLOAT_EQ(20.0, run_time);

  platform_->RunUntilIdle();
  EXPECT_THAT(run_times_, ElementsAre(start_time_ + 10.0, start_time_ + 30.0));
}

TEST_F(TimerTest, PostingTimerTwiceWithSameRunTimeDoesNothing) {
  Timer<TimerTest> timer(this, &TimerTest::CountingTask);
  timer.StartOneShot(10, BLINK_FROM_HERE);
  timer.StartOneShot(10, BLINK_FROM_HERE);

  double run_time;
  EXPECT_TRUE(TimeTillNextDelayedTask(&run_time));
  EXPECT_FLOAT_EQ(10.0, run_time);

  platform_->RunUntilIdle();
  EXPECT_THAT(run_times_, ElementsAre(start_time_ + 10.0));
}

TEST_F(TimerTest, PostingTimerTwiceWithNewerRunTimeCancelsOriginalTask) {
  Timer<TimerTest> timer(this, &TimerTest::CountingTask);
  timer.StartOneShot(10, BLINK_FROM_HERE);
  timer.StartOneShot(0, BLINK_FROM_HERE);

  platform_->RunUntilIdle();
  EXPECT_THAT(run_times_, ElementsAre(start_time_ + 0.0));
}

TEST_F(TimerTest, PostingTimerTwiceWithLaterRunTimeCancelsOriginalTask) {
  Timer<TimerTest> timer(this, &TimerTest::CountingTask);
  timer.StartOneShot(0, BLINK_FROM_HERE);
  timer.StartOneShot(10, BLINK_FROM_HERE);

  platform_->RunUntilIdle();
  EXPECT_THAT(run_times_, ElementsAre(start_time_ + 10.0));
}

TEST_F(TimerTest, StartRepeatingTask) {
  Timer<TimerTest> timer(this, &TimerTest::CountingTask);
  timer.StartRepeating(1.0, BLINK_FROM_HERE);

  double run_time;
  EXPECT_TRUE(TimeTillNextDelayedTask(&run_time));
  EXPECT_FLOAT_EQ(1.0, run_time);

  RunUntilDeadline(start_time_ + 5.5);
  EXPECT_THAT(run_times_, ElementsAre(start_time_ + 1.0, start_time_ + 2.0,
                                      start_time_ + 3.0, start_time_ + 4.0,
                                      start_time_ + 5.0));
}

TEST_F(TimerTest, StartRepeatingTask_ThenCancel) {
  Timer<TimerTest> timer(this, &TimerTest::CountingTask);
  timer.StartRepeating(1.0, BLINK_FROM_HERE);

  double run_time;
  EXPECT_TRUE(TimeTillNextDelayedTask(&run_time));
  EXPECT_FLOAT_EQ(1.0, run_time);

  RunUntilDeadline(start_time_ + 2.5);
  EXPECT_THAT(run_times_, ElementsAre(start_time_ + 1.0, start_time_ + 2.0));

  timer.Stop();
  platform_->RunUntilIdle();

  EXPECT_THAT(run_times_, ElementsAre(start_time_ + 1.0, start_time_ + 2.0));
}

TEST_F(TimerTest, StartRepeatingTask_ThenPostOneShot) {
  Timer<TimerTest> timer(this, &TimerTest::CountingTask);
  timer.StartRepeating(1.0, BLINK_FROM_HERE);

  double run_time;
  EXPECT_TRUE(TimeTillNextDelayedTask(&run_time));
  EXPECT_FLOAT_EQ(1.0, run_time);

  RunUntilDeadline(start_time_ + 2.5);
  EXPECT_THAT(run_times_, ElementsAre(start_time_ + 1.0, start_time_ + 2.0));

  timer.StartOneShot(0, BLINK_FROM_HERE);
  platform_->RunUntilIdle();

  EXPECT_THAT(run_times_, ElementsAre(start_time_ + 1.0, start_time_ + 2.0,
                                      start_time_ + 2.5));
}

TEST_F(TimerTest, IsActive_NeverPosted) {
  Timer<TimerTest> timer(this, &TimerTest::CountingTask);

  EXPECT_FALSE(timer.IsActive());
}

TEST_F(TimerTest, IsActive_AfterPosting_OneShotZero) {
  Timer<TimerTest> timer(this, &TimerTest::CountingTask);
  timer.StartOneShot(0, BLINK_FROM_HERE);

  EXPECT_TRUE(timer.IsActive());
}

TEST_F(TimerTest, IsActive_AfterPosting_OneShotNonZero) {
  Timer<TimerTest> timer(this, &TimerTest::CountingTask);
  timer.StartOneShot(10, BLINK_FROM_HERE);

  EXPECT_TRUE(timer.IsActive());
}

TEST_F(TimerTest, IsActive_AfterPosting_Repeating) {
  Timer<TimerTest> timer(this, &TimerTest::CountingTask);
  timer.StartRepeating(1.0, BLINK_FROM_HERE);

  EXPECT_TRUE(timer.IsActive());
}

TEST_F(TimerTest, IsActive_AfterRunning_OneShotZero) {
  Timer<TimerTest> timer(this, &TimerTest::CountingTask);
  timer.StartOneShot(0, BLINK_FROM_HERE);

  platform_->RunUntilIdle();
  EXPECT_FALSE(timer.IsActive());
}

TEST_F(TimerTest, IsActive_AfterRunning_OneShotNonZero) {
  Timer<TimerTest> timer(this, &TimerTest::CountingTask);
  timer.StartOneShot(10, BLINK_FROM_HERE);

  platform_->RunUntilIdle();
  EXPECT_FALSE(timer.IsActive());
}

TEST_F(TimerTest, IsActive_AfterRunning_Repeating) {
  Timer<TimerTest> timer(this, &TimerTest::CountingTask);
  timer.StartRepeating(1.0, BLINK_FROM_HERE);

  RunUntilDeadline(start_time_ + 10);
  EXPECT_TRUE(timer.IsActive());  // It should run until cancelled.
}

TEST_F(TimerTest, NextFireInterval_OneShotZero) {
  Timer<TimerTest> timer(this, &TimerTest::CountingTask);
  timer.StartOneShot(0, BLINK_FROM_HERE);

  EXPECT_FLOAT_EQ(0.0, timer.NextFireInterval());
}

TEST_F(TimerTest, NextFireInterval_OneShotNonZero) {
  Timer<TimerTest> timer(this, &TimerTest::CountingTask);
  timer.StartOneShot(10, BLINK_FROM_HERE);

  EXPECT_FLOAT_EQ(10.0, timer.NextFireInterval());
}

TEST_F(TimerTest, NextFireInterval_OneShotNonZero_AfterAFewSeconds) {
  platform_->SetAutoAdvanceNowToPendingTasks(false);

  Timer<TimerTest> timer(this, &TimerTest::CountingTask);
  timer.StartOneShot(10, BLINK_FROM_HERE);

  platform_->AdvanceClockSeconds(2.0);
  EXPECT_FLOAT_EQ(8.0, timer.NextFireInterval());
}

TEST_F(TimerTest, NextFireInterval_Repeating) {
  Timer<TimerTest> timer(this, &TimerTest::CountingTask);
  timer.StartRepeating(20, BLINK_FROM_HERE);

  EXPECT_FLOAT_EQ(20.0, timer.NextFireInterval());
}

TEST_F(TimerTest, RepeatInterval_NeverStarted) {
  Timer<TimerTest> timer(this, &TimerTest::CountingTask);

  EXPECT_FLOAT_EQ(0.0, timer.RepeatInterval());
}

TEST_F(TimerTest, RepeatInterval_OneShotZero) {
  Timer<TimerTest> timer(this, &TimerTest::CountingTask);
  timer.StartOneShot(0, BLINK_FROM_HERE);

  EXPECT_FLOAT_EQ(0.0, timer.RepeatInterval());
}

TEST_F(TimerTest, RepeatInterval_OneShotNonZero) {
  Timer<TimerTest> timer(this, &TimerTest::CountingTask);
  timer.StartOneShot(10, BLINK_FROM_HERE);

  EXPECT_FLOAT_EQ(0.0, timer.RepeatInterval());
}

TEST_F(TimerTest, RepeatInterval_Repeating) {
  Timer<TimerTest> timer(this, &TimerTest::CountingTask);
  timer.StartRepeating(20, BLINK_FROM_HERE);

  EXPECT_FLOAT_EQ(20.0, timer.RepeatInterval());
}

TEST_F(TimerTest, AugmentRepeatInterval) {
  Timer<TimerTest> timer(this, &TimerTest::CountingTask);
  timer.StartRepeating(10, BLINK_FROM_HERE);
  EXPECT_FLOAT_EQ(10.0, timer.RepeatInterval());
  EXPECT_FLOAT_EQ(10.0, timer.NextFireInterval());

  platform_->AdvanceClockSeconds(2.0);
  timer.AugmentRepeatInterval(10);

  EXPECT_FLOAT_EQ(20.0, timer.RepeatInterval());
  EXPECT_FLOAT_EQ(18.0, timer.NextFireInterval());

  // NOTE setAutoAdvanceNowToPendingTasks(true) (which uses
  // cc::OrderedSimpleTaskRunner) results in somewhat strange behavior of the
  // test clock which breaks this test.  Specifically the test clock advancing
  // logic ignores newly posted delayed tasks and advances too far.
  RunUntilDeadline(start_time_ + 50.0);
  EXPECT_THAT(run_times_, ElementsAre(start_time_ + 20.0, start_time_ + 40.0));
}

TEST_F(TimerTest, AugmentRepeatInterval_TimerFireDelayed) {
  platform_->SetAutoAdvanceNowToPendingTasks(false);

  Timer<TimerTest> timer(this, &TimerTest::CountingTask);
  timer.StartRepeating(10, BLINK_FROM_HERE);
  EXPECT_FLOAT_EQ(10.0, timer.RepeatInterval());
  EXPECT_FLOAT_EQ(10.0, timer.NextFireInterval());

  platform_->AdvanceClockSeconds(123.0);  // Make the timer long overdue.
  timer.AugmentRepeatInterval(10);

  EXPECT_FLOAT_EQ(20.0, timer.RepeatInterval());
  // The timer is overdue so it should be scheduled to fire immediatly.
  EXPECT_FLOAT_EQ(0.0, timer.NextFireInterval());
}

TEST_F(TimerTest, RepeatingTimerDoesNotDrift) {
  platform_->SetAutoAdvanceNowToPendingTasks(false);

  Timer<TimerTest> timer(this, &TimerTest::RecordNextFireTimeTask);
  timer.StartRepeating(2.0, BLINK_FROM_HERE);

  RecordNextFireTimeTask(
      &timer);  // Next scheduled task to run at m_startTime + 2.0

  // Simulate timer firing early. Next scheduled task to run at
  // m_startTime + 4.0
  platform_->AdvanceClockSeconds(1.9);
  RunUntilDeadline(MonotonicallyIncreasingTime() + 0.2);

  // Next scheduled task to run at m_startTime + 6.0
  platform_->RunForPeriodSeconds(2.0);
  // Next scheduled task to run at m_startTime + 8.0
  platform_->RunForPeriodSeconds(2.1);
  // Next scheduled task to run at m_startTime + 10.0
  platform_->RunForPeriodSeconds(2.9);
  // Next scheduled task to run at m_startTime + 14.0 (skips a beat)
  platform_->AdvanceClockSeconds(3.1);
  platform_->RunUntilIdle();
  // Next scheduled task to run at m_startTime + 18.0 (skips a beat)
  platform_->AdvanceClockSeconds(4.0);
  platform_->RunUntilIdle();
  // Next scheduled task to run at m_startTime + 28.0 (skips 5 beats)
  platform_->AdvanceClockSeconds(10.0);
  platform_->RunUntilIdle();

  EXPECT_THAT(
      next_fire_times_,
      ElementsAre(start_time_ + 2.0, start_time_ + 4.0, start_time_ + 6.0,
                  start_time_ + 8.0, start_time_ + 10.0, start_time_ + 14.0,
                  start_time_ + 18.0, start_time_ + 28.0));
}

template <typename TimerFiredClass>
class TimerForTest : public TaskRunnerTimer<TimerFiredClass> {
 public:
  using TimerFiredFunction =
      typename TaskRunnerTimer<TimerFiredClass>::TimerFiredFunction;

  ~TimerForTest() override {}

  TimerForTest(RefPtr<WebTaskRunner> web_task_runner,
               TimerFiredClass* timer_fired_class,
               TimerFiredFunction timer_fired_function)
      : TaskRunnerTimer<TimerFiredClass>(std::move(web_task_runner),
                                         timer_fired_class,
                                         timer_fired_function) {}
};

TEST_F(TimerTest, UserSuppliedWebTaskRunner) {
  scoped_refptr<scheduler::TaskQueue> task_runner(
      platform_->GetRendererScheduler()->NewTimerTaskQueue(
          scheduler::TaskQueue::QueueType::TEST));
  RefPtr<scheduler::WebTaskRunnerImpl> web_task_runner =
      scheduler::WebTaskRunnerImpl::Create(task_runner);
  TimerForTest<TimerTest> timer(web_task_runner, this,
                                &TimerTest::CountingTask);
  timer.StartOneShot(0, BLINK_FROM_HERE);

  // Make sure the task was posted on taskRunner.
  EXPECT_FALSE(task_runner->IsEmpty());
}

TEST_F(TimerTest, RunOnHeapTimer) {
  RefPtr<OnHeapTimerOwner::Record> record = OnHeapTimerOwner::Record::Create();
  Persistent<OnHeapTimerOwner> owner = new OnHeapTimerOwner(record);

  owner->StartOneShot(0, BLINK_FROM_HERE);

  EXPECT_FALSE(record->TimerHasFired());
  platform_->RunUntilIdle();
  EXPECT_TRUE(record->TimerHasFired());
}

TEST_F(TimerTest, DestructOnHeapTimer) {
  RefPtr<OnHeapTimerOwner::Record> record = OnHeapTimerOwner::Record::Create();
  Persistent<OnHeapTimerOwner> owner = new OnHeapTimerOwner(record);

  record->Dispose();
  owner->StartOneShot(0, BLINK_FROM_HERE);

  owner = nullptr;
  ThreadState::Current()->CollectGarbage(BlinkGC::kNoHeapPointersOnStack,
                                         BlinkGC::kGCWithSweep,
                                         BlinkGC::kForcedGC);
  EXPECT_TRUE(record->OwnerIsDestructed());

  EXPECT_FALSE(record->TimerHasFired());
  platform_->RunUntilIdle();
  EXPECT_FALSE(record->TimerHasFired());
}

TEST_F(TimerTest, MarkOnHeapTimerAsUnreachable) {
  RefPtr<OnHeapTimerOwner::Record> record = OnHeapTimerOwner::Record::Create();
  Persistent<OnHeapTimerOwner> owner = new OnHeapTimerOwner(record);

  record->Dispose();
  owner->StartOneShot(0, BLINK_FROM_HERE);

  owner = nullptr;
  ThreadState::Current()->CollectGarbage(BlinkGC::kNoHeapPointersOnStack,
                                         BlinkGC::kGCWithoutSweep,
                                         BlinkGC::kForcedGC);
  EXPECT_FALSE(record->OwnerIsDestructed());

  {
    GCForbiddenScope scope;
    EXPECT_FALSE(record->TimerHasFired());
    platform_->RunUntilIdle();
    EXPECT_FALSE(record->TimerHasFired());
    EXPECT_FALSE(record->OwnerIsDestructed());
  }
}

namespace {

class TaskObserver : public base::MessageLoop::TaskObserver {
 public:
  TaskObserver(RefPtr<WebTaskRunner> task_runner,
               std::vector<RefPtr<WebTaskRunner>>* run_order)
      : task_runner_(std::move(task_runner)), run_order_(run_order) {}

  void WillProcessTask(const base::PendingTask&) {}

  void DidProcessTask(const base::PendingTask&) {
    run_order_->push_back(task_runner_);
  }

 private:
  RefPtr<WebTaskRunner> task_runner_;
  std::vector<RefPtr<WebTaskRunner>>* run_order_;
};

}  // namespace

TEST_F(TimerTest, MoveToNewTaskRunnerOneShot) {
  std::vector<RefPtr<WebTaskRunner>> run_order;

  scoped_refptr<scheduler::TaskQueue> task_runner1(
      platform_->GetRendererScheduler()->NewTimerTaskQueue(
          scheduler::TaskQueue::QueueType::TEST));
  RefPtr<scheduler::WebTaskRunnerImpl> web_task_runner1 =
      scheduler::WebTaskRunnerImpl::Create(task_runner1);
  TaskObserver task_observer1(web_task_runner1, &run_order);
  task_runner1->AddTaskObserver(&task_observer1);

  scoped_refptr<scheduler::TaskQueue> task_runner2(
      platform_->GetRendererScheduler()->NewTimerTaskQueue(
          scheduler::TaskQueue::QueueType::TEST));
  RefPtr<scheduler::WebTaskRunnerImpl> web_task_runner2 =
      scheduler::WebTaskRunnerImpl::Create(task_runner2);
  TaskObserver task_observer2(web_task_runner2, &run_order);
  task_runner2->AddTaskObserver(&task_observer2);

  TimerForTest<TimerTest> timer(web_task_runner1, this,
                                &TimerTest::CountingTask);

  double start_time = MonotonicallyIncreasingTime();

  timer.StartOneShot(1, BLINK_FROM_HERE);

  platform_->RunForPeriodSeconds(0.5);

  timer.MoveToNewTaskRunner(web_task_runner2);

  platform_->RunUntilIdle();

  EXPECT_THAT(run_times_, ElementsAre(start_time + 1.0));

  EXPECT_THAT(run_order, ElementsAre(web_task_runner2));

  EXPECT_TRUE(task_runner1->IsEmpty());
  EXPECT_TRUE(task_runner2->IsEmpty());
}

TEST_F(TimerTest, MoveToNewTaskRunnerRepeating) {
  std::vector<RefPtr<WebTaskRunner>> run_order;

  scoped_refptr<scheduler::TaskQueue> task_runner1(
      platform_->GetRendererScheduler()->NewTimerTaskQueue(
          scheduler::TaskQueue::QueueType::TEST));
  RefPtr<scheduler::WebTaskRunnerImpl> web_task_runner1 =
      scheduler::WebTaskRunnerImpl::Create(task_runner1);
  TaskObserver task_observer1(web_task_runner1, &run_order);
  task_runner1->AddTaskObserver(&task_observer1);

  scoped_refptr<scheduler::TaskQueue> task_runner2(
      platform_->GetRendererScheduler()->NewTimerTaskQueue(
          scheduler::TaskQueue::QueueType::TEST));
  RefPtr<scheduler::WebTaskRunnerImpl> web_task_runner2 =
      scheduler::WebTaskRunnerImpl::Create(task_runner2);
  TaskObserver task_observer2(web_task_runner2, &run_order);
  task_runner2->AddTaskObserver(&task_observer2);

  TimerForTest<TimerTest> timer(web_task_runner1, this,
                                &TimerTest::CountingTask);

  double start_time = MonotonicallyIncreasingTime();

  timer.StartRepeating(1, BLINK_FROM_HERE);

  platform_->RunForPeriodSeconds(2.5);

  timer.MoveToNewTaskRunner(web_task_runner2);

  platform_->RunForPeriodSeconds(2);

  EXPECT_THAT(run_times_, ElementsAre(start_time + 1.0, start_time + 2.0,
                                      start_time + 3.0, start_time + 4.0));

  EXPECT_THAT(run_order, ElementsAre(web_task_runner1, web_task_runner1,
                                     web_task_runner2, web_task_runner2));

  EXPECT_TRUE(task_runner1->IsEmpty());
  EXPECT_FALSE(task_runner2->IsEmpty());
}

// This test checks that when inactive timer is moved to a different task
// runner it isn't activated.
TEST_F(TimerTest, MoveToNewTaskRunnerWithoutTasks) {
  scoped_refptr<scheduler::TaskQueue> task_runner1(
      platform_->GetRendererScheduler()->NewTimerTaskQueue(
          scheduler::TaskQueue::QueueType::TEST));
  RefPtr<scheduler::WebTaskRunnerImpl> web_task_runner1 =
      scheduler::WebTaskRunnerImpl::Create(task_runner1);

  scoped_refptr<scheduler::TaskQueue> task_runner2(
      platform_->GetRendererScheduler()->NewTimerTaskQueue(
          scheduler::TaskQueue::QueueType::TEST));
  RefPtr<scheduler::WebTaskRunnerImpl> web_task_runner2 =
      scheduler::WebTaskRunnerImpl::Create(task_runner2);

  TimerForTest<TimerTest> timer(web_task_runner1, this,
                                &TimerTest::CountingTask);

  platform_->RunUntilIdle();
  EXPECT_TRUE(!run_times_.size());
  EXPECT_TRUE(task_runner1->IsEmpty());
  EXPECT_TRUE(task_runner2->IsEmpty());
}

}  // namespace
}  // namespace blink
