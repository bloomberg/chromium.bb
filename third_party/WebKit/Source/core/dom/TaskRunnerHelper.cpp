// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/TaskRunnerHelper.h"

#include "core/dom/Document.h"
#include "core/frame/LocalFrame.h"
#include "platform/WebFrameScheduler.h"
#include "platform/WebTaskRunner.h"
#include "public/platform/Platform.h"
#include "public/platform/WebThread.h"

namespace blink {

RefPtr<WebTaskRunner> TaskRunnerHelper::get(TaskType type, LocalFrame* frame) {
  // TODO(haraken): Optimize the mapping from TaskTypes to task runners.
  switch (type) {
    case TaskType::Timer:
      return frame ? frame->frameScheduler()->timerTaskRunner()
                   : Platform::current()->currentThread()->getWebTaskRunner();
    case TaskType::UnspecedLoading:
    case TaskType::Networking:
    case TaskType::DatabaseAccess:
      return frame ? frame->frameScheduler()->loadingTaskRunner()
                   : Platform::current()->currentThread()->getWebTaskRunner();
    // Throttling following tasks may break existing web pages, so tentatively
    // these are unthrottled.
    // TODO(nhiroki): Throttle them again after we're convinced that it's safe
    // or provide a mechanism that web pages can opt-out it if throttling is not
    // desirable.
    case TaskType::DOMManipulation:
    case TaskType::UserInteraction:
    case TaskType::HistoryTraversal:
    case TaskType::Embed:
    case TaskType::MediaElementEvent:
    case TaskType::CanvasBlobSerialization:
    case TaskType::RemoteEvent:
    case TaskType::WebSocket:
    case TaskType::Microtask:
    case TaskType::PostedMessage:
    case TaskType::UnshippedPortMessage:
    case TaskType::FileReading:
    case TaskType::Presentation:
    case TaskType::Sensor:
    case TaskType::PerformanceTimeline:
    case TaskType::WebGL:
    case TaskType::UnspecedTimer:
    case TaskType::MiscPlatformAPI:
    case TaskType::Unthrottled:
      return frame ? frame->frameScheduler()->unthrottledTaskRunner()
                   : Platform::current()->currentThread()->getWebTaskRunner();
  }
  NOTREACHED();
  return nullptr;
}

RefPtr<WebTaskRunner> TaskRunnerHelper::get(TaskType type, Document* document) {
  return get(type, document ? document->frame() : nullptr);
}

RefPtr<WebTaskRunner> TaskRunnerHelper::get(
    TaskType type,
    ExecutionContext* executionContext) {
  return get(type, executionContext && executionContext->isDocument()
                       ? static_cast<Document*>(executionContext)
                       : nullptr);
}

RefPtr<WebTaskRunner> TaskRunnerHelper::get(TaskType type,
                                            ScriptState* scriptState) {
  return get(type, scriptState ? scriptState->getExecutionContext() : nullptr);
}

}  // namespace blink
