// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/events/PromiseRejectionEvent.h"

#include "bindings/core/v8/DOMWrapperWorld.h"

namespace blink {

PromiseRejectionEvent::PromiseRejectionEvent(
    ScriptState* state,
    const AtomicString& type,
    const PromiseRejectionEventInit& initializer)
    : Event(type, initializer),
      m_world(state->world()),
      m_promise(this),
      m_reason(this) {
  ThreadState::current()->registerPreFinalizer(this);
  DCHECK(initializer.hasPromise());
  m_promise.set(initializer.promise().isolate(),
                initializer.promise().v8Value());
  if (initializer.hasReason()) {
    m_reason.set(initializer.reason().isolate(),
                 initializer.reason().v8Value());
  }
}

PromiseRejectionEvent::~PromiseRejectionEvent() {}

void PromiseRejectionEvent::dispose() {
  // Clear ScopedPersistents so that V8 doesn't call phantom callbacks
  // (and touch the ScopedPersistents) after Oilpan starts lazy sweeping.
  m_promise.clear();
  m_reason.clear();
  m_world.clear();
}

ScriptPromise PromiseRejectionEvent::promise(ScriptState* scriptState) const {
  // Return null when the promise is accessed by a different world than the
  // world that created the promise.
  if (!canBeDispatchedInWorld(scriptState->world()))
    return ScriptPromise();
  return ScriptPromise(scriptState, m_promise.newLocal(scriptState->isolate()));
}

ScriptValue PromiseRejectionEvent::reason(ScriptState* scriptState) const {
  // Return null when the value is accessed by a different world than the world
  // that created the value.
  if (m_reason.isEmpty() || !canBeDispatchedInWorld(scriptState->world()))
    return ScriptValue(scriptState, v8::Undefined(scriptState->isolate()));
  return ScriptValue(scriptState, m_reason.newLocal(scriptState->isolate()));
}

void PromiseRejectionEvent::setWrapperReference(
    v8::Isolate* isolate,
    const v8::Persistent<v8::Object>& wrapper) {
  // This might create cross world references. However, the regular code path
  // will not create them, and if we get a cross world reference here, the
  // worst thing is that the lifetime is too long (similar to what happens
  // for DOM trees).
  if (!m_promise.isEmpty())
    m_promise.setReference(wrapper, isolate);
  if (!m_reason.isEmpty())
    m_reason.setReference(wrapper, isolate);
}

const AtomicString& PromiseRejectionEvent::interfaceName() const {
  return EventNames::PromiseRejectionEvent;
}

bool PromiseRejectionEvent::canBeDispatchedInWorld(
    const DOMWrapperWorld& world) const {
  return m_world && m_world->worldId() == world.worldId();
}

DEFINE_TRACE(PromiseRejectionEvent) {
  Event::trace(visitor);
}

DEFINE_TRACE_WRAPPERS(PromiseRejectionEvent) {
  visitor->traceWrappers(m_promise);
  visitor->traceWrappers(m_reason);
}

}  // namespace blink
