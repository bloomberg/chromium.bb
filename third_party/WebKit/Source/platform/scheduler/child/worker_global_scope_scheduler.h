// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_PUBLIC_PLATFORM_SCHEDULER_WORKER_GLOBAL_SCOPE_SCHEDULER_H_
#define THIRD_PARTY_WEBKIT_PUBLIC_PLATFORM_SCHEDULER_WORKER_GLOBAL_SCOPE_SCHEDULER_H_

#include "platform/scheduler/child/web_task_runner_impl.h"
#include "public/platform/WebCommon.h"
#include "public/platform/scheduler/base/task_queue.h"

namespace blink {
namespace scheduler {

class WorkerScheduler;

// A scheduler provides per-global-scope task queues. This is constructed when a
// global scope is created and destructed when it's closed.
//
// Unless stated otherwise, all methods must be called on the worker thread.
class BLINK_PLATFORM_EXPORT WorkerGlobalScopeScheduler {
 public:
  explicit WorkerGlobalScopeScheduler(WorkerScheduler* worker_scheduler);
  ~WorkerGlobalScopeScheduler();

  // Unregisters the task queues and cancels tasks in them.
  void Dispose();

  // Returns the WebTaskRunner for tasks which should never get throttled. This
  // can be called from any thread.
  RefPtr<WebTaskRunner> UnthrottledTaskRunner() {
    return unthrottled_task_runner_;
  }

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

#endif  // THIRD_PARTY_WEBKIT_PUBLIC_PLATFORM_SCHEDULER_WORKER_GLOBAL_SCOPE_SCHEDULER_H_
