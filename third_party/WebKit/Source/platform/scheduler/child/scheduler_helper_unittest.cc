// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/child/scheduler_helper.h"

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/simple_test_tick_clock.h"
#include "components/viz/test/ordered_simple_task_runner.h"
#include "platform/scheduler/base/lazy_now.h"
#include "platform/scheduler/base/task_queue.h"
#include "platform/scheduler/child/worker_scheduler_helper.h"
#include "platform/scheduler/test/task_queue_manager_for_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Invoke;
using ::testing::Return;

namespace blink {
namespace scheduler {
namespace scheduler_helper_unittest {

namespace {
void AppendToVectorTestTask(std::vector<std::string>* vector,
                            std::string value) {
  vector->push_back(value);
}

void AppendToVectorReentrantTask(base::SingleThreadTaskRunner* task_runner,
                                 std::vector<int>* vector,
                                 int* reentrant_count,
                                 int max_reentrant_count) {
  vector->push_back((*reentrant_count)++);
  if (*reentrant_count < max_reentrant_count) {
    task_runner->PostTask(FROM_HERE,
                          base::BindOnce(AppendToVectorReentrantTask,
                                         base::Unretained(task_runner), vector,
                                         reentrant_count, max_reentrant_count));
  }
}

};  // namespace

class SchedulerHelperTest : public ::testing::Test {
 public:
  SchedulerHelperTest()
      : mock_task_runner_(new cc::OrderedSimpleTaskRunner(&clock_, false)) {
    std::unique_ptr<TaskQueueManagerForTest> task_queue_manager =
        TaskQueueManagerForTest::Create(nullptr, mock_task_runner_, &clock_);
    task_queue_manager_ = task_queue_manager.get();
    scheduler_helper_ = std::make_unique<WorkerSchedulerHelper>(
        std::move(task_queue_manager), nullptr);
    default_task_runner_ = scheduler_helper_->DefaultWorkerTaskQueue();
    clock_.Advance(base::TimeDelta::FromMicroseconds(5000));
  }

  ~SchedulerHelperTest() override = default;

  void TearDown() override {
    // Check that all tests stop posting tasks.
    mock_task_runner_->SetAutoAdvanceNowToPendingTasks(true);
    while (mock_task_runner_->RunUntilIdle()) {
    }
  }

  void RunUntilIdle() { mock_task_runner_->RunUntilIdle(); }

  template <typename E>
  static void CallForEachEnumValue(E first,
                                   E last,
                                   const char* (*function)(E)) {
    for (E val = first; val < last;
         val = static_cast<E>(static_cast<int>(val) + 1)) {
      (*function)(val);
    }
  }

 protected:
  base::SimpleTestTickClock clock_;
  scoped_refptr<cc::OrderedSimpleTaskRunner> mock_task_runner_;

  std::unique_ptr<WorkerSchedulerHelper> scheduler_helper_;
  TaskQueueManagerForTest* task_queue_manager_;  // Owned by scheduler_helper.
  scoped_refptr<base::SingleThreadTaskRunner> default_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(SchedulerHelperTest);
};

TEST_F(SchedulerHelperTest, TestPostDefaultTask) {
  std::vector<std::string> run_order;
  default_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&AppendToVectorTestTask, &run_order, "D1"));
  default_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&AppendToVectorTestTask, &run_order, "D2"));
  default_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&AppendToVectorTestTask, &run_order, "D3"));
  default_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&AppendToVectorTestTask, &run_order, "D4"));

  RunUntilIdle();
  EXPECT_THAT(run_order,
              ::testing::ElementsAre(std::string("D1"), std::string("D2"),
                                     std::string("D3"), std::string("D4")));
}

TEST_F(SchedulerHelperTest, TestRentrantTask) {
  int count = 0;
  std::vector<int> run_order;
  default_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(AppendToVectorReentrantTask,
                                base::RetainedRef(default_task_runner_),
                                &run_order, &count, 5));
  RunUntilIdle();

  EXPECT_THAT(run_order, ::testing::ElementsAre(0, 1, 2, 3, 4));
}

TEST_F(SchedulerHelperTest, IsShutdown) {
  EXPECT_FALSE(scheduler_helper_->IsShutdown());

  scheduler_helper_->Shutdown();
  EXPECT_TRUE(scheduler_helper_->IsShutdown());
}

TEST_F(SchedulerHelperTest, GetNumberOfPendingTasks) {
  std::vector<std::string> run_order;
  scheduler_helper_->DefaultWorkerTaskQueue()->PostTask(
      FROM_HERE, base::BindOnce(&AppendToVectorTestTask, &run_order, "D1"));
  scheduler_helper_->DefaultWorkerTaskQueue()->PostTask(
      FROM_HERE, base::BindOnce(&AppendToVectorTestTask, &run_order, "D2"));
  scheduler_helper_->ControlWorkerTaskQueue()->PostTask(
      FROM_HERE, base::BindOnce(&AppendToVectorTestTask, &run_order, "C1"));
  EXPECT_EQ(3U, task_queue_manager_->PendingTasksCount());
  RunUntilIdle();
  EXPECT_EQ(0U, task_queue_manager_->PendingTasksCount());
}

namespace {
class MockTaskObserver : public base::MessageLoop::TaskObserver {
 public:
  MOCK_METHOD1(DidProcessTask, void(const base::PendingTask& task));
  MOCK_METHOD1(WillProcessTask, void(const base::PendingTask& task));
};

void NopTask() {}
}  // namespace

TEST_F(SchedulerHelperTest, ObserversNotifiedFor_DefaultTaskRunner) {
  MockTaskObserver observer;
  scheduler_helper_->AddTaskObserver(&observer);

  scheduler_helper_->DefaultWorkerTaskQueue()->PostTask(
      FROM_HERE, base::BindOnce(&NopTask));

  EXPECT_CALL(observer, WillProcessTask(_)).Times(1);
  EXPECT_CALL(observer, DidProcessTask(_)).Times(1);
  RunUntilIdle();
}

TEST_F(SchedulerHelperTest, ObserversNotNotifiedFor_ControlTaskQueue) {
  MockTaskObserver observer;
  scheduler_helper_->AddTaskObserver(&observer);

  scheduler_helper_->ControlWorkerTaskQueue()->PostTask(
      FROM_HERE, base::BindOnce(&NopTask));

  EXPECT_CALL(observer, WillProcessTask(_)).Times(0);
  EXPECT_CALL(observer, DidProcessTask(_)).Times(0);
  RunUntilIdle();
}

}  // namespace scheduler_helper_unittest
}  // namespace scheduler
}  // namespace blink
