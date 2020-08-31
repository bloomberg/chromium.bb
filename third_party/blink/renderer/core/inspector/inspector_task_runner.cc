// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/inspector_task_runner.h"

#include "third_party/blink/renderer/core/inspector/thread_debugger.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

InspectorTaskRunner::InspectorTaskRunner(
    scoped_refptr<base::SingleThreadTaskRunner> isolate_task_runner)
    : isolate_task_runner_(isolate_task_runner) {}

InspectorTaskRunner::~InspectorTaskRunner() = default;

void InspectorTaskRunner::InitIsolate(v8::Isolate* isolate) {
  MutexLocker lock(mutex_);
  isolate_ = isolate;
}

void InspectorTaskRunner::Dispose() {
  MutexLocker lock(mutex_);
  disposed_ = true;
  isolate_ = nullptr;
  isolate_task_runner_ = nullptr;
}

void InspectorTaskRunner::AppendTask(Task task) {
  MutexLocker lock(mutex_);
  if (disposed_)
    return;
  interrupting_task_queue_.push_back(std::move(task));
  PostCrossThreadTask(
      *isolate_task_runner_, FROM_HERE,
      CrossThreadBindOnce(
          &InspectorTaskRunner::PerformSingleInterruptingTaskDontWait,
          WrapRefCounted(this)));
  if (isolate_) {
    AddRef();
    isolate_->RequestInterrupt(&V8InterruptCallback, this);
  }
}

void InspectorTaskRunner::AppendTaskDontInterrupt(Task task) {
  MutexLocker lock(mutex_);
  if (disposed_)
    return;
  PostCrossThreadTask(*isolate_task_runner_, FROM_HERE, std::move(task));
}

InspectorTaskRunner::Task InspectorTaskRunner::TakeNextInterruptingTask() {
  MutexLocker lock(mutex_);

  if (disposed_ || interrupting_task_queue_.IsEmpty())
    return Task();

  return interrupting_task_queue_.TakeFirst();
}

void InspectorTaskRunner::PerformSingleInterruptingTaskDontWait() {
  Task task = TakeNextInterruptingTask();
  if (task) {
    DCHECK(isolate_task_runner_->BelongsToCurrentThread());
    std::move(task).Run();
  }
}

void InspectorTaskRunner::V8InterruptCallback(v8::Isolate*, void* data) {
  InspectorTaskRunner* runner = static_cast<InspectorTaskRunner*>(data);
  Task task = runner->TakeNextInterruptingTask();
  runner->Release();
  if (!task) {
    return;
  }
  std::move(task).Run();
}

}  // namespace blink
