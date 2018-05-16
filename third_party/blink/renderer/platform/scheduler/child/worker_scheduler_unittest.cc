// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/child/worker_scheduler.h"

#include <memory>
#include "base/macros.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/test_simple_task_runner.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/scheduler/base/test/task_queue_manager_for_test.h"
#include "third_party/blink/renderer/platform/scheduler/worker/worker_thread_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

using testing::ElementsAreArray;

namespace blink {
namespace scheduler {
// To avoid symbol collisions in jumbo builds.
namespace worker_scheduler_unittest {

void AppendToVectorTestTask(std::vector<std::string>* vector,
                            std::string value) {
  vector->push_back(value);
}

class WorkerThreadSchedulerForTest : public WorkerThreadScheduler {
 public:
  WorkerThreadSchedulerForTest(
      WebThreadType thread_type,
      std::unique_ptr<base::sequence_manager::TaskQueueManager> manager,
      WorkerSchedulerProxy* proxy)
      : WorkerThreadScheduler(thread_type, std::move(manager), proxy) {}

  const std::unordered_set<WorkerScheduler*>& worker_schedulers() {
    return worker_schedulers_;
  }
};

class WorkerSchedulerTest : public testing::Test {
 public:
  WorkerSchedulerTest()
      : mock_task_runner_(new base::TestSimpleTaskRunner()),
        scheduler_(new WorkerThreadSchedulerForTest(
            WebThreadType::kTestThread,
            base::sequence_manager::TaskQueueManagerForTest::Create(
                nullptr,
                mock_task_runner_,
                &clock_),
            nullptr /* proxy */)) {
    clock_.Advance(base::TimeDelta::FromMicroseconds(5000));
  }

  ~WorkerSchedulerTest() override = default;

  void SetUp() override {
    scheduler_->Init();
    worker_scheduler_ = std::make_unique<WorkerScheduler>(scheduler_.get());
  }

  void RunUntilIdle() { mock_task_runner_->RunUntilIdle(); }

  // Helper for posting a task.
  void PostTestTask(std::vector<std::string>* run_order,
                    const std::string& task_descriptor) {
    worker_scheduler_->GetTaskRunner(TaskType::kInternalTest)
        ->PostTask(FROM_HERE,
                   WTF::Bind(&AppendToVectorTestTask,
                             WTF::Unretained(run_order), task_descriptor));
  }

 protected:
  base::SimpleTestTickClock clock_;
  scoped_refptr<base::TestSimpleTaskRunner> mock_task_runner_;

  std::unique_ptr<WorkerThreadSchedulerForTest> scheduler_;
  std::unique_ptr<WorkerScheduler> worker_scheduler_;

  DISALLOW_COPY_AND_ASSIGN(WorkerSchedulerTest);
};

TEST_F(WorkerSchedulerTest, TestPostTasks) {
  std::vector<std::string> run_order;
  PostTestTask(&run_order, "T1");
  PostTestTask(&run_order, "T2");
  RunUntilIdle();
  PostTestTask(&run_order, "T3");
  RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre("T1", "T2", "T3"));

  // Tasks should not run after the scheduler is disposed of.
  worker_scheduler_->Dispose();
  run_order.clear();
  PostTestTask(&run_order, "T4");
  PostTestTask(&run_order, "T5");
  RunUntilIdle();
  EXPECT_TRUE(run_order.empty());
}

TEST_F(WorkerSchedulerTest, RegisterWorkerSchedulers) {
  EXPECT_THAT(scheduler_->worker_schedulers(),
              testing::ElementsAre(worker_scheduler_.get()));

  std::unique_ptr<WorkerScheduler> worker_scheduler2 =
      std::make_unique<WorkerScheduler>(scheduler_.get());

  EXPECT_THAT(scheduler_->worker_schedulers(),
              testing::UnorderedElementsAre(worker_scheduler_.get(),
                                            worker_scheduler2.get()));

  worker_scheduler_->Dispose();

  EXPECT_THAT(scheduler_->worker_schedulers(),
              testing::ElementsAre(worker_scheduler2.get()));

  worker_scheduler2->Dispose();

  EXPECT_THAT(scheduler_->worker_schedulers(), testing::ElementsAre());
}

}  // namespace worker_scheduler_unittest
}  // namespace scheduler
}  // namespace blink
