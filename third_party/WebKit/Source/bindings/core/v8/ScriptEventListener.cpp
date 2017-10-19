/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#include "bindings/core/v8/ScriptEventListener.h"

#include "bindings/core/v8/ScheduledAction.h"
#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/SourceLocation.h"
#include "bindings/core/v8/V8AbstractEventListener.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "bindings/core/v8/WindowProxy.h"
#include "core/dom/Document.h"
#include "core/dom/DocumentParser.h"
#include "core/dom/QualifiedName.h"
#include "core/dom/events/EventListener.h"
#include "core/frame/LocalFrame.h"
#include "platform/bindings/ScriptState.h"
#include "v8/include/v8.h"

namespace blink {

V8LazyEventListener* CreateAttributeEventListener(
    Node* node,
    const QualifiedName& name,
    const AtomicString& value,
    const AtomicString& event_parameter_name) {
  DCHECK(node);
  if (value.IsNull())
    return nullptr;

  // FIXME: Very strange: we initialize zero-based number with '1'.
  TextPosition position(OrdinalNumber::FromZeroBasedInt(1),
                        OrdinalNumber::First());
  String source_url;

  v8::Isolate* isolate = ToIsolate(&node->GetDocument());
  if (LocalFrame* frame = node->GetDocument().GetFrame()) {
    ScriptController& script_controller = frame->GetScriptController();
    if (!node->GetDocument().CanExecuteScripts(kAboutToExecuteScript))
      return nullptr;
    position = script_controller.EventHandlerPosition();
    source_url = node->GetDocument().Url().GetString();
  }

  return V8LazyEventListener::Create(name.LocalName(), event_parameter_name,
                                     value, source_url, position, node,
                                     isolate);
}

V8LazyEventListener* CreateAttributeEventListener(
    LocalFrame* frame,
    const QualifiedName& name,
    const AtomicString& value,
    const AtomicString& event_parameter_name) {
  if (!frame)
    return nullptr;

  if (value.IsNull())
    return nullptr;

  if (!frame->GetDocument()->CanExecuteScripts(kAboutToExecuteScript))
    return nullptr;

  TextPosition position = frame->GetScriptController().EventHandlerPosition();
  String source_url = frame->GetDocument()->Url().GetString();

  return V8LazyEventListener::Create(name.LocalName(), event_parameter_name,
                                     value, source_url, position, nullptr,
                                     ToIsolate(frame));
}

v8::Local<v8::Object> EventListenerHandler(ExecutionContext* execution_context,
                                           EventListener* listener) {
  if (listener->GetType() != EventListener::kJSEventListenerType)
    return v8::Local<v8::Object>();
  V8AbstractEventListener* v8_listener =
      static_cast<V8AbstractEventListener*>(listener);
  return v8_listener->GetListenerObject(execution_context);
}

v8::Local<v8::Function> EventListenerEffectiveFunction(
    v8::Isolate* isolate,
    v8::Local<v8::Object> handler) {
  v8::Local<v8::Function> function;
  if (handler->IsFunction()) {
    function = handler.As<v8::Function>();
  } else if (handler->IsObject()) {
    v8::Local<v8::Value> property;
    // Try the "handleEvent" method (EventListener interface).
    if (handler
            ->Get(handler->CreationContext(),
                  V8AtomicString(isolate, "handleEvent"))
            .ToLocal(&property) &&
        property->IsFunction())
      function = property.As<v8::Function>();
    // Fall back to the "constructor" property.
    else if (handler
                 ->Get(handler->CreationContext(),
                       V8AtomicString(isolate, "constructor"))
                 .ToLocal(&property) &&
             property->IsFunction())
      function = property.As<v8::Function>();
  }
  if (!function.IsEmpty())
    return GetBoundFunction(function);
  return v8::Local<v8::Function>();
}

void GetFunctionLocation(v8::Local<v8::Function> function,
                         String& script_id,
                         int& line_number,
                         int& column_number) {
  int script_id_value = function->ScriptId();
  script_id = String::Number(script_id_value);
  line_number = function->GetScriptLineNumber();
  column_number = function->GetScriptColumnNumber();
}

std::unique_ptr<SourceLocation> GetFunctionLocation(
    ExecutionContext* execution_context,
    EventListener* listener) {
  v8::Isolate* isolate = ToIsolate(execution_context);
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Object> handler =
      EventListenerHandler(execution_context, listener);
  if (handler.IsEmpty())
    return nullptr;
  return SourceLocation::FromFunction(
      EventListenerEffectiveFunction(isolate, handler));
}

}  // namespace blink
