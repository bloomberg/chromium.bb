// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/base/task_queue_selector.h"

#include <stddef.h>

#include <memory>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/pending_task.h"
#include "platform/scheduler/base/task_queue_impl.h"
#include "platform/scheduler/base/virtual_time_domain.h"
#include "platform/scheduler/base/work_queue.h"
#include "platform/scheduler/base/work_queue_sets.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace blink {
namespace scheduler {
namespace internal {
// To avoid symbol collisions in jumbo builds.
namespace task_queue_selector_unittest {

class MockObserver : public TaskQueueSelector::Observer {
 public:
  MockObserver() {}
  virtual ~MockObserver() {}

  MOCK_METHOD1(OnTaskQueueEnabled, void(internal::TaskQueueImpl*));
  MOCK_METHOD1(OnTriedToSelectBlockedWorkQueue, void(internal::WorkQueue*));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockObserver);
};

class TaskQueueSelectorForTest : public TaskQueueSelector {
 public:
  using TaskQueueSelector::SetImmediateStarvationCountForTest;
  using TaskQueueSelector::PrioritizingSelector;
  using TaskQueueSelector::enabled_selector_for_test;
};

class TaskQueueSelectorTest : public ::testing::Test {
 public:
  TaskQueueSelectorTest()
      : test_closure_(base::Bind(&TaskQueueSelectorTest::TestFunction)) {}
  ~TaskQueueSelectorTest() override {}

  TaskQueueSelectorForTest::PrioritizingSelector* enabled_selector() {
    return selector_.enabled_selector_for_test();
  }

  WorkQueueSets* delayed_work_queue_sets() {
    return enabled_selector()->delayed_work_queue_sets();
  }
  WorkQueueSets* immediate_work_queue_sets() {
    return enabled_selector()->immediate_work_queue_sets();
  }

  void PushTasks(const size_t queue_indices[], size_t num_tasks) {
    std::set<size_t> changed_queue_set;
    for (size_t i = 0; i < num_tasks; i++) {
      changed_queue_set.insert(queue_indices[i]);
      task_queues_[queue_indices[i]]->immediate_work_queue()->Push(
          TaskQueueImpl::Task(TaskQueue::PostedTask(test_closure_, FROM_HERE),
                              base::TimeTicks(), 0, i));
    }
  }

  void PushTasksWithEnqueueOrder(const size_t queue_indices[],
                                 const size_t enqueue_orders[],
                                 size_t num_tasks) {
    std::set<size_t> changed_queue_set;
    for (size_t i = 0; i < num_tasks; i++) {
      changed_queue_set.insert(queue_indices[i]);
      task_queues_[queue_indices[i]]->immediate_work_queue()->Push(
          TaskQueueImpl::Task(TaskQueue::PostedTask(test_closure_, FROM_HERE),
                              base::TimeTicks(), 0, enqueue_orders[i]));
    }
  }

  std::vector<size_t> PopTasks() {
    std::vector<size_t> order;
    WorkQueue* chosen_work_queue;
    while (selector_.SelectWorkQueueToService(&chosen_work_queue)) {
      size_t chosen_queue_index =
          queue_to_index_map_.find(chosen_work_queue->task_queue())->second;
      order.push_back(chosen_queue_index);
      chosen_work_queue->PopTaskForTesting();
      immediate_work_queue_sets()->OnPopQueue(chosen_work_queue);
    }
    return order;
  }

  static void TestFunction() {}

 protected:
  void SetUp() final {
    virtual_time_domain_ = base::WrapUnique<VirtualTimeDomain>(
        new VirtualTimeDomain(base::TimeTicks()));
    for (size_t i = 0; i < kTaskQueueCount; i++) {
      std::unique_ptr<TaskQueueImpl> task_queue =
          std::make_unique<TaskQueueImpl>(nullptr, virtual_time_domain_.get(),
                                          TaskQueue::Spec("test"));
      selector_.AddQueue(task_queue.get());
      task_queues_.push_back(std::move(task_queue));
    }
    for (size_t i = 0; i < kTaskQueueCount; i++) {
      EXPECT_EQ(TaskQueue::kNormalPriority, task_queues_[i]->GetQueuePriority())
          << i;
      queue_to_index_map_.insert(std::make_pair(task_queues_[i].get(), i));
    }
  }

  void TearDown() final {
    for (std::unique_ptr<TaskQueueImpl>& task_queue : task_queues_) {
      // Note since this test doesn't have a TaskQueueManager we need to
      // manually remove |task_queue| from the |selector_|.  Normally
      // UnregisterTaskQueue would do that.
      selector_.RemoveQueue(task_queue.get());
      task_queue->UnregisterTaskQueue();
    }
  }

  std::unique_ptr<TaskQueueImpl> NewTaskQueueWithBlockReporting() {
    return std::make_unique<TaskQueueImpl>(
        nullptr, virtual_time_domain_.get(),
        TaskQueue::Spec("test").SetShouldReportWhenExecutionBlocked(true));
  }

  const size_t kTaskQueueCount = 5;
  base::Closure test_closure_;
  TaskQueueSelectorForTest selector_;
  std::unique_ptr<VirtualTimeDomain> virtual_time_domain_;
  std::vector<std::unique_ptr<TaskQueueImpl>> task_queues_;
  std::map<TaskQueueImpl*, size_t> queue_to_index_map_;
};

TEST_F(TaskQueueSelectorTest, TestDefaultPriority) {
  size_t queue_order[] = {4, 3, 2, 1, 0};
  PushTasks(queue_order, 5);
  EXPECT_THAT(PopTasks(), ::testing::ElementsAre(4, 3, 2, 1, 0));
}

TEST_F(TaskQueueSelectorTest, TestHighPriority) {
  size_t queue_order[] = {0, 1, 2, 3, 4};
  PushTasks(queue_order, 5);
  selector_.SetQueuePriority(task_queues_[2].get(), TaskQueue::kHighPriority);
  EXPECT_THAT(PopTasks(), ::testing::ElementsAre(2, 0, 1, 3, 4));
}

TEST_F(TaskQueueSelectorTest, TestLowPriority) {
  size_t queue_order[] = {0, 1, 2, 3, 4};
  PushTasks(queue_order, 5);
  selector_.SetQueuePriority(task_queues_[2].get(), TaskQueue::kLowPriority);
  EXPECT_THAT(PopTasks(), ::testing::ElementsAre(0, 1, 3, 4, 2));
}

TEST_F(TaskQueueSelectorTest, TestBestEffortPriority) {
  size_t queue_order[] = {0, 1, 2, 3, 4};
  PushTasks(queue_order, 5);
  selector_.SetQueuePriority(task_queues_[0].get(),
                             TaskQueue::kBestEffortPriority);
  selector_.SetQueuePriority(task_queues_[2].get(), TaskQueue::kLowPriority);
  selector_.SetQueuePriority(task_queues_[3].get(), TaskQueue::kHighPriority);
  EXPECT_THAT(PopTasks(), ::testing::ElementsAre(3, 1, 4, 2, 0));
}

TEST_F(TaskQueueSelectorTest, TestControlPriority) {
  size_t queue_order[] = {0, 1, 2, 3, 4};
  PushTasks(queue_order, 5);
  selector_.SetQueuePriority(task_queues_[4].get(),
                             TaskQueue::kControlPriority);
  EXPECT_EQ(TaskQueue::kControlPriority, task_queues_[4]->GetQueuePriority());
  selector_.SetQueuePriority(task_queues_[2].get(), TaskQueue::kHighPriority);
  EXPECT_EQ(TaskQueue::kHighPriority, task_queues_[2]->GetQueuePriority());
  EXPECT_THAT(PopTasks(), ::testing::ElementsAre(4, 2, 0, 1, 3));
}

TEST_F(TaskQueueSelectorTest, TestObserverWithEnabledQueue) {
  task_queues_[1]->SetQueueEnabledForTest(false);
  selector_.DisableQueue(task_queues_[1].get());
  MockObserver mock_observer;
  selector_.SetTaskQueueSelectorObserver(&mock_observer);
  EXPECT_CALL(mock_observer, OnTaskQueueEnabled(_)).Times(1);
  task_queues_[1]->SetQueueEnabledForTest(true);
  selector_.EnableQueue(task_queues_[1].get());
}

TEST_F(TaskQueueSelectorTest,
       TestObserverWithSetQueuePriorityAndQueueAlreadyEnabled) {
  selector_.SetQueuePriority(task_queues_[1].get(), TaskQueue::kHighPriority);
  MockObserver mock_observer;
  selector_.SetTaskQueueSelectorObserver(&mock_observer);
  EXPECT_CALL(mock_observer, OnTaskQueueEnabled(_)).Times(0);
  selector_.SetQueuePriority(task_queues_[1].get(), TaskQueue::kNormalPriority);
}

TEST_F(TaskQueueSelectorTest, TestDisableEnable) {
  MockObserver mock_observer;
  selector_.SetTaskQueueSelectorObserver(&mock_observer);

  size_t queue_order[] = {0, 1, 2, 3, 4};
  PushTasks(queue_order, 5);
  task_queues_[2]->SetQueueEnabledForTest(false);
  selector_.DisableQueue(task_queues_[2].get());
  task_queues_[4]->SetQueueEnabledForTest(false);
  selector_.DisableQueue(task_queues_[4].get());
  // Disabling a queue should not affect its priority.
  EXPECT_EQ(TaskQueue::kNormalPriority, task_queues_[2]->GetQueuePriority());
  EXPECT_EQ(TaskQueue::kNormalPriority, task_queues_[4]->GetQueuePriority());
  EXPECT_THAT(PopTasks(), ::testing::ElementsAre(0, 1, 3));

  EXPECT_CALL(mock_observer, OnTaskQueueEnabled(_)).Times(2);
  task_queues_[2]->SetQueueEnabledForTest(true);
  selector_.EnableQueue(task_queues_[2].get());
  selector_.SetQueuePriority(task_queues_[2].get(),
                             TaskQueue::kBestEffortPriority);
  EXPECT_THAT(PopTasks(), ::testing::ElementsAre(2));
  task_queues_[4]->SetQueueEnabledForTest(true);
  selector_.EnableQueue(task_queues_[4].get());
  EXPECT_THAT(PopTasks(), ::testing::ElementsAre(4));
}

TEST_F(TaskQueueSelectorTest, TestDisableChangePriorityThenEnable) {
  EXPECT_TRUE(task_queues_[2]->delayed_work_queue()->Empty());
  EXPECT_TRUE(task_queues_[2]->immediate_work_queue()->Empty());

  task_queues_[2]->SetQueueEnabledForTest(false);
  selector_.SetQueuePriority(task_queues_[2].get(), TaskQueue::kHighPriority);

  size_t queue_order[] = {0, 1, 2, 3, 4};
  PushTasks(queue_order, 5);

  EXPECT_TRUE(task_queues_[2]->delayed_work_queue()->Empty());
  EXPECT_FALSE(task_queues_[2]->immediate_work_queue()->Empty());
  task_queues_[2]->SetQueueEnabledForTest(true);

  EXPECT_EQ(TaskQueue::kHighPriority, task_queues_[2]->GetQueuePriority());
  EXPECT_THAT(PopTasks(), ::testing::ElementsAre(2, 0, 1, 3, 4));
}

TEST_F(TaskQueueSelectorTest, TestEmptyQueues) {
  WorkQueue* chosen_work_queue = nullptr;
  EXPECT_FALSE(selector_.SelectWorkQueueToService(&chosen_work_queue));

  // Test only disabled queues.
  size_t queue_order[] = {0};
  PushTasks(queue_order, 1);
  task_queues_[0]->SetQueueEnabledForTest(false);
  selector_.DisableQueue(task_queues_[0].get());
  EXPECT_FALSE(selector_.SelectWorkQueueToService(&chosen_work_queue));

  // These tests are unusual since there's no TQM. To avoid a later DCHECK when
  // deleting the task queue, we re-enable the queue here so the selector
  // doesn't get out of sync.
  task_queues_[0]->SetQueueEnabledForTest(true);
  selector_.EnableQueue(task_queues_[0].get());
}

TEST_F(TaskQueueSelectorTest, TestAge) {
  size_t enqueue_order[] = {10, 1, 2, 9, 4};
  size_t queue_order[] = {0, 1, 2, 3, 4};
  PushTasksWithEnqueueOrder(queue_order, enqueue_order, 5);
  EXPECT_THAT(PopTasks(), ::testing::ElementsAre(1, 2, 4, 3, 0));
}

TEST_F(TaskQueueSelectorTest, TestControlStarvesOthers) {
  size_t queue_order[] = {0, 1, 2, 3};
  PushTasks(queue_order, 4);
  selector_.SetQueuePriority(task_queues_[3].get(),
                             TaskQueue::kControlPriority);
  selector_.SetQueuePriority(task_queues_[2].get(), TaskQueue::kHighPriority);
  selector_.SetQueuePriority(task_queues_[1].get(),
                             TaskQueue::kBestEffortPriority);
  for (int i = 0; i < 100; i++) {
    WorkQueue* chosen_work_queue = nullptr;
    ASSERT_TRUE(selector_.SelectWorkQueueToService(&chosen_work_queue));
    EXPECT_EQ(task_queues_[3].get(), chosen_work_queue->task_queue());
    // Don't remove task from queue to simulate all queues still being full.
  }
}

TEST_F(TaskQueueSelectorTest, TestHighPriorityDoesNotStarveNormal) {
  size_t queue_order[] = {0, 1, 2};
  PushTasks(queue_order, 3);
  selector_.SetQueuePriority(task_queues_[2].get(), TaskQueue::kHighPriority);
  selector_.SetQueuePriority(task_queues_[1].get(),
                             TaskQueue::kBestEffortPriority);
  size_t counts[] = {0, 0, 0};
  for (int i = 0; i < 100; i++) {
    WorkQueue* chosen_work_queue = nullptr;
    ASSERT_TRUE(selector_.SelectWorkQueueToService(&chosen_work_queue));
    size_t chosen_queue_index =
        queue_to_index_map_.find(chosen_work_queue->task_queue())->second;
    counts[chosen_queue_index]++;
    // Don't remove task from queue to simulate all queues still being full.
  }
  EXPECT_GT(counts[0], 0ul);        // Check high doesn't starve normal.
  EXPECT_GT(counts[2], counts[0]);  // Check high gets more chance to run.
  EXPECT_EQ(0ul, counts[1]);        // Check best effort is starved.
}

TEST_F(TaskQueueSelectorTest, TestHighPriorityDoesNotStarveNormalOrLow) {
  size_t queue_order[] = {0, 1, 2};
  PushTasks(queue_order, 3);
  selector_.SetQueuePriority(task_queues_[2].get(), TaskQueue::kHighPriority);
  selector_.SetQueuePriority(task_queues_[1].get(), TaskQueue::kLowPriority);
  size_t counts[] = {0, 0, 0};
  for (int i = 0; i < 100; i++) {
    WorkQueue* chosen_work_queue = nullptr;
    ASSERT_TRUE(selector_.SelectWorkQueueToService(&chosen_work_queue));
    size_t chosen_queue_index =
        queue_to_index_map_.find(chosen_work_queue->task_queue())->second;
    counts[chosen_queue_index]++;
    // Don't remove task from queue to simulate all queues still being full.
  }
  EXPECT_GT(counts[0], 0ul);        // Check high doesn't starve normal.
  EXPECT_GT(counts[2], counts[0]);  // Check high gets more chance to run.
  EXPECT_GT(counts[1], 0ul);        // Check low isn't starved.
  EXPECT_GT(counts[0], counts[1]);  // Check normal gets more chance to run.
}

TEST_F(TaskQueueSelectorTest, TestNormalPriorityDoesNotStarveLow) {
  size_t queue_order[] = {0, 1, 2};
  PushTasks(queue_order, 3);
  selector_.SetQueuePriority(task_queues_[0].get(), TaskQueue::kLowPriority);
  selector_.SetQueuePriority(task_queues_[1].get(),
                             TaskQueue::kBestEffortPriority);
  size_t counts[] = {0, 0, 0};
  for (int i = 0; i < 100; i++) {
    WorkQueue* chosen_work_queue = nullptr;
    ASSERT_TRUE(selector_.SelectWorkQueueToService(&chosen_work_queue));
    size_t chosen_queue_index =
        queue_to_index_map_.find(chosen_work_queue->task_queue())->second;
    counts[chosen_queue_index]++;
    // Don't remove task from queue to simulate all queues still being full.
  }
  EXPECT_GT(counts[0], 0ul);        // Check normal doesn't starve low.
  EXPECT_GT(counts[2], counts[0]);  // Check normal gets more chance to run.
  EXPECT_EQ(0ul, counts[1]);        // Check best effort is starved.
}

TEST_F(TaskQueueSelectorTest, TestBestEffortGetsStarved) {
  size_t queue_order[] = {0, 1};
  PushTasks(queue_order, 2);
  selector_.SetQueuePriority(task_queues_[0].get(),
                             TaskQueue::kBestEffortPriority);
  EXPECT_EQ(TaskQueue::kNormalPriority, task_queues_[1]->GetQueuePriority());
  WorkQueue* chosen_work_queue = nullptr;
  for (int i = 0; i < 100; i++) {
    ASSERT_TRUE(selector_.SelectWorkQueueToService(&chosen_work_queue));
    EXPECT_EQ(task_queues_[1].get(), chosen_work_queue->task_queue());
    // Don't remove task from queue to simulate all queues still being full.
  }
  selector_.SetQueuePriority(task_queues_[1].get(), TaskQueue::kHighPriority);
  for (int i = 0; i < 100; i++) {
    ASSERT_TRUE(selector_.SelectWorkQueueToService(&chosen_work_queue));
    EXPECT_EQ(task_queues_[1].get(), chosen_work_queue->task_queue());
    // Don't remove task from queue to simulate all queues still being full.
  }
  selector_.SetQueuePriority(task_queues_[1].get(),
                             TaskQueue::kControlPriority);
  for (int i = 0; i < 100; i++) {
    ASSERT_TRUE(selector_.SelectWorkQueueToService(&chosen_work_queue));
    EXPECT_EQ(task_queues_[1].get(), chosen_work_queue->task_queue());
    // Don't remove task from queue to simulate all queues still being full.
  }
}

TEST_F(TaskQueueSelectorTest, EnabledWorkQueuesEmpty) {
  EXPECT_TRUE(selector_.EnabledWorkQueuesEmpty());
  size_t queue_order[] = {0, 1};
  PushTasks(queue_order, 2);

  EXPECT_FALSE(selector_.EnabledWorkQueuesEmpty());
  PopTasks();
  EXPECT_TRUE(selector_.EnabledWorkQueuesEmpty());
}

TEST_F(TaskQueueSelectorTest, EnabledWorkQueuesEmpty_ControlPriority) {
  size_t queue_order[] = {0};
  PushTasks(queue_order, 1);

  selector_.SetQueuePriority(task_queues_[0].get(),
                             TaskQueue::kControlPriority);

  EXPECT_FALSE(selector_.EnabledWorkQueuesEmpty());
}

TEST_F(TaskQueueSelectorTest, ChooseOldestWithPriority_Empty) {
  WorkQueue* chosen_work_queue = nullptr;
  bool chose_delayed_over_immediate = false;
  EXPECT_FALSE(enabled_selector()->ChooseOldestWithPriority(
      TaskQueue::kNormalPriority, &chose_delayed_over_immediate,
      &chosen_work_queue));
  EXPECT_FALSE(chose_delayed_over_immediate);
}

TEST_F(TaskQueueSelectorTest, ChooseOldestWithPriority_OnlyDelayed) {
  task_queues_[0]->delayed_work_queue()->Push(
      TaskQueueImpl::Task(TaskQueue::PostedTask(test_closure_, FROM_HERE),
                          base::TimeTicks(), 0, 0));

  WorkQueue* chosen_work_queue = nullptr;
  bool chose_delayed_over_immediate = false;
  EXPECT_TRUE(enabled_selector()->ChooseOldestWithPriority(
      TaskQueue::kNormalPriority, &chose_delayed_over_immediate,
      &chosen_work_queue));
  EXPECT_EQ(chosen_work_queue, task_queues_[0]->delayed_work_queue());
  EXPECT_FALSE(chose_delayed_over_immediate);
}

TEST_F(TaskQueueSelectorTest, ChooseOldestWithPriority_OnlyImmediate) {
  task_queues_[0]->immediate_work_queue()->Push(
      TaskQueueImpl::Task(TaskQueue::PostedTask(test_closure_, FROM_HERE),
                          base::TimeTicks(), 0, 0));

  WorkQueue* chosen_work_queue = nullptr;
  bool chose_delayed_over_immediate = false;
  EXPECT_TRUE(enabled_selector()->ChooseOldestWithPriority(
      TaskQueue::kNormalPriority, &chose_delayed_over_immediate,
      &chosen_work_queue));
  EXPECT_EQ(chosen_work_queue, task_queues_[0]->immediate_work_queue());
  EXPECT_FALSE(chose_delayed_over_immediate);
}

TEST_F(TaskQueueSelectorTest, TestObserverWithOneBlockedQueue) {
  TaskQueueSelectorForTest selector;
  MockObserver mock_observer;
  selector.SetTaskQueueSelectorObserver(&mock_observer);

  EXPECT_CALL(mock_observer, OnTaskQueueEnabled(_)).Times(1);

  std::unique_ptr<TaskQueueImpl> task_queue(NewTaskQueueWithBlockReporting());
  selector.AddQueue(task_queue.get());

  task_queue->SetQueueEnabledForTest(false);
  selector.DisableQueue(task_queue.get());

  TaskQueueImpl::Task task(TaskQueue::PostedTask(test_closure_, FROM_HERE),
                           base::TimeTicks(), 0);
  task.set_enqueue_order(0);
  task_queue->immediate_work_queue()->Push(std::move(task));

  WorkQueue* chosen_work_queue;
  EXPECT_CALL(mock_observer, OnTriedToSelectBlockedWorkQueue(_)).Times(1);
  EXPECT_FALSE(selector.SelectWorkQueueToService(&chosen_work_queue));

  task_queue->SetQueueEnabledForTest(true);
  selector.EnableQueue(task_queue.get());
  selector.RemoveQueue(task_queue.get());
  task_queue->UnregisterTaskQueue();
}

TEST_F(TaskQueueSelectorTest, TestObserverWithTwoBlockedQueues) {
  TaskQueueSelectorForTest selector;
  MockObserver mock_observer;
  selector.SetTaskQueueSelectorObserver(&mock_observer);

  std::unique_ptr<TaskQueueImpl> task_queue(NewTaskQueueWithBlockReporting());
  std::unique_ptr<TaskQueueImpl> task_queue2(NewTaskQueueWithBlockReporting());
  selector.AddQueue(task_queue.get());
  selector.AddQueue(task_queue2.get());

  task_queue->SetQueueEnabledForTest(false);
  task_queue2->SetQueueEnabledForTest(false);
  selector.DisableQueue(task_queue.get());
  selector.DisableQueue(task_queue2.get());

  selector.SetQueuePriority(task_queue2.get(), TaskQueue::kControlPriority);

  TaskQueueImpl::Task task1(TaskQueue::PostedTask(test_closure_, FROM_HERE),
                            base::TimeTicks(), 0);
  TaskQueueImpl::Task task2(TaskQueue::PostedTask(test_closure_, FROM_HERE),
                            base::TimeTicks(), 1);
  task1.set_enqueue_order(0);
  task2.set_enqueue_order(1);
  task_queue->immediate_work_queue()->Push(std::move(task1));
  task_queue2->immediate_work_queue()->Push(std::move(task2));

  // Should still only see one call to OnTriedToSelectBlockedWorkQueue.
  WorkQueue* chosen_work_queue;
  EXPECT_CALL(mock_observer, OnTriedToSelectBlockedWorkQueue(_)).Times(1);
  EXPECT_FALSE(selector.SelectWorkQueueToService(&chosen_work_queue));
  ::testing::Mock::VerifyAndClearExpectations(&mock_observer);

  EXPECT_CALL(mock_observer, OnTaskQueueEnabled(_)).Times(2);

  task_queue->SetQueueEnabledForTest(true);
  selector.EnableQueue(task_queue.get());

  // Removing the second queue and selecting again should result in another
  // notification.
  selector.RemoveQueue(task_queue.get());
  task_queue->UnregisterTaskQueue();
  EXPECT_CALL(mock_observer, OnTriedToSelectBlockedWorkQueue(_)).Times(1);
  EXPECT_FALSE(selector.SelectWorkQueueToService(&chosen_work_queue));

  task_queue2->SetQueueEnabledForTest(true);
  selector.EnableQueue(task_queue2.get());
  selector.RemoveQueue(task_queue2.get());
  task_queue2->UnregisterTaskQueue();
}

struct ChooseOldestWithPriorityTestParam {
  int delayed_task_enqueue_order;
  int immediate_task_enqueue_order;
  int immediate_starvation_count;
  const char* expected_work_queue_name;
  bool expected_did_starve_immediate_queue;
};

static const ChooseOldestWithPriorityTestParam
    kChooseOldestWithPriorityTestCases[] = {
        {1, 2, 0, "delayed", true},    {1, 2, 1, "delayed", true},
        {1, 2, 2, "delayed", true},    {1, 2, 3, "immediate", false},
        {1, 2, 4, "immediate", false}, {2, 1, 4, "immediate", false},
        {2, 1, 4, "immediate", false},
};

class ChooseOldestWithPriorityTest
    : public TaskQueueSelectorTest,
      public ::testing::WithParamInterface<ChooseOldestWithPriorityTestParam> {
};

TEST_P(ChooseOldestWithPriorityTest, RoundRobinTest) {
  task_queues_[0]->immediate_work_queue()->Push(TaskQueueImpl::Task(
      TaskQueue::PostedTask(test_closure_, FROM_HERE), base::TimeTicks(),
      GetParam().immediate_task_enqueue_order,
      GetParam().immediate_task_enqueue_order));

  task_queues_[0]->delayed_work_queue()->Push(TaskQueueImpl::Task(
      TaskQueue::PostedTask(test_closure_, FROM_HERE), base::TimeTicks(),
      GetParam().delayed_task_enqueue_order,
      GetParam().delayed_task_enqueue_order));

  selector_.SetImmediateStarvationCountForTest(
      GetParam().immediate_starvation_count);

  WorkQueue* chosen_work_queue = nullptr;
  bool chose_delayed_over_immediate = false;
  EXPECT_TRUE(enabled_selector()->ChooseOldestWithPriority(
      TaskQueue::kNormalPriority, &chose_delayed_over_immediate,
      &chosen_work_queue));
  EXPECT_EQ(chosen_work_queue->task_queue(), task_queues_[0].get());
  EXPECT_STREQ(chosen_work_queue->name(), GetParam().expected_work_queue_name);
  EXPECT_EQ(chose_delayed_over_immediate,
            GetParam().expected_did_starve_immediate_queue);
}

INSTANTIATE_TEST_CASE_P(
    ChooseOldestWithPriorityTest,
    ChooseOldestWithPriorityTest,
    ::testing::ValuesIn(kChooseOldestWithPriorityTestCases));

}  // namespace task_queue_selector_unittest
}  // namespace internal
}  // namespace scheduler
}  // namespace blink
