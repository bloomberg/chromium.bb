// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"

#include <memory>

#include "base/single_thread_task_runner.h"
#include "base/task/sequence_manager/test/lazy_thread_controller_for_test.h"
#include "base/task/sequence_manager/test/sequence_manager_for_test.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_task_runner_handle.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_scheduler_impl.h"

namespace blink {
namespace scheduler {

std::unique_ptr<WebThreadScheduler> CreateWebMainThreadSchedulerForTests() {
  return std::make_unique<scheduler::MainThreadSchedulerImpl>(
      base::sequence_manager::SequenceManagerForTest::Create(
          std::make_unique<
              base::sequence_manager::LazyThreadControllerForTest>()),
      base::nullopt);
}

void RunIdleTasksForTesting(WebThreadScheduler* scheduler,
                            base::OnceClosure callback) {
  MainThreadSchedulerImpl* scheduler_impl =
      static_cast<MainThreadSchedulerImpl*>(scheduler);
  scheduler_impl->RunIdleTasksForTesting(std::move(callback));
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
