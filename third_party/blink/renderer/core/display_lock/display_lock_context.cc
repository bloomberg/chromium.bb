// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/display_lock/display_lock_context.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_display_lock_callback.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_suspended_handle.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/bindings/microtask.h"

namespace blink {

// Change this to 1 to enable each function stderr logging.
// TODO(vmpstr): Remove this after debugging is all done.
#if 0
class CORE_EXPORT DisplayLockScopedLogger {
 public:
  DisplayLockScopedLogger(const char* function,
                          const DisplayLockContext::State* state,
                          const DisplayLockContext::LifecycleUpdateState* lifecycle_update_state)
      : function_(function), state_(state), lifecycle_update_state_(lifecycle_update_state) {
    for (int i = 0; i < s_indent_; ++i)
      fprintf(stderr, " ");
    fprintf(stderr, "entering %s: state %s lifecycle_update_state %s\n",
            function, StateToString(*state),
            LifecycleUpdateStateToString(*lifecycle_update_state));
    ++s_indent_;
  }

  ~DisplayLockScopedLogger() {
    --s_indent_;
    for (int i = 0; i < s_indent_; ++i)
      fprintf(stderr, " ");
    fprintf(stderr, "exiting %s: state %s lifecycle_update_state %s\n",
            function_, StateToString(*state_),
            LifecycleUpdateStateToString(*lifecycle_update_state_));
  }

 private:
  const char* StateToString(DisplayLockContext::State state) {
    switch (state) {
      case DisplayLockContext::kUninitialized:
        return "kUninitialized";
      case DisplayLockContext::kSuspended:
        return "kSuspended";
      case DisplayLockContext::kCallbacksPending:
        return "kCallbacksPending";
      case DisplayLockContext::kDisconnected:
        return "kDisconnected";
      case DisplayLockContext::kCommitting:
        return "kCommitting";
      case DisplayLockContext::kResolving:
        return "kResolving";
      case DisplayLockContext::kResolved:
        return "kResolved";
    }
    return "<unknown>";
  };

  const char* LifecycleUpdateStateToString(
      DisplayLockContext::LifecycleUpdateState state) {
    switch (state) {
      case DisplayLockContext::kNeedsLayout:
        return "kNeedsLayout";
      case DisplayLockContext::kNeedsPrePaint:
        return "kNeedsPrePaint";
      case DisplayLockContext::kNeedsPaint:
        return "kNeedsPaint";
      case DisplayLockContext::kDone:
        return "kDone";
    }
    return "<unknown>";
  }

  const char* function_;
  const DisplayLockContext::State* state_;
  const DisplayLockContext::LifecycleUpdateState* lifecycle_update_state_;
  static int s_indent_;
};

int DisplayLockScopedLogger::s_indent_ = 0;

#define SCOPED_LOGGER(func) \
  DisplayLockScopedLogger logger(func, &state_, &lifecycle_update_state_)
#else
#define SCOPED_LOGGER(func)
#endif  // #if 0

DisplayLockContext::DisplayLockContext(Element* element,
                                       ExecutionContext* context)
    : ContextLifecycleObserver(context), element_(element) {
  SCOPED_LOGGER(__PRETTY_FUNCTION__);
}

DisplayLockContext::~DisplayLockContext() {
  SCOPED_LOGGER(__PRETTY_FUNCTION__);
  DCHECK(state_ == kResolved || state_ == kSuspended) << state_;
  DCHECK(callbacks_.IsEmpty());
}

void DisplayLockContext::Trace(blink::Visitor* visitor) {
  SCOPED_LOGGER(__PRETTY_FUNCTION__);
  ScriptWrappable::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
  visitor->Trace(callbacks_);
  visitor->Trace(resolver_);
  visitor->Trace(element_);
}

void DisplayLockContext::Dispose() {
  SCOPED_LOGGER(__PRETTY_FUNCTION__);
  RejectAndCleanUp();
}

void DisplayLockContext::ContextDestroyed(ExecutionContext*) {
  SCOPED_LOGGER(__PRETTY_FUNCTION__);
  RejectAndCleanUp();
}

bool DisplayLockContext::HasPendingActivity() const {
  SCOPED_LOGGER(__PRETTY_FUNCTION__);
  // If we haven't resolved it, we should stick around.
  return !IsResolved();
}

void DisplayLockContext::ScheduleCallback(V8DisplayLockCallback* callback) {
  SCOPED_LOGGER(__PRETTY_FUNCTION__);
  callbacks_.push_back(callback);

  // Suspended state supercedes any new lock requests.
  if (state_ == kSuspended)
    return;
  state_ = kCallbacksPending;

  ScheduleTaskIfNeeded();
}

void DisplayLockContext::RequestLock(V8DisplayLockCallback* callback,
                                     ScriptState* script_state) {
  SCOPED_LOGGER(__PRETTY_FUNCTION__);
  if (!resolver_) {
    DCHECK(script_state);
    resolver_ = ScriptPromiseResolver::Create(script_state);
  }
  ScheduleCallback(callback);
}

void DisplayLockContext::schedule(V8DisplayLockCallback* callback) {
  SCOPED_LOGGER(__PRETTY_FUNCTION__);
  DCHECK(state_ == kSuspended || state_ == kCallbacksPending);

  ScheduleCallback(callback);
}

DisplayLockSuspendedHandle* DisplayLockContext::suspend() {
  SCOPED_LOGGER(__PRETTY_FUNCTION__);
  ++suspended_count_;
  state_ = kSuspended;
  return new DisplayLockSuspendedHandle(this);
}

void DisplayLockContext::ProcessQueue() {
  SCOPED_LOGGER(__PRETTY_FUNCTION__);
  // It's important to clear this before running the tasks, since the tasks can
  // call ScheduleCallback() which will re-schedule a PostTask() for us to
  // continue the work.
  process_queue_task_scheduled_ = false;

  // If we're not in callbacks pending, then we shouldn't run anything at the
  // moment.
  if (state_ != kCallbacksPending)
    return;

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

  if (callbacks_.IsEmpty() && state_ != kSuspended) {
    if (element_->isConnected()) {
      StartCommit();
    } else {
      state_ = kDisconnected;
    }
  }
}

void DisplayLockContext::RejectAndCleanUp() {
  SCOPED_LOGGER(__PRETTY_FUNCTION__);
  if (resolver_) {
    state_ = kResolved;
    resolver_->Reject();
    resolver_ = nullptr;
  }
  callbacks_.clear();
}

void DisplayLockContext::Resume() {
  SCOPED_LOGGER(__PRETTY_FUNCTION__);
  DCHECK_GT(suspended_count_, 0u);
  DCHECK_EQ(state_, kSuspended);
  if (--suspended_count_ == 0) {
    // When becoming unsuspended here are the possible transitions:
    // - If there are callbacks to run, then pending callbacks
    // - If we're not connected, then we're soft suspended, ie disconnected.
    // - Otherwise, we can start committing.
    if (!callbacks_.IsEmpty()) {
      state_ = kCallbacksPending;
    } else if (element_->isConnected()) {
      StartCommit();
    } else {
      state_ = kDisconnected;
    }
  }
  ScheduleTaskIfNeeded();
}

void DisplayLockContext::NotifyWillNotResume() {
  SCOPED_LOGGER(__PRETTY_FUNCTION__);
  DCHECK_GT(suspended_count_, 0u);
  // The promise will never reject or resolve since we're now indefinitely
  // suspended.
  // TODO(vmpstr): We should probably issue a console warning.
  // We keep the state as suspended.
  DCHECK_EQ(state_, kSuspended);
  resolver_->Detach();
  resolver_ = nullptr;
}

void DisplayLockContext::ScheduleTaskIfNeeded() {
  SCOPED_LOGGER(__PRETTY_FUNCTION__);
  if (state_ != kCallbacksPending || process_queue_task_scheduled_)
    return;

  DCHECK(GetExecutionContext());
  DCHECK(GetExecutionContext()->GetTaskRunner(TaskType::kMiscPlatformAPI));
  GetExecutionContext()
      ->GetTaskRunner(TaskType::kMiscPlatformAPI)
      ->PostTask(FROM_HERE, WTF::Bind(&DisplayLockContext::ProcessQueue,
                                      WrapWeakPersistent(this)));
  process_queue_task_scheduled_ = true;
}

void DisplayLockContext::NotifyConnectedMayHaveChanged() {
  SCOPED_LOGGER(__PRETTY_FUNCTION__);
  if (element_->isConnected()) {
    if (state_ == kDisconnected)
      StartCommit();
    return;
  }
  // All other states should remain as they are. Specifically, if we're
  // acquiring the lock then we should finish doing so; if we're resolving, then
  // we should finish that as well.
  if (state_ == kCommitting)
    state_ = kDisconnected;
}

bool DisplayLockContext::ShouldLayout() const {
  SCOPED_LOGGER(__PRETTY_FUNCTION__);
  return state_ >= kCommitting;
}

void DisplayLockContext::DidLayout() {
  SCOPED_LOGGER(__PRETTY_FUNCTION__);
  if (state_ != kCommitting)
    return;

  if (lifecycle_update_state_ <= kNeedsLayout)
    lifecycle_update_state_ = kNeedsPrePaint;
}

bool DisplayLockContext::ShouldPrePaint() const {
  SCOPED_LOGGER(__PRETTY_FUNCTION__);
  return state_ >= kCommitting && lifecycle_update_state_ >= kNeedsPrePaint;
}

void DisplayLockContext::DidPrePaint() {
  SCOPED_LOGGER(__PRETTY_FUNCTION__);
  if (state_ != kCommitting)
    return;

  if (lifecycle_update_state_ <= kNeedsPrePaint)
    lifecycle_update_state_ = kNeedsPaint;
}

bool DisplayLockContext::ShouldPaint() const {
  SCOPED_LOGGER(__PRETTY_FUNCTION__);
  return state_ >= kCommitting && lifecycle_update_state_ >= kNeedsPaint;
}

void DisplayLockContext::DidPaint() {
  SCOPED_LOGGER(__PRETTY_FUNCTION__);
  if (state_ != kCommitting)
    return;

  if (lifecycle_update_state_ <= kNeedsPaint)
    lifecycle_update_state_ = kDone;

  DCHECK(resolver_);
  state_ = kResolving;
  resolver_->Resolve();
  resolver_ = nullptr;

  // After the above resolution callback runs (in a microtask), we should
  // finish resolving if the lock was not re-acquired.
  Microtask::EnqueueMicrotask(WTF::Bind(&DisplayLockContext::FinishResolution,
                                        WrapWeakPersistent(this)));
}

void DisplayLockContext::FinishResolution() {
  SCOPED_LOGGER(__PRETTY_FUNCTION__);
  if (state_ == kResolving)
    state_ = kResolved;
}

void DisplayLockContext::StartCommit() {
  SCOPED_LOGGER(__PRETTY_FUNCTION__);
  state_ = kCommitting;
  lifecycle_update_state_ = kNeedsLayout;
  element_->GetLayoutObject()->SetNeedsLayout(
      LayoutInvalidationReason::kDisplayLockCommitting);
  if (auto* parent = element_->GetLayoutObject()->Parent()) {
    parent->SetNeedsLayout(LayoutInvalidationReason::kDisplayLockCommitting);
  }
  element_->GetDocument().GetPage()->Animator().ScheduleVisualUpdate(
      element_->GetDocument().GetFrame());
  DCHECK(element_->GetLayoutObject());
}

}  // namespace blink
