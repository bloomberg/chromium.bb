// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/platform/scheduler/test/renderer_scheduler_test_support.h"

#include <memory>

#include "base/single_thread_task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_task_runner_handle.h"
#include "platform/scheduler/base/task_queue_manager_impl.h"
#include "platform/scheduler/renderer/renderer_scheduler_impl.h"
#include "platform/scheduler/test/lazy_thread_controller_for_test.h"

namespace blink {
namespace scheduler {

namespace {

// TODO(kraynov): Use CreateTaskQueueManagerForTest instead.
class TaskQueueManagerForRendererSchedulerTest : public TaskQueueManagerImpl {
 public:
  explicit TaskQueueManagerForRendererSchedulerTest(
      std::unique_ptr<internal::ThreadController> thread_controller)
      : TaskQueueManagerImpl(std::move(thread_controller)) {}
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

}  // namespace scheduler
}  // namespace blink
