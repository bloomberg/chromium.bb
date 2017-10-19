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

#include "bindings/core/v8/V8LazyEventListener.h"

#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/ScriptSourceCode.h"
#include "bindings/core/v8/SourceLocation.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "bindings/core/v8/V8Document.h"
#include "bindings/core/v8/V8HTMLFormElement.h"
#include "bindings/core/v8/V8Node.h"
#include "bindings/core/v8/V8ScriptRunner.h"
#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/Node.h"
#include "core/events/ErrorEvent.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/html/HTMLElement.h"
#include "core/html/forms/HTMLFormElement.h"
#include "platform/bindings/V8DOMWrapper.h"
#include "platform/bindings/V8PrivateProperty.h"
#include "platform/wtf/StdLibExtras.h"

namespace blink {

V8LazyEventListener::V8LazyEventListener(
    v8::Isolate* isolate,
    const AtomicString& function_name,
    const AtomicString& event_parameter_name,
    const String& code,
    const String source_url,
    const TextPosition& position,
    Node* node)
    : V8AbstractEventListener(true, DOMWrapperWorld::MainWorld(), isolate),
      was_compilation_failed_(false),
      function_name_(function_name),
      event_parameter_name_(event_parameter_name),
      code_(code),
      source_url_(source_url),
      node_(node),
      position_(position) {}

template <typename T>
v8::Local<v8::Object> ToObjectWrapper(T* dom_object,
                                      ScriptState* script_state) {
  if (!dom_object)
    return v8::Object::New(script_state->GetIsolate());
  v8::Local<v8::Value> value =
      ToV8(dom_object, script_state->GetContext()->Global(),
           script_state->GetIsolate());
  if (value.IsEmpty())
    return v8::Object::New(script_state->GetIsolate());
  return v8::Local<v8::Object>::New(script_state->GetIsolate(),
                                    value.As<v8::Object>());
}

v8::Local<v8::Value> V8LazyEventListener::CallListenerFunction(
    ScriptState* script_state,
    v8::Local<v8::Value> js_event,
    Event* event) {
  DCHECK(!js_event.IsEmpty());
  ExecutionContext* execution_context =
      ToExecutionContext(script_state->GetContext());
  v8::Local<v8::Object> listener_object = GetListenerObject(execution_context);
  if (listener_object.IsEmpty())
    return v8::Local<v8::Value>();

  v8::Local<v8::Function> handler_function = listener_object.As<v8::Function>();
  v8::Local<v8::Object> receiver = GetReceiverObject(script_state, event);
  if (handler_function.IsEmpty() || receiver.IsEmpty())
    return v8::Local<v8::Value>();

  if (!execution_context->IsDocument())
    return v8::Local<v8::Value>();

  LocalFrame* frame = ToDocument(execution_context)->GetFrame();
  if (!frame)
    return v8::Local<v8::Value>();

  if (!execution_context->CanExecuteScripts(kAboutToExecuteScript))
    return v8::Local<v8::Value>();

  v8::Local<v8::Value> parameters[1] = {js_event};
  v8::Local<v8::Value> result;
  if (!V8ScriptRunner::CallFunction(handler_function, frame->GetDocument(),
                                    receiver, WTF_ARRAY_LENGTH(parameters),
                                    parameters, script_state->GetIsolate())
           .ToLocal(&result))
    return v8::Local<v8::Value>();
  return result;
}

static void V8LazyEventListenerToString(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  V8SetReturnValue(
      info, V8PrivateProperty::GetLazyEventListenerToString(info.GetIsolate())
                .GetOrUndefined(info.Holder()));
}

v8::Local<v8::Object> V8LazyEventListener::GetListenerObjectInternal(
    ExecutionContext* execution_context) {
  if (!execution_context)
    return v8::Local<v8::Object>();

  // A ScriptState used by the event listener needs to be calculated based on
  // the ExecutionContext that fired the the event listener and the world
  // that installed the event listener.
  v8::EscapableHandleScope handle_scope(ToIsolate(execution_context));
  v8::Local<v8::Context> v8_context = ToV8Context(execution_context, World());
  if (v8_context.IsEmpty())
    return v8::Local<v8::Object>();
  ScriptState* script_state = ScriptState::From(v8_context);
  if (!script_state->ContextIsValid())
    return v8::Local<v8::Object>();

  if (!execution_context->IsDocument())
    return v8::Local<v8::Object>();

  if (!ToDocument(execution_context)
           ->AllowInlineEventHandler(node_, this, source_url_, position_.line_))
    return v8::Local<v8::Object>();

  // All checks passed and it's now okay to return the function object.

  // We may need to compile the same script twice or more because the compiled
  // function object may be garbage-collected, however, we should behave as if
  // we compile the code only once, i.e. we must not throw an error twice.
  if (!HasExistingListenerObject() && !was_compilation_failed_)
    CompileScript(script_state, execution_context);

  return handle_scope.Escape(GetExistingListenerObject());
}

void V8LazyEventListener::CompileScript(ScriptState* script_state,
                                        ExecutionContext* execution_context) {
  DCHECK(!HasExistingListenerObject());

  ScriptState::Scope scope(script_state);

  // Nodes other than the document object, when executing inline event
  // handlers push document, form owner, and the target node on the scope chain.
  // We do this by using 'with' statement.
  // See fast/forms/form-action.html
  //     fast/forms/selected-index-value.html
  //     fast/overflow/onscroll-layer-self-destruct.html
  HTMLFormElement* form_element = nullptr;
  if (node_ && node_->IsHTMLElement())
    form_element = ToHTMLElement(node_)->formOwner();

  v8::Local<v8::Object> scopes[3];
  scopes[2] = ToObjectWrapper<Node>(node_, script_state);
  scopes[1] = ToObjectWrapper<HTMLFormElement>(form_element, script_state);
  scopes[0] = ToObjectWrapper<Document>(
      node_ ? node_->ownerDocument() : nullptr, script_state);

  v8::Local<v8::String> parameter_name =
      V8String(GetIsolate(), event_parameter_name_);
  v8::ScriptOrigin origin(
      V8String(GetIsolate(), source_url_),
      v8::Integer::New(GetIsolate(), position_.line_.ZeroBasedInt()),
      v8::Integer::New(GetIsolate(), position_.column_.ZeroBasedInt()),
      v8::True(GetIsolate()));
  v8::ScriptCompiler::Source source(V8String(GetIsolate(), code_), origin);

  v8::Local<v8::Function> wrapped_function;
  {
    // JavaScript compilation error shouldn't be reported as a runtime
    // exception because we're not running any program code.  Instead,
    // it should be reported as an ErrorEvent.
    v8::TryCatch block(GetIsolate());
    wrapped_function = v8::ScriptCompiler::CompileFunctionInContext(
        GetIsolate(), &source, script_state->GetContext(), 1, &parameter_name,
        3, scopes);
    if (block.HasCaught()) {
      was_compilation_failed_ = true;  // Do not compile the same code twice.
      FireErrorEvent(script_state->GetContext(), execution_context,
                     block.Message());
      return;
    }
  }

  // Change the toString function on the wrapper function to avoid it
  // returning the source for the actual wrapper function. Instead it
  // returns source for a clean wrapper function with the event
  // argument wrapping the event source code. The reason for this is
  // that some web sites use toString on event functions and eval the
  // source returned (sometimes a RegExp is applied as well) for some
  // other use. That fails miserably if the actual wrapper source is
  // returned.
  v8::Local<v8::Function> to_string_function;
  if (!v8::Function::New(script_state->GetContext(),
                         V8LazyEventListenerToString, v8::Local<v8::Value>(), 0,
                         v8::ConstructorBehavior::kThrow)
           .ToLocal(&to_string_function))
    return;
  String to_string_string = "function " + function_name_ + "(" +
                            event_parameter_name_ + ") {\n  " + code_ + "\n}";
  V8PrivateProperty::GetLazyEventListenerToString(GetIsolate())
      .Set(wrapped_function, V8String(GetIsolate(), to_string_string));
  if (!V8CallBoolean(wrapped_function->CreateDataProperty(
          script_state->GetContext(), V8AtomicString(GetIsolate(), "toString"),
          to_string_function)))
    return;
  wrapped_function->SetName(V8String(GetIsolate(), function_name_));

  SetListenerObject(wrapped_function);
}

void V8LazyEventListener::FireErrorEvent(v8::Local<v8::Context> v8_context,
                                         ExecutionContext* execution_context,
                                         v8::Local<v8::Message> message) {
  ErrorEvent* event = ErrorEvent::Create(
      ToCoreStringWithNullCheck(message->Get()),
      SourceLocation::FromMessage(GetIsolate(), message, execution_context),
      &World());

  AccessControlStatus access_control_status = kNotSharableCrossOrigin;
  if (message->IsOpaque())
    access_control_status = kOpaqueResource;
  else if (message->IsSharedCrossOrigin())
    access_control_status = kSharableCrossOrigin;

  execution_context->DispatchErrorEvent(event, access_control_status);
}

}  // namespace blink
