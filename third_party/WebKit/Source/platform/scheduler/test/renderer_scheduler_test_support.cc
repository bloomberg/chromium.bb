// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/platform/scheduler/test/renderer_scheduler_test_support.h"

#include <memory>

#include "base/single_thread_task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_task_runner_handle.h"
#include "platform/scheduler/renderer/renderer_scheduler_impl.h"
#include "platform/scheduler/test/lazy_thread_controller_for_test.h"

namespace blink {
namespace scheduler {

namespace {

class TaskQueueManagerForRendererSchedulerTest : public TaskQueueManager {
 public:
  explicit TaskQueueManagerForRendererSchedulerTest(
      std::unique_ptr<internal::ThreadController> thread_controller)
      : TaskQueueManager(std::move(thread_controller)) {}
};

class WebTaskRunnerProxy : public base::SingleThreadTaskRunner {
 public:
  explicit WebTaskRunnerProxy(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : task_runner_(task_runner) {}

  bool PostDelayedTask(const base::Location& location,
                       base::OnceClosure closure,
                       base::TimeDelta time_delta) override {
    return task_runner_->PostDelayedTask(location, std::move(closure),
                                         time_delta);
  }

  bool PostNonNestableDelayedTask(const base::Location& location,
                                  base::OnceClosure closure,
                                  base::TimeDelta time_delta) override {
    return task_runner_->PostNonNestableDelayedTask(
        location, std::move(closure), time_delta);
  }

  bool RunsTasksInCurrentSequence() const override {
    return task_runner_->RunsTasksInCurrentSequence();
  }

 private:
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};

}  // namespace

std::unique_ptr<RendererScheduler> CreateRendererSchedulerForTests() {
  return std::make_unique<scheduler::RendererSchedulerImpl>(
      std::make_unique<TaskQueueManagerForRendererSchedulerTest>(
          std::make_unique<LazyThreadControllerForTest>()),
      base::nullopt);
}

void RunIdleTasksForTesting(RendererScheduler* scheduler,
                            const base::Closure& callback) {
  RendererSchedulerImpl* scheduler_impl =
      static_cast<RendererSchedulerImpl*>(scheduler);
  scheduler_impl->RunIdleTasksForTesting(callback);
}

scoped_refptr<base::SequencedTaskRunner> GetSequencedTaskRunnerForTesting() {
  return base::SequencedTaskRunnerHandle::Get();
}

scoped_refptr<base::SingleThreadTaskRunner>
GetSingleThreadTaskRunnerForTesting() {
  return base::ThreadTaskRunnerHandle::Get();
}

scoped_refptr<base::SingleThreadTaskRunner> CreateWebTaskRunnerForTesting() {
  return new WebTaskRunnerProxy(GetSingleThreadTaskRunnerForTesting());
}

}  // namespace scheduler
}  // namespace blink
