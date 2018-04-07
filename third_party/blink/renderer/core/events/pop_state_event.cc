/*
 * Copyright (C) 2009 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "third_party/blink/renderer/core/events/pop_state_event.h"

#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/core/frame/history.h"

namespace blink {

PopStateEvent::PopStateEvent()
    : serialized_state_(nullptr), history_(nullptr) {}

PopStateEvent::PopStateEvent(ScriptState* script_state,
                             const AtomicString& type,
                             const PopStateEventInit& initializer)
    : Event(type, initializer), history_(nullptr) {
  if (initializer.hasState()) {
    world_ = WrapRefCounted(&script_state->World());
    state_.Set(initializer.state().GetIsolate(), initializer.state().V8Value());
  }
}

PopStateEvent::PopStateEvent(
    scoped_refptr<SerializedScriptValue> serialized_state,
    History* history)
    : Event(EventTypeNames::popstate, Bubbles::kNo, Cancelable::kYes),
      serialized_state_(std::move(serialized_state)),
      history_(history) {}

PopStateEvent::~PopStateEvent() = default;

ScriptValue PopStateEvent::state(ScriptState* script_state) const {
  if (state_.IsEmpty())
    return ScriptValue();

  v8::Isolate* isolate = script_state->GetIsolate();
  if (world_->GetWorldId() != script_state->World().GetWorldId()) {
    v8::Local<v8::Value> value = state_.NewLocal(isolate);
    scoped_refptr<SerializedScriptValue> serialized =
        SerializedScriptValue::SerializeAndSwallowExceptions(isolate, value);
    return ScriptValue(script_state, serialized->Deserialize(isolate));
  }
  return ScriptValue(script_state, state_.NewLocal(isolate));
}

PopStateEvent* PopStateEvent::Create() {
  return new PopStateEvent;
}

PopStateEvent* PopStateEvent::Create(
    scoped_refptr<SerializedScriptValue> serialized_state,
    History* history) {
  return new PopStateEvent(std::move(serialized_state), history);
}

PopStateEvent* PopStateEvent::Create(ScriptState* script_state,
                                     const AtomicString& type,
                                     const PopStateEventInit& initializer) {
  return new PopStateEvent(script_state, type, initializer);
}

const AtomicString& PopStateEvent::InterfaceName() const {
  return EventNames::PopStateEvent;
}

void PopStateEvent::Trace(blink::Visitor* visitor) {
  visitor->Trace(history_);
  Event::Trace(visitor);
}

void PopStateEvent::TraceWrappers(const ScriptWrappableVisitor* visitor) const {
  visitor->TraceWrappers(state_);
  Event::TraceWrappers(visitor);
}

}  // namespace blink
