// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/events/promise_rejection_event.h"

#include "third_party/blink/renderer/core/event_interface_names.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"

namespace blink {

PromiseRejectionEvent::PromiseRejectionEvent(
    ScriptState* state,
    const AtomicString& type,
    const PromiseRejectionEventInit* initializer)
    : Event(type, initializer), world_(&state->World()) {
  DCHECK(initializer->hasPromise());
  promise_.Set(initializer->promise().GetIsolate(),
               initializer->promise().V8Value());
  if (initializer->hasReason()) {
    reason_.Set(initializer->reason().GetIsolate(),
                initializer->reason().V8Value());
  }
}

PromiseRejectionEvent::~PromiseRejectionEvent() = default;

void PromiseRejectionEvent::Dispose() {
  // Clear ScopedPersistents so that V8 doesn't call phantom callbacks
  // (and touch the ScopedPersistents) after Oilpan starts lazy sweeping.
  promise_.Clear();
  reason_.Clear();
  world_ = nullptr;
}

ScriptPromise PromiseRejectionEvent::promise(ScriptState* script_state) const {
  // Return null when the promise is accessed by a different world than the
  // world that created the promise.
  if (!CanBeDispatchedInWorld(script_state->World()))
    return ScriptPromise();
  return ScriptPromise(script_state,
                       promise_.NewLocal(script_state->GetIsolate()));
}

ScriptValue PromiseRejectionEvent::reason(ScriptState* script_state) const {
  // Return undefined when the value is accessed by a different world than the
  // world that created the value.
  if (reason_.IsEmpty() || !CanBeDispatchedInWorld(script_state->World()))
    return ScriptValue(script_state, v8::Undefined(script_state->GetIsolate()));
  return ScriptValue(script_state,
                     reason_.NewLocal(script_state->GetIsolate()));
}

const AtomicString& PromiseRejectionEvent::InterfaceName() const {
  return event_interface_names::kPromiseRejectionEvent;
}

bool PromiseRejectionEvent::CanBeDispatchedInWorld(
    const DOMWrapperWorld& world) const {
  return world_ && world_->GetWorldId() == world.GetWorldId();
}

void PromiseRejectionEvent::Trace(blink::Visitor* visitor) {
  visitor->Trace(promise_);
  visitor->Trace(reason_);
  Event::Trace(visitor);
}

}  // namespace blink
