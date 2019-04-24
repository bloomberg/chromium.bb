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
#include "base/task/task_scheduler/task_scheduler.h"
#include "content/public/browser/browser_task_traits.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAre;

namespace content {

namespace {

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
  void SetUp() override {
    browser_ui_thread_scheduler_ = std::make_unique<BrowserUIThreadScheduler>();
  }

  void TearDown() override { ShutdownBrowserUIThreadScheduler(); }

  void ShutdownBrowserUIThreadScheduler() {
    browser_ui_thread_scheduler_.reset();
  }

 protected:
  std::unique_ptr<BrowserUIThreadScheduler> browser_ui_thread_scheduler_;
};

TEST_F(BrowserUIThreadSchedulerTest, SimplePosting) {
  scoped_refptr<base::SingleThreadTaskRunner> tq =
      browser_ui_thread_scheduler_->GetTaskRunnerForTesting(
          BrowserUIThreadScheduler::QueueType::kDefault);

  std::vector<int> order;
  tq->PostTask(FROM_HERE, base::BindOnce(RecordRunOrder, &order, 1));
  tq->PostTask(FROM_HERE, base::BindOnce(RecordRunOrder, &order, 2));
  tq->PostTask(FROM_HERE, base::BindOnce(RecordRunOrder, &order, 3));

  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(order, ElementsAre(1, 2, 3));
}

TEST_F(BrowserUIThreadSchedulerTest, DestructorPostChainDuringShutdown) {
  scoped_refptr<base::SingleThreadTaskRunner> task_queue =
      browser_ui_thread_scheduler_->GetTaskRunnerForTesting(
          BrowserUIThreadScheduler::QueueType::kDefault);

  bool run = false;
  task_queue->PostTask(
      FROM_HERE,
      PostOnDestruction(
          task_queue,
          PostOnDestruction(task_queue,
                            RunOnDestruction(base::BindOnce(
                                [](bool* run) { *run = true; }, &run)))));

  EXPECT_FALSE(run);
  ShutdownBrowserUIThreadScheduler();

  EXPECT_TRUE(run);
}

}  // namespace content
