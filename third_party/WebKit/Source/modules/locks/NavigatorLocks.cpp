// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/locks/NavigatorLocks.h"

#include "core/frame/Navigator.h"
#include "core/workers/WorkerNavigator.h"
#include "modules/locks/LockManager.h"
#include "platform/Supplementable.h"
#include "platform/bindings/ScriptState.h"
#include "platform/bindings/TraceWrapperMember.h"

namespace blink {

namespace {

template <typename T>
class NavigatorLocksImpl final : public GarbageCollected<NavigatorLocksImpl<T>>,
                                 public Supplement<T>,
                                 public TraceWrapperBase {
  USING_GARBAGE_COLLECTED_MIXIN(NavigatorLocksImpl);

 public:
  static NavigatorLocksImpl& From(T& navigator) {
    NavigatorLocksImpl* supplement = static_cast<NavigatorLocksImpl*>(
        Supplement<T>::From(navigator, SupplementName()));
    if (!supplement) {
      supplement = new NavigatorLocksImpl(navigator);
      Supplement<T>::ProvideTo(navigator, SupplementName(), supplement);
    }
    return *supplement;
  }

  LockManager* GetLockManager(ExecutionContext* context) const {
    if (!lock_manager_ && context) {
      lock_manager_ = new LockManager(context);
    }
    return lock_manager_.Get();
  }

  virtual void Trace(blink::Visitor* visitor) {
    visitor->Trace(lock_manager_);
    Supplement<T>::Trace(visitor);
  }

  // Wrapper tracing is needed for callbacks. The reference chain is
  // NavigatorLocksImpl -> LockManager -> LockRequestImpl ->
  // V8LockGrantedCallback.
  void TraceWrappers(const ScriptWrappableVisitor* visitor) const {
    visitor->TraceWrappers(lock_manager_);
  }

 private:
  explicit NavigatorLocksImpl(T& navigator) : Supplement<T>(navigator) {}

  static const char* SupplementName() { return "NavigatorLocksImpl"; }

  mutable TraceWrapperMember<LockManager> lock_manager_;
};

}  // namespace

LockManager* NavigatorLocks::locks(ScriptState* script_state,
                                   Navigator& navigator) {
  return NavigatorLocksImpl<Navigator>::From(navigator).GetLockManager(
      ExecutionContext::From(script_state));
}

LockManager* NavigatorLocks::locks(ScriptState* script_state,
                                   WorkerNavigator& navigator) {
  return NavigatorLocksImpl<WorkerNavigator>::From(navigator).GetLockManager(
      ExecutionContext::From(script_state));
}

}  // namespace blink
