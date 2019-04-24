// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/idle/navigator_idle.h"

#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/core/workers/worker_navigator.h"
#include "third_party/blink/renderer/modules/idle/idle_manager.h"
#include "third_party/blink/renderer/platform/bindings/name_client.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/trace_wrapper_member.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

namespace {

template <typename T>
class NavigatorIdleImpl final : public GarbageCollected<NavigatorIdleImpl<T>>,
                                public Supplement<T> {
  USING_GARBAGE_COLLECTED_MIXIN(NavigatorIdleImpl);

 public:
  static const char kSupplementName[];

  NavigatorIdleImpl(T& navigator) : Supplement<T>(navigator) {}

  static NavigatorIdleImpl& From(T& navigator) {
    NavigatorIdleImpl* supplement = static_cast<NavigatorIdleImpl*>(
        Supplement<T>::template From<NavigatorIdleImpl>(navigator));
    if (!supplement) {
      supplement = MakeGarbageCollected<NavigatorIdleImpl>(navigator);
      // MakeGarbageCollected seems appropriate, but why was the following
      // working prior to my rebase? it doesn't seem like the new operator
      // was deleted since my rebase from GarbageCollected. What changed?
      // supplement = new NavigatorIdleImpl(navigator);
      Supplement<T>::ProvideTo(navigator, supplement);
    }
    return *supplement;
  }

  IdleManager* GetIdleManager(ExecutionContext* context) {
    if (!idle_manager_ && context) {
      // idle_manager_ = new IdleManager(context);
      // Supplement<T>::GetSupplementable()->GetFrame()->GetDocument()
      idle_manager_ = IdleManager::Create(context);
    }
    return idle_manager_.Get();
  }

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(idle_manager_);
    Supplement<T>::Trace(visitor);
  }

 private:
  mutable TraceWrapperMember<IdleManager> idle_manager_;
};

// static
template <typename T>
const char NavigatorIdleImpl<T>::kSupplementName[] = "NavigatorIdleImpl";

}  // namespace

IdleManager* NavigatorIdle::idle(ScriptState* script_state,
                                 Navigator& navigator) {
  return NavigatorIdleImpl<Navigator>::From(navigator).GetIdleManager(
      ExecutionContext::From(script_state));
}

IdleManager* NavigatorIdle::idle(ScriptState* script_state,
                                 WorkerNavigator& navigator) {
  return NavigatorIdleImpl<WorkerNavigator>::From(navigator).GetIdleManager(
      ExecutionContext::From(script_state));
}

}  // namespace blink
