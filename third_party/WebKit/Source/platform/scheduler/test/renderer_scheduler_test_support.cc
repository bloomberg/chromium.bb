// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/platform/scheduler/test/renderer_scheduler_test_support.h"

#include <memory>

#include "base/single_thread_task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_task_runner_handle.h"
#include "platform/scheduler/base/task_queue_manager_impl.h"
#include "platform/scheduler/main_thread/main_thread_scheduler.h"
#include "platform/scheduler/test/lazy_thread_controller_for_test.h"
#include "platform/scheduler/test/task_queue_manager_for_test.h"

namespace blink {
namespace scheduler {

std::unique_ptr<RendererScheduler> CreateRendererSchedulerForTests() {
  return std::make_unique<scheduler::RendererSchedulerImpl>(
      std::make_unique<TaskQueueManagerForTest>(
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
