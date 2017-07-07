// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/base/task_queue_manager.h"

#include <stddef.h>

#include <utility>

#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/trace_event_analyzer.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/blame_context.h"
#include "base/trace_event/trace_buffer.h"
#include "cc/test/ordered_simple_task_runner.h"
#include "platform/scheduler/base/real_time_domain.h"
#include "platform/scheduler/base/task_queue_impl.h"
#include "platform/scheduler/base/task_queue_manager_delegate_for_test.h"
#include "platform/scheduler/base/task_queue_selector.h"
#include "platform/scheduler/base/test_count_uses_time_source.h"
#include "platform/scheduler/base/test_task_time_observer.h"
#include "platform/scheduler/base/test_time_source.h"
#include "platform/scheduler/base/virtual_time_domain.h"
#include "platform/scheduler/base/work_queue.h"
#include "platform/scheduler/base/work_queue_sets.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::AnyNumber;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::Mock;
using ::testing::Not;
using ::testing::_;
using blink::scheduler::internal::EnqueueOrder;

namespace blink {
namespace scheduler {

class TaskQueueManagerForTest : public TaskQueueManager {
 public:
  explicit TaskQueueManagerForTest(
      scoped_refptr<TaskQueueManagerDelegate> delegate)
      : TaskQueueManager(delegate) {}

  using TaskQueueManager::NextTaskDelay;
};

class MessageLoopTaskRunner : public TaskQueueManagerDelegateForTest {
 public:
  static scoped_refptr<MessageLoopTaskRunner> Create(
      std::unique_ptr<base::TickClock> tick_clock) {
    return make_scoped_refptr(new MessageLoopTaskRunner(std::move(tick_clock)));
  }

  // TaskQueueManagerDelegateForTest:
  bool IsNested() const override {
    DCHECK(RunsTasksInCurrentSequence());
    return base::RunLoop::IsNestedOnCurrentThread();
  }

  void AddNestingObserver(base::RunLoop::NestingObserver* observer) override {
    base::RunLoop::AddNestingObserverOnCurrentThread(observer);
  }

  void RemoveNestingObserver(
      base::RunLoop::NestingObserver* observer) override {
    base::RunLoop::RemoveNestingObserverOnCurrentThread(observer);
  }

 private:
  explicit MessageLoopTaskRunner(std::unique_ptr<base::TickClock> tick_clock)
      : TaskQueueManagerDelegateForTest(base::ThreadTaskRunnerHandle::Get(),
                                        std::move(tick_clock)) {}
  ~MessageLoopTaskRunner() override {}
};

class TaskQueueManagerTest : public ::testing::Test {
 public:
  TaskQueueManagerTest() {}
  void DeleteTaskQueueManager() { manager_.reset(); }

 protected:
  void InitializeWithClock(size_t num_queues,
                           std::unique_ptr<base::TickClock> test_time_source) {
    test_task_runner_ = make_scoped_refptr(
        new cc::OrderedSimpleTaskRunner(now_src_.get(), false));
    main_task_runner_ = TaskQueueManagerDelegateForTest::Create(
        test_task_runner_.get(),
        base::MakeUnique<TestTimeSource>(now_src_.get()));

    manager_ = base::MakeUnique<TaskQueueManagerForTest>(main_task_runner_);

    for (size_t i = 0; i < num_queues; i++)
      runners_.push_back(
          manager_->NewTaskQueue(TaskQueue::Spec(TaskQueue::QueueType::TEST)));
  }

  void Initialize(size_t num_queues) {
    now_src_.reset(new base::SimpleTestTickClock());
    now_src_->Advance(base::TimeDelta::FromMicroseconds(1000));
    InitializeWithClock(num_queues,
                        base::MakeUnique<TestTimeSource>(now_src_.get()));
  }

  void InitializeWithRealMessageLoop(size_t num_queues) {
    now_src_.reset(new base::SimpleTestTickClock());
    message_loop_.reset(new base::MessageLoop());
    // A null clock triggers some assertions.
    now_src_->Advance(base::TimeDelta::FromMicroseconds(1000));
    manager_ =
        base::MakeUnique<TaskQueueManagerForTest>(MessageLoopTaskRunner::Create(
            base::WrapUnique(new TestTimeSource(now_src_.get()))));

    for (size_t i = 0; i < num_queues; i++)
      runners_.push_back(
          manager_->NewTaskQueue(TaskQueue::Spec(TaskQueue::QueueType::TEST)));
  }

  void WakeUpReadyDelayedQueues(LazyNow lazy_now) {
    manager_->WakeUpReadyDelayedQueues(&lazy_now);
  }

  using NextTaskDelay = TaskQueueManagerForTest::NextTaskDelay;

  base::Optional<NextTaskDelay> ComputeDelayTillNextTask(LazyNow* lazy_now) {
    base::AutoLock lock(manager_->any_thread_lock_);
    return manager_->ComputeDelayTillNextTaskLocked(lazy_now);
  }

  void PostDoWorkContinuation(base::Optional<NextTaskDelay> next_delay,
                              LazyNow* lazy_now) {
    MoveableAutoLock lock(manager_->any_thread_lock_);
    return manager_->PostDoWorkContinuationLocked(next_delay, lazy_now,
                                                  std::move(lock));
  }

  int immediate_do_work_posted_count() const {
    base::AutoLock lock(manager_->any_thread_lock_);
    return manager_->any_thread().immediate_do_work_posted_count;
  }

  base::TimeTicks next_delayed_do_work_time() const {
    return manager_->next_delayed_do_work_.run_time();
  }

  EnqueueOrder GetNextSequenceNumber() const {
    return manager_->GetNextSequenceNumber();
  }

  void MaybeScheduleImmediateWorkLocked(
      const tracked_objects::Location& from_here) {
    MoveableAutoLock lock(manager_->any_thread_lock_);
    manager_->MaybeScheduleImmediateWorkLocked(from_here, std::move(lock));
  }

  // Runs all immediate tasks until there is no more work to do and advances
  // time if there is a pending delayed task. |per_run_time_callback| is called
  // when the clock advances.
  void RunUntilIdle(base::Closure per_run_time_callback) {
    for (;;) {
      // Advance time if we've run out of immediate work to do.
      if (manager_->selector_.EnabledWorkQueuesEmpty()) {
        base::TimeTicks run_time;
        if (manager_->real_time_domain()->NextScheduledRunTime(&run_time)) {
          now_src_->SetNowTicks(run_time);
          per_run_time_callback.Run();
        } else {
          break;
        }
      }

      test_task_runner_->RunPendingTasks();
    }
  }

  std::unique_ptr<base::MessageLoop> message_loop_;
  std::unique_ptr<base::SimpleTestTickClock> now_src_;
  scoped_refptr<TaskQueueManagerDelegateForTest> main_task_runner_;
  scoped_refptr<cc::OrderedSimpleTaskRunner> test_task_runner_;
  std::unique_ptr<TaskQueueManagerForTest> manager_;
  std::vector<scoped_refptr<internal::TaskQueueImpl>> runners_;
  TestTaskTimeObserver test_task_time_observer_;
};

void PostFromNestedRunloop(base::MessageLoop* message_loop,
                           base::SingleThreadTaskRunner* runner,
                           std::vector<std::pair<base::Closure, bool>>* tasks) {
  base::MessageLoop::ScopedNestableTaskAllower allow(message_loop);
  for (std::pair<base::Closure, bool>& pair : *tasks) {
    if (pair.second) {
      runner->PostTask(FROM_HERE, pair.first);
    } else {
      runner->PostNonNestableTask(FROM_HERE, pair.first);
    }
  }
  base::RunLoop().RunUntilIdle();
}

void NopTask() {}

TEST_F(TaskQueueManagerTest,
       NowCalledMinimumNumberOfTimesToComputeTaskDurations) {
  message_loop_.reset(new base::MessageLoop());
  // This memory is managed by the TaskQueueManager, but we need to hold a
  // pointer to this object to read out how many times Now was called.
  TestCountUsesTimeSource* test_count_uses_time_source =
      new TestCountUsesTimeSource();

  manager_ =
      base::MakeUnique<TaskQueueManagerForTest>(MessageLoopTaskRunner::Create(
          base::WrapUnique(test_count_uses_time_source)));
  manager_->SetWorkBatchSize(6);
  manager_->AddTaskTimeObserver(&test_task_time_observer_);

  for (size_t i = 0; i < 3; i++)
    runners_.push_back(
        manager_->NewTaskQueue(TaskQueue::Spec(TaskQueue::QueueType::TEST)));

  runners_[0]->PostTask(FROM_HERE, base::Bind(&NopTask));
  runners_[0]->PostTask(FROM_HERE, base::Bind(&NopTask));
  runners_[1]->PostTask(FROM_HERE, base::Bind(&NopTask));
  runners_[1]->PostTask(FROM_HERE, base::Bind(&NopTask));
  runners_[2]->PostTask(FROM_HERE, base::Bind(&NopTask));
  runners_[2]->PostTask(FROM_HERE, base::Bind(&NopTask));

  base::RunLoop().RunUntilIdle();
  // We need to call Now for the beginning of the first task, and then the end
  // of every task after. We reuse the end time of one task for the start time
  // of the next task. In this case, there were 6 tasks, so we expect 7 calls to
  // Now.
  EXPECT_EQ(7, test_count_uses_time_source->now_calls_count());
}

TEST_F(TaskQueueManagerTest, NowNotCalledForNestedTasks) {
  message_loop_.reset(new base::MessageLoop());
  // This memory is managed by the TaskQueueManager, but we need to hold a
  // pointer to this object to read out how many times Now was called.
  TestCountUsesTimeSource* test_count_uses_time_source =
      new TestCountUsesTimeSource();

  manager_ =
      base::MakeUnique<TaskQueueManagerForTest>(MessageLoopTaskRunner::Create(
          base::WrapUnique(test_count_uses_time_source)));
  manager_->AddTaskTimeObserver(&test_task_time_observer_);

  runners_.push_back(
      manager_->NewTaskQueue(TaskQueue::Spec(TaskQueue::QueueType::TEST)));

  std::vector<std::pair<base::Closure, bool>> tasks_to_post_from_nested_loop;
  for (int i = 0; i <= 6; ++i) {
    tasks_to_post_from_nested_loop.push_back(
        std::make_pair(base::Bind(&NopTask), true));
  }

  runners_[0]->PostTask(
      FROM_HERE, base::Bind(&PostFromNestedRunloop, message_loop_.get(),
                            base::RetainedRef(runners_[0]),
                            base::Unretained(&tasks_to_post_from_nested_loop)));

  base::RunLoop().RunUntilIdle();
  // We need to call Now twice, to measure the start and end of the outermost
  // task. We shouldn't call it for any of the nested tasks.
  EXPECT_EQ(2, test_count_uses_time_source->now_calls_count());
}

void NullTask() {}

void TestTask(EnqueueOrder value, std::vector<EnqueueOrder>* out_result) {
  out_result->push_back(value);
}

TEST_F(TaskQueueManagerTest, SingleQueuePosting) {
  Initialize(1u);

  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order));
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 3, &run_order));

  test_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1, 2, 3));
}

TEST_F(TaskQueueManagerTest, MultiQueuePosting) {
  Initialize(3u);

  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order));
  runners_[1]->PostTask(FROM_HERE, base::Bind(&TestTask, 3, &run_order));
  runners_[1]->PostTask(FROM_HERE, base::Bind(&TestTask, 4, &run_order));
  runners_[2]->PostTask(FROM_HERE, base::Bind(&TestTask, 5, &run_order));
  runners_[2]->PostTask(FROM_HERE, base::Bind(&TestTask, 6, &run_order));

  test_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1, 2, 3, 4, 5, 6));
}

TEST_F(TaskQueueManagerTest, NonNestableTaskPosting) {
  InitializeWithRealMessageLoop(1u);

  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostNonNestableTask(FROM_HERE,
                                   base::Bind(&TestTask, 1, &run_order));

  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1));
}

TEST_F(TaskQueueManagerTest, NonNestableTaskExecutesInExpectedOrder) {
  InitializeWithRealMessageLoop(1u);

  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order));
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 3, &run_order));
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 4, &run_order));
  runners_[0]->PostNonNestableTask(FROM_HERE,
                                   base::Bind(&TestTask, 5, &run_order));

  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1, 2, 3, 4, 5));
}

TEST_F(TaskQueueManagerTest, NonNestableTaskDoesntExecuteInNestedLoop) {
  InitializeWithRealMessageLoop(1u);

  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order));

  std::vector<std::pair<base::Closure, bool>> tasks_to_post_from_nested_loop;
  tasks_to_post_from_nested_loop.push_back(
      std::make_pair(base::Bind(&TestTask, 3, &run_order), false));
  tasks_to_post_from_nested_loop.push_back(
      std::make_pair(base::Bind(&TestTask, 4, &run_order), true));
  tasks_to_post_from_nested_loop.push_back(
      std::make_pair(base::Bind(&TestTask, 5, &run_order), true));

  runners_[0]->PostTask(
      FROM_HERE, base::Bind(&PostFromNestedRunloop, message_loop_.get(),
                            base::RetainedRef(runners_[0]),
                            base::Unretained(&tasks_to_post_from_nested_loop)));

  base::RunLoop().RunUntilIdle();
  // Note we expect task 3 to run last because it's non-nestable.
  EXPECT_THAT(run_order, ElementsAre(1, 2, 4, 5, 3));
}

TEST_F(TaskQueueManagerTest, HasPendingImmediateWork_ImmediateTask) {
  Initialize(1u);

  std::vector<EnqueueOrder> run_order;
  EXPECT_FALSE(runners_[0]->HasTaskToRunImmediately());
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));
  EXPECT_TRUE(runners_[0]->HasTaskToRunImmediately());

  // Move the task into the |immediate_work_queue|.
  EXPECT_TRUE(runners_[0]->immediate_work_queue()->Empty());
  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter =
      runners_[0]->CreateQueueEnabledVoter();
  voter->SetQueueEnabled(false);
  test_task_runner_->RunUntilIdle();
  EXPECT_FALSE(runners_[0]->immediate_work_queue()->Empty());
  EXPECT_TRUE(runners_[0]->HasTaskToRunImmediately());

  // Run the task, making the queue empty.
  voter->SetQueueEnabled(true);
  test_task_runner_->RunUntilIdle();
  EXPECT_FALSE(runners_[0]->HasTaskToRunImmediately());
}

TEST_F(TaskQueueManagerTest, HasPendingImmediateWork_DelayedTask) {
  Initialize(1u);

  std::vector<EnqueueOrder> run_order;
  base::TimeDelta delay(base::TimeDelta::FromMilliseconds(10));
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order),
                               delay);
  EXPECT_FALSE(runners_[0]->HasTaskToRunImmediately());
  now_src_->Advance(delay);
  EXPECT_TRUE(runners_[0]->HasTaskToRunImmediately());

  // Move the task into the |delayed_work_queue|.
  WakeUpReadyDelayedQueues(LazyNow(now_src_.get()));
  EXPECT_FALSE(runners_[0]->delayed_work_queue()->Empty());
  EXPECT_TRUE(runners_[0]->HasTaskToRunImmediately());

  // Run the task, making the queue empty.
  test_task_runner_->RunUntilIdle();
  EXPECT_FALSE(runners_[0]->HasTaskToRunImmediately());
}

TEST_F(TaskQueueManagerTest, DelayedTaskPosting) {
  Initialize(1u);

  std::vector<EnqueueOrder> run_order;
  base::TimeDelta delay(base::TimeDelta::FromMilliseconds(10));
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order),
                               delay);
  EXPECT_EQ(delay, test_task_runner_->DelayToNextTaskTime());
  EXPECT_FALSE(runners_[0]->HasTaskToRunImmediately());
  EXPECT_TRUE(run_order.empty());

  // The task doesn't run before the delay has completed.
  test_task_runner_->RunForPeriod(base::TimeDelta::FromMilliseconds(9));
  EXPECT_TRUE(run_order.empty());

  // After the delay has completed, the task runs normally.
  test_task_runner_->RunForPeriod(base::TimeDelta::FromMilliseconds(1));
  EXPECT_THAT(run_order, ElementsAre(1));
  EXPECT_FALSE(runners_[0]->HasTaskToRunImmediately());
}

bool MessageLoopTaskCounter(size_t* count) {
  *count = *count + 1;
  return true;
}

TEST_F(TaskQueueManagerTest, DelayedTaskExecutedInOneMessageLoopTask) {
  Initialize(1u);

  base::TimeDelta delay(base::TimeDelta::FromMilliseconds(10));
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&NopTask), delay);

  size_t task_count = 0;
  test_task_runner_->RunTasksWhile(
      base::Bind(&MessageLoopTaskCounter, &task_count));
  EXPECT_EQ(1u, task_count);
}

TEST_F(TaskQueueManagerTest, DelayedTaskPosting_MultipleTasks_DecendingOrder) {
  Initialize(1u);

  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order),
                               base::TimeDelta::FromMilliseconds(10));

  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order),
                               base::TimeDelta::FromMilliseconds(8));

  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 3, &run_order),
                               base::TimeDelta::FromMilliseconds(5));

  EXPECT_EQ(base::TimeDelta::FromMilliseconds(5),
            test_task_runner_->DelayToNextTaskTime());

  test_task_runner_->RunForPeriod(base::TimeDelta::FromMilliseconds(5));
  EXPECT_THAT(run_order, ElementsAre(3));
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(3),
            test_task_runner_->DelayToNextTaskTime());

  test_task_runner_->RunForPeriod(base::TimeDelta::FromMilliseconds(3));
  EXPECT_THAT(run_order, ElementsAre(3, 2));
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(2),
            test_task_runner_->DelayToNextTaskTime());

  test_task_runner_->RunForPeriod(base::TimeDelta::FromMilliseconds(2));
  EXPECT_THAT(run_order, ElementsAre(3, 2, 1));
}

TEST_F(TaskQueueManagerTest, DelayedTaskPosting_MultipleTasks_AscendingOrder) {
  Initialize(1u);

  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order),
                               base::TimeDelta::FromMilliseconds(1));

  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order),
                               base::TimeDelta::FromMilliseconds(5));

  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 3, &run_order),
                               base::TimeDelta::FromMilliseconds(10));

  EXPECT_EQ(base::TimeDelta::FromMilliseconds(1),
            test_task_runner_->DelayToNextTaskTime());

  test_task_runner_->RunForPeriod(base::TimeDelta::FromMilliseconds(1));
  EXPECT_THAT(run_order, ElementsAre(1));
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(4),
            test_task_runner_->DelayToNextTaskTime());

  test_task_runner_->RunForPeriod(base::TimeDelta::FromMilliseconds(4));
  EXPECT_THAT(run_order, ElementsAre(1, 2));
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(5),
            test_task_runner_->DelayToNextTaskTime());

  test_task_runner_->RunForPeriod(base::TimeDelta::FromMilliseconds(5));
  EXPECT_THAT(run_order, ElementsAre(1, 2, 3));
}

TEST_F(TaskQueueManagerTest, PostDelayedTask_SharesUnderlyingDelayedTasks) {
  Initialize(1u);

  std::vector<EnqueueOrder> run_order;
  base::TimeDelta delay(base::TimeDelta::FromMilliseconds(10));
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order),
                               delay);
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order),
                               delay);
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 3, &run_order),
                               delay);

  EXPECT_EQ(1u, test_task_runner_->NumPendingTasks());
}

class TestObject {
 public:
  ~TestObject() { destructor_count__++; }

  void Run() { FAIL() << "TestObject::Run should not be called"; }

  static int destructor_count__;
};

int TestObject::destructor_count__ = 0;

TEST_F(TaskQueueManagerTest, PendingDelayedTasksRemovedOnShutdown) {
  Initialize(1u);

  TestObject::destructor_count__ = 0;

  base::TimeDelta delay(base::TimeDelta::FromMilliseconds(10));
  runners_[0]->PostDelayedTask(
      FROM_HERE, base::Bind(&TestObject::Run, base::Owned(new TestObject())),
      delay);
  runners_[0]->PostTask(
      FROM_HERE, base::Bind(&TestObject::Run, base::Owned(new TestObject())));

  manager_.reset();

  EXPECT_EQ(2, TestObject::destructor_count__);
}

TEST_F(TaskQueueManagerTest, InsertAndRemoveFence) {
  Initialize(1u);
  runners_[0]->InsertFence(TaskQueue::InsertFencePosition::NOW);

  std::vector<EnqueueOrder> run_order;
  // Posting a task when pumping is disabled doesn't result in work getting
  // posted.
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));
  EXPECT_FALSE(test_task_runner_->HasPendingTasks());

  // However polling still works.
  EXPECT_TRUE(runners_[0]->HasTaskToRunImmediately());

  // After removing the fence the task runs normally.
  runners_[0]->RemoveFence();
  EXPECT_TRUE(test_task_runner_->HasPendingTasks());
  test_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1));
}

TEST_F(TaskQueueManagerTest, RemovingFenceForDisabledQueueDoesNotPostDoWork) {
  Initialize(1u);

  std::vector<EnqueueOrder> run_order;
  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter =
      runners_[0]->CreateQueueEnabledVoter();
  voter->SetQueueEnabled(false);
  runners_[0]->InsertFence(TaskQueue::InsertFencePosition::NOW);
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));

  runners_[0]->RemoveFence();
  EXPECT_FALSE(test_task_runner_->HasPendingTasks());
}

TEST_F(TaskQueueManagerTest, EnablingFencedQueueDoesNotPostDoWork) {
  Initialize(1u);

  std::vector<EnqueueOrder> run_order;
  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter =
      runners_[0]->CreateQueueEnabledVoter();
  voter->SetQueueEnabled(false);
  runners_[0]->InsertFence(TaskQueue::InsertFencePosition::NOW);
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));

  voter->SetQueueEnabled(true);
  EXPECT_FALSE(test_task_runner_->HasPendingTasks());
}

TEST_F(TaskQueueManagerTest, DenyRunning_BeforePosting) {
  Initialize(1u);

  std::vector<EnqueueOrder> run_order;
  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter =
      runners_[0]->CreateQueueEnabledVoter();
  voter->SetQueueEnabled(false);
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));
  EXPECT_FALSE(test_task_runner_->HasPendingTasks());

  test_task_runner_->RunUntilIdle();
  EXPECT_TRUE(run_order.empty());

  voter->SetQueueEnabled(true);
  test_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1));
}

TEST_F(TaskQueueManagerTest, DenyRunning_AfterPosting) {
  Initialize(1u);

  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));
  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter =
      runners_[0]->CreateQueueEnabledVoter();
  EXPECT_TRUE(test_task_runner_->HasPendingTasks());
  voter->SetQueueEnabled(false);

  test_task_runner_->RunUntilIdle();
  EXPECT_TRUE(run_order.empty());

  voter->SetQueueEnabled(true);
  test_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1));
}

TEST_F(TaskQueueManagerTest, DenyRunning_AfterRemovingFence) {
  Initialize(1u);

  std::vector<EnqueueOrder> run_order;
  runners_[0]->InsertFence(TaskQueue::InsertFencePosition::NOW);
  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter =
      runners_[0]->CreateQueueEnabledVoter();
  voter->SetQueueEnabled(false);
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));

  test_task_runner_->RunUntilIdle();
  EXPECT_TRUE(run_order.empty());

  runners_[0]->RemoveFence();
  voter->SetQueueEnabled(true);
  test_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1));
}

TEST_F(TaskQueueManagerTest, RemovingFenceWithDelayedTask) {
  Initialize(1u);
  runners_[0]->InsertFence(TaskQueue::InsertFencePosition::NOW);

  std::vector<EnqueueOrder> run_order;
  // Posting a delayed task when fenced will apply the delay, but won't cause
  // work to executed afterwards.
  base::TimeDelta delay(base::TimeDelta::FromMilliseconds(10));
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order),
                               delay);

  // The task does not run even though it's delay is up.
  test_task_runner_->RunForPeriod(base::TimeDelta::FromMilliseconds(10));
  EXPECT_TRUE(run_order.empty());

  // Removing the fence causes the task to run.
  runners_[0]->RemoveFence();
  EXPECT_TRUE(test_task_runner_->HasPendingTasks());
  test_task_runner_->RunPendingTasks();
  EXPECT_THAT(run_order, ElementsAre(1));
}

TEST_F(TaskQueueManagerTest, RemovingFenceWithMultipleDelayedTasks) {
  Initialize(1u);
  runners_[0]->InsertFence(TaskQueue::InsertFencePosition::NOW);

  std::vector<EnqueueOrder> run_order;
  // Posting a delayed task when fenced will apply the delay, but won't cause
  // work to executed afterwards.
  base::TimeDelta delay1(base::TimeDelta::FromMilliseconds(1));
  base::TimeDelta delay2(base::TimeDelta::FromMilliseconds(10));
  base::TimeDelta delay3(base::TimeDelta::FromMilliseconds(20));
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order),
                               delay1);
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order),
                               delay2);
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 3, &run_order),
                               delay3);

  now_src_->Advance(base::TimeDelta::FromMilliseconds(15));
  test_task_runner_->RunUntilIdle();
  EXPECT_TRUE(run_order.empty());

  // Removing the fence causes the ready tasks to run.
  runners_[0]->RemoveFence();
  test_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1, 2));
}

TEST_F(TaskQueueManagerTest, InsertFencePreventsDelayedTasksFromRunning) {
  Initialize(1u);
  runners_[0]->InsertFence(TaskQueue::InsertFencePosition::NOW);

  std::vector<EnqueueOrder> run_order;
  base::TimeDelta delay(base::TimeDelta::FromMilliseconds(10));
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order),
                               delay);

  test_task_runner_->RunForPeriod(base::TimeDelta::FromMilliseconds(10));
  EXPECT_TRUE(run_order.empty());
}

TEST_F(TaskQueueManagerTest, MultipleFences) {
  Initialize(1u);

  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order));
  runners_[0]->InsertFence(TaskQueue::InsertFencePosition::NOW);

  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 3, &run_order));
  test_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1, 2));

  runners_[0]->InsertFence(TaskQueue::InsertFencePosition::NOW);
  // Subsequent tasks should be blocked.
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 4, &run_order));
  test_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1, 2, 3));
}

TEST_F(TaskQueueManagerTest, InsertFenceThenImmediatlyRemoveDoesNotBlock) {
  Initialize(1u);
  runners_[0]->InsertFence(TaskQueue::InsertFencePosition::NOW);
  runners_[0]->RemoveFence();

  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order));

  test_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1, 2));
}

TEST_F(TaskQueueManagerTest, InsertFencePostThenRemoveDoesNotBlock) {
  Initialize(1u);
  runners_[0]->InsertFence(TaskQueue::InsertFencePosition::NOW);

  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order));
  runners_[0]->RemoveFence();

  test_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1, 2));
}

TEST_F(TaskQueueManagerTest, MultipleFencesWithInitiallyEmptyQueue) {
  Initialize(1u);
  runners_[0]->InsertFence(TaskQueue::InsertFencePosition::NOW);

  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));
  runners_[0]->InsertFence(TaskQueue::InsertFencePosition::NOW);
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order));
  test_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1));
}

TEST_F(TaskQueueManagerTest, BlockedByFence) {
  Initialize(1u);
  EXPECT_FALSE(runners_[0]->BlockedByFence());

  runners_[0]->InsertFence(TaskQueue::InsertFencePosition::NOW);
  EXPECT_TRUE(runners_[0]->BlockedByFence());

  runners_[0]->RemoveFence();
  EXPECT_FALSE(runners_[0]->BlockedByFence());

  runners_[0]->PostTask(FROM_HERE, base::Bind(&NopTask));
  runners_[0]->InsertFence(TaskQueue::InsertFencePosition::NOW);
  EXPECT_FALSE(runners_[0]->BlockedByFence());

  test_task_runner_->RunUntilIdle();
  EXPECT_TRUE(runners_[0]->BlockedByFence());

  runners_[0]->RemoveFence();
  EXPECT_FALSE(runners_[0]->BlockedByFence());
}

TEST_F(TaskQueueManagerTest, BlockedByFence_BothTypesOfFence) {
  Initialize(1u);

  runners_[0]->PostTask(FROM_HERE, base::Bind(&NopTask));

  runners_[0]->InsertFence(TaskQueue::InsertFencePosition::NOW);
  EXPECT_FALSE(runners_[0]->BlockedByFence());

  runners_[0]->InsertFence(TaskQueue::InsertFencePosition::BEGINNING_OF_TIME);
  EXPECT_TRUE(runners_[0]->BlockedByFence());
}

void ReentrantTestTask(scoped_refptr<base::SingleThreadTaskRunner> runner,
                       int countdown,
                       std::vector<EnqueueOrder>* out_result) {
  out_result->push_back(countdown);
  if (--countdown) {
    runner->PostTask(FROM_HERE,
                     Bind(&ReentrantTestTask, runner, countdown, out_result));
  }
}

TEST_F(TaskQueueManagerTest, ReentrantPosting) {
  Initialize(1u);

  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostTask(FROM_HERE,
                        Bind(&ReentrantTestTask, runners_[0], 3, &run_order));

  test_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(3, 2, 1));
}

TEST_F(TaskQueueManagerTest, NoTasksAfterShutdown) {
  Initialize(1u);

  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));
  manager_.reset();
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));

  test_task_runner_->RunUntilIdle();
  EXPECT_TRUE(run_order.empty());
}

void PostTaskToRunner(scoped_refptr<base::SingleThreadTaskRunner> runner,
                      std::vector<EnqueueOrder>* run_order) {
  runner->PostTask(FROM_HERE, base::Bind(&TestTask, 1, run_order));
}

TEST_F(TaskQueueManagerTest, PostFromThread) {
  InitializeWithRealMessageLoop(1u);

  std::vector<EnqueueOrder> run_order;
  base::Thread thread("TestThread");
  thread.Start();
  thread.task_runner()->PostTask(
      FROM_HERE, base::Bind(&PostTaskToRunner, runners_[0], &run_order));
  thread.Stop();

  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1));
}

void RePostingTestTask(scoped_refptr<base::SingleThreadTaskRunner> runner,
                       int* run_count) {
  (*run_count)++;
  runner->PostTask(FROM_HERE, Bind(&RePostingTestTask,
                                   base::Unretained(runner.get()), run_count));
}

TEST_F(TaskQueueManagerTest, DoWorkCantPostItselfMultipleTimes) {
  Initialize(1u);

  int run_count = 0;
  runners_[0]->PostTask(
      FROM_HERE, base::Bind(&RePostingTestTask, runners_[0], &run_count));

  test_task_runner_->RunPendingTasks();
  // NOTE without the executing_task_ check in MaybeScheduleDoWork there
  // will be two tasks here.
  EXPECT_EQ(1u, test_task_runner_->NumPendingTasks());
  EXPECT_EQ(1, run_count);
}

TEST_F(TaskQueueManagerTest, PostFromNestedRunloop) {
  InitializeWithRealMessageLoop(1u);

  std::vector<EnqueueOrder> run_order;
  std::vector<std::pair<base::Closure, bool>> tasks_to_post_from_nested_loop;
  tasks_to_post_from_nested_loop.push_back(
      std::make_pair(base::Bind(&TestTask, 1, &run_order), true));

  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 0, &run_order));
  runners_[0]->PostTask(
      FROM_HERE, base::Bind(&PostFromNestedRunloop, message_loop_.get(),
                            base::RetainedRef(runners_[0]),
                            base::Unretained(&tasks_to_post_from_nested_loop)));
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order));

  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(run_order, ElementsAre(0, 2, 1));
}

TEST_F(TaskQueueManagerTest, WorkBatching) {
  Initialize(1u);

  manager_->SetWorkBatchSize(2);

  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order));
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 3, &run_order));
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 4, &run_order));

  // Running one task in the host message loop should cause two posted tasks to
  // get executed.
  EXPECT_EQ(test_task_runner_->NumPendingTasks(), 1u);
  test_task_runner_->RunPendingTasks();
  EXPECT_THAT(run_order, ElementsAre(1, 2));

  // The second task runs the remaining two posted tasks.
  EXPECT_EQ(test_task_runner_->NumPendingTasks(), 1u);
  test_task_runner_->RunPendingTasks();
  EXPECT_THAT(run_order, ElementsAre(1, 2, 3, 4));
}

class MockTaskObserver : public base::MessageLoop::TaskObserver {
 public:
  MOCK_METHOD1(DidProcessTask, void(const base::PendingTask& task));
  MOCK_METHOD1(WillProcessTask, void(const base::PendingTask& task));
};

TEST_F(TaskQueueManagerTest, TaskObserverAdding) {
  InitializeWithRealMessageLoop(1u);
  MockTaskObserver observer;

  manager_->SetWorkBatchSize(2);
  manager_->AddTaskObserver(&observer);

  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order));

  EXPECT_CALL(observer, WillProcessTask(_)).Times(2);
  EXPECT_CALL(observer, DidProcessTask(_)).Times(2);
  base::RunLoop().RunUntilIdle();
}

TEST_F(TaskQueueManagerTest, TaskObserverRemoving) {
  InitializeWithRealMessageLoop(1u);
  MockTaskObserver observer;
  manager_->SetWorkBatchSize(2);
  manager_->AddTaskObserver(&observer);
  manager_->RemoveTaskObserver(&observer);

  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));

  EXPECT_CALL(observer, WillProcessTask(_)).Times(0);
  EXPECT_CALL(observer, DidProcessTask(_)).Times(0);

  base::RunLoop().RunUntilIdle();
}

void RemoveObserverTask(TaskQueueManager* manager,
                        base::MessageLoop::TaskObserver* observer) {
  manager->RemoveTaskObserver(observer);
}

TEST_F(TaskQueueManagerTest, TaskObserverRemovingInsideTask) {
  InitializeWithRealMessageLoop(1u);
  MockTaskObserver observer;
  manager_->SetWorkBatchSize(3);
  manager_->AddTaskObserver(&observer);

  runners_[0]->PostTask(
      FROM_HERE, base::Bind(&RemoveObserverTask, manager_.get(), &observer));

  EXPECT_CALL(observer, WillProcessTask(_)).Times(1);
  EXPECT_CALL(observer, DidProcessTask(_)).Times(0);
  base::RunLoop().RunUntilIdle();
}

TEST_F(TaskQueueManagerTest, QueueTaskObserverAdding) {
  InitializeWithRealMessageLoop(2u);
  MockTaskObserver observer;

  manager_->SetWorkBatchSize(2);
  runners_[0]->AddTaskObserver(&observer);

  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));
  runners_[1]->PostTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order));

  EXPECT_CALL(observer, WillProcessTask(_)).Times(1);
  EXPECT_CALL(observer, DidProcessTask(_)).Times(1);
  base::RunLoop().RunUntilIdle();
}

TEST_F(TaskQueueManagerTest, QueueTaskObserverRemoving) {
  InitializeWithRealMessageLoop(1u);
  MockTaskObserver observer;
  manager_->SetWorkBatchSize(2);
  runners_[0]->AddTaskObserver(&observer);
  runners_[0]->RemoveTaskObserver(&observer);

  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));

  EXPECT_CALL(observer, WillProcessTask(_)).Times(0);
  EXPECT_CALL(observer, DidProcessTask(_)).Times(0);

  base::RunLoop().RunUntilIdle();
}

void RemoveQueueObserverTask(scoped_refptr<TaskQueue> queue,
                             base::MessageLoop::TaskObserver* observer) {
  queue->RemoveTaskObserver(observer);
}

TEST_F(TaskQueueManagerTest, QueueTaskObserverRemovingInsideTask) {
  InitializeWithRealMessageLoop(1u);
  MockTaskObserver observer;
  runners_[0]->AddTaskObserver(&observer);

  runners_[0]->PostTask(
      FROM_HERE, base::Bind(&RemoveQueueObserverTask, runners_[0], &observer));

  EXPECT_CALL(observer, WillProcessTask(_)).Times(1);
  EXPECT_CALL(observer, DidProcessTask(_)).Times(0);
  base::RunLoop().RunUntilIdle();
}

TEST_F(TaskQueueManagerTest, ThreadCheckAfterTermination) {
  Initialize(1u);
  EXPECT_TRUE(runners_[0]->RunsTasksInCurrentSequence());
  manager_.reset();
  EXPECT_TRUE(runners_[0]->RunsTasksInCurrentSequence());
}

TEST_F(TaskQueueManagerTest, TimeDomain_NextScheduledRunTime) {
  Initialize(2u);
  now_src_->Advance(base::TimeDelta::FromMicroseconds(10000));

  // With no delayed tasks.
  base::TimeTicks run_time;
  EXPECT_FALSE(manager_->real_time_domain()->NextScheduledRunTime(&run_time));

  // With a non-delayed task.
  runners_[0]->PostTask(FROM_HERE, base::Bind(&NopTask));
  EXPECT_FALSE(manager_->real_time_domain()->NextScheduledRunTime(&run_time));

  // With a delayed task.
  base::TimeDelta expected_delay = base::TimeDelta::FromMilliseconds(50);
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&NopTask), expected_delay);
  EXPECT_TRUE(manager_->real_time_domain()->NextScheduledRunTime(&run_time));
  EXPECT_EQ(now_src_->NowTicks() + expected_delay, run_time);

  // With another delayed task in the same queue with a longer delay.
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&NopTask),
                               base::TimeDelta::FromMilliseconds(100));
  EXPECT_TRUE(manager_->real_time_domain()->NextScheduledRunTime(&run_time));
  EXPECT_EQ(now_src_->NowTicks() + expected_delay, run_time);

  // With another delayed task in the same queue with a shorter delay.
  expected_delay = base::TimeDelta::FromMilliseconds(20);
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&NopTask), expected_delay);
  EXPECT_TRUE(manager_->real_time_domain()->NextScheduledRunTime(&run_time));
  EXPECT_EQ(now_src_->NowTicks() + expected_delay, run_time);

  // With another delayed task in a different queue with a shorter delay.
  expected_delay = base::TimeDelta::FromMilliseconds(10);
  runners_[1]->PostDelayedTask(FROM_HERE, base::Bind(&NopTask), expected_delay);
  EXPECT_TRUE(manager_->real_time_domain()->NextScheduledRunTime(&run_time));
  EXPECT_EQ(now_src_->NowTicks() + expected_delay, run_time);

  // Test it updates as time progresses
  now_src_->Advance(expected_delay);
  EXPECT_TRUE(manager_->real_time_domain()->NextScheduledRunTime(&run_time));
  EXPECT_EQ(now_src_->NowTicks(), run_time);
}

TEST_F(TaskQueueManagerTest, TimeDomain_NextScheduledRunTime_MultipleQueues) {
  Initialize(3u);

  base::TimeDelta delay1 = base::TimeDelta::FromMilliseconds(50);
  base::TimeDelta delay2 = base::TimeDelta::FromMilliseconds(5);
  base::TimeDelta delay3 = base::TimeDelta::FromMilliseconds(10);
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&NopTask), delay1);
  runners_[1]->PostDelayedTask(FROM_HERE, base::Bind(&NopTask), delay2);
  runners_[2]->PostDelayedTask(FROM_HERE, base::Bind(&NopTask), delay3);
  runners_[0]->PostTask(FROM_HERE, base::Bind(&NopTask));

  base::TimeTicks run_time;
  EXPECT_TRUE(manager_->real_time_domain()->NextScheduledRunTime(&run_time));
  EXPECT_EQ(now_src_->NowTicks() + delay2, run_time);
}

TEST_F(TaskQueueManagerTest, DeleteTaskQueueManagerInsideATask) {
  Initialize(1u);

  runners_[0]->PostTask(
      FROM_HERE, base::Bind(&TaskQueueManagerTest::DeleteTaskQueueManager,
                            base::Unretained(this)));

  // This should not crash, assuming DoWork detects the TaskQueueManager has
  // been deleted.
  test_task_runner_->RunUntilIdle();
}

TEST_F(TaskQueueManagerTest, GetAndClearSystemIsQuiescentBit) {
  Initialize(3u);

  scoped_refptr<internal::TaskQueueImpl> queue0 =
      manager_->NewTaskQueue(TaskQueue::Spec(TaskQueue::QueueType::TEST)
                                 .SetShouldMonitorQuiescence(true));
  scoped_refptr<internal::TaskQueueImpl> queue1 =
      manager_->NewTaskQueue(TaskQueue::Spec(TaskQueue::QueueType::TEST)
                                 .SetShouldMonitorQuiescence(true));
  scoped_refptr<internal::TaskQueueImpl> queue2 =
      manager_->NewTaskQueue(TaskQueue::Spec(TaskQueue::QueueType::TEST)
                                 .SetShouldMonitorQuiescence(false));

  EXPECT_TRUE(manager_->GetAndClearSystemIsQuiescentBit());

  queue0->PostTask(FROM_HERE, base::Bind(&NopTask));
  test_task_runner_->RunUntilIdle();
  EXPECT_FALSE(manager_->GetAndClearSystemIsQuiescentBit());
  EXPECT_TRUE(manager_->GetAndClearSystemIsQuiescentBit());

  queue1->PostTask(FROM_HERE, base::Bind(&NopTask));
  test_task_runner_->RunUntilIdle();
  EXPECT_FALSE(manager_->GetAndClearSystemIsQuiescentBit());
  EXPECT_TRUE(manager_->GetAndClearSystemIsQuiescentBit());

  queue2->PostTask(FROM_HERE, base::Bind(&NopTask));
  test_task_runner_->RunUntilIdle();
  EXPECT_TRUE(manager_->GetAndClearSystemIsQuiescentBit());

  queue0->PostTask(FROM_HERE, base::Bind(&NopTask));
  queue1->PostTask(FROM_HERE, base::Bind(&NopTask));
  test_task_runner_->RunUntilIdle();
  EXPECT_FALSE(manager_->GetAndClearSystemIsQuiescentBit());
  EXPECT_TRUE(manager_->GetAndClearSystemIsQuiescentBit());
}

TEST_F(TaskQueueManagerTest, HasPendingImmediateWork) {
  Initialize(1u);

  EXPECT_FALSE(runners_[0]->HasTaskToRunImmediately());
  runners_[0]->PostTask(FROM_HERE, base::Bind(NullTask));
  EXPECT_TRUE(runners_[0]->HasTaskToRunImmediately());

  test_task_runner_->RunUntilIdle();
  EXPECT_FALSE(runners_[0]->HasTaskToRunImmediately());
}

TEST_F(TaskQueueManagerTest, HasPendingImmediateWork_DelayedTasks) {
  Initialize(1u);

  EXPECT_FALSE(runners_[0]->HasTaskToRunImmediately());
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(NullTask),
                               base::TimeDelta::FromMilliseconds(12));
  EXPECT_FALSE(runners_[0]->HasTaskToRunImmediately());

  // Move time forwards until just before the delayed task should run.
  now_src_->Advance(base::TimeDelta::FromMilliseconds(10));
  WakeUpReadyDelayedQueues(LazyNow(now_src_.get()));
  EXPECT_FALSE(runners_[0]->HasTaskToRunImmediately());

  // Force the delayed task onto the work queue.
  now_src_->Advance(base::TimeDelta::FromMilliseconds(2));
  WakeUpReadyDelayedQueues(LazyNow(now_src_.get()));
  EXPECT_TRUE(runners_[0]->HasTaskToRunImmediately());

  test_task_runner_->RunUntilIdle();
  EXPECT_FALSE(runners_[0]->HasTaskToRunImmediately());
}

void ExpensiveTestTask(int value,
                       base::SimpleTestTickClock* clock,
                       std::vector<EnqueueOrder>* out_result) {
  out_result->push_back(value);
  clock->Advance(base::TimeDelta::FromMilliseconds(1));
}

TEST_F(TaskQueueManagerTest, ImmediateAndDelayedTaskInterleaving) {
  Initialize(1u);

  std::vector<EnqueueOrder> run_order;
  base::TimeDelta delay = base::TimeDelta::FromMilliseconds(10);
  for (int i = 10; i < 19; i++) {
    runners_[0]->PostDelayedTask(
        FROM_HERE,
        base::Bind(&ExpensiveTestTask, i, now_src_.get(), &run_order), delay);
  }

  test_task_runner_->RunForPeriod(delay);

  for (int i = 0; i < 9; i++) {
    runners_[0]->PostTask(FROM_HERE, base::Bind(&ExpensiveTestTask, i,
                                                now_src_.get(), &run_order));
  }

  test_task_runner_->SetAutoAdvanceNowToPendingTasks(true);
  test_task_runner_->RunUntilIdle();

  // Delayed tasks are not allowed to starve out immediate work which is why
  // some of the immediate tasks run out of order.
  int expected_run_order[] = {10, 11, 12, 13, 0, 14, 15, 16, 1,
                              17, 18, 2,  3,  4, 5,  6,  7,  8};
  EXPECT_THAT(run_order, ElementsAreArray(expected_run_order));
}

TEST_F(TaskQueueManagerTest,
       DelayedTaskDoesNotSkipAHeadOfNonDelayedTask_SameQueue) {
  Initialize(1u);

  std::vector<EnqueueOrder> run_order;
  base::TimeDelta delay = base::TimeDelta::FromMilliseconds(10);
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order));
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 3, &run_order));
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order),
                               delay);

  now_src_->Advance(delay * 2);
  test_task_runner_->RunUntilIdle();

  EXPECT_THAT(run_order, ElementsAre(2, 3, 1));
}

TEST_F(TaskQueueManagerTest,
       DelayedTaskDoesNotSkipAHeadOfNonDelayedTask_DifferentQueues) {
  Initialize(2u);

  std::vector<EnqueueOrder> run_order;
  base::TimeDelta delay = base::TimeDelta::FromMilliseconds(10);
  runners_[1]->PostTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order));
  runners_[1]->PostTask(FROM_HERE, base::Bind(&TestTask, 3, &run_order));
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order),
                               delay);

  now_src_->Advance(delay * 2);
  test_task_runner_->RunUntilIdle();

  EXPECT_THAT(run_order, ElementsAre(2, 3, 1));
}

TEST_F(TaskQueueManagerTest, DelayedTaskDoesNotSkipAHeadOfShorterDelayedTask) {
  Initialize(2u);

  std::vector<EnqueueOrder> run_order;
  base::TimeDelta delay1 = base::TimeDelta::FromMilliseconds(10);
  base::TimeDelta delay2 = base::TimeDelta::FromMilliseconds(5);
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order),
                               delay1);
  runners_[1]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order),
                               delay2);

  now_src_->Advance(delay1 * 2);
  test_task_runner_->RunUntilIdle();

  EXPECT_THAT(run_order, ElementsAre(2, 1));
}

void CheckIsNested(bool* is_nested) {
  *is_nested = base::RunLoop::IsNestedOnCurrentThread();
}

void PostAndQuitFromNestedRunloop(base::RunLoop* run_loop,
                                  base::SingleThreadTaskRunner* runner,
                                  bool* was_nested) {
  base::MessageLoop::ScopedNestableTaskAllower allow(
      base::MessageLoop::current());
  runner->PostTask(FROM_HERE, run_loop->QuitClosure());
  runner->PostTask(FROM_HERE, base::Bind(&CheckIsNested, was_nested));
  run_loop->Run();
}

TEST_F(TaskQueueManagerTest, QuitWhileNested) {
  // This test makes sure we don't continue running a work batch after a nested
  // run loop has been exited in the middle of the batch.
  InitializeWithRealMessageLoop(1u);
  manager_->SetWorkBatchSize(2);

  bool was_nested = true;
  base::RunLoop run_loop;
  runners_[0]->PostTask(FROM_HERE, base::Bind(&PostAndQuitFromNestedRunloop,
                                              base::Unretained(&run_loop),
                                              base::RetainedRef(runners_[0]),
                                              base::Unretained(&was_nested)));

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(was_nested);
}

class SequenceNumberCapturingTaskObserver
    : public base::MessageLoop::TaskObserver {
 public:
  // MessageLoop::TaskObserver overrides.
  void WillProcessTask(const base::PendingTask& pending_task) override {}
  void DidProcessTask(const base::PendingTask& pending_task) override {
    sequence_numbers_.push_back(pending_task.sequence_num);
  }

  const std::vector<EnqueueOrder>& sequence_numbers() const {
    return sequence_numbers_;
  }

 private:
  std::vector<EnqueueOrder> sequence_numbers_;
};

TEST_F(TaskQueueManagerTest, SequenceNumSetWhenTaskIsPosted) {
  Initialize(1u);

  SequenceNumberCapturingTaskObserver observer;
  manager_->AddTaskObserver(&observer);

  // Register four tasks that will run in reverse order.
  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order),
                               base::TimeDelta::FromMilliseconds(30));
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order),
                               base::TimeDelta::FromMilliseconds(20));
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 3, &run_order),
                               base::TimeDelta::FromMilliseconds(10));
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 4, &run_order));

  test_task_runner_->RunForPeriod(base::TimeDelta::FromMilliseconds(40));
  ASSERT_THAT(run_order, ElementsAre(4, 3, 2, 1));

  // The sequence numbers are a one-based monotonically incrememting counter
  // which should be set when the task is posted rather than when it's enqueued
  // onto the Incoming queue. This counter starts with 2.
  EXPECT_THAT(observer.sequence_numbers(), ElementsAre(5, 4, 3, 2));

  manager_->RemoveTaskObserver(&observer);
}

TEST_F(TaskQueueManagerTest, NewTaskQueues) {
  Initialize(1u);

  scoped_refptr<internal::TaskQueueImpl> queue1 =
      manager_->NewTaskQueue(TaskQueue::Spec(TaskQueue::QueueType::TEST));
  scoped_refptr<internal::TaskQueueImpl> queue2 =
      manager_->NewTaskQueue(TaskQueue::Spec(TaskQueue::QueueType::TEST));
  scoped_refptr<internal::TaskQueueImpl> queue3 =
      manager_->NewTaskQueue(TaskQueue::Spec(TaskQueue::QueueType::TEST));

  ASSERT_NE(queue1, queue2);
  ASSERT_NE(queue1, queue3);
  ASSERT_NE(queue2, queue3);

  std::vector<EnqueueOrder> run_order;
  queue1->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));
  queue2->PostTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order));
  queue3->PostTask(FROM_HERE, base::Bind(&TestTask, 3, &run_order));
  test_task_runner_->RunUntilIdle();

  EXPECT_THAT(run_order, ElementsAre(1, 2, 3));
}

TEST_F(TaskQueueManagerTest, UnregisterTaskQueue) {
  Initialize(1u);

  scoped_refptr<internal::TaskQueueImpl> queue1 =
      manager_->NewTaskQueue(TaskQueue::Spec(TaskQueue::QueueType::TEST));
  scoped_refptr<internal::TaskQueueImpl> queue2 =
      manager_->NewTaskQueue(TaskQueue::Spec(TaskQueue::QueueType::TEST));
  scoped_refptr<internal::TaskQueueImpl> queue3 =
      manager_->NewTaskQueue(TaskQueue::Spec(TaskQueue::QueueType::TEST));

  ASSERT_NE(queue1, queue2);
  ASSERT_NE(queue1, queue3);
  ASSERT_NE(queue2, queue3);

  std::vector<EnqueueOrder> run_order;
  queue1->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));
  queue2->PostTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order));
  queue3->PostTask(FROM_HERE, base::Bind(&TestTask, 3, &run_order));

  queue2->UnregisterTaskQueue();
  test_task_runner_->RunUntilIdle();

  EXPECT_THAT(run_order, ElementsAre(1, 3));
}

TEST_F(TaskQueueManagerTest, UnregisterTaskQueue_WithDelayedTasks) {
  Initialize(2u);

  // Register three delayed tasks
  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order),
                               base::TimeDelta::FromMilliseconds(10));
  runners_[1]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order),
                               base::TimeDelta::FromMilliseconds(20));
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 3, &run_order),
                               base::TimeDelta::FromMilliseconds(30));

  runners_[1]->UnregisterTaskQueue();
  test_task_runner_->RunUntilIdle();

  test_task_runner_->RunForPeriod(base::TimeDelta::FromMilliseconds(40));
  ASSERT_THAT(run_order, ElementsAre(1, 3));
}

namespace {
void UnregisterQueue(scoped_refptr<internal::TaskQueueImpl> queue) {
  queue->UnregisterTaskQueue();
}
}

TEST_F(TaskQueueManagerTest, UnregisterTaskQueue_InTasks) {
  Initialize(3u);

  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));
  runners_[0]->PostTask(FROM_HERE, base::Bind(&UnregisterQueue, runners_[1]));
  runners_[0]->PostTask(FROM_HERE, base::Bind(&UnregisterQueue, runners_[2]));
  runners_[1]->PostTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order));
  runners_[2]->PostTask(FROM_HERE, base::Bind(&TestTask, 3, &run_order));

  test_task_runner_->RunUntilIdle();
  ASSERT_THAT(run_order, ElementsAre(1));
}

namespace {

class MockObserver : public TaskQueueManager::Observer {
 public:
  MOCK_METHOD1(OnUnregisterTaskQueue,
               void(const scoped_refptr<TaskQueue>& queue));
  MOCK_METHOD2(OnTriedToExecuteBlockedTask,
               void(const TaskQueue& queue, const base::PendingTask& task));
};

}  // namespace

TEST_F(TaskQueueManagerTest, OnUnregisterTaskQueue) {
  Initialize(0u);

  MockObserver observer;
  manager_->SetObserver(&observer);

  scoped_refptr<internal::TaskQueueImpl> task_queue =
      manager_->NewTaskQueue(TaskQueue::Spec(TaskQueue::QueueType::TEST));

  EXPECT_CALL(observer, OnUnregisterTaskQueue(_)).Times(1);
  task_queue->UnregisterTaskQueue();

  manager_->SetObserver(nullptr);
}

TEST_F(TaskQueueManagerTest, OnTriedToExecuteBlockedTask) {
  Initialize(0u);

  MockObserver observer;
  manager_->SetObserver(&observer);

  scoped_refptr<internal::TaskQueueImpl> task_queue =
      manager_->NewTaskQueue(TaskQueue::Spec(TaskQueue::QueueType::TEST)
                                 .SetShouldReportWhenExecutionBlocked(true));
  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter =
      task_queue->CreateQueueEnabledVoter();

  voter->SetQueueEnabled(false);
  task_queue->PostTask(FROM_HERE, base::Bind(&NopTask));

  // Trick |task_queue| into posting a DoWork. By default PostTask with a
  // disabled queue won't post a DoWork until we enable the queue.
  voter->SetQueueEnabled(true);
  voter->SetQueueEnabled(false);

  EXPECT_CALL(observer, OnTriedToExecuteBlockedTask(_, _)).Times(1);
  test_task_runner_->RunPendingTasks();

  manager_->SetObserver(nullptr);
}

TEST_F(TaskQueueManagerTest, ExecutedNonBlockedTask) {
  Initialize(0u);

  MockObserver observer;
  manager_->SetObserver(&observer);

  scoped_refptr<internal::TaskQueueImpl> task_queue =
      manager_->NewTaskQueue(TaskQueue::Spec(TaskQueue::QueueType::TEST)
                                 .SetShouldReportWhenExecutionBlocked(true));
  task_queue->PostTask(FROM_HERE, base::Bind(&NopTask));

  EXPECT_CALL(observer, OnTriedToExecuteBlockedTask(_, _)).Times(0);
  test_task_runner_->RunPendingTasks();

  manager_->SetObserver(nullptr);
}

void HasOneRefTask(std::vector<bool>* log, internal::TaskQueueImpl* tq) {
  log->push_back(tq->HasOneRef());
}

TEST_F(TaskQueueManagerTest, UnregisterTaskQueueInNestedLoop) {
  InitializeWithRealMessageLoop(1u);

  // We retain a reference to the task queue even when the manager has deleted
  // its reference.
  scoped_refptr<internal::TaskQueueImpl> task_queue =
      manager_->NewTaskQueue(TaskQueue::Spec(TaskQueue::QueueType::TEST));

  std::vector<bool> log;
  std::vector<std::pair<base::Closure, bool>> tasks_to_post_from_nested_loop;

  // Inside a nested run loop, call task_queue->UnregisterTaskQueue, bookended
  // by calls to HasOneRefTask to make sure the manager doesn't release its
  // reference until the nested run loop exits.
  // NB: This first HasOneRefTask is a sanity check.
  tasks_to_post_from_nested_loop.push_back(
      std::make_pair(base::Bind(&HasOneRefTask, base::Unretained(&log),
                                base::Unretained(task_queue.get())),
                     true));
  tasks_to_post_from_nested_loop.push_back(
      std::make_pair(base::Bind(&internal::TaskQueueImpl::UnregisterTaskQueue,
                                base::Unretained(task_queue.get())),
                     true));
  tasks_to_post_from_nested_loop.push_back(
      std::make_pair(base::Bind(&HasOneRefTask, base::Unretained(&log),
                                base::Unretained(task_queue.get())),
                     true));
  runners_[0]->PostTask(
      FROM_HERE, base::Bind(&PostFromNestedRunloop, message_loop_.get(),
                            base::RetainedRef(runners_[0]),
                            base::Unretained(&tasks_to_post_from_nested_loop)));
  base::RunLoop().RunUntilIdle();

  // Add a final call to HasOneRefTask.  This gives the manager a chance to
  // release its reference, and checks that it has.
  runners_[0]->PostTask(FROM_HERE,
                        base::Bind(&HasOneRefTask, base::Unretained(&log),
                                   base::Unretained(task_queue.get())));
  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(log, ElementsAre(false, false, true));
}

TEST_F(TaskQueueManagerTest, TimeDomainsAreIndependant) {
  Initialize(2u);

  base::TimeTicks start_time = manager_->Delegate()->NowTicks();
  std::unique_ptr<VirtualTimeDomain> domain_a(
      new VirtualTimeDomain(start_time));
  std::unique_ptr<VirtualTimeDomain> domain_b(
      new VirtualTimeDomain(start_time));
  manager_->RegisterTimeDomain(domain_a.get());
  manager_->RegisterTimeDomain(domain_b.get());
  runners_[0]->SetTimeDomain(domain_a.get());
  runners_[1]->SetTimeDomain(domain_b.get());

  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order),
                               base::TimeDelta::FromMilliseconds(10));
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order),
                               base::TimeDelta::FromMilliseconds(20));
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 3, &run_order),
                               base::TimeDelta::FromMilliseconds(30));

  runners_[1]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 4, &run_order),
                               base::TimeDelta::FromMilliseconds(10));
  runners_[1]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 5, &run_order),
                               base::TimeDelta::FromMilliseconds(20));
  runners_[1]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 6, &run_order),
                               base::TimeDelta::FromMilliseconds(30));

  domain_b->AdvanceTo(start_time + base::TimeDelta::FromMilliseconds(50));
  manager_->MaybeScheduleImmediateWork(FROM_HERE);

  test_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(4, 5, 6));

  domain_a->AdvanceTo(start_time + base::TimeDelta::FromMilliseconds(50));
  manager_->MaybeScheduleImmediateWork(FROM_HERE);

  test_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(4, 5, 6, 1, 2, 3));

  runners_[0]->UnregisterTaskQueue();
  runners_[1]->UnregisterTaskQueue();

  manager_->UnregisterTimeDomain(domain_a.get());
  manager_->UnregisterTimeDomain(domain_b.get());
}

TEST_F(TaskQueueManagerTest, TimeDomainMigration) {
  Initialize(1u);

  base::TimeTicks start_time = manager_->Delegate()->NowTicks();
  std::unique_ptr<VirtualTimeDomain> domain_a(
      new VirtualTimeDomain(start_time));
  manager_->RegisterTimeDomain(domain_a.get());
  runners_[0]->SetTimeDomain(domain_a.get());

  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order),
                               base::TimeDelta::FromMilliseconds(10));
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order),
                               base::TimeDelta::FromMilliseconds(20));
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 3, &run_order),
                               base::TimeDelta::FromMilliseconds(30));
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 4, &run_order),
                               base::TimeDelta::FromMilliseconds(40));

  domain_a->AdvanceTo(start_time + base::TimeDelta::FromMilliseconds(20));
  manager_->MaybeScheduleImmediateWork(FROM_HERE);
  test_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1, 2));

  std::unique_ptr<VirtualTimeDomain> domain_b(
      new VirtualTimeDomain(start_time));
  manager_->RegisterTimeDomain(domain_b.get());
  runners_[0]->SetTimeDomain(domain_b.get());

  domain_b->AdvanceTo(start_time + base::TimeDelta::FromMilliseconds(50));
  manager_->MaybeScheduleImmediateWork(FROM_HERE);

  test_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1, 2, 3, 4));

  runners_[0]->UnregisterTaskQueue();

  manager_->UnregisterTimeDomain(domain_a.get());
  manager_->UnregisterTimeDomain(domain_b.get());
}

TEST_F(TaskQueueManagerTest, TimeDomainMigrationWithIncomingImmediateTasks) {
  Initialize(1u);

  base::TimeTicks start_time = manager_->Delegate()->NowTicks();
  std::unique_ptr<VirtualTimeDomain> domain_a(
      new VirtualTimeDomain(start_time));
  std::unique_ptr<VirtualTimeDomain> domain_b(
      new VirtualTimeDomain(start_time));
  manager_->RegisterTimeDomain(domain_a.get());
  manager_->RegisterTimeDomain(domain_b.get());

  runners_[0]->SetTimeDomain(domain_a.get());
  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));
  runners_[0]->SetTimeDomain(domain_b.get());

  test_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1));

  runners_[0]->UnregisterTaskQueue();

  manager_->UnregisterTimeDomain(domain_a.get());
  manager_->UnregisterTimeDomain(domain_b.get());
}

TEST_F(TaskQueueManagerTest,
       PostDelayedTasksReverseOrderAlternatingTimeDomains) {
  Initialize(1u);

  std::vector<EnqueueOrder> run_order;

  std::unique_ptr<RealTimeDomain> domain_a(new RealTimeDomain());
  std::unique_ptr<RealTimeDomain> domain_b(new RealTimeDomain());
  manager_->RegisterTimeDomain(domain_a.get());
  manager_->RegisterTimeDomain(domain_b.get());

  runners_[0]->SetTimeDomain(domain_a.get());
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order),
                               base::TimeDelta::FromMilliseconds(40));

  runners_[0]->SetTimeDomain(domain_b.get());
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order),
                               base::TimeDelta::FromMilliseconds(30));

  runners_[0]->SetTimeDomain(domain_a.get());
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 3, &run_order),
                               base::TimeDelta::FromMilliseconds(20));

  runners_[0]->SetTimeDomain(domain_b.get());
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 4, &run_order),
                               base::TimeDelta::FromMilliseconds(10));

  test_task_runner_->RunForPeriod(base::TimeDelta::FromMilliseconds(40));
  EXPECT_THAT(run_order, ElementsAre(4, 3, 2, 1));

  runners_[0]->UnregisterTaskQueue();

  manager_->UnregisterTimeDomain(domain_a.get());
  manager_->UnregisterTimeDomain(domain_b.get());
}

namespace {

class MockTaskQueueObserver : public TaskQueue::Observer {
 public:
  ~MockTaskQueueObserver() override {}

  MOCK_METHOD2(OnQueueNextWakeUpChanged, void(TaskQueue*, base::TimeTicks));
};

}  // namespace

TEST_F(TaskQueueManagerTest, TaskQueueObserver_ImmediateTask) {
  Initialize(1u);

  MockTaskQueueObserver observer;
  runners_[0]->SetObserver(&observer);

  // We should get a notification when a task is posted on an empty queue.
  EXPECT_CALL(observer,
              OnQueueNextWakeUpChanged(runners_[0].get(), base::TimeTicks()));
  runners_[0]->PostTask(FROM_HERE, base::Bind(&NopTask));
  Mock::VerifyAndClearExpectations(&observer);

  // But not subsequently.
  EXPECT_CALL(observer, OnQueueNextWakeUpChanged(_, _)).Times(0);
  runners_[0]->PostTask(FROM_HERE, base::Bind(&NopTask));
  Mock::VerifyAndClearExpectations(&observer);

  // Unless the immediate work queue is emptied.
  runners_[0]->ReloadImmediateWorkQueueIfEmpty();
  EXPECT_CALL(observer,
              OnQueueNextWakeUpChanged(runners_[0].get(), base::TimeTicks()));
  runners_[0]->PostTask(FROM_HERE, base::Bind(&NopTask));

  // Tidy up.
  runners_[0]->UnregisterTaskQueue();
}

TEST_F(TaskQueueManagerTest, TaskQueueObserver_DelayedTask) {
  Initialize(1u);

  base::TimeTicks start_time = manager_->Delegate()->NowTicks();
  base::TimeDelta delay10s(base::TimeDelta::FromSeconds(10));
  base::TimeDelta delay100s(base::TimeDelta::FromSeconds(100));
  base::TimeDelta delay1s(base::TimeDelta::FromSeconds(1));

  MockTaskQueueObserver observer;
  runners_[0]->SetObserver(&observer);

  // We should get a notification when a delayed task is posted on an empty
  // queue.
  EXPECT_CALL(observer, OnQueueNextWakeUpChanged(runners_[0].get(),
                                                 start_time + delay10s));
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&NopTask), delay10s);
  Mock::VerifyAndClearExpectations(&observer);

  // We should not get a notification for a longer delay.
  EXPECT_CALL(observer, OnQueueNextWakeUpChanged(_, _)).Times(0);
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&NopTask), delay100s);
  Mock::VerifyAndClearExpectations(&observer);

  // We should get a notification for a shorter delay.
  EXPECT_CALL(observer, OnQueueNextWakeUpChanged(runners_[0].get(),
                                                 start_time + delay1s));
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&NopTask), delay1s);
  Mock::VerifyAndClearExpectations(&observer);

  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter =
      runners_[0]->CreateQueueEnabledVoter();
  voter->SetQueueEnabled(false);
  Mock::VerifyAndClearExpectations(&observer);

  // When a queue has been enabled, we may get a notification if the
  // TimeDomain's next scheduled wake-up has changed.
  EXPECT_CALL(observer, OnQueueNextWakeUpChanged(runners_[0].get(),
                                                 start_time + delay1s));
  voter->SetQueueEnabled(true);
  Mock::VerifyAndClearExpectations(&observer);

  // Tidy up.
  runners_[0]->UnregisterTaskQueue();
}

TEST_F(TaskQueueManagerTest, TaskQueueObserver_DelayedTaskMultipleQueues) {
  Initialize(2u);

  MockTaskQueueObserver observer;
  runners_[0]->SetObserver(&observer);
  runners_[1]->SetObserver(&observer);

  base::TimeTicks start_time = manager_->Delegate()->NowTicks();
  base::TimeDelta delay1s(base::TimeDelta::FromSeconds(1));
  base::TimeDelta delay10s(base::TimeDelta::FromSeconds(10));

  EXPECT_CALL(observer,
              OnQueueNextWakeUpChanged(runners_[0].get(), start_time + delay1s))
      .Times(1);
  EXPECT_CALL(observer, OnQueueNextWakeUpChanged(runners_[1].get(),
                                                 start_time + delay10s))
      .Times(1);
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&NopTask), delay1s);
  runners_[1]->PostDelayedTask(FROM_HERE, base::Bind(&NopTask), delay10s);
  ::testing::Mock::VerifyAndClearExpectations(&observer);

  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter0 =
      runners_[0]->CreateQueueEnabledVoter();
  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter1 =
      runners_[1]->CreateQueueEnabledVoter();

  // Disabling a queue should not trigger a notification.
  EXPECT_CALL(observer, OnQueueNextWakeUpChanged(_, _)).Times(0);
  voter0->SetQueueEnabled(false);
  Mock::VerifyAndClearExpectations(&observer);

  // Re-enabling it should should also trigger a notification.
  EXPECT_CALL(observer, OnQueueNextWakeUpChanged(runners_[0].get(),
                                                 start_time + delay1s));
  voter0->SetQueueEnabled(true);
  Mock::VerifyAndClearExpectations(&observer);

  // Disabling a queue should not trigger a notification.
  EXPECT_CALL(observer, OnQueueNextWakeUpChanged(_, _)).Times(0);
  voter1->SetQueueEnabled(false);
  Mock::VerifyAndClearExpectations(&observer);

  // Re-enabling it should should trigger a notification.
  EXPECT_CALL(observer, OnQueueNextWakeUpChanged(runners_[1].get(),
                                                 start_time + delay10s));
  voter1->SetQueueEnabled(true);
  Mock::VerifyAndClearExpectations(&observer);

  // Tidy up.
  EXPECT_CALL(observer, OnQueueNextWakeUpChanged(_, _)).Times(AnyNumber());
  runners_[0]->UnregisterTaskQueue();
  runners_[1]->UnregisterTaskQueue();
}

TEST_F(TaskQueueManagerTest, TaskQueueObserver_DelayedWorkWhichCanRunNow) {
  // This test checks that when delayed work becomes available
  // the notification still fires. This usually happens when time advances
  // and task becomes available in the middle of the scheduling code.
  // For this test we rely on the fact that notification dispatching code
  // is the same in all conditions and just change a time domain to
  // trigger notification.

  Initialize(1u);

  base::TimeDelta delay1s(base::TimeDelta::FromSeconds(1));
  base::TimeDelta delay10s(base::TimeDelta::FromSeconds(10));

  MockTaskQueueObserver observer;
  runners_[0]->SetObserver(&observer);

  // We should get a notification when a delayed task is posted on an empty
  // queue.
  EXPECT_CALL(observer, OnQueueNextWakeUpChanged(_, _));
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&NopTask), delay1s);
  Mock::VerifyAndClearExpectations(&observer);

  std::unique_ptr<TimeDomain> mock_time_domain =
      base::MakeUnique<RealTimeDomain>();
  manager_->RegisterTimeDomain(mock_time_domain.get());

  now_src_->Advance(delay10s);

  EXPECT_CALL(observer, OnQueueNextWakeUpChanged(_, _));
  runners_[0]->SetTimeDomain(mock_time_domain.get());
  Mock::VerifyAndClearExpectations(&observer);

  // Tidy up.
  runners_[0]->UnregisterTaskQueue();
}

class CancelableTask {
 public:
  explicit CancelableTask(base::TickClock* clock)
      : clock_(clock), weak_factory_(this) {}

  void RecordTimeTask(std::vector<base::TimeTicks>* run_times) {
    run_times->push_back(clock_->NowTicks());
  }

  base::TickClock* clock_;
  base::WeakPtrFactory<CancelableTask> weak_factory_;
};

TEST_F(TaskQueueManagerTest, TaskQueueObserver_SweepCanceledDelayedTasks) {
  Initialize(1u);

  MockTaskQueueObserver observer;
  runners_[0]->SetObserver(&observer);

  base::TimeTicks start_time = manager_->Delegate()->NowTicks();
  base::TimeDelta delay1(base::TimeDelta::FromSeconds(5));
  base::TimeDelta delay2(base::TimeDelta::FromSeconds(10));

  EXPECT_CALL(observer,
              OnQueueNextWakeUpChanged(runners_[0].get(), start_time + delay1))
      .Times(1);

  CancelableTask task1(now_src_.get());
  CancelableTask task2(now_src_.get());
  std::vector<base::TimeTicks> run_times;
  runners_[0]->PostDelayedTask(
      FROM_HERE,
      base::Bind(&CancelableTask::RecordTimeTask,
                 task1.weak_factory_.GetWeakPtr(), &run_times),
      delay1);
  runners_[0]->PostDelayedTask(
      FROM_HERE,
      base::Bind(&CancelableTask::RecordTimeTask,
                 task2.weak_factory_.GetWeakPtr(), &run_times),
      delay2);

  task1.weak_factory_.InvalidateWeakPtrs();

  // Sweeping away canceled delayed tasks should trigger a notification.
  EXPECT_CALL(observer,
              OnQueueNextWakeUpChanged(runners_[0].get(), start_time + delay2))
      .Times(1);
  manager_->SweepCanceledDelayedTasks();
}

namespace {
void ChromiumRunloopInspectionTask(
    scoped_refptr<cc::OrderedSimpleTaskRunner> test_task_runner) {
  EXPECT_EQ(1u, test_task_runner->NumPendingTasks());
}
}  // namespace

TEST_F(TaskQueueManagerTest, NumberOfPendingTasksOnChromiumRunLoop) {
  Initialize(1u);

  // NOTE because tasks posted to the chromiumrun loop are not cancellable, we
  // will end up with a lot more tasks posted if the delayed tasks were posted
  // in the reverse order.
  // TODO(alexclarke): Consider talking to the message pump directly.
  test_task_runner_->SetAutoAdvanceNowToPendingTasks(true);
  for (int i = 1; i < 100; i++) {
    runners_[0]->PostDelayedTask(
        FROM_HERE,
        base::Bind(&ChromiumRunloopInspectionTask, test_task_runner_),
        base::TimeDelta::FromMilliseconds(i));
  }
  test_task_runner_->RunUntilIdle();
}

namespace {

class QuadraticTask {
 public:
  QuadraticTask(scoped_refptr<internal::TaskQueueImpl> task_queue,
                base::TimeDelta delay,
                base::SimpleTestTickClock* now_src)
      : count_(0), task_queue_(task_queue), delay_(delay), now_src_(now_src) {}

  void SetShouldExit(base::Callback<bool()> should_exit) {
    should_exit_ = should_exit;
  }

  void Run() {
    if (should_exit_.Run())
      return;
    count_++;
    task_queue_->PostDelayedTask(
        FROM_HERE, base::Bind(&QuadraticTask::Run, base::Unretained(this)),
        delay_);
    task_queue_->PostDelayedTask(
        FROM_HERE, base::Bind(&QuadraticTask::Run, base::Unretained(this)),
        delay_);
    now_src_->Advance(base::TimeDelta::FromMilliseconds(5));
  }

  int Count() const { return count_; }

 private:
  int count_;
  scoped_refptr<internal::TaskQueueImpl> task_queue_;
  base::TimeDelta delay_;
  base::Callback<bool()> should_exit_;
  base::SimpleTestTickClock* now_src_;
};

class LinearTask {
 public:
  LinearTask(scoped_refptr<internal::TaskQueueImpl> task_queue,
             base::TimeDelta delay,
             base::SimpleTestTickClock* now_src)
      : count_(0), task_queue_(task_queue), delay_(delay), now_src_(now_src) {}

  void SetShouldExit(base::Callback<bool()> should_exit) {
    should_exit_ = should_exit;
  }

  void Run() {
    if (should_exit_.Run())
      return;
    count_++;
    task_queue_->PostDelayedTask(
        FROM_HERE, base::Bind(&LinearTask::Run, base::Unretained(this)),
        delay_);
    now_src_->Advance(base::TimeDelta::FromMilliseconds(5));
  }

  int Count() const { return count_; }

 private:
  int count_;
  scoped_refptr<internal::TaskQueueImpl> task_queue_;
  base::TimeDelta delay_;
  base::Callback<bool()> should_exit_;
  base::SimpleTestTickClock* now_src_;
};

bool ShouldExit(QuadraticTask* quadratic_task, LinearTask* linear_task) {
  return quadratic_task->Count() == 1000 || linear_task->Count() == 1000;
}

}  // namespace

TEST_F(TaskQueueManagerTest,
       DelayedTasksDontBadlyStarveNonDelayedWork_SameQueue) {
  Initialize(1u);

  QuadraticTask quadratic_delayed_task(
      runners_[0], base::TimeDelta::FromMilliseconds(10), now_src_.get());
  LinearTask linear_immediate_task(runners_[0], base::TimeDelta(),
                                   now_src_.get());
  base::Callback<bool()> should_exit =
      base::Bind(ShouldExit, &quadratic_delayed_task, &linear_immediate_task);
  quadratic_delayed_task.SetShouldExit(should_exit);
  linear_immediate_task.SetShouldExit(should_exit);

  quadratic_delayed_task.Run();
  linear_immediate_task.Run();

  test_task_runner_->SetAutoAdvanceNowToPendingTasks(true);
  test_task_runner_->RunUntilIdle();

  double ratio = static_cast<double>(linear_immediate_task.Count()) /
                 static_cast<double>(quadratic_delayed_task.Count());

  EXPECT_GT(ratio, 0.333);
  EXPECT_LT(ratio, 1.1);
}

TEST_F(TaskQueueManagerTest, ImmediateWorkCanStarveDelayedTasks_SameQueue) {
  Initialize(1u);

  QuadraticTask quadratic_immediate_task(runners_[0], base::TimeDelta(),
                                         now_src_.get());
  LinearTask linear_delayed_task(
      runners_[0], base::TimeDelta::FromMilliseconds(10), now_src_.get());
  base::Callback<bool()> should_exit =
      base::Bind(&ShouldExit, &quadratic_immediate_task, &linear_delayed_task);

  quadratic_immediate_task.SetShouldExit(should_exit);
  linear_delayed_task.SetShouldExit(should_exit);

  quadratic_immediate_task.Run();
  linear_delayed_task.Run();

  test_task_runner_->SetAutoAdvanceNowToPendingTasks(true);
  test_task_runner_->RunUntilIdle();

  double ratio = static_cast<double>(linear_delayed_task.Count()) /
                 static_cast<double>(quadratic_immediate_task.Count());

  // This is by design, we want to enforce a strict ordering in task execution
  // where by delayed tasks can not skip ahead of non-delayed work.
  EXPECT_GT(ratio, 0.0);
  EXPECT_LT(ratio, 0.1);
}

TEST_F(TaskQueueManagerTest,
       DelayedTasksDontBadlyStarveNonDelayedWork_DifferentQueue) {
  Initialize(2u);

  QuadraticTask quadratic_delayed_task(
      runners_[0], base::TimeDelta::FromMilliseconds(10), now_src_.get());
  LinearTask linear_immediate_task(runners_[1], base::TimeDelta(),
                                   now_src_.get());
  base::Callback<bool()> should_exit =
      base::Bind(ShouldExit, &quadratic_delayed_task, &linear_immediate_task);
  quadratic_delayed_task.SetShouldExit(should_exit);
  linear_immediate_task.SetShouldExit(should_exit);

  quadratic_delayed_task.Run();
  linear_immediate_task.Run();

  test_task_runner_->SetAutoAdvanceNowToPendingTasks(true);
  test_task_runner_->RunUntilIdle();

  double ratio = static_cast<double>(linear_immediate_task.Count()) /
                 static_cast<double>(quadratic_delayed_task.Count());

  EXPECT_GT(ratio, 0.333);
  EXPECT_LT(ratio, 1.1);
}

TEST_F(TaskQueueManagerTest,
       ImmediateWorkCanStarveDelayedTasks_DifferentQueue) {
  Initialize(2u);

  QuadraticTask quadratic_immediate_task(runners_[0], base::TimeDelta(),
                                         now_src_.get());
  LinearTask linear_delayed_task(
      runners_[1], base::TimeDelta::FromMilliseconds(10), now_src_.get());
  base::Callback<bool()> should_exit =
      base::Bind(&ShouldExit, &quadratic_immediate_task, &linear_delayed_task);

  quadratic_immediate_task.SetShouldExit(should_exit);
  linear_delayed_task.SetShouldExit(should_exit);

  quadratic_immediate_task.Run();
  linear_delayed_task.Run();

  test_task_runner_->SetAutoAdvanceNowToPendingTasks(true);
  test_task_runner_->RunUntilIdle();

  double ratio = static_cast<double>(linear_delayed_task.Count()) /
                 static_cast<double>(quadratic_immediate_task.Count());

  // This is by design, we want to enforce a strict ordering in task execution
  // where by delayed tasks can not skip ahead of non-delayed work.
  EXPECT_GT(ratio, 0.0);
  EXPECT_LT(ratio, 0.1);
}

TEST_F(TaskQueueManagerTest, CurrentlyExecutingTaskQueue_NoTaskRunning) {
  Initialize(1u);

  EXPECT_EQ(nullptr, manager_->currently_executing_task_queue());
}

namespace {
void CurrentlyExecutingTaskQueueTestTask(
    TaskQueueManager* task_queue_manager,
    std::vector<TaskQueue*>* task_sources) {
  task_sources->push_back(task_queue_manager->currently_executing_task_queue());
}
}

TEST_F(TaskQueueManagerTest, CurrentlyExecutingTaskQueue_TaskRunning) {
  Initialize(2u);

  internal::TaskQueueImpl* queue0 = runners_[0].get();
  internal::TaskQueueImpl* queue1 = runners_[1].get();

  std::vector<TaskQueue*> task_sources;
  queue0->PostTask(FROM_HERE, base::Bind(&CurrentlyExecutingTaskQueueTestTask,
                                         manager_.get(), &task_sources));
  queue1->PostTask(FROM_HERE, base::Bind(&CurrentlyExecutingTaskQueueTestTask,
                                         manager_.get(), &task_sources));
  test_task_runner_->RunUntilIdle();

  EXPECT_THAT(task_sources, ElementsAre(queue0, queue1));
  EXPECT_EQ(nullptr, manager_->currently_executing_task_queue());
}

namespace {
void RunloopCurrentlyExecutingTaskQueueTestTask(
    base::MessageLoop* message_loop,
    TaskQueueManager* task_queue_manager,
    std::vector<TaskQueue*>* task_sources,
    std::vector<std::pair<base::Closure, TaskQueue*>>* tasks) {
  base::MessageLoop::ScopedNestableTaskAllower allow(message_loop);
  task_sources->push_back(task_queue_manager->currently_executing_task_queue());

  for (std::pair<base::Closure, TaskQueue*>& pair : *tasks) {
    pair.second->PostTask(FROM_HERE, pair.first);
  }

  base::RunLoop().RunUntilIdle();
  task_sources->push_back(task_queue_manager->currently_executing_task_queue());
}
}

TEST_F(TaskQueueManagerTest, CurrentlyExecutingTaskQueue_NestedLoop) {
  InitializeWithRealMessageLoop(3u);

  TaskQueue* queue0 = runners_[0].get();
  TaskQueue* queue1 = runners_[1].get();
  TaskQueue* queue2 = runners_[2].get();

  std::vector<TaskQueue*> task_sources;
  std::vector<std::pair<base::Closure, TaskQueue*>>
      tasks_to_post_from_nested_loop;
  tasks_to_post_from_nested_loop.push_back(
      std::make_pair(base::Bind(&CurrentlyExecutingTaskQueueTestTask,
                                manager_.get(), &task_sources),
                     queue1));
  tasks_to_post_from_nested_loop.push_back(
      std::make_pair(base::Bind(&CurrentlyExecutingTaskQueueTestTask,
                                manager_.get(), &task_sources),
                     queue2));

  queue0->PostTask(
      FROM_HERE, base::Bind(&RunloopCurrentlyExecutingTaskQueueTestTask,
                            message_loop_.get(), manager_.get(), &task_sources,
                            &tasks_to_post_from_nested_loop));

  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(task_sources, ElementsAre(queue0, queue1, queue2, queue0));
  EXPECT_EQ(nullptr, manager_->currently_executing_task_queue());
}

void OnTraceDataCollected(base::Closure quit_closure,
                          base::trace_event::TraceResultBuffer* buffer,
                          const scoped_refptr<base::RefCountedString>& json,
                          bool has_more_events) {
  buffer->AddFragment(json->data());
  if (!has_more_events)
    quit_closure.Run();
}

class TaskQueueManagerTestWithTracing : public TaskQueueManagerTest {
 public:
  void StartTracing();
  void StopTracing();
  std::unique_ptr<trace_analyzer::TraceAnalyzer> CreateTraceAnalyzer();
};

void TaskQueueManagerTestWithTracing::StartTracing() {
  base::trace_event::TraceLog::GetInstance()->SetEnabled(
      base::trace_event::TraceConfig("*"),
      base::trace_event::TraceLog::RECORDING_MODE);
}

void TaskQueueManagerTestWithTracing::StopTracing() {
  base::trace_event::TraceLog::GetInstance()->SetDisabled();
}

std::unique_ptr<trace_analyzer::TraceAnalyzer>
TaskQueueManagerTestWithTracing::CreateTraceAnalyzer() {
  base::trace_event::TraceResultBuffer buffer;
  base::trace_event::TraceResultBuffer::SimpleOutput trace_output;
  buffer.SetOutputCallback(trace_output.GetCallback());
  base::RunLoop run_loop;
  buffer.Start();
  base::trace_event::TraceLog::GetInstance()->Flush(
      Bind(&OnTraceDataCollected, run_loop.QuitClosure(),
           base::Unretained(&buffer)));
  run_loop.Run();
  buffer.Finish();

  return base::WrapUnique(
      trace_analyzer::TraceAnalyzer::Create(trace_output.json_output));
}

TEST_F(TaskQueueManagerTestWithTracing, BlameContextAttribution) {
  using trace_analyzer::Query;

  InitializeWithRealMessageLoop(1u);
  TaskQueue* queue = runners_[0].get();

  StartTracing();
  {
    base::trace_event::BlameContext blame_context("cat", "name", "type",
                                                  "scope", 0, nullptr);
    blame_context.Initialize();
    queue->SetBlameContext(&blame_context);
    queue->PostTask(FROM_HERE, base::Bind(&NopTask));
    base::RunLoop().RunUntilIdle();
  }
  StopTracing();
  std::unique_ptr<trace_analyzer::TraceAnalyzer> analyzer =
      CreateTraceAnalyzer();

  trace_analyzer::TraceEventVector events;
  Query q = Query::EventPhaseIs(TRACE_EVENT_PHASE_ENTER_CONTEXT) ||
            Query::EventPhaseIs(TRACE_EVENT_PHASE_LEAVE_CONTEXT);
  analyzer->FindEvents(q, &events);

  EXPECT_EQ(2u, events.size());
}

TEST_F(TaskQueueManagerTest, NoWakeUpsForCanceledDelayedTasks) {
  Initialize(1u);

  base::TimeTicks start_time = manager_->Delegate()->NowTicks();

  CancelableTask task1(now_src_.get());
  CancelableTask task2(now_src_.get());
  CancelableTask task3(now_src_.get());
  CancelableTask task4(now_src_.get());
  base::TimeDelta delay1(base::TimeDelta::FromSeconds(5));
  base::TimeDelta delay2(base::TimeDelta::FromSeconds(10));
  base::TimeDelta delay3(base::TimeDelta::FromSeconds(15));
  base::TimeDelta delay4(base::TimeDelta::FromSeconds(30));
  std::vector<base::TimeTicks> run_times;
  runners_[0]->PostDelayedTask(
      FROM_HERE, base::Bind(&CancelableTask::RecordTimeTask,
                            task1.weak_factory_.GetWeakPtr(), &run_times),
      delay1);
  runners_[0]->PostDelayedTask(
      FROM_HERE, base::Bind(&CancelableTask::RecordTimeTask,
                            task2.weak_factory_.GetWeakPtr(), &run_times),
      delay2);
  runners_[0]->PostDelayedTask(
      FROM_HERE, base::Bind(&CancelableTask::RecordTimeTask,
                            task3.weak_factory_.GetWeakPtr(), &run_times),
      delay3);
  runners_[0]->PostDelayedTask(
      FROM_HERE, base::Bind(&CancelableTask::RecordTimeTask,
                            task4.weak_factory_.GetWeakPtr(), &run_times),
      delay4);

  task2.weak_factory_.InvalidateWeakPtrs();
  task3.weak_factory_.InvalidateWeakPtrs();

  std::set<base::TimeTicks> wake_up_times;

  RunUntilIdle(base::Bind(
      [](std::set<base::TimeTicks>* wake_up_times,
         base::SimpleTestTickClock* clock) {
        wake_up_times->insert(clock->NowTicks());
      },
      &wake_up_times, now_src_.get()));

  EXPECT_THAT(wake_up_times,
              ElementsAre(start_time + delay1, start_time + delay4));
  EXPECT_THAT(run_times, ElementsAre(start_time + delay1, start_time + delay4));
}

TEST_F(TaskQueueManagerTest, NoWakeUpsForCanceledDelayedTasksReversePostOrder) {
  Initialize(1u);

  base::TimeTicks start_time = manager_->Delegate()->NowTicks();

  CancelableTask task1(now_src_.get());
  CancelableTask task2(now_src_.get());
  CancelableTask task3(now_src_.get());
  CancelableTask task4(now_src_.get());
  base::TimeDelta delay1(base::TimeDelta::FromSeconds(5));
  base::TimeDelta delay2(base::TimeDelta::FromSeconds(10));
  base::TimeDelta delay3(base::TimeDelta::FromSeconds(15));
  base::TimeDelta delay4(base::TimeDelta::FromSeconds(30));
  std::vector<base::TimeTicks> run_times;
  runners_[0]->PostDelayedTask(
      FROM_HERE, base::Bind(&CancelableTask::RecordTimeTask,
                            task4.weak_factory_.GetWeakPtr(), &run_times),
      delay4);
  runners_[0]->PostDelayedTask(
      FROM_HERE, base::Bind(&CancelableTask::RecordTimeTask,
                            task3.weak_factory_.GetWeakPtr(), &run_times),
      delay3);
  runners_[0]->PostDelayedTask(
      FROM_HERE, base::Bind(&CancelableTask::RecordTimeTask,
                            task2.weak_factory_.GetWeakPtr(), &run_times),
      delay2);
  runners_[0]->PostDelayedTask(
      FROM_HERE, base::Bind(&CancelableTask::RecordTimeTask,
                            task1.weak_factory_.GetWeakPtr(), &run_times),
      delay1);

  task2.weak_factory_.InvalidateWeakPtrs();
  task3.weak_factory_.InvalidateWeakPtrs();

  std::set<base::TimeTicks> wake_up_times;

  RunUntilIdle(base::Bind(
      [](std::set<base::TimeTicks>* wake_up_times,
         base::SimpleTestTickClock* clock) {
        wake_up_times->insert(clock->NowTicks());
      },
      &wake_up_times, now_src_.get()));

  EXPECT_THAT(wake_up_times,
              ElementsAre(start_time + delay1, start_time + delay4));
  EXPECT_THAT(run_times, ElementsAre(start_time + delay1, start_time + delay4));
}

TEST_F(TaskQueueManagerTest, TimeDomainWakeUpOnlyCancelledIfAllUsesCancelled) {
  Initialize(1u);

  base::TimeTicks start_time = manager_->Delegate()->NowTicks();

  CancelableTask task1(now_src_.get());
  CancelableTask task2(now_src_.get());
  CancelableTask task3(now_src_.get());
  CancelableTask task4(now_src_.get());
  base::TimeDelta delay1(base::TimeDelta::FromSeconds(5));
  base::TimeDelta delay2(base::TimeDelta::FromSeconds(10));
  base::TimeDelta delay3(base::TimeDelta::FromSeconds(15));
  base::TimeDelta delay4(base::TimeDelta::FromSeconds(30));
  std::vector<base::TimeTicks> run_times;
  runners_[0]->PostDelayedTask(
      FROM_HERE, base::Bind(&CancelableTask::RecordTimeTask,
                            task1.weak_factory_.GetWeakPtr(), &run_times),
      delay1);
  runners_[0]->PostDelayedTask(
      FROM_HERE, base::Bind(&CancelableTask::RecordTimeTask,
                            task2.weak_factory_.GetWeakPtr(), &run_times),
      delay2);
  runners_[0]->PostDelayedTask(
      FROM_HERE, base::Bind(&CancelableTask::RecordTimeTask,
                            task3.weak_factory_.GetWeakPtr(), &run_times),
      delay3);
  runners_[0]->PostDelayedTask(
      FROM_HERE, base::Bind(&CancelableTask::RecordTimeTask,
                            task4.weak_factory_.GetWeakPtr(), &run_times),
      delay4);

  // Post a non-canceled task with |delay3|. So we should still get a wake-up at
  // |delay3| even though we cancel |task3|.
  runners_[0]->PostDelayedTask(FROM_HERE,
                               base::Bind(&CancelableTask::RecordTimeTask,
                                          base::Unretained(&task3), &run_times),
                               delay3);

  task2.weak_factory_.InvalidateWeakPtrs();
  task3.weak_factory_.InvalidateWeakPtrs();
  task1.weak_factory_.InvalidateWeakPtrs();

  std::set<base::TimeTicks> wake_up_times;

  RunUntilIdle(base::Bind(
      [](std::set<base::TimeTicks>* wake_up_times,
         base::SimpleTestTickClock* clock) {
        wake_up_times->insert(clock->NowTicks());
      },
      &wake_up_times, now_src_.get()));

  EXPECT_THAT(wake_up_times,
              ElementsAre(start_time + delay1, start_time + delay3,
                          start_time + delay4));

  EXPECT_THAT(run_times, ElementsAre(start_time + delay3, start_time + delay4));
}

TEST_F(TaskQueueManagerTest, TaskQueueVoters) {
  Initialize(1u);

  // The task queue should be initially enabled.
  EXPECT_TRUE(runners_[0]->IsQueueEnabled());

  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter1 =
      runners_[0]->CreateQueueEnabledVoter();
  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter2 =
      runners_[0]->CreateQueueEnabledVoter();
  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter3 =
      runners_[0]->CreateQueueEnabledVoter();
  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter4 =
      runners_[0]->CreateQueueEnabledVoter();

  // Voters should initially vote for the queue to be enabled.
  EXPECT_TRUE(runners_[0]->IsQueueEnabled());

  // If any voter wants to disable, the queue is disabled.
  voter1->SetQueueEnabled(false);
  EXPECT_FALSE(runners_[0]->IsQueueEnabled());

  // If the voter is deleted then the queue should be re-enabled.
  voter1.reset();
  EXPECT_TRUE(runners_[0]->IsQueueEnabled());

  // If any of the remaining voters wants to disable, the queue should be
  // disabled.
  voter2->SetQueueEnabled(false);
  EXPECT_FALSE(runners_[0]->IsQueueEnabled());

  // If another queue votes to disable, nothing happens because it's already
  // disabled.
  voter3->SetQueueEnabled(false);
  EXPECT_FALSE(runners_[0]->IsQueueEnabled());

  // There are two votes to disable, so one of them voting to enable does
  // nothing.
  voter2->SetQueueEnabled(true);
  EXPECT_FALSE(runners_[0]->IsQueueEnabled());

  // IF all queues vote to enable then the queue is enabled.
  voter3->SetQueueEnabled(true);
  EXPECT_TRUE(runners_[0]->IsQueueEnabled());
}

TEST_F(TaskQueueManagerTest, UnregisterQueueBeforeEnabledVoterDeleted) {
  Initialize(1u);

  scoped_refptr<internal::TaskQueueImpl> queue =
      manager_->NewTaskQueue(TaskQueue::Spec(TaskQueue::QueueType::TEST));

  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter =
      queue->CreateQueueEnabledVoter();

  voter->SetQueueEnabled(true);  // NOP
  queue->UnregisterTaskQueue();

  // This should complete without DCHECKing.
  voter.reset();
}

TEST_F(TaskQueueManagerTest, UnregisterQueueBeforeDisabledVoterDeleted) {
  Initialize(1u);

  scoped_refptr<internal::TaskQueueImpl> queue =
      manager_->NewTaskQueue(TaskQueue::Spec(TaskQueue::QueueType::TEST));

  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter =
      queue->CreateQueueEnabledVoter();

  voter->SetQueueEnabled(false);
  queue->UnregisterTaskQueue();

  // This should complete without DCHECKing.
  voter.reset();
}

TEST_F(TaskQueueManagerTest, SweepCanceledDelayedTasks) {
  Initialize(1u);

  CancelableTask task1(now_src_.get());
  CancelableTask task2(now_src_.get());
  CancelableTask task3(now_src_.get());
  CancelableTask task4(now_src_.get());
  base::TimeDelta delay1(base::TimeDelta::FromSeconds(5));
  base::TimeDelta delay2(base::TimeDelta::FromSeconds(10));
  base::TimeDelta delay3(base::TimeDelta::FromSeconds(15));
  base::TimeDelta delay4(base::TimeDelta::FromSeconds(30));
  std::vector<base::TimeTicks> run_times;
  runners_[0]->PostDelayedTask(
      FROM_HERE, base::Bind(&CancelableTask::RecordTimeTask,
                            task1.weak_factory_.GetWeakPtr(), &run_times),
      delay1);
  runners_[0]->PostDelayedTask(
      FROM_HERE, base::Bind(&CancelableTask::RecordTimeTask,
                            task2.weak_factory_.GetWeakPtr(), &run_times),
      delay2);
  runners_[0]->PostDelayedTask(
      FROM_HERE, base::Bind(&CancelableTask::RecordTimeTask,
                            task3.weak_factory_.GetWeakPtr(), &run_times),
      delay3);
  runners_[0]->PostDelayedTask(
      FROM_HERE, base::Bind(&CancelableTask::RecordTimeTask,
                            task4.weak_factory_.GetWeakPtr(), &run_times),
      delay4);

  EXPECT_EQ(4u, runners_[0]->GetNumberOfPendingTasks());
  task2.weak_factory_.InvalidateWeakPtrs();
  task3.weak_factory_.InvalidateWeakPtrs();
  EXPECT_EQ(4u, runners_[0]->GetNumberOfPendingTasks());

  manager_->SweepCanceledDelayedTasks();
  EXPECT_EQ(2u, runners_[0]->GetNumberOfPendingTasks());

  task1.weak_factory_.InvalidateWeakPtrs();
  task4.weak_factory_.InvalidateWeakPtrs();

  manager_->SweepCanceledDelayedTasks();
  EXPECT_EQ(0u, runners_[0]->GetNumberOfPendingTasks());
}

TEST_F(TaskQueueManagerTest, ComputeDelayTillNextTask) {
  Initialize(2u);

  LazyNow lazy_now(now_src_.get());
  EXPECT_FALSE(static_cast<bool>(ComputeDelayTillNextTask(&lazy_now)));

  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&NopTask),
                               base::TimeDelta::FromSeconds(10));

  EXPECT_EQ(base::TimeDelta::FromSeconds(10),
            ComputeDelayTillNextTask(&lazy_now)->Delay());

  runners_[1]->PostDelayedTask(FROM_HERE, base::Bind(&NopTask),
                               base::TimeDelta::FromSeconds(15));

  EXPECT_EQ(base::TimeDelta::FromSeconds(10),
            ComputeDelayTillNextTask(&lazy_now)->Delay());

  runners_[1]->PostDelayedTask(FROM_HERE, base::Bind(&NopTask),
                               base::TimeDelta::FromSeconds(5));

  EXPECT_EQ(base::TimeDelta::FromSeconds(5),
            ComputeDelayTillNextTask(&lazy_now)->Delay());

  runners_[0]->PostTask(FROM_HERE, base::Bind(&NopTask));

  EXPECT_EQ(base::TimeDelta(), ComputeDelayTillNextTask(&lazy_now)->Delay());
}

TEST_F(TaskQueueManagerTest, ComputeDelayTillNextTask_Disabled) {
  Initialize(1u);

  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter =
      runners_[0]->CreateQueueEnabledVoter();
  voter->SetQueueEnabled(false);
  runners_[0]->PostTask(FROM_HERE, base::Bind(&NopTask));

  LazyNow lazy_now(now_src_.get());
  EXPECT_FALSE(ComputeDelayTillNextTask(&lazy_now));
}

TEST_F(TaskQueueManagerTest, ComputeDelayTillNextTask_Fence) {
  Initialize(1u);

  runners_[0]->InsertFence(TaskQueue::InsertFencePosition::NOW);
  runners_[0]->PostTask(FROM_HERE, base::Bind(&NopTask));

  LazyNow lazy_now(now_src_.get());
  EXPECT_FALSE(ComputeDelayTillNextTask(&lazy_now));
}

TEST_F(TaskQueueManagerTest, ComputeDelayTillNextTask_FenceUnblocking) {
  Initialize(1u);

  runners_[0]->InsertFence(TaskQueue::InsertFencePosition::NOW);
  runners_[0]->PostTask(FROM_HERE, base::Bind(&NopTask));
  runners_[0]->InsertFence(TaskQueue::InsertFencePosition::NOW);

  LazyNow lazy_now(now_src_.get());
  EXPECT_EQ(base::TimeDelta(), ComputeDelayTillNextTask(&lazy_now)->Delay());
}

TEST_F(TaskQueueManagerTest, ComputeDelayTillNextTask_DelayedTaskReady) {
  Initialize(1u);

  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&NopTask),
                               base::TimeDelta::FromSeconds(1));

  now_src_->Advance(base::TimeDelta::FromSeconds(10));

  LazyNow lazy_now(now_src_.get());
  EXPECT_EQ(base::TimeDelta(), ComputeDelayTillNextTask(&lazy_now)->Delay());
}

TEST_F(TaskQueueManagerTest, PostDoWorkContinuation_NoMoreWork) {
  Initialize(1u);

  LazyNow lazy_now(now_src_.get());
  PostDoWorkContinuation(base::Optional<NextTaskDelay>(), &lazy_now);

  EXPECT_EQ(0u, test_task_runner_->NumPendingTasks());
  EXPECT_EQ(0, immediate_do_work_posted_count());
  EXPECT_TRUE(next_delayed_do_work_time().is_null());
}

TEST_F(TaskQueueManagerTest, PostDoWorkContinuation_ImmediateWork) {
  Initialize(1u);

  LazyNow lazy_now(now_src_.get());
  PostDoWorkContinuation(NextTaskDelay(), &lazy_now);

  EXPECT_EQ(1u, test_task_runner_->NumPendingTasks());
  EXPECT_EQ(base::TimeDelta(), test_task_runner_->DelayToNextTaskTime());
  EXPECT_EQ(1, immediate_do_work_posted_count());
  EXPECT_TRUE(next_delayed_do_work_time().is_null());
}

TEST_F(TaskQueueManagerTest, PostDoWorkContinuation_DelayedWorkInThePast) {
  Initialize(1u);

  LazyNow lazy_now(now_src_.get());
  // Note this isn't supposed to happen in practice.
  PostDoWorkContinuation(
      NextTaskDelay(base::TimeDelta::FromSeconds(-1),
                    runners_[0]->GetTimeDomain(),
                    NextTaskDelay::AllowAnyDelayForTesting()),
      &lazy_now);

  EXPECT_EQ(1u, test_task_runner_->NumPendingTasks());
  EXPECT_EQ(base::TimeDelta(), test_task_runner_->DelayToNextTaskTime());
  EXPECT_EQ(1, immediate_do_work_posted_count());
  EXPECT_TRUE(next_delayed_do_work_time().is_null());
}

TEST_F(TaskQueueManagerTest, PostDoWorkContinuation_DelayedWork) {
  Initialize(1u);

  LazyNow lazy_now(now_src_.get());
  PostDoWorkContinuation(NextTaskDelay(base::TimeDelta::FromSeconds(1),
                                       runners_[0]->GetTimeDomain()),
                         &lazy_now);

  EXPECT_EQ(1u, test_task_runner_->NumPendingTasks());
  EXPECT_EQ(base::TimeDelta::FromSeconds(1),
            test_task_runner_->DelayToNextTaskTime());
  EXPECT_EQ(0, immediate_do_work_posted_count());
  EXPECT_EQ(lazy_now.Now() + base::TimeDelta::FromSeconds(1),
            next_delayed_do_work_time());
}

TEST_F(TaskQueueManagerTest,
       PostDoWorkContinuation_DelayedWorkButImmediateDoWorkAlreadyPosted) {
  Initialize(1u);

  MaybeScheduleImmediateWorkLocked(FROM_HERE);
  EXPECT_EQ(1u, test_task_runner_->NumPendingTasks());
  EXPECT_EQ(base::TimeDelta(), test_task_runner_->DelayToNextTaskTime());
  EXPECT_EQ(1, immediate_do_work_posted_count());

  LazyNow lazy_now(now_src_.get());
  PostDoWorkContinuation(NextTaskDelay(base::TimeDelta::FromSeconds(1),
                                       runners_[0]->GetTimeDomain()),
                         &lazy_now);

  // Test that a delayed task didn't get posted.
  EXPECT_EQ(1u, test_task_runner_->NumPendingTasks());
  EXPECT_EQ(base::TimeDelta(), test_task_runner_->DelayToNextTaskTime());
  EXPECT_EQ(1, immediate_do_work_posted_count());
  EXPECT_TRUE(next_delayed_do_work_time().is_null());
}

TEST_F(TaskQueueManagerTest, PostDoWorkContinuation_DelayedWorkTimeChanges) {
  Initialize(1u);

  LazyNow lazy_now(now_src_.get());
  PostDoWorkContinuation(NextTaskDelay(base::TimeDelta::FromSeconds(1),
                                       runners_[0]->GetTimeDomain()),
                         &lazy_now);

  EXPECT_TRUE(test_task_runner_->HasPendingTasks());
  EXPECT_EQ(0, immediate_do_work_posted_count());
  EXPECT_EQ(base::TimeDelta::FromSeconds(1),
            test_task_runner_->DelayToNextTaskTime());
  EXPECT_EQ(lazy_now.Now() + base::TimeDelta::FromSeconds(1),
            next_delayed_do_work_time());

  PostDoWorkContinuation(NextTaskDelay(base::TimeDelta::FromSeconds(10),
                                       runners_[0]->GetTimeDomain()),
                         &lazy_now);

  // This should have resulted in the previous task getting canceled and a new
  // one getting posted.
  EXPECT_EQ(2u, test_task_runner_->NumPendingTasks());
  test_task_runner_->RemoveCancelledTasks();
  EXPECT_EQ(1u, test_task_runner_->NumPendingTasks());
  EXPECT_EQ(base::TimeDelta::FromSeconds(10),
            test_task_runner_->DelayToNextTaskTime());
  EXPECT_EQ(0, immediate_do_work_posted_count());
  EXPECT_EQ(lazy_now.Now() + base::TimeDelta::FromSeconds(10),
            next_delayed_do_work_time());
}

TEST_F(TaskQueueManagerTest,
       PostDoWorkContinuation_ImmediateWorkButDelayedDoWorkPending) {
  Initialize(1u);

  LazyNow lazy_now(now_src_.get());
  PostDoWorkContinuation(NextTaskDelay(base::TimeDelta::FromSeconds(1),
                                       runners_[0]->GetTimeDomain()),
                         &lazy_now);

  now_src_->Advance(base::TimeDelta::FromSeconds(1));
  lazy_now = LazyNow(now_src_.get());
  PostDoWorkContinuation(NextTaskDelay(), &lazy_now);

  // Because the delayed DoWork was pending we don't expect an immediate DoWork
  // to get posted.
  EXPECT_EQ(1u, test_task_runner_->NumPendingTasks());
  EXPECT_EQ(base::TimeDelta(), test_task_runner_->DelayToNextTaskTime());
  EXPECT_EQ(0, immediate_do_work_posted_count());
  EXPECT_EQ(lazy_now.Now(), next_delayed_do_work_time());
}

namespace {
void MessageLoopTaskWithDelayedQuit(
    base::MessageLoop* message_loop,
    base::SimpleTestTickClock* now_src,
    scoped_refptr<internal::TaskQueueImpl> task_queue) {
  base::MessageLoop::ScopedNestableTaskAllower allow(message_loop);
  base::RunLoop run_loop;
  task_queue->PostDelayedTask(FROM_HERE, run_loop.QuitClosure(),
                              base::TimeDelta::FromMilliseconds(100));
  now_src->Advance(base::TimeDelta::FromMilliseconds(200));
  run_loop.Run();
}
}  // namespace

TEST_F(TaskQueueManagerTest, DelayedTaskRunsInNestedMessageLoop) {
  InitializeWithRealMessageLoop(1u);
  base::RunLoop run_loop;
  runners_[0]->PostTask(
      FROM_HERE,
      base::Bind(&MessageLoopTaskWithDelayedQuit, message_loop_.get(),
                 now_src_.get(), base::RetainedRef(runners_[0])));
  run_loop.RunUntilIdle();
}

namespace {
void MessageLoopTaskWithImmediateQuit(
    base::MessageLoop* message_loop,
    base::Closure non_nested_quit_closure,
    scoped_refptr<internal::TaskQueueImpl> task_queue) {
  base::MessageLoop::ScopedNestableTaskAllower allow(message_loop);

  base::RunLoop run_loop;
  // Needed because entering the nested run loop causes a DoWork to get
  // posted.
  task_queue->PostTask(FROM_HERE, base::Bind(&NopTask));
  task_queue->PostTask(FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
  non_nested_quit_closure.Run();
}
}  // namespace

TEST_F(TaskQueueManagerTest,
       DelayedNestedMessageLoopDoesntPreventTasksRunning) {
  InitializeWithRealMessageLoop(1u);
  base::RunLoop run_loop;
  runners_[0]->PostDelayedTask(
      FROM_HERE,
      base::Bind(&MessageLoopTaskWithImmediateQuit, message_loop_.get(),
                 run_loop.QuitClosure(), base::RetainedRef(runners_[0])),
      base::TimeDelta::FromMilliseconds(100));

  now_src_->Advance(base::TimeDelta::FromMilliseconds(200));
  run_loop.Run();
}

TEST_F(TaskQueueManagerTest, CouldTaskRun_DisableAndReenable) {
  Initialize(1u);

  EnqueueOrder enqueue_order = GetNextSequenceNumber();
  EXPECT_TRUE(runners_[0]->CouldTaskRun(enqueue_order));

  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter =
      runners_[0]->CreateQueueEnabledVoter();
  voter->SetQueueEnabled(false);
  EXPECT_FALSE(runners_[0]->CouldTaskRun(enqueue_order));

  voter->SetQueueEnabled(true);
  EXPECT_TRUE(runners_[0]->CouldTaskRun(enqueue_order));
}

TEST_F(TaskQueueManagerTest, CouldTaskRun_Fence) {
  Initialize(1u);

  EnqueueOrder enqueue_order = GetNextSequenceNumber();
  EXPECT_TRUE(runners_[0]->CouldTaskRun(enqueue_order));

  runners_[0]->InsertFence(TaskQueue::InsertFencePosition::NOW);
  EXPECT_TRUE(runners_[0]->CouldTaskRun(enqueue_order));

  runners_[0]->InsertFence(TaskQueue::InsertFencePosition::BEGINNING_OF_TIME);
  EXPECT_FALSE(runners_[0]->CouldTaskRun(enqueue_order));

  runners_[0]->RemoveFence();
  EXPECT_TRUE(runners_[0]->CouldTaskRun(enqueue_order));
}

TEST_F(TaskQueueManagerTest, CouldTaskRun_FenceBeforeThenAfter) {
  Initialize(1u);

  runners_[0]->InsertFence(TaskQueue::InsertFencePosition::NOW);

  EnqueueOrder enqueue_order = GetNextSequenceNumber();
  EXPECT_FALSE(runners_[0]->CouldTaskRun(enqueue_order));

  runners_[0]->InsertFence(TaskQueue::InsertFencePosition::NOW);
  EXPECT_TRUE(runners_[0]->CouldTaskRun(enqueue_order));
}

TEST_F(TaskQueueManagerTest, DelayedDoWorkNotPostedForDisabledQueue) {
  Initialize(1u);

  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&NopTask),
                               base::TimeDelta::FromMilliseconds(1));
  ASSERT_TRUE(test_task_runner_->HasPendingTasks());
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(1),
            test_task_runner_->DelayToNextTaskTime());

  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter =
      runners_[0]->CreateQueueEnabledVoter();
  voter->SetQueueEnabled(false);

  EXPECT_TRUE(test_task_runner_->HasPendingTasks());
  test_task_runner_->RemoveCancelledTasks();
  EXPECT_FALSE(test_task_runner_->HasPendingTasks());

  voter->SetQueueEnabled(true);
  ASSERT_TRUE(test_task_runner_->HasPendingTasks());
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(1),
            test_task_runner_->DelayToNextTaskTime());
}

TEST_F(TaskQueueManagerTest, DisablingQueuesChangesDelayTillNextDoWork) {
  Initialize(3u);
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&NopTask),
                               base::TimeDelta::FromMilliseconds(1));
  runners_[1]->PostDelayedTask(FROM_HERE, base::Bind(&NopTask),
                               base::TimeDelta::FromMilliseconds(10));
  runners_[2]->PostDelayedTask(FROM_HERE, base::Bind(&NopTask),
                               base::TimeDelta::FromMilliseconds(100));

  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter0 =
      runners_[0]->CreateQueueEnabledVoter();
  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter1 =
      runners_[1]->CreateQueueEnabledVoter();
  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter2 =
      runners_[2]->CreateQueueEnabledVoter();

  ASSERT_TRUE(test_task_runner_->HasPendingTasks());
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(1),
            test_task_runner_->DelayToNextTaskTime());

  voter0->SetQueueEnabled(false);
  test_task_runner_->RemoveCancelledTasks();
  ASSERT_TRUE(test_task_runner_->HasPendingTasks());
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(10),
            test_task_runner_->DelayToNextTaskTime());

  voter1->SetQueueEnabled(false);
  test_task_runner_->RemoveCancelledTasks();
  ASSERT_TRUE(test_task_runner_->HasPendingTasks());
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(100),
            test_task_runner_->DelayToNextTaskTime());

  voter2->SetQueueEnabled(false);
  test_task_runner_->RemoveCancelledTasks();
  EXPECT_FALSE(test_task_runner_->HasPendingTasks());
}

TEST_F(TaskQueueManagerTest, GetNextScheduledWakeUp) {
  Initialize(1u);

  EXPECT_EQ(base::nullopt, runners_[0]->GetNextScheduledWakeUp());

  base::TimeTicks start_time = manager_->Delegate()->NowTicks();
  base::TimeDelta delay1 = base::TimeDelta::FromMilliseconds(10);
  base::TimeDelta delay2 = base::TimeDelta::FromMilliseconds(2);

  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&NopTask), delay1);
  EXPECT_EQ(start_time + delay1, runners_[0]->GetNextScheduledWakeUp());

  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&NopTask), delay2);
  EXPECT_EQ(start_time + delay2, runners_[0]->GetNextScheduledWakeUp());

  // We don't have wake-ups scheduled for disabled queues.
  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter =
      runners_[0]->CreateQueueEnabledVoter();
  voter->SetQueueEnabled(false);
  EXPECT_EQ(base::nullopt, runners_[0]->GetNextScheduledWakeUp());

  voter->SetQueueEnabled(true);
  EXPECT_EQ(start_time + delay2, runners_[0]->GetNextScheduledWakeUp());

  // Immediate tasks shouldn't make any difference.
  runners_[0]->PostTask(FROM_HERE, base::Bind(&NopTask));
  EXPECT_EQ(start_time + delay2, runners_[0]->GetNextScheduledWakeUp());

  // Neither should fences.
  runners_[0]->InsertFence(TaskQueue::InsertFencePosition::BEGINNING_OF_TIME);
  EXPECT_EQ(start_time + delay2, runners_[0]->GetNextScheduledWakeUp());
}

TEST_F(TaskQueueManagerTest, SetTimeDomainForDisabledQueue) {
  Initialize(1u);

  MockTaskQueueObserver observer;
  runners_[0]->SetObserver(&observer);

  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&NopTask),
                               base::TimeDelta::FromMilliseconds(1));

  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter =
      runners_[0]->CreateQueueEnabledVoter();
  voter->SetQueueEnabled(false);

  // We should not get a notification for a disabled queue.
  EXPECT_CALL(observer, OnQueueNextWakeUpChanged(_, _)).Times(0);

  std::unique_ptr<VirtualTimeDomain> domain(
      new VirtualTimeDomain(manager_->Delegate()->NowTicks()));
  manager_->RegisterTimeDomain(domain.get());
  runners_[0]->SetTimeDomain(domain.get());

  // Tidy up.
  runners_[0]->UnregisterTaskQueue();
  manager_->UnregisterTimeDomain(domain.get());
}

}  // namespace scheduler
}  // namespace blink
