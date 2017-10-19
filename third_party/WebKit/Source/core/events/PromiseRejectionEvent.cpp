// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/events/PromiseRejectionEvent.h"

#include "platform/bindings/DOMWrapperWorld.h"

namespace blink {

PromiseRejectionEvent::PromiseRejectionEvent(
    ScriptState* state,
    const AtomicString& type,
    const PromiseRejectionEventInit& initializer)
    : Event(type, initializer),
      world_(&state->World()),
      promise_(this),
      reason_(this) {
  DCHECK(initializer.hasPromise());
  promise_.Set(initializer.promise().GetIsolate(),
               initializer.promise().V8Value());
  if (initializer.hasReason()) {
    reason_.Set(initializer.reason().GetIsolate(),
                initializer.reason().V8Value());
  }
}

PromiseRejectionEvent::~PromiseRejectionEvent() {}

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
  // Return null when the value is accessed by a different world than the world
  // that created the value.
  if (reason_.IsEmpty() || !CanBeDispatchedInWorld(script_state->World()))
    return ScriptValue(script_state, v8::Undefined(script_state->GetIsolate()));
  return ScriptValue(script_state,
                     reason_.NewLocal(script_state->GetIsolate()));
}

const AtomicString& PromiseRejectionEvent::InterfaceName() const {
  return EventNames::PromiseRejectionEvent;
}

bool PromiseRejectionEvent::CanBeDispatchedInWorld(
    const DOMWrapperWorld& world) const {
  return world_ && world_->GetWorldId() == world.GetWorldId();
}

void PromiseRejectionEvent::Trace(blink::Visitor* visitor) {
  Event::Trace(visitor);
}

void PromiseRejectionEvent::TraceWrappers(
    const ScriptWrappableVisitor* visitor) const {
  visitor->TraceWrappers(promise_);
  visitor->TraceWrappers(reason_);
}

}  // namespace blink
