// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/base/task_queue_manager_impl.h"

#include <stddef.h>
#include <memory>
#include <utility>

#include "base/location.h"
#include "base/memory/ref_counted_memory.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/trace_event_analyzer.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/blame_context.h"
#include "components/viz/test/ordered_simple_task_runner.h"
#include "platform/scheduler/base/real_time_domain.h"
#include "platform/scheduler/base/task_queue_impl.h"
#include "platform/scheduler/base/task_queue_selector.h"
#include "platform/scheduler/base/test_count_uses_time_source.h"
#include "platform/scheduler/base/test_task_time_observer.h"
#include "platform/scheduler/base/thread_controller_impl.h"
#include "platform/scheduler/base/virtual_time_domain.h"
#include "platform/scheduler/base/work_queue.h"
#include "platform/scheduler/base/work_queue_sets.h"
#include "platform/scheduler/test/task_queue_manager_for_test.h"
#include "platform/scheduler/test/test_task_queue.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/WebKit/public/platform/scheduler/test/renderer_scheduler_test_support.h"

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

class TaskQueueManagerTest : public ::testing::Test {
 public:
  TaskQueueManagerTest() = default;
  void DeleteTaskQueueManager() { manager_.reset(); }

 protected:
  void TearDown() { manager_.reset(); }

  scoped_refptr<TestTaskQueue> CreateTaskQueueWithSpec(TaskQueue::Spec spec) {
    return manager_->CreateTaskQueue<TestTaskQueue>(spec);
  }

  scoped_refptr<TestTaskQueue> CreateTaskQueue() {
    return CreateTaskQueueWithSpec(TaskQueue::Spec("test"));
  }

  scoped_refptr<TestTaskQueue> CreateTaskQueueWithMonitoredQuiescence() {
    return CreateTaskQueueWithSpec(
        TaskQueue::Spec("test").SetShouldMonitorQuiescence(true));
  }

  void Initialize(size_t num_queues) {
    now_src_.Advance(base::TimeDelta::FromMicroseconds(1000));

    test_task_runner_ =
        base::WrapRefCounted(new cc::OrderedSimpleTaskRunner(&now_src_, false));

    manager_ = TaskQueueManagerForTest::Create(nullptr, test_task_runner_.get(),
                                               &now_src_);

    for (size_t i = 0; i < num_queues; i++)
      runners_.push_back(CreateTaskQueue());
  }

  void InitializeWithRealMessageLoop(size_t num_queues) {
    message_loop_.reset(new base::MessageLoop());
    original_message_loop_task_runner_ = message_loop_->task_runner();
    // A null clock triggers some assertions.
    now_src_.Advance(base::TimeDelta::FromMicroseconds(1000));
    manager_ = TaskQueueManagerForTest::Create(
        message_loop_.get(), GetSingleThreadTaskRunnerForTesting(), &now_src_);

    for (size_t i = 0; i < num_queues; i++)
      runners_.push_back(CreateTaskQueue());
  }

  void WakeUpReadyDelayedQueues(LazyNow lazy_now) {
    manager_->WakeUpReadyDelayedQueues(&lazy_now);
  }

  EnqueueOrder GetNextSequenceNumber() const {
    return manager_->GetNextSequenceNumber();
  }

  void MaybeScheduleImmediateWork(const base::Location& from_here) {
    manager_->MaybeScheduleImmediateWork(from_here);
  }

  // Runs all immediate tasks until there is no more work to do and advances
  // time if there is a pending delayed task. |per_run_time_callback| is called
  // when the clock advances.
  void RunUntilIdle(base::RepeatingClosure per_run_time_callback) {
    for (;;) {
      // Advance time if we've run out of immediate work to do.
      if (!manager_->HasImmediateWork()) {
        base::TimeTicks run_time;
        if (manager_->GetRealTimeDomain()->NextScheduledRunTime(&run_time)) {
          now_src_.SetNowTicks(run_time);
          per_run_time_callback.Run();
        } else {
          break;
        }
      }

      test_task_runner_->RunPendingTasks();
    }
  }

  base::TimeTicks Now() { return now_src_.NowTicks(); }

  std::unique_ptr<base::MessageLoop> message_loop_;
  scoped_refptr<base::SingleThreadTaskRunner>
      original_message_loop_task_runner_;
  base::SimpleTestTickClock now_src_;
  scoped_refptr<cc::OrderedSimpleTaskRunner> test_task_runner_;
  std::unique_ptr<TaskQueueManagerForTest> manager_;
  std::vector<scoped_refptr<TestTaskQueue>> runners_;
  TestTaskTimeObserver test_task_time_observer_;
};

void PostFromNestedRunloop(
    base::MessageLoop* message_loop,
    base::SingleThreadTaskRunner* runner,
    std::vector<std::pair<base::OnceClosure, bool>>* tasks) {
  base::MessageLoop::ScopedNestableTaskAllower allow(message_loop);
  for (std::pair<base::OnceClosure, bool>& pair : *tasks) {
    if (pair.second) {
      runner->PostTask(FROM_HERE, std::move(pair.first));
    } else {
      runner->PostNonNestableTask(FROM_HERE, std::move(pair.first));
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
  TestCountUsesTimeSource test_count_uses_time_source;

  manager_ = TaskQueueManagerForTest::Create(
      nullptr, GetSingleThreadTaskRunnerForTesting(),
      &test_count_uses_time_source);
  manager_->SetWorkBatchSize(6);
  manager_->AddTaskTimeObserver(&test_task_time_observer_);

  for (size_t i = 0; i < 3; i++)
    runners_.push_back(CreateTaskQueue());

  runners_[0]->PostTask(FROM_HERE, base::BindOnce(&NopTask));
  runners_[0]->PostTask(FROM_HERE, base::BindOnce(&NopTask));
  runners_[1]->PostTask(FROM_HERE, base::BindOnce(&NopTask));
  runners_[1]->PostTask(FROM_HERE, base::BindOnce(&NopTask));
  runners_[2]->PostTask(FROM_HERE, base::BindOnce(&NopTask));
  runners_[2]->PostTask(FROM_HERE, base::BindOnce(&NopTask));

  base::RunLoop().RunUntilIdle();
  // Now is called each time a task is queued, when first task is started
  // running, and when a task is completed. 6 * 3 = 18 calls.
  EXPECT_EQ(18, test_count_uses_time_source.now_calls_count());
}

TEST_F(TaskQueueManagerTest, NowNotCalledForNestedTasks) {
  message_loop_.reset(new base::MessageLoop());
  // This memory is managed by the TaskQueueManager, but we need to hold a
  // pointer to this object to read out how many times Now was called.
  TestCountUsesTimeSource test_count_uses_time_source;

  manager_ = TaskQueueManagerForTest::Create(message_loop_.get(),
                                             message_loop_->task_runner(),
                                             &test_count_uses_time_source);
  manager_->AddTaskTimeObserver(&test_task_time_observer_);

  runners_.push_back(CreateTaskQueue());

  std::vector<std::pair<base::OnceClosure, bool>>
      tasks_to_post_from_nested_loop;
  for (int i = 0; i < 7; ++i) {
    tasks_to_post_from_nested_loop.push_back(
        std::make_pair(base::BindOnce(&NopTask), true));
  }

  runners_[0]->PostTask(
      FROM_HERE,
      base::BindOnce(&PostFromNestedRunloop, message_loop_.get(),
                     base::RetainedRef(runners_[0]),
                     base::Unretained(&tasks_to_post_from_nested_loop)));

  base::RunLoop().RunUntilIdle();
  // We need to call Now twice, to measure the start and end of the outermost
  // task. We shouldn't call it for any of the nested tasks.
  // Also Now is called when a task is scheduled (8 times).
  // That brings expected call count for Now() to 2 + 8 = 10
  EXPECT_EQ(10, test_count_uses_time_source.now_calls_count());
}

void NullTask() {}

void TestTask(EnqueueOrder value, std::vector<EnqueueOrder>* out_result) {
  out_result->push_back(value);
}

void DisableQueueTestTask(EnqueueOrder value,
                          std::vector<EnqueueOrder>* out_result,
                          TaskQueue::QueueEnabledVoter* voter) {
  out_result->push_back(value);
  voter->SetQueueEnabled(false);
}

TEST_F(TaskQueueManagerTest, SingleQueuePosting) {
  Initialize(1u);

  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostTask(FROM_HERE, base::BindOnce(&TestTask, 1, &run_order));
  runners_[0]->PostTask(FROM_HERE, base::BindOnce(&TestTask, 2, &run_order));
  runners_[0]->PostTask(FROM_HERE, base::BindOnce(&TestTask, 3, &run_order));

  test_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1, 2, 3));
}

TEST_F(TaskQueueManagerTest, MultiQueuePosting) {
  Initialize(3u);

  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostTask(FROM_HERE, base::BindOnce(&TestTask, 1, &run_order));
  runners_[0]->PostTask(FROM_HERE, base::BindOnce(&TestTask, 2, &run_order));
  runners_[1]->PostTask(FROM_HERE, base::BindOnce(&TestTask, 3, &run_order));
  runners_[1]->PostTask(FROM_HERE, base::BindOnce(&TestTask, 4, &run_order));
  runners_[2]->PostTask(FROM_HERE, base::BindOnce(&TestTask, 5, &run_order));
  runners_[2]->PostTask(FROM_HERE, base::BindOnce(&TestTask, 6, &run_order));

  test_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1, 2, 3, 4, 5, 6));
}

TEST_F(TaskQueueManagerTest, NonNestableTaskPosting) {
  InitializeWithRealMessageLoop(1u);

  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostNonNestableTask(FROM_HERE,
                                   base::BindOnce(&TestTask, 1, &run_order));

  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1));
}

TEST_F(TaskQueueManagerTest, NonNestableTaskExecutesInExpectedOrder) {
  InitializeWithRealMessageLoop(1u);

  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostTask(FROM_HERE, base::BindOnce(&TestTask, 1, &run_order));
  runners_[0]->PostTask(FROM_HERE, base::BindOnce(&TestTask, 2, &run_order));
  runners_[0]->PostTask(FROM_HERE, base::BindOnce(&TestTask, 3, &run_order));
  runners_[0]->PostTask(FROM_HERE, base::BindOnce(&TestTask, 4, &run_order));
  runners_[0]->PostNonNestableTask(FROM_HERE,
                                   base::BindOnce(&TestTask, 5, &run_order));

  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1, 2, 3, 4, 5));
}

TEST_F(TaskQueueManagerTest, NonNestableTasksDoesntExecuteInNestedLoop) {
  InitializeWithRealMessageLoop(1u);

  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostTask(FROM_HERE, base::BindOnce(&TestTask, 1, &run_order));
  runners_[0]->PostTask(FROM_HERE, base::BindOnce(&TestTask, 2, &run_order));

  std::vector<std::pair<base::OnceClosure, bool>>
      tasks_to_post_from_nested_loop;
  tasks_to_post_from_nested_loop.push_back(
      std::make_pair(base::BindOnce(&TestTask, 3, &run_order), false));
  tasks_to_post_from_nested_loop.push_back(
      std::make_pair(base::BindOnce(&TestTask, 4, &run_order), false));
  tasks_to_post_from_nested_loop.push_back(
      std::make_pair(base::BindOnce(&TestTask, 5, &run_order), true));
  tasks_to_post_from_nested_loop.push_back(
      std::make_pair(base::BindOnce(&TestTask, 6, &run_order), true));

  runners_[0]->PostTask(
      FROM_HERE,
      base::BindOnce(&PostFromNestedRunloop, message_loop_.get(),
                     base::RetainedRef(runners_[0]),
                     base::Unretained(&tasks_to_post_from_nested_loop)));

  base::RunLoop().RunUntilIdle();
  // Note we expect tasks 3 & 4 to run last because they're non-nestable.
  EXPECT_THAT(run_order, ElementsAre(1, 2, 5, 6, 3, 4));
}

namespace {

void InsertFenceAndPostTestTask(EnqueueOrder id,
                                std::vector<EnqueueOrder>* run_order,
                                scoped_refptr<TestTaskQueue> task_queue) {
  run_order->push_back(id);
  task_queue->InsertFence(TaskQueue::InsertFencePosition::kNow);
  task_queue->PostTask(FROM_HERE, base::BindOnce(&TestTask, id + 1, run_order));

  // Force reload of immediate work queue. In real life the same effect can be
  // achieved with cross-thread posting.
  task_queue->GetTaskQueueImpl()->ReloadImmediateWorkQueueIfEmpty();
}

}  // namespace

TEST_F(TaskQueueManagerTest, TaskQueueDisabledFromNestedLoop) {
  InitializeWithRealMessageLoop(1u);
  std::vector<EnqueueOrder> run_order;

  std::vector<std::pair<base::OnceClosure, bool>>
      tasks_to_post_from_nested_loop;

  tasks_to_post_from_nested_loop.push_back(
      std::make_pair(base::BindOnce(&TestTask, 1, &run_order), false));
  tasks_to_post_from_nested_loop.push_back(std::make_pair(
      base::BindOnce(&InsertFenceAndPostTestTask, 2, &run_order, runners_[0]),
      true));

  runners_[0]->PostTask(
      FROM_HERE,
      base::BindOnce(&PostFromNestedRunloop, message_loop_.get(),
                     base::RetainedRef(runners_[0]),
                     base::Unretained(&tasks_to_post_from_nested_loop)));
  base::RunLoop().RunUntilIdle();

  // Task 1 shouldn't run first due to it being non-nestable and queue gets
  // blocked after task 2. Task 1 runs after existing nested message loop
  // due to being posted before inserting a fence.
  // This test checks that breaks when nestable task is pushed into a redo
  // queue.
  EXPECT_THAT(run_order, ElementsAre(2, 1));

  runners_[0]->RemoveFence();
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(2, 1, 3));
}

TEST_F(TaskQueueManagerTest, HasPendingImmediateWork_ImmediateTask) {
  Initialize(1u);

  std::vector<EnqueueOrder> run_order;
  EXPECT_FALSE(runners_[0]->HasTaskToRunImmediately());
  runners_[0]->PostTask(FROM_HERE, base::BindOnce(&TestTask, 1, &run_order));
  EXPECT_TRUE(runners_[0]->HasTaskToRunImmediately());

  // Move the task into the |immediate_work_queue|.
  EXPECT_TRUE(runners_[0]->GetTaskQueueImpl()->immediate_work_queue()->Empty());
  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter =
      runners_[0]->CreateQueueEnabledVoter();
  voter->SetQueueEnabled(false);
  test_task_runner_->RunUntilIdle();
  EXPECT_FALSE(
      runners_[0]->GetTaskQueueImpl()->immediate_work_queue()->Empty());
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
  runners_[0]->PostDelayedTask(FROM_HERE,
                               base::BindOnce(&TestTask, 1, &run_order), delay);
  EXPECT_FALSE(runners_[0]->HasTaskToRunImmediately());
  now_src_.Advance(delay);
  EXPECT_TRUE(runners_[0]->HasTaskToRunImmediately());

  // Move the task into the |delayed_work_queue|.
  WakeUpReadyDelayedQueues(LazyNow(&now_src_));
  EXPECT_FALSE(runners_[0]->GetTaskQueueImpl()->delayed_work_queue()->Empty());
  EXPECT_TRUE(runners_[0]->HasTaskToRunImmediately());

  // Run the task, making the queue empty.
  test_task_runner_->RunUntilIdle();
  EXPECT_FALSE(runners_[0]->HasTaskToRunImmediately());
}

TEST_F(TaskQueueManagerTest, DelayedTaskPosting) {
  Initialize(1u);

  std::vector<EnqueueOrder> run_order;
  base::TimeDelta delay(base::TimeDelta::FromMilliseconds(10));
  runners_[0]->PostDelayedTask(FROM_HERE,
                               base::BindOnce(&TestTask, 1, &run_order), delay);
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
  runners_[0]->PostDelayedTask(FROM_HERE, base::BindOnce(&NopTask), delay);

  size_t task_count = 0;
  test_task_runner_->RunTasksWhile(
      base::BindRepeating(&MessageLoopTaskCounter, &task_count));
  EXPECT_EQ(1u, task_count);
}

TEST_F(TaskQueueManagerTest, DelayedTaskPosting_MultipleTasks_DecendingOrder) {
  Initialize(1u);

  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostDelayedTask(FROM_HERE,
                               base::BindOnce(&TestTask, 1, &run_order),
                               base::TimeDelta::FromMilliseconds(10));

  runners_[0]->PostDelayedTask(FROM_HERE,
                               base::BindOnce(&TestTask, 2, &run_order),
                               base::TimeDelta::FromMilliseconds(8));

  runners_[0]->PostDelayedTask(FROM_HERE,
                               base::BindOnce(&TestTask, 3, &run_order),
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
  runners_[0]->PostDelayedTask(FROM_HERE,
                               base::BindOnce(&TestTask, 1, &run_order),
                               base::TimeDelta::FromMilliseconds(1));

  runners_[0]->PostDelayedTask(FROM_HERE,
                               base::BindOnce(&TestTask, 2, &run_order),
                               base::TimeDelta::FromMilliseconds(5));

  runners_[0]->PostDelayedTask(FROM_HERE,
                               base::BindOnce(&TestTask, 3, &run_order),
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
  runners_[0]->PostDelayedTask(FROM_HERE,
                               base::BindOnce(&TestTask, 1, &run_order), delay);
  runners_[0]->PostDelayedTask(FROM_HERE,
                               base::BindOnce(&TestTask, 2, &run_order), delay);
  runners_[0]->PostDelayedTask(FROM_HERE,
                               base::BindOnce(&TestTask, 3, &run_order), delay);

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
      FROM_HERE,
      base::BindOnce(&TestObject::Run, base::Owned(new TestObject())), delay);
  runners_[0]->PostTask(
      FROM_HERE,
      base::BindOnce(&TestObject::Run, base::Owned(new TestObject())));

  manager_.reset();

  EXPECT_EQ(2, TestObject::destructor_count__);
}

TEST_F(TaskQueueManagerTest, InsertAndRemoveFence) {
  Initialize(1u);
  runners_[0]->InsertFence(TaskQueue::InsertFencePosition::kNow);

  std::vector<EnqueueOrder> run_order;
  // Posting a task when pumping is disabled doesn't result in work getting
  // posted.
  runners_[0]->PostTask(FROM_HERE, base::BindOnce(&TestTask, 1, &run_order));
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
  runners_[0]->InsertFence(TaskQueue::InsertFencePosition::kNow);
  runners_[0]->PostTask(FROM_HERE, base::BindOnce(&TestTask, 1, &run_order));

  runners_[0]->RemoveFence();
  EXPECT_FALSE(test_task_runner_->HasPendingTasks());
}

TEST_F(TaskQueueManagerTest, EnablingFencedQueueDoesNotPostDoWork) {
  Initialize(1u);

  std::vector<EnqueueOrder> run_order;
  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter =
      runners_[0]->CreateQueueEnabledVoter();
  voter->SetQueueEnabled(false);
  runners_[0]->InsertFence(TaskQueue::InsertFencePosition::kNow);
  runners_[0]->PostTask(FROM_HERE, base::BindOnce(&TestTask, 1, &run_order));

  voter->SetQueueEnabled(true);
  EXPECT_FALSE(test_task_runner_->HasPendingTasks());
}

TEST_F(TaskQueueManagerTest, DenyRunning_BeforePosting) {
  Initialize(1u);

  std::vector<EnqueueOrder> run_order;
  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter =
      runners_[0]->CreateQueueEnabledVoter();
  voter->SetQueueEnabled(false);
  runners_[0]->PostTask(FROM_HERE, base::BindOnce(&TestTask, 1, &run_order));
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
  runners_[0]->PostTask(FROM_HERE, base::BindOnce(&TestTask, 1, &run_order));
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
  runners_[0]->InsertFence(TaskQueue::InsertFencePosition::kNow);
  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter =
      runners_[0]->CreateQueueEnabledVoter();
  voter->SetQueueEnabled(false);
  runners_[0]->PostTask(FROM_HERE, base::BindOnce(&TestTask, 1, &run_order));

  test_task_runner_->RunUntilIdle();
  EXPECT_TRUE(run_order.empty());

  runners_[0]->RemoveFence();
  voter->SetQueueEnabled(true);
  test_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1));
}

TEST_F(TaskQueueManagerTest, RemovingFenceWithDelayedTask) {
  Initialize(1u);
  runners_[0]->InsertFence(TaskQueue::InsertFencePosition::kNow);

  std::vector<EnqueueOrder> run_order;
  // Posting a delayed task when fenced will apply the delay, but won't cause
  // work to executed afterwards.
  base::TimeDelta delay(base::TimeDelta::FromMilliseconds(10));
  runners_[0]->PostDelayedTask(FROM_HERE,
                               base::BindOnce(&TestTask, 1, &run_order), delay);

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
  runners_[0]->InsertFence(TaskQueue::InsertFencePosition::kNow);

  std::vector<EnqueueOrder> run_order;
  // Posting a delayed task when fenced will apply the delay, but won't cause
  // work to executed afterwards.
  base::TimeDelta delay1(base::TimeDelta::FromMilliseconds(1));
  base::TimeDelta delay2(base::TimeDelta::FromMilliseconds(10));
  base::TimeDelta delay3(base::TimeDelta::FromMilliseconds(20));
  runners_[0]->PostDelayedTask(
      FROM_HERE, base::BindOnce(&TestTask, 1, &run_order), delay1);
  runners_[0]->PostDelayedTask(
      FROM_HERE, base::BindOnce(&TestTask, 2, &run_order), delay2);
  runners_[0]->PostDelayedTask(
      FROM_HERE, base::BindOnce(&TestTask, 3, &run_order), delay3);

  now_src_.Advance(base::TimeDelta::FromMilliseconds(15));
  test_task_runner_->RunUntilIdle();
  EXPECT_TRUE(run_order.empty());

  // Removing the fence causes the ready tasks to run.
  runners_[0]->RemoveFence();
  test_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1, 2));
}

TEST_F(TaskQueueManagerTest, InsertFencePreventsDelayedTasksFromRunning) {
  Initialize(1u);
  runners_[0]->InsertFence(TaskQueue::InsertFencePosition::kNow);

  std::vector<EnqueueOrder> run_order;
  base::TimeDelta delay(base::TimeDelta::FromMilliseconds(10));
  runners_[0]->PostDelayedTask(FROM_HERE,
                               base::BindOnce(&TestTask, 1, &run_order), delay);

  test_task_runner_->RunForPeriod(base::TimeDelta::FromMilliseconds(10));
  EXPECT_TRUE(run_order.empty());
}

TEST_F(TaskQueueManagerTest, MultipleFences) {
  Initialize(1u);

  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostTask(FROM_HERE, base::BindOnce(&TestTask, 1, &run_order));
  runners_[0]->PostTask(FROM_HERE, base::BindOnce(&TestTask, 2, &run_order));
  runners_[0]->InsertFence(TaskQueue::InsertFencePosition::kNow);

  runners_[0]->PostTask(FROM_HERE, base::BindOnce(&TestTask, 3, &run_order));
  test_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1, 2));

  runners_[0]->InsertFence(TaskQueue::InsertFencePosition::kNow);
  // Subsequent tasks should be blocked.
  runners_[0]->PostTask(FROM_HERE, base::BindOnce(&TestTask, 4, &run_order));
  test_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1, 2, 3));
}

TEST_F(TaskQueueManagerTest, InsertFenceThenImmediatlyRemoveDoesNotBlock) {
  Initialize(1u);
  runners_[0]->InsertFence(TaskQueue::InsertFencePosition::kNow);
  runners_[0]->RemoveFence();

  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostTask(FROM_HERE, base::BindOnce(&TestTask, 1, &run_order));
  runners_[0]->PostTask(FROM_HERE, base::BindOnce(&TestTask, 2, &run_order));

  test_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1, 2));
}

TEST_F(TaskQueueManagerTest, InsertFencePostThenRemoveDoesNotBlock) {
  Initialize(1u);
  runners_[0]->InsertFence(TaskQueue::InsertFencePosition::kNow);

  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostTask(FROM_HERE, base::BindOnce(&TestTask, 1, &run_order));
  runners_[0]->PostTask(FROM_HERE, base::BindOnce(&TestTask, 2, &run_order));
  runners_[0]->RemoveFence();

  test_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1, 2));
}

TEST_F(TaskQueueManagerTest, MultipleFencesWithInitiallyEmptyQueue) {
  Initialize(1u);
  runners_[0]->InsertFence(TaskQueue::InsertFencePosition::kNow);

  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostTask(FROM_HERE, base::BindOnce(&TestTask, 1, &run_order));
  runners_[0]->InsertFence(TaskQueue::InsertFencePosition::kNow);
  runners_[0]->PostTask(FROM_HERE, base::BindOnce(&TestTask, 2, &run_order));
  test_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1));
}

TEST_F(TaskQueueManagerTest, BlockedByFence) {
  Initialize(1u);
  EXPECT_FALSE(runners_[0]->BlockedByFence());

  runners_[0]->InsertFence(TaskQueue::InsertFencePosition::kNow);
  EXPECT_TRUE(runners_[0]->BlockedByFence());

  runners_[0]->RemoveFence();
  EXPECT_FALSE(runners_[0]->BlockedByFence());

  runners_[0]->PostTask(FROM_HERE, base::BindOnce(&NopTask));
  runners_[0]->InsertFence(TaskQueue::InsertFencePosition::kNow);
  EXPECT_FALSE(runners_[0]->BlockedByFence());

  test_task_runner_->RunUntilIdle();
  EXPECT_TRUE(runners_[0]->BlockedByFence());

  runners_[0]->RemoveFence();
  EXPECT_FALSE(runners_[0]->BlockedByFence());
}

TEST_F(TaskQueueManagerTest, BlockedByFence_BothTypesOfFence) {
  Initialize(1u);

  runners_[0]->PostTask(FROM_HERE, base::BindOnce(&NopTask));

  runners_[0]->InsertFence(TaskQueue::InsertFencePosition::kNow);
  EXPECT_FALSE(runners_[0]->BlockedByFence());

  runners_[0]->InsertFence(TaskQueue::InsertFencePosition::kBeginningOfTime);
  EXPECT_TRUE(runners_[0]->BlockedByFence());
}

namespace {

void RecordTimeTask(std::vector<base::TimeTicks>* run_times,
                    base::SimpleTestTickClock* clock) {
  run_times->push_back(clock->NowTicks());
}

void RecordTimeAndQueueTask(
    std::vector<std::pair<scoped_refptr<TestTaskQueue>, base::TimeTicks>>*
        run_times,
    scoped_refptr<TestTaskQueue> task_queue,
    base::SimpleTestTickClock* clock) {
  run_times->emplace_back(task_queue, clock->NowTicks());
}

}  // namespace

TEST_F(TaskQueueManagerTest, DelayedFence_DelayedTasks) {
  Initialize(1u);
  test_task_runner_->SetAutoAdvanceNowToPendingTasks(true);

  std::vector<base::TimeTicks> run_times;
  runners_[0]->PostDelayedTask(
      FROM_HERE, base::BindOnce(&RecordTimeTask, &run_times, &now_src_),
      base::TimeDelta::FromMilliseconds(100));
  runners_[0]->PostDelayedTask(
      FROM_HERE, base::BindOnce(&RecordTimeTask, &run_times, &now_src_),
      base::TimeDelta::FromMilliseconds(200));
  runners_[0]->PostDelayedTask(
      FROM_HERE, base::BindOnce(&RecordTimeTask, &run_times, &now_src_),
      base::TimeDelta::FromMilliseconds(300));

  runners_[0]->InsertFenceAt(Now() + base::TimeDelta::FromMilliseconds(250));
  EXPECT_FALSE(runners_[0]->HasActiveFence());

  test_task_runner_->RunUntilIdle();

  EXPECT_TRUE(runners_[0]->HasActiveFence());
  EXPECT_THAT(
      run_times,
      ElementsAre(base::TimeTicks() + base::TimeDelta::FromMilliseconds(101),
                  base::TimeTicks() + base::TimeDelta::FromMilliseconds(201)));
  run_times.clear();

  runners_[0]->RemoveFence();
  test_task_runner_->RunUntilIdle();

  EXPECT_FALSE(runners_[0]->HasActiveFence());
  EXPECT_THAT(run_times, ElementsAre(base::TimeTicks() +
                                     base::TimeDelta::FromMilliseconds(301)));
}

TEST_F(TaskQueueManagerTest, DelayedFence_ImmediateTasks) {
  Initialize(1u);
  test_task_runner_->SetAutoAdvanceNowToPendingTasks(true);

  std::vector<base::TimeTicks> run_times;
  runners_[0]->InsertFenceAt(Now() + base::TimeDelta::FromMilliseconds(250));

  for (int i = 0; i < 5; ++i) {
    runners_[0]->PostTask(
        FROM_HERE, base::BindOnce(&RecordTimeTask, &run_times, &now_src_));
    test_task_runner_->RunForPeriod(base::TimeDelta::FromMilliseconds(100));
    if (i < 2) {
      EXPECT_FALSE(runners_[0]->HasActiveFence());
    } else {
      EXPECT_TRUE(runners_[0]->HasActiveFence());
    }
  }

  EXPECT_THAT(
      run_times,
      ElementsAre(base::TimeTicks() + base::TimeDelta::FromMilliseconds(1),
                  base::TimeTicks() + base::TimeDelta::FromMilliseconds(101),
                  base::TimeTicks() + base::TimeDelta::FromMilliseconds(201)));
  run_times.clear();

  runners_[0]->RemoveFence();
  test_task_runner_->RunUntilIdle();

  EXPECT_THAT(
      run_times,
      ElementsAre(base::TimeTicks() + base::TimeDelta::FromMilliseconds(501),
                  base::TimeTicks() + base::TimeDelta::FromMilliseconds(501)));
}

TEST_F(TaskQueueManagerTest, DelayedFence_RemovedFenceDoesNotActivate) {
  Initialize(1u);
  test_task_runner_->SetAutoAdvanceNowToPendingTasks(true);

  std::vector<base::TimeTicks> run_times;
  runners_[0]->InsertFenceAt(Now() + base::TimeDelta::FromMilliseconds(250));

  for (int i = 0; i < 3; ++i) {
    runners_[0]->PostTask(
        FROM_HERE, base::BindOnce(&RecordTimeTask, &run_times, &now_src_));
    EXPECT_FALSE(runners_[0]->HasActiveFence());
    test_task_runner_->RunForPeriod(base::TimeDelta::FromMilliseconds(100));
  }

  EXPECT_TRUE(runners_[0]->HasActiveFence());
  runners_[0]->RemoveFence();

  for (int i = 0; i < 2; ++i) {
    runners_[0]->PostTask(
        FROM_HERE, base::BindOnce(&RecordTimeTask, &run_times, &now_src_));
    test_task_runner_->RunForPeriod(base::TimeDelta::FromMilliseconds(100));
    EXPECT_FALSE(runners_[0]->HasActiveFence());
  }

  EXPECT_THAT(
      run_times,
      ElementsAre(base::TimeTicks() + base::TimeDelta::FromMilliseconds(1),
                  base::TimeTicks() + base::TimeDelta::FromMilliseconds(101),
                  base::TimeTicks() + base::TimeDelta::FromMilliseconds(201),
                  base::TimeTicks() + base::TimeDelta::FromMilliseconds(301),
                  base::TimeTicks() + base::TimeDelta::FromMilliseconds(401)));
}

TEST_F(TaskQueueManagerTest, DelayedFence_TakeIncomingImmediateQueue) {
  // This test checks that everything works correctly when a work queue
  // is swapped with an immediate incoming queue and a delayed fence
  // is activated, forcing a different queue to become active.
  Initialize(2u);
  test_task_runner_->SetAutoAdvanceNowToPendingTasks(true);

  scoped_refptr<TestTaskQueue> queue1 = runners_[0];
  scoped_refptr<TestTaskQueue> queue2 = runners_[1];

  std::vector<std::pair<scoped_refptr<TestTaskQueue>, base::TimeTicks>>
      run_times;

  // Fence ensures that the task posted after advancing time is blocked.
  queue1->InsertFenceAt(Now() + base::TimeDelta::FromMilliseconds(250));

  // This task should not be blocked and should run immediately after
  // advancing time at 301ms.
  queue1->PostTask(FROM_HERE, base::BindOnce(&RecordTimeAndQueueTask,
                                             &run_times, queue1, &now_src_));
  // Force reload of immediate work queue. In real life the same effect can be
  // achieved with cross-thread posting.
  queue1->GetTaskQueueImpl()->ReloadImmediateWorkQueueIfEmpty();

  now_src_.Advance(base::TimeDelta::FromMilliseconds(300));

  // This task should be blocked.
  queue1->PostTask(FROM_HERE, base::BindOnce(&RecordTimeAndQueueTask,
                                             &run_times, queue1, &now_src_));
  // This task on a different runner should run as expected.
  queue2->PostTask(FROM_HERE, base::BindOnce(&RecordTimeAndQueueTask,
                                             &run_times, queue2, &now_src_));

  test_task_runner_->RunUntilIdle();

  EXPECT_THAT(
      run_times,
      ElementsAre(
          std::make_pair(queue1, base::TimeTicks() +
                                     base::TimeDelta::FromMilliseconds(301)),
          std::make_pair(queue2, base::TimeTicks() +
                                     base::TimeDelta::FromMilliseconds(301))));
}

namespace {

void ReentrantTestTask(scoped_refptr<base::SingleThreadTaskRunner> runner,
                       int countdown,
                       std::vector<EnqueueOrder>* out_result) {
  out_result->push_back(countdown);
  if (--countdown) {
    runner->PostTask(
        FROM_HERE, BindOnce(&ReentrantTestTask, runner, countdown, out_result));
  }
}

}  // namespace

TEST_F(TaskQueueManagerTest, ReentrantPosting) {
  Initialize(1u);

  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostTask(
      FROM_HERE, BindOnce(&ReentrantTestTask, runners_[0], 3, &run_order));

  test_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(3, 2, 1));
}

TEST_F(TaskQueueManagerTest, NoTasksAfterShutdown) {
  Initialize(1u);

  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostTask(FROM_HERE, base::BindOnce(&TestTask, 1, &run_order));
  manager_.reset();
  runners_[0]->PostTask(FROM_HERE, base::BindOnce(&TestTask, 1, &run_order));

  test_task_runner_->RunUntilIdle();
  EXPECT_TRUE(run_order.empty());
}

void PostTaskToRunner(scoped_refptr<base::SingleThreadTaskRunner> runner,
                      std::vector<EnqueueOrder>* run_order) {
  runner->PostTask(FROM_HERE, base::BindOnce(&TestTask, 1, run_order));
}

TEST_F(TaskQueueManagerTest, PostFromThread) {
  InitializeWithRealMessageLoop(1u);

  std::vector<EnqueueOrder> run_order;
  base::Thread thread("TestThread");
  thread.Start();
  thread.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&PostTaskToRunner, runners_[0], &run_order));
  thread.Stop();

  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1));
}

void RePostingTestTask(scoped_refptr<base::SingleThreadTaskRunner> runner,
                       int* run_count) {
  (*run_count)++;
  runner->PostTask(
      FROM_HERE,
      BindOnce(&RePostingTestTask, base::Unretained(runner.get()), run_count));
}

TEST_F(TaskQueueManagerTest, DoWorkCantPostItselfMultipleTimes) {
  Initialize(1u);

  int run_count = 0;
  runners_[0]->PostTask(
      FROM_HERE, base::BindOnce(&RePostingTestTask, runners_[0], &run_count));

  test_task_runner_->RunPendingTasks();
  // NOTE without the executing_task_ check in MaybeScheduleDoWork there
  // will be two tasks here.
  EXPECT_EQ(1u, test_task_runner_->NumPendingTasks());
  EXPECT_EQ(1, run_count);
}

TEST_F(TaskQueueManagerTest, PostFromNestedRunloop) {
  InitializeWithRealMessageLoop(1u);

  std::vector<EnqueueOrder> run_order;
  std::vector<std::pair<base::OnceClosure, bool>>
      tasks_to_post_from_nested_loop;
  tasks_to_post_from_nested_loop.push_back(
      std::make_pair(base::BindOnce(&TestTask, 1, &run_order), true));

  runners_[0]->PostTask(FROM_HERE, base::BindOnce(&TestTask, 0, &run_order));
  runners_[0]->PostTask(
      FROM_HERE,
      base::BindOnce(&PostFromNestedRunloop, message_loop_.get(),
                     base::RetainedRef(runners_[0]),
                     base::Unretained(&tasks_to_post_from_nested_loop)));
  runners_[0]->PostTask(FROM_HERE, base::BindOnce(&TestTask, 2, &run_order));

  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(run_order, ElementsAre(0, 2, 1));
}

TEST_F(TaskQueueManagerTest, WorkBatching) {
  Initialize(1u);

  manager_->SetWorkBatchSize(2);

  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostTask(FROM_HERE, base::BindOnce(&TestTask, 1, &run_order));
  runners_[0]->PostTask(FROM_HERE, base::BindOnce(&TestTask, 2, &run_order));
  runners_[0]->PostTask(FROM_HERE, base::BindOnce(&TestTask, 3, &run_order));
  runners_[0]->PostTask(FROM_HERE, base::BindOnce(&TestTask, 4, &run_order));

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
  runners_[0]->PostTask(FROM_HERE, base::BindOnce(&TestTask, 1, &run_order));
  runners_[0]->PostTask(FROM_HERE, base::BindOnce(&TestTask, 2, &run_order));

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
  runners_[0]->PostTask(FROM_HERE, base::BindOnce(&TestTask, 1, &run_order));

  EXPECT_CALL(observer, WillProcessTask(_)).Times(0);
  EXPECT_CALL(observer, DidProcessTask(_)).Times(0);

  base::RunLoop().RunUntilIdle();
}

void RemoveObserverTask(TaskQueueManagerImpl* manager,
                        base::MessageLoop::TaskObserver* observer) {
  manager->RemoveTaskObserver(observer);
}

TEST_F(TaskQueueManagerTest, TaskObserverRemovingInsideTask) {
  InitializeWithRealMessageLoop(1u);
  MockTaskObserver observer;
  manager_->SetWorkBatchSize(3);
  manager_->AddTaskObserver(&observer);

  runners_[0]->PostTask(FROM_HERE, base::BindOnce(&RemoveObserverTask,
                                                  manager_.get(), &observer));

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
  runners_[0]->PostTask(FROM_HERE, base::BindOnce(&TestTask, 1, &run_order));
  runners_[1]->PostTask(FROM_HERE, base::BindOnce(&TestTask, 2, &run_order));

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
  runners_[0]->PostTask(FROM_HERE, base::BindOnce(&TestTask, 1, &run_order));

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

  runners_[0]->PostTask(FROM_HERE, base::BindOnce(&RemoveQueueObserverTask,
                                                  runners_[0], &observer));

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
  now_src_.Advance(base::TimeDelta::FromMicroseconds(10000));

  // With no delayed tasks.
  base::TimeTicks run_time;
  EXPECT_FALSE(manager_->GetRealTimeDomain()->NextScheduledRunTime(&run_time));

  // With a non-delayed task.
  runners_[0]->PostTask(FROM_HERE, base::BindOnce(&NopTask));
  EXPECT_FALSE(manager_->GetRealTimeDomain()->NextScheduledRunTime(&run_time));

  // With a delayed task.
  base::TimeDelta expected_delay = base::TimeDelta::FromMilliseconds(50);
  runners_[0]->PostDelayedTask(FROM_HERE, base::BindOnce(&NopTask),
                               expected_delay);
  EXPECT_TRUE(manager_->GetRealTimeDomain()->NextScheduledRunTime(&run_time));
  EXPECT_EQ(now_src_.NowTicks() + expected_delay, run_time);

  // With another delayed task in the same queue with a longer delay.
  runners_[0]->PostDelayedTask(FROM_HERE, base::BindOnce(&NopTask),
                               base::TimeDelta::FromMilliseconds(100));
  EXPECT_TRUE(manager_->GetRealTimeDomain()->NextScheduledRunTime(&run_time));
  EXPECT_EQ(now_src_.NowTicks() + expected_delay, run_time);

  // With another delayed task in the same queue with a shorter delay.
  expected_delay = base::TimeDelta::FromMilliseconds(20);
  runners_[0]->PostDelayedTask(FROM_HERE, base::BindOnce(&NopTask),
                               expected_delay);
  EXPECT_TRUE(manager_->GetRealTimeDomain()->NextScheduledRunTime(&run_time));
  EXPECT_EQ(now_src_.NowTicks() + expected_delay, run_time);

  // With another delayed task in a different queue with a shorter delay.
  expected_delay = base::TimeDelta::FromMilliseconds(10);
  runners_[1]->PostDelayedTask(FROM_HERE, base::BindOnce(&NopTask),
                               expected_delay);
  EXPECT_TRUE(manager_->GetRealTimeDomain()->NextScheduledRunTime(&run_time));
  EXPECT_EQ(now_src_.NowTicks() + expected_delay, run_time);

  // Test it updates as time progresses
  now_src_.Advance(expected_delay);
  EXPECT_TRUE(manager_->GetRealTimeDomain()->NextScheduledRunTime(&run_time));
  EXPECT_EQ(now_src_.NowTicks(), run_time);
}

TEST_F(TaskQueueManagerTest, TimeDomain_NextScheduledRunTime_MultipleQueues) {
  Initialize(3u);

  base::TimeDelta delay1 = base::TimeDelta::FromMilliseconds(50);
  base::TimeDelta delay2 = base::TimeDelta::FromMilliseconds(5);
  base::TimeDelta delay3 = base::TimeDelta::FromMilliseconds(10);
  runners_[0]->PostDelayedTask(FROM_HERE, base::BindOnce(&NopTask), delay1);
  runners_[1]->PostDelayedTask(FROM_HERE, base::BindOnce(&NopTask), delay2);
  runners_[2]->PostDelayedTask(FROM_HERE, base::BindOnce(&NopTask), delay3);
  runners_[0]->PostTask(FROM_HERE, base::BindOnce(&NopTask));

  base::TimeTicks run_time;
  EXPECT_TRUE(manager_->GetRealTimeDomain()->NextScheduledRunTime(&run_time));
  EXPECT_EQ(now_src_.NowTicks() + delay2, run_time);
}

TEST_F(TaskQueueManagerTest, DeleteTaskQueueManagerInsideATask) {
  Initialize(1u);

  runners_[0]->PostTask(
      FROM_HERE, base::BindOnce(&TaskQueueManagerTest::DeleteTaskQueueManager,
                                base::Unretained(this)));

  // This should not crash, assuming DoWork detects the TaskQueueManager has
  // been deleted.
  test_task_runner_->RunUntilIdle();
}

TEST_F(TaskQueueManagerTest, GetAndClearSystemIsQuiescentBit) {
  Initialize(3u);

  scoped_refptr<TaskQueue> queue0 = CreateTaskQueueWithMonitoredQuiescence();
  scoped_refptr<TaskQueue> queue1 = CreateTaskQueueWithMonitoredQuiescence();
  scoped_refptr<TaskQueue> queue2 = CreateTaskQueue();

  EXPECT_TRUE(manager_->GetAndClearSystemIsQuiescentBit());

  queue0->PostTask(FROM_HERE, base::BindOnce(&NopTask));
  test_task_runner_->RunUntilIdle();
  EXPECT_FALSE(manager_->GetAndClearSystemIsQuiescentBit());
  EXPECT_TRUE(manager_->GetAndClearSystemIsQuiescentBit());

  queue1->PostTask(FROM_HERE, base::BindOnce(&NopTask));
  test_task_runner_->RunUntilIdle();
  EXPECT_FALSE(manager_->GetAndClearSystemIsQuiescentBit());
  EXPECT_TRUE(manager_->GetAndClearSystemIsQuiescentBit());

  queue2->PostTask(FROM_HERE, base::BindOnce(&NopTask));
  test_task_runner_->RunUntilIdle();
  EXPECT_TRUE(manager_->GetAndClearSystemIsQuiescentBit());

  queue0->PostTask(FROM_HERE, base::BindOnce(&NopTask));
  queue1->PostTask(FROM_HERE, base::BindOnce(&NopTask));
  test_task_runner_->RunUntilIdle();
  EXPECT_FALSE(manager_->GetAndClearSystemIsQuiescentBit());
  EXPECT_TRUE(manager_->GetAndClearSystemIsQuiescentBit());
}

TEST_F(TaskQueueManagerTest, HasPendingImmediateWork) {
  Initialize(1u);

  EXPECT_FALSE(runners_[0]->HasTaskToRunImmediately());
  runners_[0]->PostTask(FROM_HERE, base::BindOnce(NullTask));
  EXPECT_TRUE(runners_[0]->HasTaskToRunImmediately());

  test_task_runner_->RunUntilIdle();
  EXPECT_FALSE(runners_[0]->HasTaskToRunImmediately());
}

TEST_F(TaskQueueManagerTest, HasPendingImmediateWork_DelayedTasks) {
  Initialize(1u);

  EXPECT_FALSE(runners_[0]->HasTaskToRunImmediately());
  runners_[0]->PostDelayedTask(FROM_HERE, base::BindOnce(NullTask),
                               base::TimeDelta::FromMilliseconds(12));
  EXPECT_FALSE(runners_[0]->HasTaskToRunImmediately());

  // Move time forwards until just before the delayed task should run.
  now_src_.Advance(base::TimeDelta::FromMilliseconds(10));
  WakeUpReadyDelayedQueues(LazyNow(&now_src_));
  EXPECT_FALSE(runners_[0]->HasTaskToRunImmediately());

  // Force the delayed task onto the work queue.
  now_src_.Advance(base::TimeDelta::FromMilliseconds(2));
  WakeUpReadyDelayedQueues(LazyNow(&now_src_));
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
        FROM_HERE, base::BindOnce(&ExpensiveTestTask, i, &now_src_, &run_order),
        delay);
  }

  test_task_runner_->RunForPeriod(delay);

  for (int i = 0; i < 9; i++) {
    runners_[0]->PostTask(FROM_HERE, base::BindOnce(&ExpensiveTestTask, i,
                                                    &now_src_, &run_order));
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
  runners_[0]->PostTask(FROM_HERE, base::BindOnce(&TestTask, 2, &run_order));
  runners_[0]->PostTask(FROM_HERE, base::BindOnce(&TestTask, 3, &run_order));
  runners_[0]->PostDelayedTask(FROM_HERE,
                               base::BindOnce(&TestTask, 1, &run_order), delay);

  now_src_.Advance(delay * 2);
  test_task_runner_->RunUntilIdle();

  EXPECT_THAT(run_order, ElementsAre(2, 3, 1));
}

TEST_F(TaskQueueManagerTest,
       DelayedTaskDoesNotSkipAHeadOfNonDelayedTask_DifferentQueues) {
  Initialize(2u);

  std::vector<EnqueueOrder> run_order;
  base::TimeDelta delay = base::TimeDelta::FromMilliseconds(10);
  runners_[1]->PostTask(FROM_HERE, base::BindOnce(&TestTask, 2, &run_order));
  runners_[1]->PostTask(FROM_HERE, base::BindOnce(&TestTask, 3, &run_order));
  runners_[0]->PostDelayedTask(FROM_HERE,
                               base::BindOnce(&TestTask, 1, &run_order), delay);

  now_src_.Advance(delay * 2);
  test_task_runner_->RunUntilIdle();

  EXPECT_THAT(run_order, ElementsAre(2, 3, 1));
}

TEST_F(TaskQueueManagerTest, DelayedTaskDoesNotSkipAHeadOfShorterDelayedTask) {
  Initialize(2u);

  std::vector<EnqueueOrder> run_order;
  base::TimeDelta delay1 = base::TimeDelta::FromMilliseconds(10);
  base::TimeDelta delay2 = base::TimeDelta::FromMilliseconds(5);
  runners_[0]->PostDelayedTask(
      FROM_HERE, base::BindOnce(&TestTask, 1, &run_order), delay1);
  runners_[1]->PostDelayedTask(
      FROM_HERE, base::BindOnce(&TestTask, 2, &run_order), delay2);

  now_src_.Advance(delay1 * 2);
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
  runner->PostTask(FROM_HERE, base::BindOnce(&CheckIsNested, was_nested));
  run_loop->Run();
}

TEST_F(TaskQueueManagerTest, QuitWhileNested) {
  // This test makes sure we don't continue running a work batch after a nested
  // run loop has been exited in the middle of the batch.
  InitializeWithRealMessageLoop(1u);
  manager_->SetWorkBatchSize(2);

  bool was_nested = true;
  base::RunLoop run_loop;
  runners_[0]->PostTask(
      FROM_HERE,
      base::BindOnce(&PostAndQuitFromNestedRunloop, base::Unretained(&run_loop),
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
  runners_[0]->PostDelayedTask(FROM_HERE,
                               base::BindOnce(&TestTask, 1, &run_order),
                               base::TimeDelta::FromMilliseconds(30));
  runners_[0]->PostDelayedTask(FROM_HERE,
                               base::BindOnce(&TestTask, 2, &run_order),
                               base::TimeDelta::FromMilliseconds(20));
  runners_[0]->PostDelayedTask(FROM_HERE,
                               base::BindOnce(&TestTask, 3, &run_order),
                               base::TimeDelta::FromMilliseconds(10));
  runners_[0]->PostTask(FROM_HERE, base::BindOnce(&TestTask, 4, &run_order));

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

  scoped_refptr<TaskQueue> queue1 = CreateTaskQueue();
  scoped_refptr<TaskQueue> queue2 = CreateTaskQueue();
  scoped_refptr<TaskQueue> queue3 = CreateTaskQueue();

  ASSERT_NE(queue1, queue2);
  ASSERT_NE(queue1, queue3);
  ASSERT_NE(queue2, queue3);

  std::vector<EnqueueOrder> run_order;
  queue1->PostTask(FROM_HERE, base::BindOnce(&TestTask, 1, &run_order));
  queue2->PostTask(FROM_HERE, base::BindOnce(&TestTask, 2, &run_order));
  queue3->PostTask(FROM_HERE, base::BindOnce(&TestTask, 3, &run_order));
  test_task_runner_->RunUntilIdle();

  EXPECT_THAT(run_order, ElementsAre(1, 2, 3));
}

TEST_F(TaskQueueManagerTest, ShutdownTaskQueue) {
  Initialize(1u);

  scoped_refptr<TaskQueue> queue1 = CreateTaskQueue();
  scoped_refptr<TaskQueue> queue2 = CreateTaskQueue();
  scoped_refptr<TaskQueue> queue3 = CreateTaskQueue();

  ASSERT_NE(queue1, queue2);
  ASSERT_NE(queue1, queue3);
  ASSERT_NE(queue2, queue3);

  std::vector<EnqueueOrder> run_order;
  queue1->PostTask(FROM_HERE, base::BindOnce(&TestTask, 1, &run_order));
  queue2->PostTask(FROM_HERE, base::BindOnce(&TestTask, 2, &run_order));
  queue3->PostTask(FROM_HERE, base::BindOnce(&TestTask, 3, &run_order));

  queue2->ShutdownTaskQueue();
  test_task_runner_->RunUntilIdle();

  EXPECT_THAT(run_order, ElementsAre(1, 3));
}

TEST_F(TaskQueueManagerTest, ShutdownTaskQueue_WithDelayedTasks) {
  Initialize(2u);

  // Register three delayed tasks
  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostDelayedTask(FROM_HERE,
                               base::BindOnce(&TestTask, 1, &run_order),
                               base::TimeDelta::FromMilliseconds(10));
  runners_[1]->PostDelayedTask(FROM_HERE,
                               base::BindOnce(&TestTask, 2, &run_order),
                               base::TimeDelta::FromMilliseconds(20));
  runners_[0]->PostDelayedTask(FROM_HERE,
                               base::BindOnce(&TestTask, 3, &run_order),
                               base::TimeDelta::FromMilliseconds(30));

  runners_[1]->ShutdownTaskQueue();
  test_task_runner_->RunUntilIdle();

  test_task_runner_->RunForPeriod(base::TimeDelta::FromMilliseconds(40));
  ASSERT_THAT(run_order, ElementsAre(1, 3));
}

namespace {
void ShutdownQueue(scoped_refptr<TaskQueue> queue) {
  queue->ShutdownTaskQueue();
}
}  // namespace

TEST_F(TaskQueueManagerTest, ShutdownTaskQueue_InTasks) {
  Initialize(3u);

  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostTask(FROM_HERE, base::BindOnce(&TestTask, 1, &run_order));
  runners_[0]->PostTask(FROM_HERE, base::BindOnce(&ShutdownQueue, runners_[1]));
  runners_[0]->PostTask(FROM_HERE, base::BindOnce(&ShutdownQueue, runners_[2]));
  runners_[1]->PostTask(FROM_HERE, base::BindOnce(&TestTask, 2, &run_order));
  runners_[2]->PostTask(FROM_HERE, base::BindOnce(&TestTask, 3, &run_order));

  test_task_runner_->RunUntilIdle();
  ASSERT_THAT(run_order, ElementsAre(1));
}

namespace {

class MockObserver : public TaskQueueManager::Observer {
 public:
  MOCK_METHOD0(OnTriedToExecuteBlockedTask, void());
  MOCK_METHOD0(OnBeginNestedRunLoop, void());
  MOCK_METHOD0(OnExitNestedRunLoop, void());
};

}  // namespace

TEST_F(TaskQueueManagerTest, ShutdownTaskQueueInNestedLoop) {
  InitializeWithRealMessageLoop(1u);

  // We retain a reference to the task queue even when the manager has deleted
  // its reference.
  scoped_refptr<TaskQueue> task_queue = CreateTaskQueue();

  std::vector<bool> log;
  std::vector<std::pair<base::OnceClosure, bool>>
      tasks_to_post_from_nested_loop;

  // Inside a nested run loop, call task_queue->ShutdownTaskQueue, bookended
  // by calls to HasOneRefTask to make sure the manager doesn't release its
  // reference until the nested run loop exits.
  // NB: This first HasOneRefTask is a sanity check.
  tasks_to_post_from_nested_loop.push_back(
      std::make_pair(base::BindOnce(&NopTask), true));
  tasks_to_post_from_nested_loop.push_back(
      std::make_pair(base::BindOnce(&TaskQueue::ShutdownTaskQueue,
                                    base::Unretained(task_queue.get())),
                     true));
  tasks_to_post_from_nested_loop.push_back(
      std::make_pair(base::BindOnce(&NopTask), true));
  runners_[0]->PostTask(
      FROM_HERE,
      base::BindOnce(&PostFromNestedRunloop, message_loop_.get(),
                     base::RetainedRef(runners_[0]),
                     base::Unretained(&tasks_to_post_from_nested_loop)));
  base::RunLoop().RunUntilIdle();

  // Just make sure that we don't crash.
}

TEST_F(TaskQueueManagerTest, TimeDomainsAreIndependant) {
  Initialize(2u);

  base::TimeTicks start_time_ticks = manager_->NowTicks();
  std::unique_ptr<VirtualTimeDomain> domain_a(
      new VirtualTimeDomain(start_time_ticks));
  std::unique_ptr<VirtualTimeDomain> domain_b(
      new VirtualTimeDomain(start_time_ticks));
  manager_->RegisterTimeDomain(domain_a.get());
  manager_->RegisterTimeDomain(domain_b.get());
  runners_[0]->SetTimeDomain(domain_a.get());
  runners_[1]->SetTimeDomain(domain_b.get());

  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostDelayedTask(FROM_HERE,
                               base::BindOnce(&TestTask, 1, &run_order),
                               base::TimeDelta::FromMilliseconds(10));
  runners_[0]->PostDelayedTask(FROM_HERE,
                               base::BindOnce(&TestTask, 2, &run_order),
                               base::TimeDelta::FromMilliseconds(20));
  runners_[0]->PostDelayedTask(FROM_HERE,
                               base::BindOnce(&TestTask, 3, &run_order),
                               base::TimeDelta::FromMilliseconds(30));

  runners_[1]->PostDelayedTask(FROM_HERE,
                               base::BindOnce(&TestTask, 4, &run_order),
                               base::TimeDelta::FromMilliseconds(10));
  runners_[1]->PostDelayedTask(FROM_HERE,
                               base::BindOnce(&TestTask, 5, &run_order),
                               base::TimeDelta::FromMilliseconds(20));
  runners_[1]->PostDelayedTask(FROM_HERE,
                               base::BindOnce(&TestTask, 6, &run_order),
                               base::TimeDelta::FromMilliseconds(30));

  domain_b->AdvanceNowTo(start_time_ticks +
                         base::TimeDelta::FromMilliseconds(50));
  manager_->MaybeScheduleImmediateWork(FROM_HERE);

  test_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(4, 5, 6));

  domain_a->AdvanceNowTo(start_time_ticks +
                         base::TimeDelta::FromMilliseconds(50));
  manager_->MaybeScheduleImmediateWork(FROM_HERE);

  test_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(4, 5, 6, 1, 2, 3));

  runners_[0]->ShutdownTaskQueue();
  runners_[1]->ShutdownTaskQueue();

  manager_->UnregisterTimeDomain(domain_a.get());
  manager_->UnregisterTimeDomain(domain_b.get());
}

TEST_F(TaskQueueManagerTest, TimeDomainMigration) {
  Initialize(1u);

  base::TimeTicks start_time_ticks = manager_->NowTicks();
  std::unique_ptr<VirtualTimeDomain> domain_a(
      new VirtualTimeDomain(start_time_ticks));
  manager_->RegisterTimeDomain(domain_a.get());
  runners_[0]->SetTimeDomain(domain_a.get());

  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostDelayedTask(FROM_HERE,
                               base::BindOnce(&TestTask, 1, &run_order),
                               base::TimeDelta::FromMilliseconds(10));
  runners_[0]->PostDelayedTask(FROM_HERE,
                               base::BindOnce(&TestTask, 2, &run_order),
                               base::TimeDelta::FromMilliseconds(20));
  runners_[0]->PostDelayedTask(FROM_HERE,
                               base::BindOnce(&TestTask, 3, &run_order),
                               base::TimeDelta::FromMilliseconds(30));
  runners_[0]->PostDelayedTask(FROM_HERE,
                               base::BindOnce(&TestTask, 4, &run_order),
                               base::TimeDelta::FromMilliseconds(40));

  domain_a->AdvanceNowTo(start_time_ticks +
                         base::TimeDelta::FromMilliseconds(20));
  manager_->MaybeScheduleImmediateWork(FROM_HERE);
  test_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1, 2));

  std::unique_ptr<VirtualTimeDomain> domain_b(
      new VirtualTimeDomain(start_time_ticks));
  manager_->RegisterTimeDomain(domain_b.get());
  runners_[0]->SetTimeDomain(domain_b.get());

  domain_b->AdvanceNowTo(start_time_ticks +
                         base::TimeDelta::FromMilliseconds(50));
  manager_->MaybeScheduleImmediateWork(FROM_HERE);

  test_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1, 2, 3, 4));

  runners_[0]->ShutdownTaskQueue();

  manager_->UnregisterTimeDomain(domain_a.get());
  manager_->UnregisterTimeDomain(domain_b.get());
}

TEST_F(TaskQueueManagerTest, TimeDomainMigrationWithIncomingImmediateTasks) {
  Initialize(1u);

  base::TimeTicks start_time_ticks = manager_->NowTicks();
  std::unique_ptr<VirtualTimeDomain> domain_a(
      new VirtualTimeDomain(start_time_ticks));
  std::unique_ptr<VirtualTimeDomain> domain_b(
      new VirtualTimeDomain(start_time_ticks));
  manager_->RegisterTimeDomain(domain_a.get());
  manager_->RegisterTimeDomain(domain_b.get());

  runners_[0]->SetTimeDomain(domain_a.get());
  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostTask(FROM_HERE, base::BindOnce(&TestTask, 1, &run_order));
  runners_[0]->SetTimeDomain(domain_b.get());

  test_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1));

  runners_[0]->ShutdownTaskQueue();

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
  runners_[0]->PostDelayedTask(FROM_HERE,
                               base::BindOnce(&TestTask, 1, &run_order),
                               base::TimeDelta::FromMilliseconds(40));

  runners_[0]->SetTimeDomain(domain_b.get());
  runners_[0]->PostDelayedTask(FROM_HERE,
                               base::BindOnce(&TestTask, 2, &run_order),
                               base::TimeDelta::FromMilliseconds(30));

  runners_[0]->SetTimeDomain(domain_a.get());
  runners_[0]->PostDelayedTask(FROM_HERE,
                               base::BindOnce(&TestTask, 3, &run_order),
                               base::TimeDelta::FromMilliseconds(20));

  runners_[0]->SetTimeDomain(domain_b.get());
  runners_[0]->PostDelayedTask(FROM_HERE,
                               base::BindOnce(&TestTask, 4, &run_order),
                               base::TimeDelta::FromMilliseconds(10));

  test_task_runner_->RunForPeriod(base::TimeDelta::FromMilliseconds(40));
  EXPECT_THAT(run_order, ElementsAre(4, 3, 2, 1));

  runners_[0]->ShutdownTaskQueue();

  manager_->UnregisterTimeDomain(domain_a.get());
  manager_->UnregisterTimeDomain(domain_b.get());
}

namespace {

class MockTaskQueueObserver : public TaskQueue::Observer {
 public:
  ~MockTaskQueueObserver() override = default;

  MOCK_METHOD2(OnQueueNextWakeUpChanged, void(TaskQueue*, base::TimeTicks));
};

}  // namespace

TEST_F(TaskQueueManagerTest, TaskQueueObserver_ImmediateTask) {
  Initialize(1u);

  MockTaskQueueObserver observer;
  runners_[0]->SetObserver(&observer);

  // We should get a notification when a task is posted on an empty queue.
  EXPECT_CALL(observer, OnQueueNextWakeUpChanged(runners_[0].get(), _));
  runners_[0]->PostTask(FROM_HERE, base::BindOnce(&NopTask));
  Mock::VerifyAndClearExpectations(&observer);

  // But not subsequently.
  EXPECT_CALL(observer, OnQueueNextWakeUpChanged(_, _)).Times(0);
  runners_[0]->PostTask(FROM_HERE, base::BindOnce(&NopTask));
  Mock::VerifyAndClearExpectations(&observer);

  // Unless the immediate work queue is emptied.
  runners_[0]->GetTaskQueueImpl()->ReloadImmediateWorkQueueIfEmpty();
  EXPECT_CALL(observer, OnQueueNextWakeUpChanged(runners_[0].get(), _));
  runners_[0]->PostTask(FROM_HERE, base::BindOnce(&NopTask));

  // Tidy up.
  runners_[0]->ShutdownTaskQueue();
}

TEST_F(TaskQueueManagerTest, TaskQueueObserver_DelayedTask) {
  Initialize(1u);

  base::TimeTicks start_time = manager_->NowTicks();
  base::TimeDelta delay10s(base::TimeDelta::FromSeconds(10));
  base::TimeDelta delay100s(base::TimeDelta::FromSeconds(100));
  base::TimeDelta delay1s(base::TimeDelta::FromSeconds(1));

  MockTaskQueueObserver observer;
  runners_[0]->SetObserver(&observer);

  // We should get a notification when a delayed task is posted on an empty
  // queue.
  EXPECT_CALL(observer, OnQueueNextWakeUpChanged(runners_[0].get(),
                                                 start_time + delay10s));
  runners_[0]->PostDelayedTask(FROM_HERE, base::BindOnce(&NopTask), delay10s);
  Mock::VerifyAndClearExpectations(&observer);

  // We should not get a notification for a longer delay.
  EXPECT_CALL(observer, OnQueueNextWakeUpChanged(_, _)).Times(0);
  runners_[0]->PostDelayedTask(FROM_HERE, base::BindOnce(&NopTask), delay100s);
  Mock::VerifyAndClearExpectations(&observer);

  // We should get a notification for a shorter delay.
  EXPECT_CALL(observer, OnQueueNextWakeUpChanged(runners_[0].get(),
                                                 start_time + delay1s));
  runners_[0]->PostDelayedTask(FROM_HERE, base::BindOnce(&NopTask), delay1s);
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
  runners_[0]->ShutdownTaskQueue();
}

TEST_F(TaskQueueManagerTest, TaskQueueObserver_DelayedTaskMultipleQueues) {
  Initialize(2u);

  MockTaskQueueObserver observer;
  runners_[0]->SetObserver(&observer);
  runners_[1]->SetObserver(&observer);

  base::TimeTicks start_time = manager_->NowTicks();
  base::TimeDelta delay1s(base::TimeDelta::FromSeconds(1));
  base::TimeDelta delay10s(base::TimeDelta::FromSeconds(10));

  EXPECT_CALL(observer,
              OnQueueNextWakeUpChanged(runners_[0].get(), start_time + delay1s))
      .Times(1);
  EXPECT_CALL(observer, OnQueueNextWakeUpChanged(runners_[1].get(),
                                                 start_time + delay10s))
      .Times(1);
  runners_[0]->PostDelayedTask(FROM_HERE, base::BindOnce(&NopTask), delay1s);
  runners_[1]->PostDelayedTask(FROM_HERE, base::BindOnce(&NopTask), delay10s);
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
  runners_[0]->ShutdownTaskQueue();
  runners_[1]->ShutdownTaskQueue();
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
  runners_[0]->PostDelayedTask(FROM_HERE, base::BindOnce(&NopTask), delay1s);
  Mock::VerifyAndClearExpectations(&observer);

  std::unique_ptr<TimeDomain> mock_time_domain =
      std::make_unique<RealTimeDomain>();
  manager_->RegisterTimeDomain(mock_time_domain.get());

  now_src_.Advance(delay10s);

  EXPECT_CALL(observer, OnQueueNextWakeUpChanged(_, _));
  runners_[0]->SetTimeDomain(mock_time_domain.get());
  Mock::VerifyAndClearExpectations(&observer);

  // Tidy up.
  runners_[0]->ShutdownTaskQueue();
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

  base::TimeTicks start_time = manager_->NowTicks();
  base::TimeDelta delay1(base::TimeDelta::FromSeconds(5));
  base::TimeDelta delay2(base::TimeDelta::FromSeconds(10));

  EXPECT_CALL(observer,
              OnQueueNextWakeUpChanged(runners_[0].get(), start_time + delay1))
      .Times(1);

  CancelableTask task1(&now_src_);
  CancelableTask task2(&now_src_);
  std::vector<base::TimeTicks> run_times;
  runners_[0]->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&CancelableTask::RecordTimeTask,
                     task1.weak_factory_.GetWeakPtr(), &run_times),
      delay1);
  runners_[0]->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&CancelableTask::RecordTimeTask,
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
        base::BindOnce(&ChromiumRunloopInspectionTask, test_task_runner_),
        base::TimeDelta::FromMilliseconds(i));
  }
  test_task_runner_->RunUntilIdle();
}

namespace {

class QuadraticTask {
 public:
  QuadraticTask(scoped_refptr<TaskQueue> task_queue,
                base::TimeDelta delay,
                base::SimpleTestTickClock* now_src)
      : count_(0), task_queue_(task_queue), delay_(delay), now_src_(now_src) {}

  void SetShouldExit(base::RepeatingCallback<bool()> should_exit) {
    should_exit_ = should_exit;
  }

  void Run() {
    if (should_exit_.Run())
      return;
    count_++;
    task_queue_->PostDelayedTask(
        FROM_HERE, base::BindOnce(&QuadraticTask::Run, base::Unretained(this)),
        delay_);
    task_queue_->PostDelayedTask(
        FROM_HERE, base::BindOnce(&QuadraticTask::Run, base::Unretained(this)),
        delay_);
    now_src_->Advance(base::TimeDelta::FromMilliseconds(5));
  }

  int Count() const { return count_; }

 private:
  int count_;
  scoped_refptr<TaskQueue> task_queue_;
  base::TimeDelta delay_;
  base::RepeatingCallback<bool()> should_exit_;
  base::SimpleTestTickClock* now_src_;
};

class LinearTask {
 public:
  LinearTask(scoped_refptr<TaskQueue> task_queue,
             base::TimeDelta delay,
             base::SimpleTestTickClock* now_src)
      : count_(0), task_queue_(task_queue), delay_(delay), now_src_(now_src) {}

  void SetShouldExit(base::RepeatingCallback<bool()> should_exit) {
    should_exit_ = should_exit;
  }

  void Run() {
    if (should_exit_.Run())
      return;
    count_++;
    task_queue_->PostDelayedTask(
        FROM_HERE, base::BindOnce(&LinearTask::Run, base::Unretained(this)),
        delay_);
    now_src_->Advance(base::TimeDelta::FromMilliseconds(5));
  }

  int Count() const { return count_; }

 private:
  int count_;
  scoped_refptr<TaskQueue> task_queue_;
  base::TimeDelta delay_;
  base::RepeatingCallback<bool()> should_exit_;
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
      runners_[0], base::TimeDelta::FromMilliseconds(10), &now_src_);
  LinearTask linear_immediate_task(runners_[0], base::TimeDelta(), &now_src_);
  base::RepeatingCallback<bool()> should_exit = base::BindRepeating(
      ShouldExit, &quadratic_delayed_task, &linear_immediate_task);
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
                                         &now_src_);
  LinearTask linear_delayed_task(
      runners_[0], base::TimeDelta::FromMilliseconds(10), &now_src_);
  base::RepeatingCallback<bool()> should_exit = base::BindRepeating(
      &ShouldExit, &quadratic_immediate_task, &linear_delayed_task);

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
      runners_[0], base::TimeDelta::FromMilliseconds(10), &now_src_);
  LinearTask linear_immediate_task(runners_[1], base::TimeDelta(), &now_src_);
  base::RepeatingCallback<bool()> should_exit = base::BindRepeating(
      ShouldExit, &quadratic_delayed_task, &linear_immediate_task);
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
                                         &now_src_);
  LinearTask linear_delayed_task(
      runners_[1], base::TimeDelta::FromMilliseconds(10), &now_src_);
  base::RepeatingCallback<bool()> should_exit = base::BindRepeating(
      &ShouldExit, &quadratic_immediate_task, &linear_delayed_task);

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
    TaskQueueManagerImpl* task_queue_manager,
    std::vector<internal::TaskQueueImpl*>* task_sources) {
  task_sources->push_back(task_queue_manager->currently_executing_task_queue());
}
}  // namespace

TEST_F(TaskQueueManagerTest, CurrentlyExecutingTaskQueue_TaskRunning) {
  Initialize(2u);

  TestTaskQueue* queue0 = runners_[0].get();
  TestTaskQueue* queue1 = runners_[1].get();

  std::vector<internal::TaskQueueImpl*> task_sources;
  queue0->PostTask(FROM_HERE,
                   base::BindOnce(&CurrentlyExecutingTaskQueueTestTask,
                                  manager_.get(), &task_sources));
  queue1->PostTask(FROM_HERE,
                   base::BindOnce(&CurrentlyExecutingTaskQueueTestTask,
                                  manager_.get(), &task_sources));
  test_task_runner_->RunUntilIdle();

  EXPECT_THAT(task_sources, ElementsAre(queue0->GetTaskQueueImpl(),
                                        queue1->GetTaskQueueImpl()));
  EXPECT_EQ(nullptr, manager_->currently_executing_task_queue());
}

namespace {
void RunloopCurrentlyExecutingTaskQueueTestTask(
    base::MessageLoop* message_loop,
    TaskQueueManagerImpl* task_queue_manager,
    std::vector<internal::TaskQueueImpl*>* task_sources,
    std::vector<std::pair<base::OnceClosure, TestTaskQueue*>>* tasks) {
  base::MessageLoop::ScopedNestableTaskAllower allow(message_loop);
  task_sources->push_back(task_queue_manager->currently_executing_task_queue());

  for (std::pair<base::OnceClosure, TestTaskQueue*>& pair : *tasks) {
    pair.second->PostTask(FROM_HERE, std::move(pair.first));
  }

  base::RunLoop().RunUntilIdle();
  task_sources->push_back(task_queue_manager->currently_executing_task_queue());
}
}  // namespace

TEST_F(TaskQueueManagerTest, CurrentlyExecutingTaskQueue_NestedLoop) {
  InitializeWithRealMessageLoop(3u);

  TestTaskQueue* queue0 = runners_[0].get();
  TestTaskQueue* queue1 = runners_[1].get();
  TestTaskQueue* queue2 = runners_[2].get();

  std::vector<internal::TaskQueueImpl*> task_sources;
  std::vector<std::pair<base::OnceClosure, TestTaskQueue*>>
      tasks_to_post_from_nested_loop;
  tasks_to_post_from_nested_loop.push_back(
      std::make_pair(base::BindOnce(&CurrentlyExecutingTaskQueueTestTask,
                                    manager_.get(), &task_sources),
                     queue1));
  tasks_to_post_from_nested_loop.push_back(
      std::make_pair(base::BindOnce(&CurrentlyExecutingTaskQueueTestTask,
                                    manager_.get(), &task_sources),
                     queue2));

  queue0->PostTask(
      FROM_HERE,
      base::BindOnce(&RunloopCurrentlyExecutingTaskQueueTestTask,
                     message_loop_.get(), manager_.get(), &task_sources,
                     &tasks_to_post_from_nested_loop));

  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(
      task_sources,
      ElementsAre(queue0->GetTaskQueueImpl(), queue1->GetTaskQueueImpl(),
                  queue2->GetTaskQueueImpl(), queue0->GetTaskQueueImpl()));
  EXPECT_EQ(nullptr, manager_->currently_executing_task_queue());
}

TEST_F(TaskQueueManagerTest, BlameContextAttribution) {
  using trace_analyzer::Query;

  InitializeWithRealMessageLoop(1u);
  TestTaskQueue* queue = runners_[0].get();

  trace_analyzer::Start("*");
  {
    base::trace_event::BlameContext blame_context("cat", "name", "type",
                                                  "scope", 0, nullptr);
    blame_context.Initialize();
    queue->SetBlameContext(&blame_context);
    queue->PostTask(FROM_HERE, base::BindOnce(&NopTask));
    base::RunLoop().RunUntilIdle();
  }
  auto analyzer = trace_analyzer::Stop();

  trace_analyzer::TraceEventVector events;
  Query q = Query::EventPhaseIs(TRACE_EVENT_PHASE_ENTER_CONTEXT) ||
            Query::EventPhaseIs(TRACE_EVENT_PHASE_LEAVE_CONTEXT);
  analyzer->FindEvents(q, &events);

  EXPECT_EQ(2u, events.size());
}

TEST_F(TaskQueueManagerTest, NoWakeUpsForCanceledDelayedTasks) {
  Initialize(1u);

  base::TimeTicks start_time = manager_->NowTicks();

  CancelableTask task1(&now_src_);
  CancelableTask task2(&now_src_);
  CancelableTask task3(&now_src_);
  CancelableTask task4(&now_src_);
  base::TimeDelta delay1(base::TimeDelta::FromSeconds(5));
  base::TimeDelta delay2(base::TimeDelta::FromSeconds(10));
  base::TimeDelta delay3(base::TimeDelta::FromSeconds(15));
  base::TimeDelta delay4(base::TimeDelta::FromSeconds(30));
  std::vector<base::TimeTicks> run_times;
  runners_[0]->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&CancelableTask::RecordTimeTask,
                     task1.weak_factory_.GetWeakPtr(), &run_times),
      delay1);
  runners_[0]->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&CancelableTask::RecordTimeTask,
                     task2.weak_factory_.GetWeakPtr(), &run_times),
      delay2);
  runners_[0]->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&CancelableTask::RecordTimeTask,
                     task3.weak_factory_.GetWeakPtr(), &run_times),
      delay3);
  runners_[0]->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&CancelableTask::RecordTimeTask,
                     task4.weak_factory_.GetWeakPtr(), &run_times),
      delay4);

  task2.weak_factory_.InvalidateWeakPtrs();
  task3.weak_factory_.InvalidateWeakPtrs();

  std::set<base::TimeTicks> wake_up_times;

  RunUntilIdle(base::BindRepeating(
      [](std::set<base::TimeTicks>* wake_up_times,
         base::SimpleTestTickClock* clock) {
        wake_up_times->insert(clock->NowTicks());
      },
      &wake_up_times, &now_src_));

  EXPECT_THAT(wake_up_times,
              ElementsAre(start_time + delay1, start_time + delay4));
  EXPECT_THAT(run_times, ElementsAre(start_time + delay1, start_time + delay4));
}

TEST_F(TaskQueueManagerTest, NoWakeUpsForCanceledDelayedTasksReversePostOrder) {
  Initialize(1u);

  base::TimeTicks start_time = manager_->NowTicks();

  CancelableTask task1(&now_src_);
  CancelableTask task2(&now_src_);
  CancelableTask task3(&now_src_);
  CancelableTask task4(&now_src_);
  base::TimeDelta delay1(base::TimeDelta::FromSeconds(5));
  base::TimeDelta delay2(base::TimeDelta::FromSeconds(10));
  base::TimeDelta delay3(base::TimeDelta::FromSeconds(15));
  base::TimeDelta delay4(base::TimeDelta::FromSeconds(30));
  std::vector<base::TimeTicks> run_times;
  runners_[0]->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&CancelableTask::RecordTimeTask,
                     task4.weak_factory_.GetWeakPtr(), &run_times),
      delay4);
  runners_[0]->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&CancelableTask::RecordTimeTask,
                     task3.weak_factory_.GetWeakPtr(), &run_times),
      delay3);
  runners_[0]->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&CancelableTask::RecordTimeTask,
                     task2.weak_factory_.GetWeakPtr(), &run_times),
      delay2);
  runners_[0]->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&CancelableTask::RecordTimeTask,
                     task1.weak_factory_.GetWeakPtr(), &run_times),
      delay1);

  task2.weak_factory_.InvalidateWeakPtrs();
  task3.weak_factory_.InvalidateWeakPtrs();

  std::set<base::TimeTicks> wake_up_times;

  RunUntilIdle(base::BindRepeating(
      [](std::set<base::TimeTicks>* wake_up_times,
         base::SimpleTestTickClock* clock) {
        wake_up_times->insert(clock->NowTicks());
      },
      &wake_up_times, &now_src_));

  EXPECT_THAT(wake_up_times,
              ElementsAre(start_time + delay1, start_time + delay4));
  EXPECT_THAT(run_times, ElementsAre(start_time + delay1, start_time + delay4));
}

TEST_F(TaskQueueManagerTest, TimeDomainWakeUpOnlyCancelledIfAllUsesCancelled) {
  Initialize(1u);

  base::TimeTicks start_time = manager_->NowTicks();

  CancelableTask task1(&now_src_);
  CancelableTask task2(&now_src_);
  CancelableTask task3(&now_src_);
  CancelableTask task4(&now_src_);
  base::TimeDelta delay1(base::TimeDelta::FromSeconds(5));
  base::TimeDelta delay2(base::TimeDelta::FromSeconds(10));
  base::TimeDelta delay3(base::TimeDelta::FromSeconds(15));
  base::TimeDelta delay4(base::TimeDelta::FromSeconds(30));
  std::vector<base::TimeTicks> run_times;
  runners_[0]->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&CancelableTask::RecordTimeTask,
                     task1.weak_factory_.GetWeakPtr(), &run_times),
      delay1);
  runners_[0]->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&CancelableTask::RecordTimeTask,
                     task2.weak_factory_.GetWeakPtr(), &run_times),
      delay2);
  runners_[0]->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&CancelableTask::RecordTimeTask,
                     task3.weak_factory_.GetWeakPtr(), &run_times),
      delay3);
  runners_[0]->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&CancelableTask::RecordTimeTask,
                     task4.weak_factory_.GetWeakPtr(), &run_times),
      delay4);

  // Post a non-canceled task with |delay3|. So we should still get a wake-up at
  // |delay3| even though we cancel |task3|.
  runners_[0]->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&CancelableTask::RecordTimeTask, base::Unretained(&task3),
                     &run_times),
      delay3);

  task2.weak_factory_.InvalidateWeakPtrs();
  task3.weak_factory_.InvalidateWeakPtrs();
  task1.weak_factory_.InvalidateWeakPtrs();

  std::set<base::TimeTicks> wake_up_times;

  RunUntilIdle(base::BindRepeating(
      [](std::set<base::TimeTicks>* wake_up_times,
         base::SimpleTestTickClock* clock) {
        wake_up_times->insert(clock->NowTicks());
      },
      &wake_up_times, &now_src_));

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

TEST_F(TaskQueueManagerTest, ShutdownQueueBeforeEnabledVoterDeleted) {
  Initialize(1u);

  scoped_refptr<TaskQueue> queue = CreateTaskQueue();

  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter =
      queue->CreateQueueEnabledVoter();

  voter->SetQueueEnabled(true);  // NOP
  queue->ShutdownTaskQueue();

  // This should complete without DCHECKing.
  voter.reset();
}

TEST_F(TaskQueueManagerTest, ShutdownQueueBeforeDisabledVoterDeleted) {
  Initialize(1u);

  scoped_refptr<TaskQueue> queue = CreateTaskQueue();

  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter =
      queue->CreateQueueEnabledVoter();

  voter->SetQueueEnabled(false);
  queue->ShutdownTaskQueue();

  // This should complete without DCHECKing.
  voter.reset();
}

TEST_F(TaskQueueManagerTest, SweepCanceledDelayedTasks) {
  Initialize(1u);

  CancelableTask task1(&now_src_);
  CancelableTask task2(&now_src_);
  CancelableTask task3(&now_src_);
  CancelableTask task4(&now_src_);
  base::TimeDelta delay1(base::TimeDelta::FromSeconds(5));
  base::TimeDelta delay2(base::TimeDelta::FromSeconds(10));
  base::TimeDelta delay3(base::TimeDelta::FromSeconds(15));
  base::TimeDelta delay4(base::TimeDelta::FromSeconds(30));
  std::vector<base::TimeTicks> run_times;
  runners_[0]->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&CancelableTask::RecordTimeTask,
                     task1.weak_factory_.GetWeakPtr(), &run_times),
      delay1);
  runners_[0]->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&CancelableTask::RecordTimeTask,
                     task2.weak_factory_.GetWeakPtr(), &run_times),
      delay2);
  runners_[0]->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&CancelableTask::RecordTimeTask,
                     task3.weak_factory_.GetWeakPtr(), &run_times),
      delay3);
  runners_[0]->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&CancelableTask::RecordTimeTask,
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

TEST_F(TaskQueueManagerTest, DelayTillNextTask) {
  Initialize(2u);

  LazyNow lazy_now(&now_src_);
  EXPECT_EQ(base::TimeDelta::Max(), manager_->DelayTillNextTask(&lazy_now));

  runners_[0]->PostDelayedTask(FROM_HERE, base::BindOnce(&NopTask),
                               base::TimeDelta::FromSeconds(10));

  EXPECT_EQ(base::TimeDelta::FromSeconds(10),
            manager_->DelayTillNextTask(&lazy_now));

  runners_[1]->PostDelayedTask(FROM_HERE, base::BindOnce(&NopTask),
                               base::TimeDelta::FromSeconds(15));

  EXPECT_EQ(base::TimeDelta::FromSeconds(10),
            manager_->DelayTillNextTask(&lazy_now));

  runners_[1]->PostDelayedTask(FROM_HERE, base::BindOnce(&NopTask),
                               base::TimeDelta::FromSeconds(5));

  EXPECT_EQ(base::TimeDelta::FromSeconds(5),
            manager_->DelayTillNextTask(&lazy_now));

  runners_[0]->PostTask(FROM_HERE, base::BindOnce(&NopTask));

  EXPECT_EQ(base::TimeDelta(), manager_->DelayTillNextTask(&lazy_now));
}

TEST_F(TaskQueueManagerTest, DelayTillNextTask_Disabled) {
  Initialize(1u);

  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter =
      runners_[0]->CreateQueueEnabledVoter();
  voter->SetQueueEnabled(false);
  runners_[0]->PostTask(FROM_HERE, base::BindOnce(&NopTask));

  LazyNow lazy_now(&now_src_);
  EXPECT_EQ(base::TimeDelta::Max(), manager_->DelayTillNextTask(&lazy_now));
}

TEST_F(TaskQueueManagerTest, DelayTillNextTask_Fence) {
  Initialize(1u);

  runners_[0]->InsertFence(TaskQueue::InsertFencePosition::kNow);
  runners_[0]->PostTask(FROM_HERE, base::BindOnce(&NopTask));

  LazyNow lazy_now(&now_src_);
  EXPECT_EQ(base::TimeDelta::Max(), manager_->DelayTillNextTask(&lazy_now));
}

TEST_F(TaskQueueManagerTest, DelayTillNextTask_FenceUnblocking) {
  Initialize(1u);

  runners_[0]->InsertFence(TaskQueue::InsertFencePosition::kNow);
  runners_[0]->PostTask(FROM_HERE, base::BindOnce(&NopTask));
  runners_[0]->InsertFence(TaskQueue::InsertFencePosition::kNow);

  LazyNow lazy_now(&now_src_);
  EXPECT_EQ(base::TimeDelta(), manager_->DelayTillNextTask(&lazy_now));
}

TEST_F(TaskQueueManagerTest, DelayTillNextTask_DelayedTaskReady) {
  Initialize(1u);

  runners_[0]->PostDelayedTask(FROM_HERE, base::BindOnce(&NopTask),
                               base::TimeDelta::FromSeconds(1));

  now_src_.Advance(base::TimeDelta::FromSeconds(10));

  LazyNow lazy_now(&now_src_);
  EXPECT_EQ(base::TimeDelta(), manager_->DelayTillNextTask(&lazy_now));
}

namespace {
void MessageLoopTaskWithDelayedQuit(base::MessageLoop* message_loop,
                                    base::SimpleTestTickClock* now_src,
                                    scoped_refptr<TaskQueue> task_queue) {
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
      base::BindOnce(&MessageLoopTaskWithDelayedQuit, message_loop_.get(),
                     &now_src_, base::RetainedRef(runners_[0])));
  run_loop.RunUntilIdle();
}

namespace {
void MessageLoopTaskWithImmediateQuit(base::MessageLoop* message_loop,
                                      base::OnceClosure non_nested_quit_closure,
                                      scoped_refptr<TaskQueue> task_queue) {
  base::MessageLoop::ScopedNestableTaskAllower allow(message_loop);

  base::RunLoop run_loop;
  // Needed because entering the nested run loop causes a DoWork to get
  // posted.
  task_queue->PostTask(FROM_HERE, base::BindOnce(&NopTask));
  task_queue->PostTask(FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
  std::move(non_nested_quit_closure).Run();
}
}  // namespace

TEST_F(TaskQueueManagerTest,
       DelayedNestedMessageLoopDoesntPreventTasksRunning) {
  InitializeWithRealMessageLoop(1u);
  base::RunLoop run_loop;
  runners_[0]->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&MessageLoopTaskWithImmediateQuit, message_loop_.get(),
                     run_loop.QuitClosure(), base::RetainedRef(runners_[0])),
      base::TimeDelta::FromMilliseconds(100));

  now_src_.Advance(base::TimeDelta::FromMilliseconds(200));
  run_loop.Run();
}

TEST_F(TaskQueueManagerTest, CouldTaskRun_DisableAndReenable) {
  Initialize(1u);

  EnqueueOrder enqueue_order = GetNextSequenceNumber();
  EXPECT_TRUE(runners_[0]->GetTaskQueueImpl()->CouldTaskRun(enqueue_order));

  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter =
      runners_[0]->CreateQueueEnabledVoter();
  voter->SetQueueEnabled(false);
  EXPECT_FALSE(runners_[0]->GetTaskQueueImpl()->CouldTaskRun(enqueue_order));

  voter->SetQueueEnabled(true);
  EXPECT_TRUE(runners_[0]->GetTaskQueueImpl()->CouldTaskRun(enqueue_order));
}

TEST_F(TaskQueueManagerTest, CouldTaskRun_Fence) {
  Initialize(1u);

  EnqueueOrder enqueue_order = GetNextSequenceNumber();
  EXPECT_TRUE(runners_[0]->GetTaskQueueImpl()->CouldTaskRun(enqueue_order));

  runners_[0]->InsertFence(TaskQueue::InsertFencePosition::kNow);
  EXPECT_TRUE(runners_[0]->GetTaskQueueImpl()->CouldTaskRun(enqueue_order));

  runners_[0]->InsertFence(TaskQueue::InsertFencePosition::kBeginningOfTime);
  EXPECT_FALSE(runners_[0]->GetTaskQueueImpl()->CouldTaskRun(enqueue_order));

  runners_[0]->RemoveFence();
  EXPECT_TRUE(runners_[0]->GetTaskQueueImpl()->CouldTaskRun(enqueue_order));
}

TEST_F(TaskQueueManagerTest, CouldTaskRun_FenceBeforeThenAfter) {
  Initialize(1u);

  runners_[0]->InsertFence(TaskQueue::InsertFencePosition::kNow);

  EnqueueOrder enqueue_order = GetNextSequenceNumber();
  EXPECT_FALSE(runners_[0]->GetTaskQueueImpl()->CouldTaskRun(enqueue_order));

  runners_[0]->InsertFence(TaskQueue::InsertFencePosition::kNow);
  EXPECT_TRUE(runners_[0]->GetTaskQueueImpl()->CouldTaskRun(enqueue_order));
}

TEST_F(TaskQueueManagerTest, DelayedDoWorkNotPostedForDisabledQueue) {
  Initialize(1u);

  runners_[0]->PostDelayedTask(FROM_HERE, base::BindOnce(&NopTask),
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
  runners_[0]->PostDelayedTask(FROM_HERE, base::BindOnce(&NopTask),
                               base::TimeDelta::FromMilliseconds(1));
  runners_[1]->PostDelayedTask(FROM_HERE, base::BindOnce(&NopTask),
                               base::TimeDelta::FromMilliseconds(10));
  runners_[2]->PostDelayedTask(FROM_HERE, base::BindOnce(&NopTask),
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

  base::TimeTicks start_time = manager_->NowTicks();
  base::TimeDelta delay1 = base::TimeDelta::FromMilliseconds(10);
  base::TimeDelta delay2 = base::TimeDelta::FromMilliseconds(2);

  runners_[0]->PostDelayedTask(FROM_HERE, base::BindOnce(&NopTask), delay1);
  EXPECT_EQ(start_time + delay1, runners_[0]->GetNextScheduledWakeUp());

  runners_[0]->PostDelayedTask(FROM_HERE, base::BindOnce(&NopTask), delay2);
  EXPECT_EQ(start_time + delay2, runners_[0]->GetNextScheduledWakeUp());

  // We don't have wake-ups scheduled for disabled queues.
  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter =
      runners_[0]->CreateQueueEnabledVoter();
  voter->SetQueueEnabled(false);
  EXPECT_EQ(base::nullopt, runners_[0]->GetNextScheduledWakeUp());

  voter->SetQueueEnabled(true);
  EXPECT_EQ(start_time + delay2, runners_[0]->GetNextScheduledWakeUp());

  // Immediate tasks shouldn't make any difference.
  runners_[0]->PostTask(FROM_HERE, base::BindOnce(&NopTask));
  EXPECT_EQ(start_time + delay2, runners_[0]->GetNextScheduledWakeUp());

  // Neither should fences.
  runners_[0]->InsertFence(TaskQueue::InsertFencePosition::kBeginningOfTime);
  EXPECT_EQ(start_time + delay2, runners_[0]->GetNextScheduledWakeUp());
}

TEST_F(TaskQueueManagerTest, SetTimeDomainForDisabledQueue) {
  Initialize(1u);

  MockTaskQueueObserver observer;
  runners_[0]->SetObserver(&observer);

  runners_[0]->PostDelayedTask(FROM_HERE, base::BindOnce(&NopTask),
                               base::TimeDelta::FromMilliseconds(1));

  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter =
      runners_[0]->CreateQueueEnabledVoter();
  voter->SetQueueEnabled(false);

  // We should not get a notification for a disabled queue.
  EXPECT_CALL(observer, OnQueueNextWakeUpChanged(_, _)).Times(0);

  std::unique_ptr<VirtualTimeDomain> domain(
      new VirtualTimeDomain(manager_->NowTicks()));
  manager_->RegisterTimeDomain(domain.get());
  runners_[0]->SetTimeDomain(domain.get());

  // Tidy up.
  runners_[0]->ShutdownTaskQueue();
  manager_->UnregisterTimeDomain(domain.get());
}

namespace {
void SetOnTaskHandlers(scoped_refptr<TestTaskQueue> task_queue,
                       int* start_counter,
                       int* complete_counter) {
  task_queue->GetTaskQueueImpl()->SetOnTaskStartedHandler(
      base::BindRepeating([](int* counter, const TaskQueue::Task& task,
                             base::TimeTicks start) { ++(*counter); },
                          start_counter));
  task_queue->GetTaskQueueImpl()->SetOnTaskCompletedHandler(base::BindRepeating(
      [](int* counter, const TaskQueue::Task& task, base::TimeTicks start,
         base::TimeTicks end,
         base::Optional<base::TimeDelta> thread_time) { ++(*counter); },
      complete_counter));
}

void UnsetOnTaskHandlers(scoped_refptr<TestTaskQueue> task_queue) {
  task_queue->GetTaskQueueImpl()->SetOnTaskStartedHandler(
      base::RepeatingCallback<void(const TaskQueue::Task& task,
                                   base::TimeTicks start)>());
  task_queue->GetTaskQueueImpl()->SetOnTaskCompletedHandler(
      base::RepeatingCallback<void(
          const TaskQueue::Task& task, base::TimeTicks start,
          base::TimeTicks end, base::Optional<base::TimeDelta> thread_time)>());
}
}  // namespace

TEST_F(TaskQueueManagerTest, ProcessTasksWithoutTaskTimeObservers) {
  Initialize(1u);
  int start_counter = 0;
  int complete_counter = 0;
  std::vector<EnqueueOrder> run_order;
  SetOnTaskHandlers(runners_[0], &start_counter, &complete_counter);
  EXPECT_TRUE(runners_[0]->GetTaskQueueImpl()->RequiresTaskTiming());
  runners_[0]->PostTask(FROM_HERE, base::BindOnce(&TestTask, 1, &run_order));
  runners_[0]->PostTask(FROM_HERE, base::BindOnce(&TestTask, 2, &run_order));
  runners_[0]->PostTask(FROM_HERE, base::BindOnce(&TestTask, 3, &run_order));
  test_task_runner_->RunUntilIdle();
  EXPECT_EQ(start_counter, 3);
  EXPECT_EQ(complete_counter, 3);
  EXPECT_THAT(run_order, ElementsAre(1, 2, 3));

  UnsetOnTaskHandlers(runners_[0]);
  EXPECT_FALSE(runners_[0]->GetTaskQueueImpl()->RequiresTaskTiming());
  runners_[0]->PostTask(FROM_HERE, base::BindOnce(&TestTask, 4, &run_order));
  runners_[0]->PostTask(FROM_HERE, base::BindOnce(&TestTask, 5, &run_order));
  runners_[0]->PostTask(FROM_HERE, base::BindOnce(&TestTask, 6, &run_order));
  test_task_runner_->RunUntilIdle();
  EXPECT_EQ(start_counter, 3);
  EXPECT_EQ(complete_counter, 3);
  EXPECT_THAT(run_order, ElementsAre(1, 2, 3, 4, 5, 6));
}

TEST_F(TaskQueueManagerTest, ProcessTasksWithTaskTimeObservers) {
  Initialize(1u);
  int start_counter = 0;
  int complete_counter = 0;

  manager_->AddTaskTimeObserver(&test_task_time_observer_);
  SetOnTaskHandlers(runners_[0], &start_counter, &complete_counter);
  EXPECT_TRUE(runners_[0]->GetTaskQueueImpl()->RequiresTaskTiming());
  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostTask(FROM_HERE, base::BindOnce(&TestTask, 1, &run_order));
  runners_[0]->PostTask(FROM_HERE, base::BindOnce(&TestTask, 2, &run_order));
  test_task_runner_->RunUntilIdle();
  EXPECT_EQ(start_counter, 2);
  EXPECT_EQ(complete_counter, 2);
  EXPECT_THAT(run_order, ElementsAre(1, 2));

  UnsetOnTaskHandlers(runners_[0]);
  EXPECT_FALSE(runners_[0]->GetTaskQueueImpl()->RequiresTaskTiming());
  runners_[0]->PostTask(FROM_HERE, base::BindOnce(&TestTask, 3, &run_order));
  runners_[0]->PostTask(FROM_HERE, base::BindOnce(&TestTask, 4, &run_order));
  test_task_runner_->RunUntilIdle();
  EXPECT_EQ(start_counter, 2);
  EXPECT_EQ(complete_counter, 2);
  EXPECT_THAT(run_order, ElementsAre(1, 2, 3, 4));

  manager_->RemoveTaskTimeObserver(&test_task_time_observer_);
  runners_[0]->PostTask(FROM_HERE, base::BindOnce(&TestTask, 5, &run_order));
  runners_[0]->PostTask(FROM_HERE, base::BindOnce(&TestTask, 6, &run_order));
  test_task_runner_->RunUntilIdle();
  EXPECT_EQ(start_counter, 2);
  EXPECT_EQ(complete_counter, 2);
  EXPECT_FALSE(runners_[0]->GetTaskQueueImpl()->RequiresTaskTiming());
  EXPECT_THAT(run_order, ElementsAre(1, 2, 3, 4, 5, 6));

  SetOnTaskHandlers(runners_[0], &start_counter, &complete_counter);
  runners_[0]->PostTask(FROM_HERE, base::BindOnce(&TestTask, 7, &run_order));
  runners_[0]->PostTask(FROM_HERE, base::BindOnce(&TestTask, 8, &run_order));
  test_task_runner_->RunUntilIdle();
  EXPECT_EQ(start_counter, 4);
  EXPECT_EQ(complete_counter, 4);
  EXPECT_TRUE(runners_[0]->GetTaskQueueImpl()->RequiresTaskTiming());
  EXPECT_THAT(run_order, ElementsAre(1, 2, 3, 4, 5, 6, 7, 8));
  UnsetOnTaskHandlers(runners_[0]);
}

TEST_F(TaskQueueManagerTest, GracefulShutdown) {
  Initialize(0u);
  test_task_runner_->SetAutoAdvanceNowToPendingTasks(true);

  std::vector<base::TimeTicks> run_times;
  scoped_refptr<TestTaskQueue> main_tq = CreateTaskQueue();
  base::WeakPtr<TestTaskQueue> main_tq_weak_ptr = main_tq->GetWeakPtr();

  EXPECT_EQ(1u, manager_->ActiveQueuesCount());
  EXPECT_EQ(0u, manager_->QueuesToShutdownCount());
  EXPECT_EQ(0u, manager_->QueuesToDeleteCount());

  for (int i = 1; i <= 5; ++i) {
    main_tq->PostDelayedTask(
        FROM_HERE, base::BindOnce(&RecordTimeTask, &run_times, &now_src_),
        base::TimeDelta::FromMilliseconds(i * 100));
  }
  test_task_runner_->RunForPeriod(base::TimeDelta::FromMilliseconds(250));

  main_tq = nullptr;
  // Ensure that task queue went away.
  EXPECT_FALSE(main_tq_weak_ptr.get());

  test_task_runner_->RunForPeriod(base::TimeDelta::FromMilliseconds(1));

  EXPECT_EQ(1u, manager_->ActiveQueuesCount());
  EXPECT_EQ(1u, manager_->QueuesToShutdownCount());
  EXPECT_EQ(0u, manager_->QueuesToDeleteCount());

  test_task_runner_->RunUntilIdle();

  // Even with TaskQueue gone, tasks are executed.
  EXPECT_THAT(
      run_times,
      ElementsAre(base::TimeTicks() + base::TimeDelta::FromMilliseconds(101),
                  base::TimeTicks() + base::TimeDelta::FromMilliseconds(201),
                  base::TimeTicks() + base::TimeDelta::FromMilliseconds(301),
                  base::TimeTicks() + base::TimeDelta::FromMilliseconds(401),
                  base::TimeTicks() + base::TimeDelta::FromMilliseconds(501)));

  EXPECT_EQ(0u, manager_->ActiveQueuesCount());
  EXPECT_EQ(0u, manager_->QueuesToShutdownCount());
  EXPECT_EQ(0u, manager_->QueuesToDeleteCount());
}

TEST_F(TaskQueueManagerTest, GracefulShutdown_ManagerDeletedInFlight) {
  Initialize(0u);
  test_task_runner_->SetAutoAdvanceNowToPendingTasks(true);

  std::vector<base::TimeTicks> run_times;
  scoped_refptr<TestTaskQueue> control_tq = CreateTaskQueue();
  std::vector<scoped_refptr<TestTaskQueue>> main_tqs;
  std::vector<base::WeakPtr<TestTaskQueue>> main_tq_weak_ptrs;

  // There might be a race condition - async task queues should be unregistered
  // first. Increase the number of task queues to surely detect that.
  // The problem is that pointers are compared in a set and generally for
  // a small number of allocations value of the pointers increases
  // monotonically. 100 is large enough to force allocations from different
  // pages.
  const int N = 100;
  for (int i = 0; i < N; ++i) {
    scoped_refptr<TestTaskQueue> tq = CreateTaskQueue();
    main_tq_weak_ptrs.push_back(tq->GetWeakPtr());
    main_tqs.push_back(std::move(tq));
  }

  for (int i = 1; i <= 5; ++i) {
    main_tqs[0]->PostDelayedTask(
        FROM_HERE, base::BindOnce(&RecordTimeTask, &run_times, &now_src_),
        base::TimeDelta::FromMilliseconds(i * 100));
  }
  test_task_runner_->RunForPeriod(base::TimeDelta::FromMilliseconds(250));

  main_tqs.clear();
  // Ensure that task queues went away.
  for (int i = 0; i < N; ++i) {
    EXPECT_FALSE(main_tq_weak_ptrs[i].get());
  }

  // No leaks should occur when TQM was destroyed before processing
  // shutdown task and TaskQueueImpl should be safely deleted on a correct
  // thread.
  manager_.reset();

  test_task_runner_->RunUntilIdle();

  EXPECT_THAT(
      run_times,
      ElementsAre(base::TimeTicks() + base::TimeDelta::FromMilliseconds(101),
                  base::TimeTicks() + base::TimeDelta::FromMilliseconds(201)));
}

TEST_F(TaskQueueManagerTest,
       GracefulShutdown_ManagerDeletedWithQueuesToShutdown) {
  Initialize(0u);
  test_task_runner_->SetAutoAdvanceNowToPendingTasks(true);

  std::vector<base::TimeTicks> run_times;
  scoped_refptr<TestTaskQueue> main_tq = CreateTaskQueue();
  base::WeakPtr<TestTaskQueue> main_tq_weak_ptr = main_tq->GetWeakPtr();

  EXPECT_EQ(1u, manager_->ActiveQueuesCount());
  EXPECT_EQ(0u, manager_->QueuesToShutdownCount());
  EXPECT_EQ(0u, manager_->QueuesToDeleteCount());

  for (int i = 1; i <= 5; ++i) {
    main_tq->PostDelayedTask(
        FROM_HERE, base::BindOnce(&RecordTimeTask, &run_times, &now_src_),
        base::TimeDelta::FromMilliseconds(i * 100));
  }
  test_task_runner_->RunForPeriod(base::TimeDelta::FromMilliseconds(250));

  main_tq = nullptr;
  // Ensure that task queue went away.
  EXPECT_FALSE(main_tq_weak_ptr.get());

  test_task_runner_->RunForPeriod(base::TimeDelta::FromMilliseconds(1));

  EXPECT_EQ(1u, manager_->ActiveQueuesCount());
  EXPECT_EQ(1u, manager_->QueuesToShutdownCount());
  EXPECT_EQ(0u, manager_->QueuesToDeleteCount());

  // Ensure that all queues-to-gracefully-shutdown are properly unregistered.
  manager_.reset();

  test_task_runner_->RunUntilIdle();

  EXPECT_THAT(
      run_times,
      ElementsAre(base::TimeTicks() + base::TimeDelta::FromMilliseconds(101),
                  base::TimeTicks() + base::TimeDelta::FromMilliseconds(201)));
}

TEST_F(TaskQueueManagerTest, DefaultTaskRunnerSupport) {
  base::MessageLoop message_loop;
  scoped_refptr<base::SingleThreadTaskRunner> original_task_runner =
      message_loop.task_runner();
  scoped_refptr<base::SingleThreadTaskRunner> custom_task_runner =
      base::MakeRefCounted<base::TestSimpleTaskRunner>();
  {
    std::unique_ptr<TaskQueueManagerForTest> manager =
        TaskQueueManagerForTest::Create(&message_loop,
                                        message_loop.task_runner(), nullptr);
    manager->SetDefaultTaskRunner(custom_task_runner);
    DCHECK_EQ(custom_task_runner, message_loop.task_runner());
  }
  DCHECK_EQ(original_task_runner, message_loop.task_runner());
}

TEST_F(TaskQueueManagerTest, CanceledTasksInQueueCantMakeOtherTasksSkipAhead) {
  Initialize(2u);

  CancelableTask task1(&now_src_);
  CancelableTask task2(&now_src_);
  std::vector<base::TimeTicks> run_times;

  runners_[0]->PostTask(
      FROM_HERE, base::BindOnce(&CancelableTask::RecordTimeTask,
                                task1.weak_factory_.GetWeakPtr(), &run_times));
  runners_[0]->PostTask(
      FROM_HERE, base::BindOnce(&CancelableTask::RecordTimeTask,
                                task2.weak_factory_.GetWeakPtr(), &run_times));

  std::vector<EnqueueOrder> run_order;
  runners_[1]->PostTask(FROM_HERE, base::BindOnce(&TestTask, 1, &run_order));

  runners_[0]->PostTask(FROM_HERE, base::BindOnce(&TestTask, 2, &run_order));

  task1.weak_factory_.InvalidateWeakPtrs();
  task2.weak_factory_.InvalidateWeakPtrs();
  test_task_runner_->RunUntilIdle();

  EXPECT_THAT(run_order, ElementsAre(1, 2));
}

TEST_F(TaskQueueManagerTest, TaskQueueDeletedOnAnotherThread) {
  Initialize(0u);
  test_task_runner_->SetAutoAdvanceNowToPendingTasks(true);

  std::vector<base::TimeTicks> run_times;
  scoped_refptr<TestTaskQueue> main_tq = CreateTaskQueue();

  int start_counter = 0;
  int complete_counter = 0;
  SetOnTaskHandlers(main_tq, &start_counter, &complete_counter);

  EXPECT_EQ(1u, manager_->ActiveQueuesCount());
  EXPECT_EQ(0u, manager_->QueuesToShutdownCount());
  EXPECT_EQ(0u, manager_->QueuesToDeleteCount());

  for (int i = 1; i <= 5; ++i) {
    main_tq->PostDelayedTask(
        FROM_HERE, base::BindOnce(&RecordTimeTask, &run_times, &now_src_),
        base::TimeDelta::FromMilliseconds(i * 100));
  }

  // TODO(altimin): do not do this after switching to weak pointer-based
  // task handlers.
  UnsetOnTaskHandlers(main_tq);

  base::WaitableEvent task_queue_deleted(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  std::unique_ptr<base::Thread> thread =
      std::make_unique<base::Thread>("test thread");
  thread->StartAndWaitForTesting();

  thread->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](scoped_refptr<base::SingleThreadTaskRunner> task_queue,
                        base::WaitableEvent* task_queue_deleted) {
                       task_queue = nullptr;
                       task_queue_deleted->Signal();
                     },
                     std::move(main_tq), &task_queue_deleted));
  task_queue_deleted.Wait();

  EXPECT_EQ(1u, manager_->ActiveQueuesCount());
  EXPECT_EQ(1u, manager_->QueuesToShutdownCount());
  EXPECT_EQ(0u, manager_->QueuesToDeleteCount());

  test_task_runner_->RunUntilIdle();

  // Even with TaskQueue gone, tasks are executed.
  EXPECT_THAT(
      run_times,
      ElementsAre(base::TimeTicks() + base::TimeDelta::FromMilliseconds(101),
                  base::TimeTicks() + base::TimeDelta::FromMilliseconds(201),
                  base::TimeTicks() + base::TimeDelta::FromMilliseconds(301),
                  base::TimeTicks() + base::TimeDelta::FromMilliseconds(401),
                  base::TimeTicks() + base::TimeDelta::FromMilliseconds(501)));

  EXPECT_EQ(0u, manager_->ActiveQueuesCount());
  EXPECT_EQ(0u, manager_->QueuesToShutdownCount());
  EXPECT_EQ(0u, manager_->QueuesToDeleteCount());

  thread->Stop();
}

namespace {

void DoNothing() {}

class PostTaskInDestructor {
 public:
  explicit PostTaskInDestructor(scoped_refptr<TaskQueue> task_queue)
      : task_queue_(task_queue) {}

  ~PostTaskInDestructor() {
    task_queue_->PostTask(FROM_HERE, base::BindOnce(&DoNothing));
  }

  void Do() {}

 private:
  scoped_refptr<TaskQueue> task_queue_;
};

}  // namespace

TEST_F(TaskQueueManagerTest, TaskQueueUsedInTaskDestructorAfterShutdown) {
  // This test checks that when a task is posted to a shutdown queue and
  // destroyed, it can try to post a task to the same queue without deadlocks.
  Initialize(0u);
  test_task_runner_->SetAutoAdvanceNowToPendingTasks(true);

  scoped_refptr<TestTaskQueue> main_tq = CreateTaskQueue();

  base::WaitableEvent test_executed(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  std::unique_ptr<base::Thread> thread =
      std::make_unique<base::Thread>("test thread");
  thread->StartAndWaitForTesting();

  manager_.reset();

  thread->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](scoped_refptr<base::SingleThreadTaskRunner> task_queue,
                        std::unique_ptr<PostTaskInDestructor> test_object,
                        base::WaitableEvent* test_executed) {
                       task_queue->PostTask(
                           FROM_HERE, base::BindOnce(&PostTaskInDestructor::Do,
                                                     std::move(test_object)));
                       test_executed->Signal();
                     },
                     main_tq, std::make_unique<PostTaskInDestructor>(main_tq),
                     &test_executed));
  test_executed.Wait();
}

}  // namespace scheduler
}  // namespace blink
