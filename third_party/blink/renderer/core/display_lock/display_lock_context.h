// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DISPLAY_LOCK_DISPLAY_LOCK_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DISPLAY_LOCK_DISPLAY_LOCK_CONTEXT_H_

#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_budget.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/wtf/compiler.h"

namespace blink {

class DisplayLockSuspendedHandle;
class Element;
class V8DisplayLockCallback;
class DisplayLockScopedLogger;
class CORE_EXPORT DisplayLockContext final
    : public ScriptWrappable,
      public ActiveScriptWrappable<DisplayLockContext>,
      public ContextLifecycleObserver,
      public LocalFrameView::LifecycleNotificationObserver {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(DisplayLockContext);
  USING_PRE_FINALIZER(DisplayLockContext, Dispose);

 public:
  // Determines what type of budget to use. This can be overridden via
  // DisplayLockContext::SetBudgetType().
  // - kDoNotYield:
  //     This will effectively do all of the lifecycle phases when we're
  //     committing. That is, this is the "no budget" option.
  // - kStrictYieldBetweenLifecyclePhases:
  //     This type always yields between each lifecycle phase, even if that
  //     phase was quick.
  // - kYieldBetweenLifecyclePhases:
  //     This type will only yield between lifecycle phases (not in the middle
  //     of one). However, if there is sufficient time left (TODO(vmpstr):
  //     define this), then it will continue on to the next lifecycle phase.
  enum class BudgetType {
    kDoNotYield,
    kStrictYieldBetweenLifecyclePhases,
    kYieldBetweenLifecyclePhases,
    kDefault = kStrictYieldBetweenLifecyclePhases
  };

  // Conceptually the states are private, but made public for testing.
  // TODO(vmpstr): Make this private and update the tests.
  enum State {
    kUninitialized,
    kSuspended,
    kCallbacksPending,
    kDisconnected,
    kCommitting,
    kResolving,
    kResolved
  };

  // See GetScopedPendingFrameRect() for description.
  class ScopedPendingFrameRect {
   public:
    ScopedPendingFrameRect(ScopedPendingFrameRect&&);
    ~ScopedPendingFrameRect();

   private:
    friend class DisplayLockContext;

    ScopedPendingFrameRect(DisplayLockContext*);

    UntracedMember<DisplayLockContext> context_ = nullptr;
  };

  // See GetScopedForcedUpdate() for description.
  class ScopedForcedUpdate {
   public:
    ScopedForcedUpdate(ScopedForcedUpdate&&);
    ~ScopedForcedUpdate();

   private:
    friend class DisplayLockContext;

    ScopedForcedUpdate(DisplayLockContext*);

    UntracedMember<DisplayLockContext> context_ = nullptr;
  };

  DisplayLockContext(Element*, ExecutionContext*);
  ~DisplayLockContext() override;

  // GC functions.
  void Trace(blink::Visitor*) override;
  void Dispose();

  // ContextLifecycleObserver overrides.
  void ContextDestroyed(ExecutionContext*) override;
  // ActiveScriptWrappable overrides. If there is an outstanding task scheduled
  // to process the callback queue, then this return true.
  // TODO(vmpstr): In the future this would also be true while we're doing
  // co-operative work.
  bool HasPendingActivity() const final;

  // Notify that the lock was requested. Note that for a new context, this has
  // to be called first. For an existing lock, this will either extend the
  // lifetime of the current lock, or start acquiring a new lock (depending on
  // whether this lock is active or passive).
  void RequestLock(V8DisplayLockCallback*, ScriptState*);

  // Returns true if the promise associated with this context was already
  // resolved (or rejected).
  bool IsResolved() const { return state_ == kResolved; }

  // Returns a ScriptPromise associated with this context.
  ScriptPromise Promise() const {
    DCHECK(resolver_);
    return resolver_->Promise();
  }

  // Called when the connected state may have changed.
  void NotifyConnectedMayHaveChanged();

  // Rejects the associated promise if one exists, and clears the current queue.
  // This effectively makes the context finalized.
  void RejectAndCleanUp();

  // JavaScript interface implementation.
  void schedule(V8DisplayLockCallback*);
  DisplayLockSuspendedHandle* suspend();
  Element* lockedElement() const;

  // Lifecycle observation / state functions.
  bool ShouldStyle() const;
  void DidStyle();
  bool ShouldLayout() const;
  void DidLayout();
  bool ShouldPrePaint() const;
  void DidPrePaint();
  bool ShouldPaint() const;
  void DidPaint();

  void DidAttachLayoutTree();

  // Returns a ScopedPendingFrameRect object which exposes the pending layout
  // frame rect to LayoutBox. This is used to ensure that children of the locked
  // element use the pending layout frame to update the size of the element.
  // After the scoped object is destroyed, the previous frame rect is restored
  // and the pending one is stored in the context until it is needed.
  ScopedPendingFrameRect GetScopedPendingFrameRect();

  // Returns a ScopedForcedUpdate object which for the duration of its lifetime
  // will allow updates to happen on this element's subtree. For the element
  // itself, the frame rect will still be the same as at the time the lock was
  // acquired.
  // Only one ScopedForcedUpdate can be retrieved from the same context at a
  // time.
  ScopedForcedUpdate GetScopedForcedUpdate();

  // This is called when the element with which this context is associated is
  // moved to a new document.
  void DidMoveToNewDocument(Document& old_document);

  // LifecycleNotificationObserver overrides.
  void WillStartLifecycleUpdate() override;
  void DidFinishLifecycleUpdate() override;

 private:
  friend class DisplayLockContextTest;
  friend class DisplayLockSuspendedHandle;
  friend class DisplayLockBudget;

  // Sets a new budget type. Note that since this is static, all of the existing
  // locks will be affected as well (unless they already have a budget). This is
  // meant to be used to set the budget type once (such as in a test), but
  // technically could be used outside of tests as well to force a certain
  // budget type. Use with caution. Note that if a budget already exists on the
  // lock, then it will continue to use the existing budget until a new budget
  // is requested.
  static void SetBudgetType(BudgetType type) {
    if (s_budget_type_ == type)
      return;
    s_budget_type_ = type;
  }

  // Schedules a new callback. If this is the first callback to be scheduled,
  // then a valid ScriptState must be provided, which will be used to create a
  // new ScriptPromiseResolver. In other cases, the ScriptState is ignored.
  void ScheduleCallback(V8DisplayLockCallback*);

  // Processes the current queue of callbacks.
  void ProcessQueue();

  // Called by the suspended handle in order to resume context operations.
  void Resume();

  // Called by the suspended handle informing us that it was disposed without
  // resuming, meaning it will never resume.
  void NotifyWillNotResume();

  // Schedule a task if one is required. Specifically, this would schedule a
  // task if one was not already scheduled and if we need to either process
  // callbacks or to resolve the associated promise.
  void ScheduleTaskIfNeeded();

  // A function that finishes resolving the promise by establishing a microtask
  // checkpoint. Note that this should be scheduled after entering the
  // kResolving state. If the state is still kResolving after the microtask
  // checkpoint finishes (ie, the lock was not re-acquired), we enter the final
  // kResolved state.
  void FinishResolution();

  // Initiate a commit.
  void StartCommit();

  // The following functions propagate dirty bits from the locked element up to
  // the ancestors in order to be reached. They return true if the element or
  // its subtree were dirty, and false otherwise.
  bool MarkAncestorsForStyleRecalcIfNeeded();
  bool MarkAncestorsForLayoutIfNeeded();
  bool MarkAncestorsForPaintInvalidationCheckIfNeeded();
  bool MarkPaintLayerNeedsRepaint();

  // Marks the display lock as being disconnected. Note that this removes the
  // strong reference to the element in case the element is deleted. Once we're
  // disconnected, there are will be no calls to the context until we reconnect,
  // so we should allow for the element to get GCed meanwhile.
  void MarkAsDisconnected();

  // Returns the element asserting that it exists.
  ALWAYS_INLINE Element* GetElement() const {
    DCHECK(weak_element_handle_);
    return weak_element_handle_;
  }

  // When ScopedPendingFrameRect is destroyed, it calls this function. See
  // GetScopedPendingFrameRect() for more information.
  void NotifyPendingFrameRectScopeEnded();

  // When ScopedForcedUpdate is destroyed, it calls this function. See
  // GetScopedForcedUpdate() for more information.
  void NotifyForcedUpdateScopeEnded();

  // This resets the existing budget (if any) and establishes a new budget
  // starting "now".
  void ResetNewBudget();

  // Helper to schedule an animation to delay lifecycle updates for the next
  // frame.
  void ScheduleAnimation();

  static BudgetType s_budget_type_;
  std::unique_ptr<DisplayLockBudget> budget_;

  HeapVector<Member<V8DisplayLockCallback>> callbacks_;
  Member<ScriptPromiseResolver> resolver_;

  // Note that we hold both a weak and a strong reference to the element. The
  // strong reference is sometimes set to nullptr, meaning that we can GC it if
  // nothing else is holding a strong reference. We use weak_element_handle_ to
  // detect such a case so that we don't also keep the context alive needlessly.
  //
  // We need a strong reference, since the element can be accessed from script
  // via this context. Specifically DisplayLockContext::lockedElement() returns
  // the element to script. This means that callbacks pending in the context can
  // actually append the element to the tree. This means a strong reference is
  // needed to prevent GC from destroying the element.
  //
  // However, once we're in kDisconnected state, it means that we have no
  // callbacks pending and we haven't been connected to the DOM tree. It's also
  // possible that the script has lost the reference to the locked element. This
  // means the element should be GCed, so we need to let go of the strong
  // reference. If we don't, we can cause a leak of an element and a display
  // lock context.
  //
  // In case the script did not lose the element handle and will connect it in
  // the future, we do retain a weak handle. We also use the weak handle to
  // check if the element was destroyed for the purposes of allowing this
  // context to be GCed as well.
  Member<Element> element_;
  WeakMember<Element> weak_element_handle_;

  unsigned suspended_count_ = 0;
  State state_ = kUninitialized;
  LayoutRect pending_frame_rect_;
  base::Optional<LayoutRect> locked_frame_rect_;

  bool process_queue_task_scheduled_ = false;
  bool update_forced_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DISPLAY_LOCK_DISPLAY_LOCK_CONTEXT_H_
