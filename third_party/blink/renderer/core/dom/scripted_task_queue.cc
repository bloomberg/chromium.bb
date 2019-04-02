// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/scripted_task_queue.h"

#include <memory>
#include <utility>

#include "base/macros.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_task_queue_post_callback.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cancellable_task.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

class ScriptedTaskQueue::WrappedCallback
    : public GarbageCollectedFinalized<WrappedCallback> {
 public:
  WrappedCallback(V8TaskQueuePostCallback* callback,
                  ScriptPromiseResolver* resolver,
                  TaskHandle task_handle)
      : callback_(callback),
        resolver_(resolver),
        task_handle_(std::move(task_handle)) {}

  void Trace(Visitor* visitor) {
    visitor->Trace(callback_);
    visitor->Trace(resolver_);
  }

  void Invoke() {
    callback_->InvokeAndReportException(nullptr);
    resolver_->Resolve();
  }

  void Reject() { resolver_->Reject(); }

 private:
  Member<V8TaskQueuePostCallback> callback_;
  Member<ScriptPromiseResolver> resolver_;
  TaskHandle task_handle_;

  DISALLOW_COPY_AND_ASSIGN(WrappedCallback);
};

ScriptedTaskQueue::ScriptedTaskQueue(ExecutionContext* context,
                                     TaskType task_type)
    : ContextLifecycleObserver(context) {
  task_runner_ = GetExecutionContext()->GetTaskRunner(task_type);
}

void ScriptedTaskQueue::Trace(Visitor* visitor) {
  visitor->Trace(pending_tasks_);
  ScriptWrappable::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
}

ScriptPromise ScriptedTaskQueue::postTask(ScriptState* script_state,
                                          V8TaskQueuePostCallback* callback,
                                          AbortSignal* signal) {
  CallbackId id = next_callback_id_++;

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);

  if (signal) {
    if (signal->aborted()) {
      resolver->Reject();
      return resolver->Promise();
    }

    signal->AddAlgorithm(
        WTF::Bind(&ScriptedTaskQueue::AbortTask, WrapPersistent(this), id));
  }

  TaskHandle task_handle = PostCancellableTask(
      *task_runner_, FROM_HERE,
      WTF::Bind(&ScriptedTaskQueue::CallbackFired, WrapPersistent(this), id));

  pending_tasks_.Set(id, MakeGarbageCollected<WrappedCallback>(
                             callback, resolver, std::move(task_handle)));

  return resolver->Promise();
}

void ScriptedTaskQueue::CallbackFired(CallbackId id) {
  auto task_iter = pending_tasks_.find(id);
  if (task_iter == pending_tasks_.end())
    return;

  task_iter->value->Invoke();
  // Can't use the iterator here since running the task
  // might invalidate it.
  pending_tasks_.erase(id);
}

void ScriptedTaskQueue::AbortTask(CallbackId id) {
  auto task_iter = pending_tasks_.find(id);
  if (task_iter == pending_tasks_.end())
    return;

  task_iter->value->Reject();
  pending_tasks_.erase(id);
}

void ScriptedTaskQueue::ContextDestroyed(ExecutionContext*) {
  pending_tasks_.clear();
}

}  // namespace blink
