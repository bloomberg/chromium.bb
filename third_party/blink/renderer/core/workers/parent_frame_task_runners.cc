// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/workers/parent_frame_task_runners.h"

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/threading_primitives.h"

namespace blink {

ParentFrameTaskRunners* ParentFrameTaskRunners::Create(
    ExecutionContext* context) {
  DCHECK(context);
  DCHECK(context->IsContextThread());
  return new ParentFrameTaskRunners(context);
}

ParentFrameTaskRunners* ParentFrameTaskRunners::Create() {
  return new ParentFrameTaskRunners(nullptr);
}

ParentFrameTaskRunners::ParentFrameTaskRunners(ExecutionContext* context)
    : ContextLifecycleObserver(context) {
  // For now we only support very limited task types.
  for (auto type : {TaskType::kUnspecedTimer, TaskType::kInternalLoading,
                    TaskType::kNetworking, TaskType::kPostedMessage,
                    TaskType::kUnthrottled, TaskType::kInternalTest}) {
    auto task_runner =
        context ? context->GetTaskRunner(type)
                : Platform::Current()->CurrentThread()->GetTaskRunner();
    task_runners_.insert(type, std::move(task_runner));
  }
}

scoped_refptr<base::SingleThreadTaskRunner> ParentFrameTaskRunners::Get(
    TaskType type) {
  MutexLocker lock(mutex_);
  return task_runners_.at(type);
}

void ParentFrameTaskRunners::Trace(blink::Visitor* visitor) {
  ContextLifecycleObserver::Trace(visitor);
}

void ParentFrameTaskRunners::ContextDestroyed(ExecutionContext*) {
  MutexLocker lock(mutex_);
  for (auto& entry : task_runners_)
    entry.value = Platform::Current()->CurrentThread()->GetTaskRunner();
}

}  // namespace blink
