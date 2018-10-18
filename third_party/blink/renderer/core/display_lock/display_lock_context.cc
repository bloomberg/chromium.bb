// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/display_lock/display_lock_context.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_display_lock_callback.h"
#include "third_party/blink/renderer/platform/bindings/microtask.h"

namespace blink {

DisplayLockContext::DisplayLockContext(ExecutionContext* context)
    : ContextLifecycleObserver(context) {}

DisplayLockContext::~DisplayLockContext() {
  DCHECK(!resolver_);
  DCHECK(callbacks_.IsEmpty());
}

void DisplayLockContext::Trace(blink::Visitor* visitor) {
  ScriptWrappable::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
  visitor->Trace(callbacks_);
  visitor->Trace(resolver_);
}

void DisplayLockContext::Dispose() {
  RejectAndCleanUp();
}

void DisplayLockContext::ContextDestroyed(ExecutionContext*) {
  RejectAndCleanUp();
}

bool DisplayLockContext::HasPendingActivity() const {
  // If we don't have a task scheduled, then we should already be resolved.
  // TODO(vmpstr): This should also be kept alive if we're doing co-operative
  // work.
  DCHECK(process_queue_task_scheduled_ || !resolver_);
  return process_queue_task_scheduled_;
}

void DisplayLockContext::ScheduleTask(V8DisplayLockCallback* callback,
                                      ScriptState* script_state) {
  callbacks_.push_back(callback);
  if (!resolver_) {
    DCHECK(script_state);
    resolver_ = ScriptPromiseResolver::Create(script_state);
  }
  if (!process_queue_task_scheduled_) {
    DCHECK(GetExecutionContext());
    DCHECK(GetExecutionContext()->GetTaskRunner(TaskType::kMiscPlatformAPI));
    GetExecutionContext()
        ->GetTaskRunner(TaskType::kMiscPlatformAPI)
        ->PostTask(FROM_HERE, WTF::Bind(&DisplayLockContext::ProcessQueue,
                                        WrapWeakPersistent(this)));
    process_queue_task_scheduled_ = true;
  }
}

void DisplayLockContext::schedule(V8DisplayLockCallback* callback) {
  ScheduleTask(callback);
}

void DisplayLockContext::ProcessQueue() {
  // It's important to clear this before running the tasks, since the tasks can
  // call ScheduleTask() which will re-schedule a PostTask() for us to continue
  // the work.
  process_queue_task_scheduled_ = false;

  // We might have cleaned up already due to exeuction context being destroyed.
  if (callbacks_.IsEmpty()) {
    DCHECK(!resolver_);
    return;
  }

  // Get a local copy of all the tasks we will run.
  // TODO(vmpstr): This should possibly be subject to a budget instead.
  HeapVector<Member<V8DisplayLockCallback>> callbacks;
  callbacks.swap(callbacks_);

  for (auto& callback : callbacks) {
    DCHECK(callback);
    {
      // A re-implementation of InvokeAndReportException, in order for us to
      // be able to query |try_catch| to determine whether or not we need to
      // reject our promise.
      v8::TryCatch try_catch(callback->GetIsolate());
      try_catch.SetVerbose(true);

      auto result = callback->Invoke(nullptr, this);
      ALLOW_UNUSED_LOCAL(result);
      if (try_catch.HasCaught()) {
        RejectAndCleanUp();
        return;
      }
    }
    Microtask::PerformCheckpoint(callback->GetIsolate());
  }

  // TODO(vmpstr): This should be resolved after all of the co-operative work
  // finishes, not here.
  if (callbacks_.IsEmpty()) {
    DCHECK(!process_queue_task_scheduled_);
    resolver_->Resolve();
    resolver_ = nullptr;
  }
}

void DisplayLockContext::RejectAndCleanUp() {
  if (resolver_) {
    resolver_->Reject();
    resolver_ = nullptr;
  }
  callbacks_.clear();
}

}  // namespace blink
