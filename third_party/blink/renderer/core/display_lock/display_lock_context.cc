// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/display_lock/display_lock_context.h"

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_options.h"
#include "third_party/blink/renderer/core/display_lock/strict_yielding_display_lock_budget.h"
#include "third_party/blink/renderer/core/display_lock/unyielding_display_lock_budget.h"
#include "third_party/blink/renderer/core/display_lock/yielding_display_lock_budget.h"
#include "third_party/blink/renderer/core/dom/document.h"
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

// Helper function that returns an immediately rejected promise.
ScriptPromise GetRejectedPromise(ScriptState* script_state) {
  auto* resolver = ScriptPromiseResolver::Create(script_state);
  auto promise = resolver->Promise();
  resolver->Reject();
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
      weak_factory_(this) {
  DCHECK(element_->GetDocument().View());
  element_->GetDocument().View()->RegisterForLifecycleNotifications(this);
}

DisplayLockContext::~DisplayLockContext() {
  DCHECK_EQ(state_, kUnlocked);
}

void DisplayLockContext::Trace(blink::Visitor* visitor) {
  visitor->Trace(update_resolver_);
  visitor->Trace(commit_resolver_);
  visitor->Trace(acquire_resolver_);
  visitor->Trace(element_);
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

  if (element_ && element_->GetDocument().View())
    element_->GetDocument().View()->UnregisterFromLifecycleNotifications(this);
  weak_factory_.InvalidateWeakPtrs();
}

void DisplayLockContext::ContextDestroyed(ExecutionContext*) {
  FinishUpdateResolver(kReject);
  FinishCommitResolver(kReject);
  FinishAcquireResolver(kReject);
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
  double timeout_ms = (options && options->hasTimeout())
                          ? options->timeout()
                          : kDefaultLockTimeoutMs;
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
  if (element_->isConnected()) {
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
  // Reject if we're unlocked or disconnected.
  if (state_ == kUnlocked || state_ == kPendingAcquire ||
      !element_->isConnected()) {
    return GetRejectedPromise(script_state);
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
  // Reject if we're unlocked.
  if (state_ == kUnlocked)
    return GetRejectedPromise(script_state);

  // If we have a resolver, we must be committing already, just return the same
  // promise.
  if (commit_resolver_) {
    DCHECK(state_ == kCommitting) << state_;
    return commit_resolver_->Promise();
  }

  // Now that we've explicitly been requested to commit, we have cancel the
  // timeout task.
  CancelTimeoutTask();

  // Note that we don't resolve the update promise here, since it should still
  // finish updating before resolution. That is, calling update() and commit()
  // together will still wait until the lifecycle is clean before resolving any
  // of the promises.
  DCHECK_NE(state_, kCommitting);
  commit_resolver_ = ScriptPromiseResolver::Create(script_state);
  auto promise = commit_resolver_->Promise();
  StartCommit();
  return promise;
}

void DisplayLockContext::FinishUpdateResolver(ResolverState state) {
  FinishResolver(&update_resolver_, state);
}

void DisplayLockContext::FinishCommitResolver(ResolverState state) {
  FinishResolver(&commit_resolver_, state);
}

void DisplayLockContext::FinishAcquireResolver(ResolverState state) {
  FinishResolver(&acquire_resolver_, state);
}

void DisplayLockContext::FinishResolver(Member<ScriptPromiseResolver>* resolver,
                                        ResolverState state) {
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
      (*resolver)->Reject();
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

  // We must have "contain: style layout" for display locking.
  // Note that we should also have this containment even if we're forcing
  // this update to happen. Otherwise, proceeding with layout may cause
  // unexpected behavior. By rejecting the promise, the behavior can be detected
  // by script.
  if (!ElementSupportsDisplayLocking()) {
    FinishUpdateResolver(kReject);
    FinishCommitResolver(kReject);
    FinishAcquireResolver(kReject);
    state_ = state_ == kUpdating ? kLocked : kUnlocked;
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

bool DisplayLockContext::IsSearchable() const {
  // TODO(vmpstr): Support "searchable: true" option, which allows locked
  // elements to be searched.
  return state_ == kUnlocked;
}

void DisplayLockContext::DidAttachLayoutTree() {
  if (state_ == kUnlocked)
    return;

  // Note that although we checked at style recalc time that the element has
  // "contain: style layout", it might not actually apply the containment at the
  // layout object level. This confirms that containment should apply.
  if (!ElementSupportsDisplayLocking()) {
    FinishUpdateResolver(kReject);
    FinishCommitResolver(kReject);
    state_ = state_ == kUpdating ? kLocked : kUnlocked;
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
  if (!element_ || !element_->isConnected()) {
    state_ = kUnlocked;
    update_budget_.reset();
    FinishUpdateResolver(kReject);
    // TODO(vmpstr): Should we resolve here? What's the path to unlocking an
    // element without connecting it (i.e. acquire the lock, then change your
    // mind).
    FinishCommitResolver(kReject);
    return;
  }

  // If we have just started to acquire, we can unlock immediately since we
  // didn't have a chance to lock yet.
  if (state_ == kPendingAcquire) {
    FinishAcquireResolver(kReject);
    FinishCommitResolver(kResolve);
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
  // Since we're observing the lifecycle updates, ensure that we listen to the
  // right document's view.
  if (old_document.View())
    old_document.View()->UnregisterFromLifecycleNotifications(this);
  if (element_ && element_->GetDocument().View())
    element_->GetDocument().View()->RegisterForLifecycleNotifications(this);
}

void DisplayLockContext::WillStartLifecycleUpdate() {
  if (state_ == kUpdating)
    update_budget_->WillStartLifecycleUpdate();
}

void DisplayLockContext::DidFinishLifecycleUpdate() {
  if (state_ == kPendingAcquire) {
    if (!ElementSupportsDisplayLocking()) {
      FinishAcquireResolver(kReject);
      state_ = kUnlocked;
      return;
    }

    FinishAcquireResolver(kResolve);
    state_ = kLocked;
    auto* layout_object = element_->GetLayoutObject();
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
  if (!element_ || !element_->isConnected()) {
    FinishUpdateResolver(kReject);
    update_budget_.reset();
    state_ = kLocked;
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
}

void DisplayLockContext::ScheduleAnimation() {
  DCHECK(element_->isConnected());

  // Schedule an animation to perform the lifecycle phases.
  element_->GetDocument().GetPage()->Animator().ScheduleVisualUpdate(
      element_->GetDocument().GetFrame());
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
  StartCommit();
  timeout_task_is_scheduled_ = false;
}

bool DisplayLockContext::ElementSupportsDisplayLocking() const {
  DCHECK(element_ && !IsElementDirtyForStyleRecalc());
  // If we have a layout object, check that since it's a more authoritative
  // source of containment information.
  if (auto* layout_object = element_->GetLayoutObject()) {
    return layout_object->ShouldApplyStyleContainment() &&
           layout_object->ShouldApplyLayoutContainment();
  }

  // Otherwise, fallback on just checking style.
  auto* style = element_->GetComputedStyle();
  return style && style->ContainsStyle() && style->ContainsLayout();
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

}  // namespace blink
