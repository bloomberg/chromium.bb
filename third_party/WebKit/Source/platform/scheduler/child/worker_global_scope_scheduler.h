// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_CHILD_WORKER_GLOBAL_SCOPE_SCHEDULER_H_
#define THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_CHILD_WORKER_GLOBAL_SCOPE_SCHEDULER_H_

#include "platform/PlatformExport.h"
#include "platform/scheduler/base/task_queue.h"
#include "platform/scheduler/child/web_task_runner_impl.h"
#include "public/platform/TaskType.h"

namespace blink {
namespace scheduler {

class WorkerScheduler;

// A scheduler provides per-global-scope task queues. This is constructed when a
// global scope is created and destructed when it's closed.
//
// Unless stated otherwise, all methods must be called on the worker thread.
class PLATFORM_EXPORT WorkerGlobalScopeScheduler {
 public:
  explicit WorkerGlobalScopeScheduler(WorkerScheduler* worker_scheduler);
  ~WorkerGlobalScopeScheduler();

  // Unregisters the task queues and cancels tasks in them.
  void Dispose();

  // Returns a WebTaskRunner that is suitable with the given task type. This can
  // be called from any thread.
  //
  // This must be called only from TaskRunnerHelper::Get().
  RefPtr<WebTaskRunner> GetTaskRunner(TaskType) const;

  // TODO(nhiroki): Add mechanism to throttle/suspend tasks in response to the
  // state of the parent document (https://crbug.com/670534).

 private:
  RefPtr<WebTaskRunnerImpl> unthrottled_task_runner_;

#if DCHECK_IS_ON()
  bool is_disposed_ = false;
#endif
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_CHILD_WORKER_GLOBAL_SCOPE_SCHEDULER_H_
