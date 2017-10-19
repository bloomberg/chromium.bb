/*
 * Copyright (C) 2006, 2007, 2008, 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "bindings/core/v8/V8AbstractEventListener.h"

#include "bindings/core/v8/V8BindingForCore.h"
#include "bindings/core/v8/V8Event.h"
#include "bindings/core/v8/V8EventListenerHelper.h"
#include "bindings/core/v8/V8EventTarget.h"
#include "bindings/core/v8/WorkerOrWorkletScriptController.h"
#include "core/dom/Document.h"
#include "core/dom/DocumentParser.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/events/Event.h"
#include "core/events/BeforeUnloadEvent.h"
#include "core/workers/WorkerGlobalScope.h"
#include "platform/InstanceCounters.h"
#include "platform/bindings/V8PrivateProperty.h"

namespace blink {

V8AbstractEventListener::V8AbstractEventListener(bool is_attribute,
                                                 DOMWrapperWorld& world,
                                                 v8::Isolate* isolate)
    : EventListener(kJSEventListenerType),
      listener_(nullptr),
      is_attribute_(is_attribute),
      world_(&world),
      isolate_(isolate),
      worker_global_scope_(nullptr) {
  if (IsMainThread())
    InstanceCounters::IncrementCounter(
        InstanceCounters::kJSEventListenerCounter);
  else
    worker_global_scope_ =
        ToWorkerGlobalScope(CurrentExecutionContext(isolate));
}

V8AbstractEventListener::~V8AbstractEventListener() {
  DCHECK(listener_.IsEmpty());
  if (IsMainThread())
    InstanceCounters::DecrementCounter(
        InstanceCounters::kJSEventListenerCounter);
}

void V8AbstractEventListener::handleEvent(ExecutionContext* execution_context,
                                          Event* event) {
  if (!execution_context)
    return;
  // Don't reenter V8 if execution was terminated in this instance of V8.
  if (execution_context->IsJSExecutionForbidden())
    return;

  // A ScriptState used by the event listener needs to be calculated based on
  // the ExecutionContext that fired the the event listener and the world
  // that installed the event listener.
  DCHECK(event);
  v8::HandleScope handle_scope(ToIsolate(execution_context));
  v8::Local<v8::Context> v8_context = ToV8Context(execution_context, World());
  if (v8_context.IsEmpty())
    return;
  ScriptState* script_state = ScriptState::From(v8_context);
  if (!script_state->ContextIsValid())
    return;
  HandleEvent(script_state, event);
}

void V8AbstractEventListener::HandleEvent(ScriptState* script_state,
                                          Event* event) {
  ScriptState::Scope scope(script_state);

  // Get the V8 wrapper for the event object.
  v8::Local<v8::Value> js_event =
      ToV8(event, script_state->GetContext()->Global(), GetIsolate());
  if (js_event.IsEmpty())
    return;
  InvokeEventHandler(script_state, event,
                     v8::Local<v8::Value>::New(GetIsolate(), js_event));
}

void V8AbstractEventListener::SetListenerObject(
    v8::Local<v8::Object> listener) {
  DCHECK(listener_.IsEmpty());
  // Balanced in wrapperCleared xor clearListenerObject.
  if (worker_global_scope_) {
    worker_global_scope_->RegisterEventListener(this);
  } else {
    keep_alive_ = this;
  }
  listener_.Set(GetIsolate(), listener, this, &WrapperCleared);
}

void V8AbstractEventListener::InvokeEventHandler(
    ScriptState* script_state,
    Event* event,
    v8::Local<v8::Value> js_event) {
  if (!event->CanBeDispatchedInWorld(World()))
    return;

  v8::Local<v8::Value> return_value;
  v8::Local<v8::Context> context = script_state->GetContext();
  {
    // Catch exceptions thrown in the event handler so they do not propagate to
    // javascript code that caused the event to fire.
    v8::TryCatch try_catch(GetIsolate());
    try_catch.SetVerbose(true);

    v8::Local<v8::Object> global = context->Global();
    V8PrivateProperty::Symbol event_symbol =
        V8PrivateProperty::GetGlobalEvent(GetIsolate());
    // Save the old 'event' property so we can restore it later.
    v8::Local<v8::Value> saved_event = event_symbol.GetOrUndefined(global);
    try_catch.Reset();

    // Make the event available in the global object, so LocalDOMWindow can
    // expose it.
    event_symbol.Set(global, js_event);
    try_catch.Reset();

    return_value = CallListenerFunction(script_state, js_event, event);
    if (try_catch.HasCaught())
      event->target()->UncaughtExceptionInEventHandler();

    if (!try_catch.CanContinue()) {  // Result of TerminateExecution().
      ExecutionContext* execution_context = ToExecutionContext(context);
      if (execution_context->IsWorkerGlobalScope())
        ToWorkerGlobalScope(execution_context)
            ->ScriptController()
            ->ForbidExecution();
      return;
    }
    try_catch.Reset();

    // Restore the old event. This must be done for all exit paths through this
    // method.
    event_symbol.Set(global, saved_event);
    try_catch.Reset();
  }

  if (return_value.IsEmpty())
    return;

  if (is_attribute_ && !return_value->IsNull() &&
      !return_value->IsUndefined() && event->IsBeforeUnloadEvent()) {
    TOSTRING_VOID(V8StringResource<>, string_return_value, return_value);
    ToBeforeUnloadEvent(event)->setReturnValue(string_return_value);
  }

  if (is_attribute_ && ShouldPreventDefault(return_value))
    event->preventDefault();
}

bool V8AbstractEventListener::ShouldPreventDefault(
    v8::Local<v8::Value> return_value) {
  // Prevent default action if the return value is false in accord with the spec
  // http://www.w3.org/TR/html5/webappapis.html#event-handler-attributes
  return return_value->IsBoolean() && !return_value.As<v8::Boolean>()->Value();
}

v8::Local<v8::Object> V8AbstractEventListener::GetReceiverObject(
    ScriptState* script_state,
    Event* event) {
  v8::Local<v8::Object> listener = listener_.NewLocal(GetIsolate());
  if (!listener_.IsEmpty() && !listener->IsFunction())
    return listener;

  EventTarget* target = event->currentTarget();
  v8::Local<v8::Value> value =
      ToV8(target, script_state->GetContext()->Global(), GetIsolate());
  if (value.IsEmpty())
    return v8::Local<v8::Object>();
  return v8::Local<v8::Object>::New(GetIsolate(),
                                    v8::Local<v8::Object>::Cast(value));
}

bool V8AbstractEventListener::BelongsToTheCurrentWorld(
    ExecutionContext* execution_context) const {
  if (!GetIsolate()->GetCurrentContext().IsEmpty() &&
      &World() == &DOMWrapperWorld::Current(GetIsolate()))
    return true;
  // If currently parsing, the parser could be accessing this listener
  // outside of any v8 context; check if it belongs to the main world.
  if (!GetIsolate()->InContext() && execution_context &&
      execution_context->IsDocument()) {
    Document* document = ToDocument(execution_context);
    if (document->Parser() && document->Parser()->IsParsing())
      return World().IsMainWorld();
  }
  return false;
}

void V8AbstractEventListener::ClearListenerObject() {
  if (!HasExistingListenerObject())
    return;
  listener_.Clear();
  if (worker_global_scope_) {
    worker_global_scope_->DeregisterEventListener(this);
  } else {
    keep_alive_.Clear();
  }
}

void V8AbstractEventListener::WrapperCleared(
    const v8::WeakCallbackInfo<V8AbstractEventListener>& data) {
  data.GetParameter()->ClearListenerObject();
}

void V8AbstractEventListener::Trace(blink::Visitor* visitor) {
  visitor->Trace(worker_global_scope_);
  EventListener::Trace(visitor);
}

void V8AbstractEventListener::TraceWrappers(
    const ScriptWrappableVisitor* visitor) const {
  visitor->TraceWrappers(listener_.Cast<v8::Value>());
}

}  // namespace blink
