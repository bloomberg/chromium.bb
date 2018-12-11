// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DISPLAY_LOCK_DISPLAY_LOCK_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DISPLAY_LOCK_DISPLAY_LOCK_CONTEXT_H_

#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/core_export.h"
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
      public ContextLifecycleObserver {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(DisplayLockContext);
  USING_PRE_FINALIZER(DisplayLockContext, Dispose);

 public:
  // Conceptually the states are private, but made public for debugging /
  // logging.
  enum State {
    kUninitialized,
    kSuspended,
    kCallbacksPending,
    kDisconnected,
    kCommitting,
    kResolving,
    kResolved
  };

  enum LifecycleUpdateState {
    kNeedsStyle,
    kNeedsLayout,
    kNeedsPrePaint,
    kNeedsPaint,
    kDone
  };

  class ScopedPendingFrameRect {
   public:
    ScopedPendingFrameRect(ScopedPendingFrameRect&&);
    ~ScopedPendingFrameRect();

   private:
    friend class DisplayLockContext;

    ScopedPendingFrameRect(DisplayLockContext*);

    UntracedMember<DisplayLockContext> context_ = nullptr;
  };

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

 private:
  friend class DisplayLockContextTest;
  friend class DisplayLockSuspendedHandle;

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
  LifecycleUpdateState lifecycle_update_state_ = kNeedsStyle;
  LayoutRect pending_frame_rect_;
  base::Optional<LayoutRect> locked_frame_rect_;

  bool process_queue_task_scheduled_ = false;
  bool update_forced_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DISPLAY_LOCK_DISPLAY_LOCK_CONTEXT_H_
