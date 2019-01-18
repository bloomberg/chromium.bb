// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_IDLE_IDLE_STATUS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_IDLE_IDLE_STATUS_H_

#include "mojo/public/cpp/bindings/binding.h"
#include "third_party/blink/public/platform/modules/idle/idle_manager.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/core/dom/context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/execution_context/pausable_object.h"
#include "third_party/blink/renderer/modules/event_modules.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

class IdleStatus final : public EventTargetWithInlineData,
                         public ActiveScriptWrappable<IdleStatus>,
                         public PausableObject,
                         public mojom::blink::IdleMonitor {
  USING_GARBAGE_COLLECTED_MIXIN(IdleStatus);
  DEFINE_WRAPPERTYPEINFO();
  USING_PRE_FINALIZER(IdleStatus, Dispose);
  WTF_MAKE_NONCOPYABLE(IdleStatus);

  using IdleState = mojom::blink::IdleState;

 public:
  // Constructed by the IdleManager when queried by script, but not returned
  // to script until the monitor has been registered by the service and
  // returned an initial state.
  IdleStatus(ExecutionContext*,
             uint32_t threshold,
             mojom::blink::IdleMonitorRequest);

  ~IdleStatus() override;
  void Dispose();

  // Called when the service has returned an initial state.
  void Init(IdleState);

  // EventTarget implementation.
  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;

  // ActiveScriptWrappable implementation.
  bool HasPendingActivity() const final;

  // PausableObject implementation.
  void ContextPaused(PauseState) override;
  void ContextUnpaused() override;
  void ContextDestroyed(ExecutionContext*) override;

  // IdleStatus IDL interface.
  String state() const;
  DEFINE_ATTRIBUTE_EVENT_LISTENER(change, kChange);

  // mojom::blink::IdleMonitor implementation. Invoked on a state change, and
  // causes an event to be dispatched.
  void Update(IdleState state) override;

  void Trace(blink::Visitor*) override;

 private:
  // Called internally to re-start monitoring by establishing a new binding,
  // after a previous call to StopMonitoring().
  void StartMonitoring();

  // Close the binding, for example when paused or the context has been
  // destroyed.
  void StopMonitoring();

  IdleState state_;

  const uint32_t threshold_;

  // Holds a pipe which the service uses to notify this object
  // when the idle state has changed.
  mojo::Binding<mojom::blink::IdleMonitor> binding_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_IDLE_IDLE_STATUS_H_
