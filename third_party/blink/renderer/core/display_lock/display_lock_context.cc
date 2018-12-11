// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/display_lock/display_lock_context.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_display_lock_callback.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_suspended_handle.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
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
      case DisplayLockContext::kNeedsStyle:
        return "kNeedsStyle";
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
    : ContextLifecycleObserver(context),
      element_(element),
      weak_element_handle_(element) {
  SCOPED_LOGGER(__PRETTY_FUNCTION__);
}

DisplayLockContext::~DisplayLockContext() {
  SCOPED_LOGGER(__PRETTY_FUNCTION__);
  DCHECK(state_ == kResolved || state_ == kSuspended) << state_;
  DCHECK(callbacks_.IsEmpty());
}

void DisplayLockContext::Trace(blink::Visitor* visitor) {
  SCOPED_LOGGER(__PRETTY_FUNCTION__);
  visitor->Trace(callbacks_);
  visitor->Trace(resolver_);
  visitor->Trace(element_);
  visitor->Trace(weak_element_handle_);
  ScriptWrappable::Trace(visitor);
  ActiveScriptWrappable::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
}

void DisplayLockContext::Dispose() {
  SCOPED_LOGGER(__PRETTY_FUNCTION__);
  // Note that if we have a resolver_ at dispose time, then it's too late to
  // reject the promise, since we are not allowed to create new strong
  // references to objects already set for destruction (and rejecting would do
  // this since the rejection has to be deferred). We need to detach instead.
  // TODO(vmpstr): See if there is another earlier time we can detect that we're
  // going to be disposed.
  if (resolver_) {
    resolver_->Detach();
    resolver_ = nullptr;
    state_ = kResolved;
  }
  RejectAndCleanUp();
}

void DisplayLockContext::ContextDestroyed(ExecutionContext*) {
  SCOPED_LOGGER(__PRETTY_FUNCTION__);
  RejectAndCleanUp();
}

bool DisplayLockContext::HasPendingActivity() const {
  SCOPED_LOGGER(__PRETTY_FUNCTION__);
  // If we haven't resolved it, we should stick around.
  // Note that if the element we're locking is gone, then we can also be GCed.
  return !IsResolved() && weak_element_handle_;
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
  return MakeGarbageCollected<DisplayLockSuspendedHandle>(this);
}

Element* DisplayLockContext::lockedElement() const {
  return GetElement();
}

void DisplayLockContext::ProcessQueue() {
  SCOPED_LOGGER(__PRETTY_FUNCTION__);
  // It's important to clear this before running the tasks, since the tasks can
  // call ScheduleCallback() which will re-schedule a PostTask() for us to
  // continue the work.
  process_queue_task_scheduled_ = false;

  // This is the first operation we do on a locked subtree, and a guaranteed
  // operation before we try to use it, so save off the locked_frame_rect_ here
  // if we don't have one yet.
  // TODO(vmpstr): In mode 1, we should add an explicit states for lock pending
  // and lock acquired.
  if (!locked_frame_rect_) {
    auto* layout_object = GetElement()->GetLayoutObject();
    if (layout_object && layout_object->IsBox()) {
      locked_frame_rect_ = ToLayoutBox(layout_object)->FrameRect();
    } else {
      locked_frame_rect_ = LayoutRect();
    }
  }

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
        // We should run the checkpoint here, since the rejection callback (for
        // the promise rejected in RejectAndCleanUp()) may modify DOM which
        // should happen here, as opposed to after a potential lifecycle update
        // (or whenever the next microtask checkpoint is going to happen).
        Microtask::PerformCheckpoint(callback->GetIsolate());
        return;
      }
    }
  }

  if (callbacks_.IsEmpty() && state_ != kSuspended) {
    if (GetElement()->isConnected()) {
      StartCommit();
    } else {
      MarkAsDisconnected();
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
  locked_frame_rect_.reset();

  // We may have a dirty subtree and have not propagated the dirty bit up the
  // ancestor tree. Since we're now rejecting the promise and unlocking the
  // element, ensure that we can reach both style and layout subtrees if they
  // are dirty by propagating the bit.
  if (weak_element_handle_) {
    MarkAncestorsForStyleRecalcIfNeeded();
    MarkAncestorsForLayoutIfNeeded();
  }
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
    } else if (GetElement()->isConnected()) {
      StartCommit();
    } else {
      MarkAsDisconnected();
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
  element_ = nullptr;
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
  if (GetElement()->isConnected()) {
    if (state_ == kDisconnected)
      StartCommit();
    // Now that we're connected, we can hold a strong reference. See comment on
    // |element_| in the header.
    element_ = weak_element_handle_;
    return;
  }
  // All other states should remain as they are. Specifically, if we're
  // acquiring the lock then we should finish doing so; if we're resolving, then
  // we should finish that as well.
  if (state_ == kCommitting)
    MarkAsDisconnected();
}

bool DisplayLockContext::ShouldStyle() const {
  SCOPED_LOGGER(__PRETTY_FUNCTION__);
  return update_forced_ || state_ >= kCommitting;
}

void DisplayLockContext::DidStyle() {
  SCOPED_LOGGER(__PRETTY_FUNCTION__);
  if (state_ != kCommitting && !update_forced_)
    return;

  // We must have contain: content for display locking.
  // Note that we should also have content containment even if we're forcing
  // this update to happen. Otherwise, proceeding with layout may cause
  // unexpected behavior. By rejecting the promise, the behavior can be detected
  // by script.
  auto* style = GetElement()->GetComputedStyle();
  if (!style || !style->ContainsContent()) {
    RejectAndCleanUp();
    return;
  }

  if (state_ != kCommitting)
    return;

  if (lifecycle_update_state_ <= kNeedsStyle) {
    // Normally we need to do layout next, but if it's not dirty then we can
    // skip ahead to pre-paint.
    if (MarkAncestorsForLayoutIfNeeded())
      lifecycle_update_state_ = kNeedsLayout;
    else
      lifecycle_update_state_ = kNeedsPrePaint;
  }
}

bool DisplayLockContext::ShouldLayout() const {
  SCOPED_LOGGER(__PRETTY_FUNCTION__);
  return update_forced_ ||
         std::tie(state_, lifecycle_update_state_) >=
             std::tuple<State, LifecycleUpdateState>{kCommitting, kNeedsLayout};
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
  return update_forced_ || std::tie(state_, lifecycle_update_state_) >=
                               std::tuple<State, LifecycleUpdateState>{
                                   kCommitting, kNeedsPrePaint};
}

void DisplayLockContext::DidPrePaint() {
  SCOPED_LOGGER(__PRETTY_FUNCTION__);
  if (state_ != kCommitting)
    return;

  // Since we should be under containment, we should have a layer. If we don't,
  // then paint might not happen and we'll never resolve.
  DCHECK(GetElement()->GetLayoutObject()->HasLayer());
  if (lifecycle_update_state_ <= kNeedsPrePaint)
    lifecycle_update_state_ = kNeedsPaint;
}

bool DisplayLockContext::ShouldPaint() const {
  SCOPED_LOGGER(__PRETTY_FUNCTION__);
  // Note that forced updates should never require us to paint, so we don't
  // check |update_forced_| here. In other words, although |update_forced_|
  // could be true here, we still should not paint.
  return std::tie(state_, lifecycle_update_state_) >=
         std::tuple<State, LifecycleUpdateState>{kCommitting, kNeedsPaint};
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

void DisplayLockContext::DidAttachLayoutTree() {
  SCOPED_LOGGER(__PRETTY_FUNCTION__);

  // Note that although we checked at style recalc time that the element has
  // "contain: content", it might not actually apply the containment (e.g. see
  // ShouldApplyContentContainment()). This confirms that containment should
  // apply.
  auto* layout_object = GetElement()->GetLayoutObject();
  if (!layout_object || !layout_object->ShouldApplyContentContainment())
    RejectAndCleanUp();
}

DisplayLockContext::ScopedPendingFrameRect
DisplayLockContext::GetScopedPendingFrameRect() {
  SCOPED_LOGGER(__PRETTY_FUNCTION__);
  if (IsResolved())
    return ScopedPendingFrameRect(nullptr);
  DCHECK(GetElement()->GetLayoutObject() && GetElement()->GetLayoutBox());
  GetElement()->GetLayoutBox()->SetFrameRectForDisplayLock(pending_frame_rect_);
  return ScopedPendingFrameRect(this);
}

void DisplayLockContext::NotifyPendingFrameRectScopeEnded() {
  SCOPED_LOGGER(__PRETTY_FUNCTION__);
  DCHECK(GetElement()->GetLayoutObject() && GetElement()->GetLayoutBox());
  DCHECK(locked_frame_rect_);
  pending_frame_rect_ = GetElement()->GetLayoutBox()->FrameRect();
  GetElement()->GetLayoutBox()->SetFrameRectForDisplayLock(*locked_frame_rect_);
}

DisplayLockContext::ScopedForcedUpdate
DisplayLockContext::GetScopedForcedUpdate() {
  SCOPED_LOGGER(__PRETTY_FUNCTION__);
  DCHECK(!update_forced_);
  update_forced_ = true;
  // Now that the update is forced, we should ensure that style and layout code
  // can reach it via dirty bits.
  MarkAncestorsForStyleRecalcIfNeeded();
  MarkAncestorsForLayoutIfNeeded();
  return ScopedForcedUpdate(this);
}

void DisplayLockContext::NotifyForcedUpdateScopeEnded() {
  SCOPED_LOGGER(__PRETTY_FUNCTION__);
  DCHECK(update_forced_);
  update_forced_ = false;
}

void DisplayLockContext::FinishResolution() {
  SCOPED_LOGGER(__PRETTY_FUNCTION__);
  if (state_ == kResolving)
    state_ = kResolved;
  locked_frame_rect_.reset();

  // Update the pending frame rect if we still have a layout object.

  // First, if the element is gone, then there's nothing to do.
  if (!weak_element_handle_)
    return;
  auto* layout_object = GetElement()->GetLayoutObject();
  // The resolution could have disconnected us, which would destroy the layout
  // object.
  if (!layout_object || !layout_object->IsBox())
    return;
  bool frame_rect_changed =
      ToLayoutBox(layout_object)->FrameRect() != pending_frame_rect_;
  // If the frame rect hasn't actually changed then we don't need to do
  // anything.
  // TODO(vmpstr): Is this true? What about things like paint? This also needs
  // to be updated if there are more "stashed" things.
  if (!frame_rect_changed)
    return;

  // Set the pending frame rect as the new one, and ensure to schedule a layout
  // for just the box itself. Note that we use the non-display locked version to
  // ensure all the hooks are property invoked.
  ToLayoutBox(layout_object)->SetFrameRect(pending_frame_rect_);
  layout_object->SetNeedsLayout(
      layout_invalidation_reason::kDisplayLockCommitting);
  // Schedule an animation to perform the lifecycle phases.
  GetElement()->GetDocument().GetPage()->Animator().ScheduleVisualUpdate(
      GetElement()->GetDocument().GetFrame());
}

void DisplayLockContext::StartCommit() {
  SCOPED_LOGGER(__PRETTY_FUNCTION__);
  state_ = kCommitting;
  lifecycle_update_state_ = kNeedsStyle;

  if (!MarkAncestorsForStyleRecalcIfNeeded())
    DidStyle();

  // The above DidStyle() may reject the promise since it checks that we have
  // containment before proceeding.
  if (state_ != kCommitting)
    return;

  // Schedule an animation to perform the lifecycle phases.
  GetElement()->GetDocument().GetPage()->Animator().ScheduleVisualUpdate(
      GetElement()->GetDocument().GetFrame());
}

bool DisplayLockContext::MarkAncestorsForStyleRecalcIfNeeded() {
  if (GetElement()->NeedsStyleRecalc() ||
      GetElement()->ChildNeedsStyleRecalc()) {
    GetElement()->MarkAncestorsWithChildNeedsStyleRecalc();
    return true;
  }
  return false;
}

bool DisplayLockContext::MarkAncestorsForLayoutIfNeeded() {
  if (auto* layout_object = GetElement()->GetLayoutObject()) {
    if (layout_object->NeedsLayout()) {
      layout_object->MarkContainerChainForLayout();
      return true;
    }
  }
  return false;
}

void DisplayLockContext::MarkAsDisconnected() {
  SCOPED_LOGGER(__PRETTY_FUNCTION__);
  state_ = kDisconnected;
  // Let go of the strong reference to the element, allowing it to be GCed. See
  // the comment on |element_| in the header.
  element_ = nullptr;
}

DisplayLockContext::ScopedPendingFrameRect::ScopedPendingFrameRect(
    DisplayLockContext* context)
    : context_(context) {}

DisplayLockContext::ScopedPendingFrameRect::ScopedPendingFrameRect(
    ScopedPendingFrameRect&& other)
    : context_(other.context_) {
  other.context_ = nullptr;
}

DisplayLockContext::ScopedPendingFrameRect::~ScopedPendingFrameRect() {
  if (context_)
    context_->NotifyPendingFrameRectScopeEnded();
}

DisplayLockContext::ScopedForcedUpdate::ScopedForcedUpdate(
    DisplayLockContext* context)
    : context_(context) {}

DisplayLockContext::ScopedForcedUpdate::ScopedForcedUpdate(
    ScopedForcedUpdate&& other)
    : context_(other.context_) {
  other.context_ = nullptr;
}

DisplayLockContext::ScopedForcedUpdate::~ScopedForcedUpdate() {
  if (context_)
    context_->NotifyForcedUpdateScopeEnded();
}

}  // namespace blink
