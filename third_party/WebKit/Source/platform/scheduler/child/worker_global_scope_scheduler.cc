// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/child/worker_global_scope_scheduler.h"

#include "platform/scheduler/child/worker_scheduler.h"

namespace blink {
namespace scheduler {

WorkerGlobalScopeScheduler::WorkerGlobalScopeScheduler(
    WorkerScheduler* worker_scheduler) {
  scoped_refptr<TaskQueue> task_queue = worker_scheduler->CreateTaskRunner();
  unthrottled_task_runner_ = WebTaskRunnerImpl::Create(std::move(task_queue));
}

WorkerGlobalScopeScheduler::~WorkerGlobalScopeScheduler() {
#if DCHECK_IS_ON()
  DCHECK(is_disposed_);
#endif
}

void WorkerGlobalScopeScheduler::Dispose() {
  unthrottled_task_runner_->GetTaskQueue()->UnregisterTaskQueue();
#if DCHECK_IS_ON()
  is_disposed_ = true;
#endif
}

}  // namespace scheduler
}  // namespace blink
