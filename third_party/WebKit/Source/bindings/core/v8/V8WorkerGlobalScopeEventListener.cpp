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

#include "bindings/core/v8/V8WorkerGlobalScopeEventListener.h"

#include "bindings/core/v8/V8BindingForCore.h"
#include "bindings/core/v8/V8Event.h"
#include "bindings/core/v8/V8EventTarget.h"
#include "bindings/core/v8/V8GCController.h"
#include "bindings/core/v8/V8ScriptRunner.h"
#include "bindings/core/v8/WorkerOrWorkletScriptController.h"
#include "core/dom/ExecutionContext.h"
#include "core/inspector/InspectorTraceEvents.h"
#include "core/probe/CoreProbes.h"
#include "core/workers/WorkerGlobalScope.h"
#include "platform/bindings/V8DOMWrapper.h"

namespace blink {

V8WorkerGlobalScopeEventListener::V8WorkerGlobalScopeEventListener(
    bool is_inline,
    ScriptState* script_state)
    : V8EventListener(is_inline, script_state) {}

void V8WorkerGlobalScopeEventListener::HandleEvent(ScriptState* script_state,
                                                   Event* event) {
  v8::Local<v8::Context> context = script_state->GetContext();
  WorkerOrWorkletScriptController* script =
      ToWorkerGlobalScope(ToExecutionContext(context))->ScriptController();
  if (!script)
    return;

  ScriptState::Scope scope(script_state);

  // Get the V8 wrapper for the event object.
  v8::Local<v8::Value> js_event = ToV8(event, context->Global(), GetIsolate());
  if (js_event.IsEmpty())
    return;

  InvokeEventHandler(script_state, event,
                     v8::Local<v8::Value>::New(GetIsolate(), js_event));
}

v8::Local<v8::Value> V8WorkerGlobalScopeEventListener::CallListenerFunction(
    ScriptState* script_state,
    v8::Local<v8::Value> js_event,
    Event* event) {
  DCHECK(!js_event.IsEmpty());
  v8::Local<v8::Function> handler_function = GetListenerFunction(script_state);
  v8::Local<v8::Object> receiver = GetReceiverObject(script_state, event);
  if (handler_function.IsEmpty() || receiver.IsEmpty())
    return v8::Local<v8::Value>();

  v8::Local<v8::Value> parameters[1] = {js_event};
  v8::MaybeLocal<v8::Value> maybe_result = V8ScriptRunner::CallFunction(
      handler_function, ToExecutionContext(script_state->GetContext()),
      receiver, WTF_ARRAY_LENGTH(parameters), parameters, GetIsolate());

  v8::Local<v8::Value> result;
  if (!maybe_result.ToLocal(&result))
    return v8::Local<v8::Value>();
  return result;
}

// FIXME: Remove getReceiverObject().
// This is almost identical to V8AbstractEventListener::getReceiverObject().
v8::Local<v8::Object> V8WorkerGlobalScopeEventListener::GetReceiverObject(
    ScriptState* script_state,
    Event* event) {
  v8::Local<v8::Object> listener =
      GetListenerObject(ExecutionContext::From(script_state));

  if (!listener.IsEmpty() && !listener->IsFunction())
    return listener;

  EventTarget* target = event->currentTarget();
  v8::Local<v8::Value> value =
      ToV8(target, script_state->GetContext()->Global(), GetIsolate());
  if (value.IsEmpty())
    return v8::Local<v8::Object>();
  return v8::Local<v8::Object>::New(GetIsolate(),
                                    v8::Local<v8::Object>::Cast(value));
}

}  // namespace blink
