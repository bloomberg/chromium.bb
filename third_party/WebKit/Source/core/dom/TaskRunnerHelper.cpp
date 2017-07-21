// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/TaskRunnerHelper.h"

#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/LocalFrame.h"
#include "core/workers/MainThreadWorkletGlobalScope.h"
#include "core/workers/WorkerOrWorkletGlobalScope.h"
#include "core/workers/WorkerThread.h"
#include "platform/WebFrameScheduler.h"
#include "platform/WebTaskRunner.h"
#include "public/platform/Platform.h"
#include "public/platform/WebThread.h"

namespace blink {

RefPtr<WebTaskRunner> TaskRunnerHelper::Get(TaskType type, LocalFrame* frame) {
  // TODO(haraken): Optimize the mapping from TaskTypes to task runners.
  switch (type) {
    case TaskType::kTimer:
      return frame ? frame->FrameScheduler()->TimerTaskRunner()
                   : Platform::Current()->CurrentThread()->GetWebTaskRunner();
    case TaskType::kUnspecedLoading:
    case TaskType::kNetworking:
      return frame ? frame->FrameScheduler()->LoadingTaskRunner()
                   : Platform::Current()->CurrentThread()->GetWebTaskRunner();
    case TaskType::kNetworkingControl:
      return frame ? frame->FrameScheduler()->LoadingControlTaskRunner()
                   : Platform::Current()->CurrentThread()->GetWebTaskRunner();
    // Throttling following tasks may break existing web pages, so tentatively
    // these are unthrottled.
    // TODO(nhiroki): Throttle them again after we're convinced that it's safe
    // or provide a mechanism that web pages can opt-out it if throttling is not
    // desirable.
    case TaskType::kDatabaseAccess:
      return frame ? frame->FrameScheduler()->SuspendableTaskRunner()
                   : Platform::Current()->CurrentThread()->GetWebTaskRunner();
    case TaskType::kDOMManipulation:
    case TaskType::kHistoryTraversal:
    case TaskType::kEmbed:
    case TaskType::kMediaElementEvent:
    case TaskType::kCanvasBlobSerialization:
    case TaskType::kRemoteEvent:
    case TaskType::kWebSocket:
    case TaskType::kMicrotask:
    case TaskType::kUnshippedPortMessage:
    case TaskType::kFileReading:
    case TaskType::kPresentation:
    case TaskType::kSensor:
    case TaskType::kPerformanceTimeline:
    case TaskType::kWebGL:
    case TaskType::kUnspecedTimer:
    case TaskType::kMiscPlatformAPI:
      // TODO(altimin): Move all these tasks to suspendable or unthrottled
      // task runner.
      return frame
                 ? frame->FrameScheduler()->UnthrottledButBlockableTaskRunner()
                 : Platform::Current()->CurrentThread()->GetWebTaskRunner();
    // PostedMessage can be used for navigation, so we shouldn't block it
    // when expecting a user gesture.
    case TaskType::kPostedMessage:
    // UserInteraction tasks should be run even when expecting a user gesture.
    case TaskType::kUserInteraction:
    case TaskType::kUnthrottled:
      return frame ? frame->FrameScheduler()->UnthrottledTaskRunner()
                   : Platform::Current()->CurrentThread()->GetWebTaskRunner();
  }
  NOTREACHED();
  return nullptr;
}

RefPtr<WebTaskRunner> TaskRunnerHelper::Get(TaskType type, Document* document) {
  return Get(type, document ? document->GetFrame() : nullptr);
}

RefPtr<WebTaskRunner> TaskRunnerHelper::Get(
    TaskType type,
    ExecutionContext* execution_context) {
  if (!execution_context)
    return Get(type, ToDocument(execution_context));
  if (execution_context->IsDocument())
    return Get(type, ToDocument(execution_context));
  if (execution_context->IsMainThreadWorkletGlobalScope()) {
    return Get(type,
               ToMainThreadWorkletGlobalScope(execution_context)->GetFrame());
  }
  if (execution_context->IsWorkerOrWorkletGlobalScope())
    return Get(type, ToWorkerOrWorkletGlobalScope(execution_context));
  execution_context = nullptr;
  return Get(type, ToDocument(execution_context));
}

RefPtr<WebTaskRunner> TaskRunnerHelper::Get(TaskType type,
                                            ScriptState* script_state) {
  return Get(type,
             script_state ? ExecutionContext::From(script_state) : nullptr);
}

RefPtr<WebTaskRunner> TaskRunnerHelper::Get(
    TaskType type,
    WorkerOrWorkletGlobalScope* global_scope) {
  DCHECK(global_scope);
  DCHECK(global_scope->IsContextThread());
  return Get(type, global_scope->GetThread());
}

RefPtr<WebTaskRunner> TaskRunnerHelper::Get(TaskType type,
                                            WorkerThread* worker_thread) {
  switch (type) {
    case TaskType::kDOMManipulation:
    case TaskType::kUserInteraction:
    case TaskType::kNetworking:
    case TaskType::kNetworkingControl:
    case TaskType::kHistoryTraversal:
    case TaskType::kEmbed:
    case TaskType::kMediaElementEvent:
    case TaskType::kCanvasBlobSerialization:
    case TaskType::kMicrotask:
    case TaskType::kTimer:
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
    case TaskType::kMiscPlatformAPI:
    case TaskType::kUnspecedTimer:
    case TaskType::kUnspecedLoading:
    case TaskType::kUnthrottled:
      // UnthrottledTaskRunner is generally discouraged in future.
      // TODO(nhiroki): Identify which tasks can be throttled / suspendable and
      // move them into other task runners. See also comments in
      // Get(LocalFrame). (https://crbug.com/670534)
      return worker_thread->GetGlobalScopeScheduler()->UnthrottledTaskRunner();
  }
  NOTREACHED();
  return nullptr;
}

}  // namespace blink
