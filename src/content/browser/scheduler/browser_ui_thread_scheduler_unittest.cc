// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/scheduler/browser_ui_thread_scheduler.h"

#include <utility>
#include <vector>

#include "base/callback_helpers.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool/thread_pool.h"
#include "base/test/mock_callback.h"
#include "content/public/browser/browser_task_traits.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

using ::testing::ElementsAre;
using ::testing::Invoke;
using ::testing::Mock;

void RecordRunOrder(std::vector<int>* run_order, int order) {
  run_order->push_back(order);
}

base::OnceClosure RunOnDestruction(base::OnceClosure task) {
  return base::BindOnce(
      [](std::unique_ptr<base::ScopedClosureRunner>) {},
      base::Passed(
          std::make_unique<base::ScopedClosureRunner>(std::move(task))));
}

base::OnceClosure PostOnDestruction(
    scoped_refptr<base::SingleThreadTaskRunner> task_queue,
    base::OnceClosure task) {
  return RunOnDestruction(base::BindOnce(
      [](base::OnceClosure task,
         scoped_refptr<base::SingleThreadTaskRunner> task_queue) {
        task_queue->PostTask(FROM_HERE, std::move(task));
      },
      base::Passed(std::move(task)), task_queue));
}

}  // namespace

class BrowserUIThreadSchedulerTest : public testing::Test {
 public:
  BrowserUIThreadSchedulerTest() {
    browser_ui_thread_scheduler_ = std::make_unique<BrowserUIThreadScheduler>();
    for (int i = 0;
         i < static_cast<int>(BrowserUIThreadTaskQueue::QueueType::kCount);
         i++) {
      auto queue_type = static_cast<BrowserUIThreadTaskQueue::QueueType>(i);
      task_runners_.emplace(
          queue_type,
          browser_ui_thread_scheduler_->GetTaskRunnerForTesting(queue_type));
    }
  }

 protected:
  using QueueType = BrowserUIThreadScheduler::QueueType;
  using MockTask =
      testing::StrictMock<base::MockCallback<base::RepeatingCallback<void()>>>;

  std::unique_ptr<BrowserUIThreadScheduler> browser_ui_thread_scheduler_;
  base::flat_map<BrowserUIThreadScheduler::QueueType,
                 scoped_refptr<base::SingleThreadTaskRunner>>
      task_runners_;
};

TEST_F(BrowserUIThreadSchedulerTest, RunAllPendingTasksForTesting) {
  browser_ui_thread_scheduler_->EnableBestEffortQueues();
  MockTask task_1;
  MockTask task_2;
  EXPECT_CALL(task_1, Run).WillOnce(Invoke([&]() {
    task_runners_[QueueType::kDefault]->PostTask(FROM_HERE, task_2.Get());
    task_runners_[QueueType::kBestEffort]->PostTask(FROM_HERE, task_2.Get());
  }));

  task_runners_[QueueType::kDefault]->PostTask(FROM_HERE, task_1.Get());

  browser_ui_thread_scheduler_->RunAllPendingTasksForTesting();

  Mock::VerifyAndClearExpectations(&task_1);
  EXPECT_CALL(task_2, Run).Times(2);

  browser_ui_thread_scheduler_->RunAllPendingTasksForTesting();
}

TEST_F(BrowserUIThreadSchedulerTest,
       RunAllPendingTasksForTestingIgnoresBestEffortIfNotEnabled) {
  MockTask best_effort_task;
  MockTask default_task;

  task_runners_[QueueType::kBestEffort]->PostTask(FROM_HERE,
                                                  best_effort_task.Get());
  task_runners_[QueueType::kDefault]->PostTask(FROM_HERE, default_task.Get());

  EXPECT_CALL(default_task, Run);

  browser_ui_thread_scheduler_->RunAllPendingTasksForTesting();
}

TEST_F(BrowserUIThreadSchedulerTest,
       RunAllPendingTasksForTestingRunsBestEffortTasksWhenEnabled) {
  MockTask task_1;
  MockTask task_2;
  MockTask task_3;
  EXPECT_CALL(task_1, Run).WillOnce(Invoke([&]() {
    // This task should not run as it is posted after the
    // RunAllPendingTasksForTesting() call
    task_runners_[QueueType::kBestEffort]->PostTask(FROM_HERE, task_3.Get());
    browser_ui_thread_scheduler_->EnableBestEffortQueues();
  }));
  EXPECT_CALL(task_2, Run);

  task_runners_[QueueType::kDefault]->PostTask(FROM_HERE, task_1.Get());
  task_runners_[QueueType::kBestEffort]->PostTask(FROM_HERE, task_2.Get());

  browser_ui_thread_scheduler_->RunAllPendingTasksForTesting();
}

TEST_F(BrowserUIThreadSchedulerTest, RunAllPendingTasksForTestingIsReentrant) {
  MockTask task_1;
  MockTask task_2;
  MockTask task_3;

  EXPECT_CALL(task_1, Run).WillOnce(Invoke([&]() {
    task_runners_[QueueType::kDefault]->PostTask(FROM_HERE, task_2.Get());
    browser_ui_thread_scheduler_->RunAllPendingTasksForTesting();
  }));
  EXPECT_CALL(task_2, Run).WillOnce(Invoke([&]() {
    task_runners_[QueueType::kDefault]->PostTask(FROM_HERE, task_3.Get());
  }));

  task_runners_[QueueType::kDefault]->PostTask(FROM_HERE, task_1.Get());
  browser_ui_thread_scheduler_->RunAllPendingTasksForTesting();
}

TEST_F(BrowserUIThreadSchedulerTest, SimplePosting) {
  scoped_refptr<base::SingleThreadTaskRunner> tq =
      task_runners_[QueueType::kDefault];

  std::vector<int> order;
  tq->PostTask(FROM_HERE, base::BindOnce(RecordRunOrder, &order, 1));
  tq->PostTask(FROM_HERE, base::BindOnce(RecordRunOrder, &order, 2));
  tq->PostTask(FROM_HERE, base::BindOnce(RecordRunOrder, &order, 3));

  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(order, ElementsAre(1, 2, 3));
}

TEST_F(BrowserUIThreadSchedulerTest, DestructorPostChainDuringShutdown) {
  scoped_refptr<base::SingleThreadTaskRunner> task_queue =
      task_runners_[QueueType::kDefault];

  bool run = false;
  task_queue->PostTask(
      FROM_HERE,
      PostOnDestruction(
          task_queue,
          PostOnDestruction(task_queue,
                            RunOnDestruction(base::BindOnce(
                                [](bool* run) { *run = true; }, &run)))));

  EXPECT_FALSE(run);
  browser_ui_thread_scheduler_.reset();

  EXPECT_TRUE(run);
}

}  // namespace content
