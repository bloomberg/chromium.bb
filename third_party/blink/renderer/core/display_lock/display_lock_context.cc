// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/display_lock/display_lock_context.h"

#include <string>

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_options.h"
#include "third_party/blink/renderer/core/display_lock/strict_yielding_display_lock_budget.h"
#include "third_party/blink/renderer/core/display_lock/unyielding_display_lock_budget.h"
#include "third_party/blink/renderer/core/display_lock/yielding_display_lock_budget.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/platform/bindings/microtask.h"

namespace blink {

namespace {
// The default timeout for the lock if a timeout is not specified. Defaults to 1
// sec.
double kDefaultLockTimeoutMs = 1000.;

namespace rejection_names {
const char* kElementIsUnlocked = "Element is unlocked.";
const char* kExecutionContextDestroyed = "Execution context destroyed.";
const char* kContainmentNotSatisfied =
    "Containment requirement is not satisfied.";
const char* kElementIsDisconnected = "Element is disconnected.";
const char* kLockCommitted = "Lock commit was requested.";
}  // namespace rejection_names

// Helper function to convert a display locking state to a string. Used in
// traces.
std::string StateToString(DisplayLockContext::State state) {
  switch (state) {
    case DisplayLockContext::kLocked:
      return "kLocked";
    case DisplayLockContext::kUpdating:
      return "kUpdating";
    case DisplayLockContext::kCommitting:
      return "kCommitting";
    case DisplayLockContext::kUnlocked:
      return "kUnlocked";
    case DisplayLockContext::kPendingAcquire:
      return "kPendingAcquire";
  }
  return "";
}

// Helper function that returns an immediately rejected promise.
ScriptPromise GetRejectedPromise(ScriptState* script_state,
                                 const char* rejection_reason) {
  auto* resolver = ScriptPromiseResolver::Create(script_state);
  auto promise = resolver->Promise();
  resolver->Reject(DOMException::Create(DOMExceptionCode::kNotAllowedError,
                                        rejection_reason));
  return promise;
}

// Helper function that returns an immediately resolved promise.
ScriptPromise GetResolvedPromise(ScriptState* script_state) {
  auto* resolver = ScriptPromiseResolver::Create(script_state);
  auto promise = resolver->Promise();
  resolver->Resolve();
  return promise;
}

}  // namespace

DisplayLockContext::DisplayLockContext(Element* element,
                                       ExecutionContext* context)
    : ContextLifecycleObserver(context),
      element_(element),
      document_(&element_->GetDocument()),
      state_(this),
      weak_factory_(this) {
  if (document_->View())
    document_->View()->RegisterForLifecycleNotifications(this);
}

DisplayLockContext::~DisplayLockContext() {
  DCHECK_EQ(state_, kUnlocked);
}

void DisplayLockContext::Trace(blink::Visitor* visitor) {
  visitor->Trace(update_resolver_);
  visitor->Trace(commit_resolver_);
  visitor->Trace(acquire_resolver_);
  visitor->Trace(element_);
  visitor->Trace(document_);
  visitor->Trace(whitespace_reattach_set_);
  ScriptWrappable::Trace(visitor);
  ActiveScriptWrappable::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
}

void DisplayLockContext::Dispose() {
  // Note that if we have any resolvers at dispose time, then it's too late to
  // reject the promise, since we are not allowed to create new strong
  // references to objects already set for destruction (and rejecting would do
  // this since the rejection has to be deferred). We need to detach instead.
  // TODO(vmpstr): See if there is another earlier time we can detect that we're
  // going to be disposed.
  FinishUpdateResolver(kDetach);
  FinishCommitResolver(kDetach);
  FinishAcquireResolver(kDetach);
  CancelTimeoutTask();
  state_ = kUnlocked;

  if (document_ && document_->View())
    document_->View()->UnregisterFromLifecycleNotifications(this);
  weak_factory_.InvalidateWeakPtrs();
}

void DisplayLockContext::ContextDestroyed(ExecutionContext*) {
  FinishUpdateResolver(kReject, rejection_names::kExecutionContextDestroyed);
  FinishCommitResolver(kReject, rejection_names::kExecutionContextDestroyed);
  FinishAcquireResolver(kReject, rejection_names::kExecutionContextDestroyed);
  CancelTimeoutTask();
  state_ = kUnlocked;
}

bool DisplayLockContext::HasPendingActivity() const {
  // If we're locked or doing any work and have an element, then we should stay
  // alive. If the element is gone, then there is no reason for the context to
  // remain. Also, if we're unlocked we're essentially "idle" so GC can clean us
  // up. If the script needs the context, the element would create a new one.
  return element_ && state_ != kUnlocked;
}

ScriptPromise DisplayLockContext::acquire(ScriptState* script_state,
                                          DisplayLockOptions* options) {
  TRACE_EVENT0("blink", "DisplayLockContext::acquire()");
  double timeout_ms = (options && options->hasTimeout())
                          ? options->timeout()
                          : kDefaultLockTimeoutMs;

  if (IsLocked()) {
    // If we're locked, the activatable flag might change the activation
    // blocking lock count. If we're not locked, the activation blocking lock
    // count will be updated when we changed the state.
    state_.UpdateActivationBlockingCount(activatable_,
                                         options && options->activatable());
  }
  activatable_ = options && options->activatable();

  // We always reschedule a timeout task even if we're not starting a new
  // acquire. The reason for this is that the last acquire dictates the timeout
  // interval. Note that the following call cancels any existing timeout tasks.
  RescheduleTimeoutTask(timeout_ms);

  if (state_ == kPendingAcquire) {
    DCHECK(acquire_resolver_);
    return acquire_resolver_->Promise();
  }
  DCHECK(!acquire_resolver_);

  // At this point, if we're not unlocked, then we must already be locked.
  if (state_ != kUnlocked)
    return GetResolvedPromise(script_state);

  update_budget_.reset();

  // If we're already connected then we need to ensure that 1. layout is clean
  // and 2. we have removed the current painted output.
  if (ConnectedToView()) {
    acquire_resolver_ = ScriptPromiseResolver::Create(script_state);
    state_ = kPendingAcquire;
    MarkPaintLayerNeedsRepaint();
    ScheduleAnimation();
    return acquire_resolver_->Promise();
  }

  // Otherwise (if we're not connected), we can acquire the lock immediately.
  locked_frame_rect_ = LayoutRect();
  state_ = kLocked;
  return GetResolvedPromise(script_state);
}

ScriptPromise DisplayLockContext::update(ScriptState* script_state) {
  TRACE_EVENT0("blink", "DisplayLockContext::update()");
  // Reject if we're unlocked or disconnected.
  if (state_ == kUnlocked || state_ == kPendingAcquire || !ConnectedToView()) {
    return GetRejectedPromise(script_state,
                              rejection_names::kElementIsUnlocked);
  }

  // If we have a resolver, then we're at least updating already, just return
  // the same promise.
  if (update_resolver_) {
    DCHECK(state_ == kUpdating || state_ == kCommitting) << state_;
    return update_resolver_->Promise();
  }

  update_resolver_ = ScriptPromiseResolver::Create(script_state);
  StartUpdateIfNeeded();
  return update_resolver_->Promise();
}

ScriptPromise DisplayLockContext::commit(ScriptState* script_state) {
  TRACE_EVENT0("blink", "DisplayLockContext::commit()");
  // Resolve if we're already unlocked.
  if (state_ == kUnlocked)
    return GetResolvedPromise(script_state);

  // If we're already committing then return the promise.
  if (state_ == kCommitting)
    return commit_resolver_->Promise();

  // Now that we've explicitly been requested to commit, we have cancel the
  // timeout task.
  CancelTimeoutTask();

  // Note that we don't resolve the update promise here, since it should still
  // finish updating before resolution. That is, calling update() and commit()
  // together will still wait until the lifecycle is clean before resolving any
  // of the promises.
  DCHECK_NE(state_, kCommitting);
  // We might already have a resolver if we called updateAndCommit() before
  // this.
  if (!commit_resolver_)
    commit_resolver_ = ScriptPromiseResolver::Create(script_state);
  auto promise = commit_resolver_->Promise();
  StartCommit();
  return promise;
}

ScriptPromise DisplayLockContext::updateAndCommit(ScriptState* script_state) {
  TRACE_EVENT0("blink", "DisplayLockContext::updateAndCommit()");

  // Resolve if we're already unlocked.
  if (state_ == kUnlocked)
    return GetResolvedPromise(script_state);

  // If we're in a state where a co-operative update doesn't make sense (e.g. we
  // haven't acquired the lock, or we're already sync committing), then do
  // whatever commit() would do.
  if (state_ == kPendingAcquire || state_ == kCommitting ||
      !ConnectedToView()) {
    return commit(script_state);
  }

  // If we have a commit resolver already, return it.
  if (commit_resolver_) {
    // We must be in a second call to updateAndCommit(), meaning that we're in
    // the kUpdating state with a commit_resolver_.
    DCHECK_EQ(state_, kUpdating);
    return commit_resolver_->Promise();
  }

  CancelTimeoutTask();
  commit_resolver_ = ScriptPromiseResolver::Create(script_state);
  StartUpdateIfNeeded();
  return commit_resolver_->Promise();
}

void DisplayLockContext::FinishUpdateResolver(ResolverState state,
                                              const char* rejection_reason) {
  FinishResolver(&update_resolver_, state, rejection_reason);
}

void DisplayLockContext::FinishCommitResolver(ResolverState state,
                                              const char* rejection_reason) {
  FinishResolver(&commit_resolver_, state, rejection_reason);
}

void DisplayLockContext::FinishAcquireResolver(ResolverState state,
                                               const char* rejection_reason) {
  FinishResolver(&acquire_resolver_, state, rejection_reason);
}

void DisplayLockContext::FinishResolver(Member<ScriptPromiseResolver>* resolver,
                                        ResolverState state,
                                        const char* rejection_reason) {
  if (!*resolver)
    return;
  switch (state) {
    case kResolve:
      // In order to avoid script doing work as a part of the lifecycle update,
      // we delay the resolution to be in a task.
      GetExecutionContext()
          ->GetTaskRunner(TaskType::kMiscPlatformAPI)
          ->PostTask(FROM_HERE, WTF::Bind(
                                    +[](ScriptPromiseResolver* resolver) {
                                      resolver->Resolve();
                                    },
                                    WrapPersistent(resolver->Get())));
      break;
    case kReject:
      DCHECK(rejection_reason);
      (*resolver)->Reject(DOMException::Create(
          DOMExceptionCode::kNotAllowedError, rejection_reason));
      break;
    case kDetach:
      (*resolver)->Detach();
  }
  *resolver = nullptr;
}

bool DisplayLockContext::ShouldStyle() const {
  return update_forced_ || state_ > kUpdating ||
         (state_ == kUpdating &&
          update_budget_->ShouldPerformPhase(DisplayLockBudget::Phase::kStyle));
}

void DisplayLockContext::DidStyle() {
  if (state_ != kCommitting && state_ != kUpdating &&
      state_ != kPendingAcquire && !update_forced_) {
    return;
  }

  MarkElementsForWhitespaceReattachment();
  // We must have "contain: style layout" for display locking.
  // Note that we should also have this containment even if we're forcing
  // this update to happen. Otherwise, proceeding with layout may cause
  // unexpected behavior. By rejecting the promise, the behavior can be detected
  // by script.
  if (!ElementSupportsDisplayLocking()) {
    bool should_stay_locked = state_ == kUpdating && !commit_resolver_;
    FinishUpdateResolver(kReject, rejection_names::kContainmentNotSatisfied);
    FinishCommitResolver(kReject, rejection_names::kContainmentNotSatisfied);
    FinishAcquireResolver(kReject, rejection_names::kContainmentNotSatisfied);
    state_ = should_stay_locked ? kLocked : kUnlocked;
    return;
  }

  if (state_ == kUpdating)
    update_budget_->DidPerformPhase(DisplayLockBudget::Phase::kStyle);
}

bool DisplayLockContext::ShouldLayout() const {
  return update_forced_ || state_ > kUpdating ||
         (state_ == kUpdating && update_budget_->ShouldPerformPhase(
                                     DisplayLockBudget::Phase::kLayout));
}

void DisplayLockContext::DidLayout() {
  if (state_ == kUpdating)
    update_budget_->DidPerformPhase(DisplayLockBudget::Phase::kLayout);
}

bool DisplayLockContext::ShouldPrePaint() const {
  return update_forced_ || state_ > kUpdating ||
         (state_ == kUpdating && update_budget_->ShouldPerformPhase(
                                     DisplayLockBudget::Phase::kPrePaint));
}

void DisplayLockContext::DidPrePaint() {
  if (state_ == kUpdating)
    update_budget_->DidPerformPhase(DisplayLockBudget::Phase::kPrePaint);

#if DCHECK_IS_ON()
  if (state_ == kUpdating || state_ == kCommitting) {
    // Since we should be under containment, we should have a layer. If we
    // don't, then paint might not happen and we'll never resolve.
    DCHECK(element_->GetLayoutObject()->HasLayer());
  }
#endif
}

bool DisplayLockContext::ShouldPaint() const {
  // Note that forced updates should never require us to paint, so we don't
  // check |update_forced_| here. In other words, although |update_forced_|
  // could be true here, we still should not paint. This also holds for
  // kUpdating state, since updates should not paint.
  return state_ == kCommitting || state_ == kUnlocked;
}

void DisplayLockContext::DidPaint() {
  // This is here for symmetry, but could be removed if necessary.
}

bool DisplayLockContext::IsActivatable() const {
  return activatable_ || !IsLocked();
}

void DisplayLockContext::CommitForActivation() {
  DCHECK(element_);
  DCHECK(ConnectedToView());
  DCHECK(ShouldCommitForActivation());
  StartCommit();
}

bool DisplayLockContext::ShouldCommitForActivation() const {
  return IsActivatable() && state_ != kUnlocked && state_ != kCommitting;
}

void DisplayLockContext::DidAttachLayoutTree() {
  if (state_ >= kUnlocked)
    return;

  // Note that although we checked at style recalc time that the element has
  // "contain: style layout", it might not actually apply the containment at the
  // layout object level. This confirms that containment should apply.
  if (!ElementSupportsDisplayLocking()) {
    bool should_stay_locked = state_ == kUpdating && !commit_resolver_;
    FinishUpdateResolver(kReject, rejection_names::kContainmentNotSatisfied);
    FinishCommitResolver(kReject, rejection_names::kContainmentNotSatisfied);
    state_ = should_stay_locked ? kLocked : kUnlocked;
  }
}

DisplayLockContext::ScopedPendingFrameRect
DisplayLockContext::GetScopedPendingFrameRect() {
  if (state_ >= kCommitting)
    return ScopedPendingFrameRect(nullptr);

  DCHECK(element_->GetLayoutObject() && element_->GetLayoutBox());
  element_->GetLayoutBox()->SetFrameRectForDisplayLock(pending_frame_rect_);
  return ScopedPendingFrameRect(this);
}

void DisplayLockContext::NotifyPendingFrameRectScopeEnded() {
  DCHECK(element_->GetLayoutObject() && element_->GetLayoutBox());
  DCHECK(locked_frame_rect_);
  pending_frame_rect_ = element_->GetLayoutBox()->FrameRect();
  element_->GetLayoutBox()->SetFrameRectForDisplayLock(*locked_frame_rect_);
}

DisplayLockContext::ScopedForcedUpdate
DisplayLockContext::GetScopedForcedUpdate() {
  if (state_ >= kCommitting)
    return ScopedForcedUpdate(nullptr);

  DCHECK(!update_forced_);
  update_forced_ = true;

  // Now that the update is forced, we should ensure that style layout, and
  // prepaint code can reach it via dirty bits. Note that paint isn't a part of
  // this, since |update_forced_| doesn't force paint to happen. See
  // ShouldPaint().
  MarkAncestorsForStyleRecalcIfNeeded();
  MarkAncestorsForLayoutIfNeeded();
  MarkAncestorsForPrePaintIfNeeded();
  return ScopedForcedUpdate(this);
}

void DisplayLockContext::NotifyForcedUpdateScopeEnded() {
  DCHECK(update_forced_);
  update_forced_ = false;
}

void DisplayLockContext::StartCommit() {
  // If we don't have an element or we're not connected, then the process of
  // committing is the same as just unlocking the element.
  if (!element_ || !ConnectedToView()) {
    state_ = kUnlocked;
    update_budget_.reset();
    CancelTimeoutTask();
    // Note that we reject the update, but resolve the commit.
    FinishUpdateResolver(kReject, rejection_names::kElementIsDisconnected);
    FinishCommitResolver(kResolve);
    return;
  }

  // If we have just started to acquire, we can unlock immediately since we
  // didn't have a chance to lock yet.
  if (state_ == kPendingAcquire) {
    FinishAcquireResolver(kReject, rejection_names::kLockCommitted);
    FinishCommitResolver(kResolve);
    CancelTimeoutTask();
    state_ = kUnlocked;
    return;
  }

  if (state_ != kUpdating)
    ScheduleAnimation();

  DCHECK_LT(state_, kCommitting);
  state_ = kCommitting;
  update_budget_.reset();

  // We're committing without a budget, so ensure we can reach style.
  MarkAncestorsForStyleRecalcIfNeeded();

  auto* layout_object = element_->GetLayoutObject();
  // We might commit without connecting, so there is no layout object yet.
  if (!layout_object)
    return;

  // Now that we know we have a layout object, we should ensure that we can
  // reach the rest of the phases as well.
  MarkAncestorsForLayoutIfNeeded();
  MarkAncestorsForPrePaintIfNeeded();
  MarkPaintLayerNeedsRepaint();

  // We also need to commit the pending frame rect at this point.
  bool frame_rect_changed =
      ToLayoutBox(layout_object)->FrameRect() != pending_frame_rect_;

  // If the frame rect hasn't actually changed then we don't need to do
  // anything. Other than wait for commit to happen
  if (!frame_rect_changed)
    return;

  // Set the pending frame rect as the new one, and ensure to schedule a layout
  // for just the box itself. Note that we use the non-display locked version to
  // ensure all the hooks are property invoked.
  ToLayoutBox(layout_object)->SetFrameRect(pending_frame_rect_);
  layout_object->SetNeedsLayout(
      layout_invalidation_reason::kDisplayLockCommitting);
}

void DisplayLockContext::StartUpdateIfNeeded() {
  // We should not be calling this if we're unlocked.
  DCHECK_NE(state_, kUnlocked);
  // Any state other than kLocked means that we are already in the process of
  // updating/committing, so we can piggy back on that process without kicking
  // off any new updates.
  if (state_ != kLocked)
    return;

  // We don't need to mark anything dirty since the budget will take care of
  // that for us.
  update_budget_ = CreateNewBudget();
  state_ = kUpdating;
  ScheduleAnimation();
}

std::unique_ptr<DisplayLockBudget> DisplayLockContext::CreateNewBudget() {
  switch (BudgetType::kDefault) {
    case BudgetType::kDoNotYield:
      return base::WrapUnique(new UnyieldingDisplayLockBudget(this));
    case BudgetType::kStrictYieldBetweenLifecyclePhases:
      return base::WrapUnique(new StrictYieldingDisplayLockBudget(this));
    case BudgetType::kYieldBetweenLifecyclePhases:
      return base::WrapUnique(new YieldingDisplayLockBudget(this));
  }
  NOTREACHED();
  return nullptr;
}

void DisplayLockContext::AddToWhitespaceReattachSet(Element& element) {
  whitespace_reattach_set_.insert(&element);
}

void DisplayLockContext::MarkElementsForWhitespaceReattachment() {
  for (auto element : whitespace_reattach_set_) {
    if (!element || element->NeedsReattachLayoutTree() ||
        !element->GetLayoutObject())
      continue;

    if (Node* first_child = LayoutTreeBuilderTraversal::FirstChild(*element))
      first_child->MarkAncestorsWithChildNeedsReattachLayoutTree();
  }
  whitespace_reattach_set_.clear();
}

bool DisplayLockContext::MarkAncestorsForStyleRecalcIfNeeded() {
  if (IsElementDirtyForStyleRecalc()) {
    element_->MarkAncestorsWithChildNeedsStyleRecalc();
    return true;
  }
  return false;
}

bool DisplayLockContext::MarkAncestorsForLayoutIfNeeded() {
  if (IsElementDirtyForLayout()) {
    element_->GetLayoutObject()->MarkContainerChainForLayout();
    return true;
  }
  return false;
}

bool DisplayLockContext::MarkAncestorsForPrePaintIfNeeded() {
  if (IsElementDirtyForPrePaint()) {
    auto* layout_object = element_->GetLayoutObject();
    if (auto* parent = layout_object->Parent())
      parent->SetSubtreeShouldCheckForPaintInvalidation();
    return true;
  }
  return false;
}

bool DisplayLockContext::MarkPaintLayerNeedsRepaint() {
  if (auto* layout_object = element_->GetLayoutObject()) {
    layout_object->PaintingLayer()->SetNeedsRepaint();
    return true;
  }
  return false;
}

bool DisplayLockContext::IsElementDirtyForStyleRecalc() const {
  return element_->NeedsStyleRecalc() || element_->ChildNeedsStyleRecalc();
}

bool DisplayLockContext::IsElementDirtyForLayout() const {
  if (auto* layout_object = element_->GetLayoutObject())
    return layout_object->NeedsLayout();
  return false;
}

bool DisplayLockContext::IsElementDirtyForPrePaint() const {
  if (auto* layout_object = element_->GetLayoutObject()) {
    return layout_object->ShouldCheckForPaintInvalidation() ||
           layout_object->SubtreeShouldCheckForPaintInvalidation() ||
           layout_object->NeedsPaintPropertyUpdate() ||
           layout_object->DescendantNeedsPaintPropertyUpdate();
  }
  return false;
}

void DisplayLockContext::DidMoveToNewDocument(Document& old_document) {
  DCHECK(element_);
  document_ = &element_->GetDocument();

  // Since we're observing the lifecycle updates, ensure that we listen to the
  // right document's view.
  if (old_document.View())
    old_document.View()->UnregisterFromLifecycleNotifications(this);
  if (document_->View())
    document_->View()->RegisterForLifecycleNotifications(this);

  if (!IsActivatable()) {
    old_document.RemoveActivationBlockingDisplayLock();
    document_->AddActivationBlockingDisplayLock();
  }
  if (IsLocked()) {
    old_document.RemoveLockedDisplayLock();
    document_->AddLockedDisplayLock();
  }
}

void DisplayLockContext::WillStartLifecycleUpdate() {
  if (state_ == kUpdating)
    update_budget_->WillStartLifecycleUpdate();
}

void DisplayLockContext::DidFinishLifecycleUpdate() {
  if (state_ == kPendingAcquire) {
    if (!ElementSupportsDisplayLocking()) {
      FinishAcquireResolver(kReject, rejection_names::kContainmentNotSatisfied);
      CancelTimeoutTask();
      state_ = kUnlocked;
      return;
    }

    FinishAcquireResolver(kResolve);
    state_ = kLocked;
    auto* layout_object = element_->GetLayoutObject();
    // TODO(vmpstr): Note that we may be in a nested display lock, so it's
    // unclear which frame rect to save here since we have not necessarily done
    // a layout to acquire this lock. We need to figure out what to do here. For
    // now, just stash whatever the current frame rect is. See crbug.com/926276.
    if (layout_object && layout_object->IsBox()) {
      locked_frame_rect_ = ToLayoutBox(layout_object)->FrameRect();
    } else {
      locked_frame_rect_ = LayoutRect();
    }
    return;
  }

  if (state_ == kCommitting) {
    FinishUpdateResolver(kResolve);
    FinishCommitResolver(kResolve);
    CancelTimeoutTask();
    state_ = kUnlocked;
    return;
  }

  if (state_ != kUpdating)
    return;

  // If we became disconnected for any reason, then we should reject the
  // update promise and go back to the locked state.
  if (!element_ || !ConnectedToView()) {
    FinishUpdateResolver(kReject, rejection_names::kElementIsDisconnected);
    update_budget_.reset();

    if (commit_resolver_) {
      // We resolve the commit if we're not connected.
      FinishCommitResolver(kResolve);
      CancelTimeoutTask();
      state_ = kUnlocked;
    } else {
      state_ = kLocked;
    }
    return;
  }

  if (update_budget_->NeedsLifecycleUpdates()) {
    // Note that we post a task to schedule an animation, since rAF requests can
    // be ignored if they happen from within a lifecycle update.
    GetExecutionContext()
        ->GetTaskRunner(TaskType::kMiscPlatformAPI)
        ->PostTask(FROM_HERE, WTF::Bind(&DisplayLockContext::ScheduleAnimation,
                                        WrapWeakPersistent(this)));
    return;
  }

  FinishUpdateResolver(kResolve);
  update_budget_.reset();
  state_ = kLocked;

  if (commit_resolver_) {
    // Schedule a commit to run. Note that we can't call StartCommit directly
    // here, since we're in the lifecycle updates right now and the code that
    // runs after may depend on having clean layout state, which StartCommit
    // might dirty.
    GetExecutionContext()
        ->GetTaskRunner(TaskType::kMiscPlatformAPI)
        ->PostTask(FROM_HERE, WTF::Bind(&DisplayLockContext::StartCommit,
                                        WrapWeakPersistent(this)));
  }
}

void DisplayLockContext::ScheduleAnimation() {
  DCHECK(element_);
  DCHECK(ConnectedToView());

  // Schedule an animation to perform the lifecycle phases.
  document_->GetPage()->Animator().ScheduleVisualUpdate(document_->GetFrame());
}

void DisplayLockContext::RescheduleTimeoutTask(double delay) {
  CancelTimeoutTask();

  if (!std::isfinite(delay))
    return;

  // Make sure the delay is at least 1ms.
  delay = std::max(delay, 1.);
  GetExecutionContext()
      ->GetTaskRunner(TaskType::kMiscPlatformAPI)
      ->PostDelayedTask(FROM_HERE,
                        WTF::Bind(&DisplayLockContext::TriggerTimeout,
                                  weak_factory_.GetWeakPtr()),
                        TimeDelta::FromMillisecondsD(delay));
  timeout_task_is_scheduled_ = true;
}

void DisplayLockContext::CancelTimeoutTask() {
  if (!timeout_task_is_scheduled_)
    return;
  weak_factory_.InvalidateWeakPtrs();
  timeout_task_is_scheduled_ = false;
}

void DisplayLockContext::TriggerTimeout() {
  // We might have started destroyed the element or started to shut down while
  // we're triggering a timeout. In that case, do nothing.
  if (!element_ || !document_->Lifecycle().IsActive())
    return;
  StartCommit();
  timeout_task_is_scheduled_ = false;
}

bool DisplayLockContext::ElementSupportsDisplayLocking() const {
  DCHECK(element_);
  // The style can be dirty if we're in a nested lock.
  // TODO(vmpstr): We need to figure out what to do here, since we don't know
  // what the style is and whether this element has proper containment. However,
  // forcing an update from the ancestor locks seems inefficient. For now, we
  // just optimistically assume that we have all of the right containment in
  // place. See crbug.com/926276 for more information.
  if (IsElementDirtyForStyleRecalc())
    return true;

  // If we have a layout object, check that since it's a more authoritative
  // source of containment information.
  if (auto* layout_object = element_->GetLayoutObject()) {
    return layout_object->ShouldApplyStyleContainment() &&
           layout_object->ShouldApplyLayoutContainment();
  }

  // Otherwise, fallback on just checking style.
  auto* style = element_->GetComputedStyle();
  // Note that if for whatever reason we don't have computed style, then
  // optimistically assume that we have containment.
  // TODO(vmpstr): Perhaps we need to add render=lockable which will ensure
  // containment.
  return !style || (style->ContainsStyle() && style->ContainsLayout());
}

bool DisplayLockContext::ConnectedToView() const {
  DCHECK(element_);
  return element_->isConnected() && document_->View();
}

// Scoped objects implementation
// -----------------------------------------------

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

// StateChangeHelper implementation
// -----------------------------------------------
DisplayLockContext::StateChangeHelper::StateChangeHelper(
    DisplayLockContext* context)
    : context_(context) {}

DisplayLockContext::StateChangeHelper& DisplayLockContext::StateChangeHelper::
operator=(State new_state) {
  if (new_state == state_)
    return *this;

  if (state_ == kUnlocked) {
    TRACE_EVENT_ASYNC_BEGIN0(
        TRACE_DISABLED_BY_DEFAULT("blink.debug.display_lock"),
        "LockedDisplayLock", this);
  } else if (new_state == kUnlocked) {
    TRACE_EVENT_ASYNC_END0(
        TRACE_DISABLED_BY_DEFAULT("blink.debug.display_lock"),
        "LockedDisplayLock", this);
  }

  bool was_activatable = context_->IsActivatable();
  bool was_locked = context_->IsLocked();

  state_ = new_state;
  if (state_ != kUnlocked) {
    TRACE_EVENT_ASYNC_STEP_INTO0(
        TRACE_DISABLED_BY_DEFAULT("blink.debug.display_lock"),
        "LockedDisplayLock", this, StateToString(state_));
  }

  if (!context_->document_)
    return *this;

  UpdateActivationBlockingCount(was_activatable, context_->IsActivatable());

  // Adjust the total number of locked display locks.
  auto& document = *context_->document_;
  if (context_->IsLocked() != was_locked) {
    if (was_locked)
      document.RemoveLockedDisplayLock();
    else
      document.AddLockedDisplayLock();
  }

  return *this;
}

void DisplayLockContext::StateChangeHelper::UpdateActivationBlockingCount(
    bool old_activatable,
    bool new_activatable) {
  auto& document = *context_->document_;
  // Adjust activation blocking lock counts.
  if (old_activatable != new_activatable) {
    if (old_activatable)
      document.AddActivationBlockingDisplayLock();
    else
      document.RemoveActivationBlockingDisplayLock();
  }
}

}  // namespace blink
