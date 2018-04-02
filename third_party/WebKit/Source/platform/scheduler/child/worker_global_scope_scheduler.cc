// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/child/worker_global_scope_scheduler.h"

#include "platform/scheduler/child/task_runner_impl.h"
#include "platform/scheduler/public/non_main_thread_scheduler.h"

namespace blink {
namespace scheduler {

WorkerGlobalScopeScheduler::WorkerGlobalScopeScheduler(
    WorkerScheduler* worker_scheduler) {
  task_queue_ = worker_scheduler->CreateTaskRunner();
}

WorkerGlobalScopeScheduler::~WorkerGlobalScopeScheduler() {
#if DCHECK_IS_ON()
  DCHECK(is_disposed_);
#endif
}

void WorkerGlobalScopeScheduler::Dispose() {
  task_queue_->ShutdownTaskQueue();
#if DCHECK_IS_ON()
  is_disposed_ = true;
#endif
}

scoped_refptr<base::SingleThreadTaskRunner>
WorkerGlobalScopeScheduler::GetTaskRunner(TaskType type) const {
  switch (type) {
    case TaskType::kDeprecatedNone:
    case TaskType::kDOMManipulation:
    case TaskType::kUserInteraction:
    case TaskType::kNetworking:
    case TaskType::kNetworkingControl:
    case TaskType::kHistoryTraversal:
    case TaskType::kEmbed:
    case TaskType::kMediaElementEvent:
    case TaskType::kCanvasBlobSerialization:
    case TaskType::kMicrotask:
    case TaskType::kJavascriptTimer:
    case TaskType::kRemoteEvent:
    case TaskType::kWebSocket:
    case TaskType::kPostedMessage:
    case TaskType::kUnshippedPortMessage:
    case TaskType::kFileReading:
    case TaskType::kDatabaseAccess:
    case TaskType::kPresentation:
    case TaskType::kSensor:
    case TaskType::kPerformanceTimeline:
    case TaskType::kWebGL:
    case TaskType::kIdleTask:
    case TaskType::kMiscPlatformAPI:
    case TaskType::kUnspecedTimer:
    case TaskType::kUnspecedLoading:
    case TaskType::kUnthrottled:
    case TaskType::kInternalTest:
    case TaskType::kInternalWebCrypto:
    case TaskType::kInternalIndexedDB:
    case TaskType::kInternalMedia:
    case TaskType::kInternalMediaRealTime:
    case TaskType::kInternalIPC:
      // UnthrottledTaskRunner is generally discouraged in future.
      // TODO(nhiroki): Identify which tasks can be throttled / suspendable and
      // move them into other task runners. See also comments in
      // Get(LocalFrame). (https://crbug.com/670534)
      return TaskRunnerImpl::Create(task_queue_, type);
    case TaskType::kCount:
      NOTREACHED();
      break;
  }
  NOTREACHED();
  return nullptr;
}

}  // namespace scheduler
}  // namespace blink
