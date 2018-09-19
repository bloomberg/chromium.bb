// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/v8_event_listener_impl.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/worker_or_worklet_script_controller.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/workers/worker_or_worklet_global_scope.h"
#include "third_party/blink/renderer/platform/bindings/v8_private_property.h"
#include "third_party/blink/renderer/platform/instance_counters.h"

namespace blink {

V8EventListenerImpl::V8EventListenerImpl(
    v8::Local<v8::Object> listener,
    ScriptState* script_state,
    const V8PrivateProperty::Symbol& property)
    : EventListener(kJSEventListenerType),
      event_listener_(V8EventListener::CreateOrNull(listener)) {
  DCHECK(event_listener_);
  Attach(script_state, listener, property, this);
  if (IsMainThread()) {
    InstanceCounters::IncrementCounter(
        InstanceCounters::kJSEventListenerCounter);
  }
}

V8EventListenerImpl::~V8EventListenerImpl() {
  if (IsMainThread()) {
    InstanceCounters::DecrementCounter(
        InstanceCounters::kJSEventListenerCounter);
  }
}

// https://dom.spec.whatwg.org/#concept-event-listener-inner-invoke
void V8EventListenerImpl::handleEvent(
    ExecutionContext* execution_context_of_event_target,
    Event* event) {
  // Don't reenter V8 if execution was terminated in this instance of V8.
  // For example, worker can be terminated in event listener, and also window
  // can be terminated from inspector by the TerminateExecution method.
  if (event_listener_->GetIsolate()->IsExecutionTerminating())
    return;

  DCHECK(execution_context_of_event_target);
  DCHECK(event);
  if (!event->CanBeDispatchedInWorld(World()))
    return;

  ScriptState* script_state_of_listener =
      event_listener_->CallbackRelevantScriptState();
  if (!script_state_of_listener->ContextIsValid())
    return;

  ScriptState::Scope scope(script_state_of_listener);

  // TODO(crbug.com/881688): Remove this check because listener should not be
  // empty on calling it here.
  if (GetListenerObject().IsEmpty())
    return;

  // https://dom.spec.whatwg.org/#firing-events
  // Step 2. of firing events: Let event be the result of creating an event
  // given eventConstructor, in the relevant Realm of target.
  //
  // (Note: a |js_event|, an v8::Value for |event| in the relevant realm of
  // |event|'s target, is created here. It should be done before dispatching
  // events, but Event and EventTarget do not have world to get |v8_context|, so
  // it is done here with |execution_context_of_event_target|.)
  v8::Local<v8::Context> v8_context =
      ToV8Context(execution_context_of_event_target, World());
  if (v8_context.IsEmpty())
    return;
  v8::Local<v8::Value> js_event =
      ToV8(event, v8_context->Global(), event_listener_->GetIsolate());
  if (js_event.IsEmpty())
    return;

  // Step 6: Let |global| be listener callback’s associated Realm’s global
  // object.
  v8::Local<v8::Object> global =
      script_state_of_listener->GetContext()->Global();

  // Step 8: If global is a Window object, then:
  // Set currentEvent to global’s current event.
  // If tuple’s item-in-shadow-tree is false, then set global’s current event to
  // event.
  V8PrivateProperty::Symbol event_symbol =
      V8PrivateProperty::GetGlobalEvent(event_listener_->GetIsolate());
  ExecutionContext* execution_context_of_listener =
      ExecutionContext::From(script_state_of_listener);
  v8::Local<v8::Value> current_event;
  if (execution_context_of_listener->IsDocument()) {
    current_event = event_symbol.GetOrUndefined(global).ToLocalChecked();
    // Expose the event object as |window.event|, except when the event's target
    // is in a V1 shadow tree.
    Node* target_node = event->target()->ToNode();
    if (!(target_node && target_node->IsInV1ShadowTree()))
      event_symbol.Set(global, js_event);
  }

  {
    // Catch exceptions thrown in the event listener if any and report them to
    // DevTools console.
    v8::TryCatch try_catch(event_listener_->GetIsolate());
    try_catch.SetVerbose(true);

    // Step 10: Call a listener with event's currentTarget as receiver and event
    // and handle errors if thrown.
    v8::Maybe<void> maybe_result =
        event_listener_->handleEvent(event->currentTarget(), event);
    ALLOW_UNUSED_LOCAL(maybe_result);

    if (try_catch.HasCaught()) {
      // Step 10-2: Set legacyOutputDidListenersThrowFlag if given.
      event->LegacySetDidListenersThrowFlag();
    }

    // TODO(yukiy): consider to set |global|’s current event to |current_event|
    // after execution is terminated if it is necessary and possible.
    if (event_listener_->GetIsolate()->IsExecutionTerminating())
      return;
  }

  // Step 12: If |global| is a Window object, then set |global|’s current event
  // to |current_event|.
  if (execution_context_of_listener->IsDocument())
    event_symbol.Set(global, current_event);
}

void V8EventListenerImpl::Trace(blink::Visitor* visitor) {
  visitor->Trace(event_listener_);
  EventListener::Trace(visitor);
}

}  // namespace blink
