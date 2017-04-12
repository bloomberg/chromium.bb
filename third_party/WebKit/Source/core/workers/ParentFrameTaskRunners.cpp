// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/workers/ParentFrameTaskRunners.h"

#include "core/dom/Document.h"
#include "core/frame/LocalFrame.h"
#include "platform/wtf/Assertions.h"
#include "platform/wtf/ThreadingPrimitives.h"
#include "public/platform/Platform.h"

namespace blink {

ParentFrameTaskRunners::ParentFrameTaskRunners(LocalFrame* frame)
    : ContextLifecycleObserver(frame ? frame->GetDocument() : nullptr) {
  if (frame && frame->GetDocument())
    DCHECK(frame->GetDocument()->IsContextThread());

  // For now we only support very limited task types.
  for (auto type :
       {TaskType::kUnspecedTimer, TaskType::kUnspecedLoading,
        TaskType::kNetworking, TaskType::kPostedMessage,
        TaskType::kCanvasBlobSerialization, TaskType::kUnthrottled}) {
    task_runners_.insert(type, TaskRunnerHelper::Get(type, frame));
  }
}

RefPtr<WebTaskRunner> ParentFrameTaskRunners::Get(TaskType type) {
  MutexLocker lock(task_runners_mutex_);
  return task_runners_.at(type);
}

DEFINE_TRACE(ParentFrameTaskRunners) {
  ContextLifecycleObserver::Trace(visitor);
}

void ParentFrameTaskRunners::ContextDestroyed(ExecutionContext*) {
  MutexLocker lock(task_runners_mutex_);
  for (auto& entry : task_runners_)
    entry.value = Platform::Current()->CurrentThread()->GetWebTaskRunner();
}

}  // namespace blink
