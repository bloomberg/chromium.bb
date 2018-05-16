// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/child/worker_scheduler.h"

#include "third_party/blink/renderer/platform/scheduler/child/task_queue_with_task_type.h"
#include "third_party/blink/renderer/platform/scheduler/public/non_main_thread_scheduler.h"

namespace blink {
namespace scheduler {

WorkerScheduler::WorkerScheduler(
    NonMainThreadScheduler* non_main_thread_scheduler) {
  task_queue_ = non_main_thread_scheduler->CreateTaskRunner();
}

WorkerScheduler::~WorkerScheduler() {
#if DCHECK_IS_ON()
  DCHECK(is_disposed_);
#endif
}

std::unique_ptr<FrameOrWorkerScheduler::ActiveConnectionHandle>
WorkerScheduler::OnActiveConnectionCreated() {
  return nullptr;
}

void WorkerScheduler::Dispose() {
  task_queue_->ShutdownTaskQueue();
#if DCHECK_IS_ON()
  is_disposed_ = true;
#endif
}

scoped_refptr<base::SingleThreadTaskRunner> WorkerScheduler::GetTaskRunner(
    TaskType type) const {
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
    case TaskType::kInternalDefault:
    case TaskType::kInternalLoading:
    case TaskType::kUnthrottled:
    case TaskType::kInternalTest:
    case TaskType::kInternalWebCrypto:
    case TaskType::kInternalIndexedDB:
    case TaskType::kInternalMedia:
    case TaskType::kInternalMediaRealTime:
    case TaskType::kInternalIPC:
    case TaskType::kInternalUserInteraction:
    case TaskType::kInternalInspector:
    case TaskType::kInternalWorker:
      // UnthrottledTaskRunner is generally discouraged in future.
      // TODO(nhiroki): Identify which tasks can be throttled / suspendable and
      // move them into other task runners. See also comments in
      // Get(LocalFrame). (https://crbug.com/670534)
      return TaskQueueWithTaskType::Create(task_queue_, type);
    case TaskType::kMainThreadTaskQueueV8:
    case TaskType::kMainThreadTaskQueueCompositor:
    case TaskType::kCount:
      NOTREACHED();
      break;
  }
  NOTREACHED();
  return nullptr;
}

}  // namespace scheduler
}  // namespace blink
