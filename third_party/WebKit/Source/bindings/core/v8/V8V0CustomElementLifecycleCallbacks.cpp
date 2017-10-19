/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "bindings/core/v8/V8V0CustomElementLifecycleCallbacks.h"

#include <memory>
#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "bindings/core/v8/V8Element.h"
#include "core/dom/ExecutionContext.h"
#include "platform/bindings/DOMDataStore.h"
#include "platform/bindings/V0CustomElementBinding.h"
#include "platform/bindings/V8PerContextData.h"
#include "platform/bindings/V8PrivateProperty.h"

namespace blink {

#define CALLBACK_LIST(V)        \
  V(created, CreatedCallback)   \
  V(attached, AttachedCallback) \
  V(detached, DetachedCallback) \
  V(attribute_changed, AttributeChangedCallback)

V8V0CustomElementLifecycleCallbacks*
V8V0CustomElementLifecycleCallbacks::Create(
    ScriptState* script_state,
    v8::Local<v8::Object> prototype,
    v8::MaybeLocal<v8::Function> created,
    v8::MaybeLocal<v8::Function> attached,
    v8::MaybeLocal<v8::Function> detached,
    v8::MaybeLocal<v8::Function> attribute_changed) {
  v8::Isolate* isolate = script_state->GetIsolate();

// A given object can only be used as a Custom Element prototype
// once; see customElementIsInterfacePrototypeObject
#define SET_PRIVATE_PROPERTY(Maybe, Name)                          \
  V8PrivateProperty::Symbol symbol##Name =                         \
      V8PrivateProperty::GetCustomElementLifecycle##Name(isolate); \
  DCHECK(!symbol##Name.HasValue(prototype));                       \
  {                                                                \
    v8::Local<v8::Function> function;                              \
    if (Maybe.ToLocal(&function))                                  \
      symbol##Name.Set(prototype, function);                       \
  }

  CALLBACK_LIST(SET_PRIVATE_PROPERTY)
#undef SET_PRIVATE_PROPERTY

  return new V8V0CustomElementLifecycleCallbacks(
      script_state, prototype, created, attached, detached, attribute_changed);
}

static V0CustomElementLifecycleCallbacks::CallbackType FlagSet(
    v8::MaybeLocal<v8::Function> attached,
    v8::MaybeLocal<v8::Function> detached,
    v8::MaybeLocal<v8::Function> attribute_changed) {
  // V8 Custom Elements always run created to swizzle prototypes.
  int flags = V0CustomElementLifecycleCallbacks::kCreatedCallback;

  if (!attached.IsEmpty())
    flags |= V0CustomElementLifecycleCallbacks::kAttachedCallback;

  if (!detached.IsEmpty())
    flags |= V0CustomElementLifecycleCallbacks::kDetachedCallback;

  if (!attribute_changed.IsEmpty())
    flags |= V0CustomElementLifecycleCallbacks::kAttributeChangedCallback;

  return V0CustomElementLifecycleCallbacks::CallbackType(flags);
}

V8V0CustomElementLifecycleCallbacks::V8V0CustomElementLifecycleCallbacks(
    ScriptState* script_state,
    v8::Local<v8::Object> prototype,
    v8::MaybeLocal<v8::Function> created,
    v8::MaybeLocal<v8::Function> attached,
    v8::MaybeLocal<v8::Function> detached,
    v8::MaybeLocal<v8::Function> attribute_changed)
    : V0CustomElementLifecycleCallbacks(
          FlagSet(attached, detached, attribute_changed)),
      script_state_(script_state),
      prototype_(script_state->GetIsolate(), prototype),
      created_(script_state->GetIsolate(), created),
      attached_(script_state->GetIsolate(), attached),
      detached_(script_state->GetIsolate(), detached),
      attribute_changed_(script_state->GetIsolate(), attribute_changed) {
  prototype_.SetPhantom();

#define MAKE_WEAK(Var, Ignored) \
  if (!Var##_.IsEmpty())        \
    Var##_.SetPhantom();

  CALLBACK_LIST(MAKE_WEAK)
#undef MAKE_WEAK
}

V8PerContextData* V8V0CustomElementLifecycleCallbacks::CreationContextData() {
  if (!script_state_->ContextIsValid())
    return nullptr;

  v8::Local<v8::Context> context = script_state_->GetContext();
  if (context.IsEmpty())
    return nullptr;

  return V8PerContextData::From(context);
}

V8V0CustomElementLifecycleCallbacks::~V8V0CustomElementLifecycleCallbacks() {}

bool V8V0CustomElementLifecycleCallbacks::SetBinding(
    std::unique_ptr<V0CustomElementBinding> binding) {
  V8PerContextData* per_context_data = CreationContextData();
  if (!per_context_data)
    return false;

  // The context is responsible for keeping the prototype
  // alive. This in turn keeps callbacks alive through hidden
  // references; see CALLBACK_LIST(SET_HIDDEN_VALUE).
  per_context_data->AddCustomElementBinding(std::move(binding));
  return true;
}

void V8V0CustomElementLifecycleCallbacks::Created(Element* element) {
  // FIXME: callbacks while paused should be queued up for execution to
  // continue then be delivered in order rather than delivered immediately.
  // Bug 329665 tracks similar behavior for other synchronous events.
  if (!script_state_->ContextIsValid())
    return;

  element->SetV0CustomElementState(Element::kV0Upgraded);

  ScriptState::Scope scope(script_state_.get());
  v8::Isolate* isolate = script_state_->GetIsolate();
  v8::Local<v8::Context> context = script_state_->GetContext();
  v8::Local<v8::Value> receiver_value =
      ToV8(element, context->Global(), isolate);
  if (receiver_value.IsEmpty())
    return;
  v8::Local<v8::Object> receiver = receiver_value.As<v8::Object>();

  // Swizzle the prototype of the wrapper.
  v8::Local<v8::Object> prototype = prototype_.NewLocal(isolate);
  if (prototype.IsEmpty())
    return;
  if (!V8CallBoolean(receiver->SetPrototype(context, prototype)))
    return;

  v8::Local<v8::Function> callback = created_.NewLocal(isolate);
  if (callback.IsEmpty())
    return;

  v8::TryCatch exception_catcher(isolate);
  exception_catcher.SetVerbose(true);
  V8ScriptRunner::CallFunction(callback,
                               ExecutionContext::From(script_state_.get()),
                               receiver, 0, nullptr, isolate);
}

void V8V0CustomElementLifecycleCallbacks::Attached(Element* element) {
  Call(attached_, element);
}

void V8V0CustomElementLifecycleCallbacks::Detached(Element* element) {
  Call(detached_, element);
}

void V8V0CustomElementLifecycleCallbacks::AttributeChanged(
    Element* element,
    const AtomicString& name,
    const AtomicString& old_value,
    const AtomicString& new_value) {
  // FIXME: callbacks while paused should be queued up for execution to
  // continue then be delivered in order rather than delivered immediately.
  // Bug 329665 tracks similar behavior for other synchronous events.
  if (!script_state_->ContextIsValid())
    return;
  ScriptState::Scope scope(script_state_.get());
  v8::Isolate* isolate = script_state_->GetIsolate();
  v8::Local<v8::Context> context = script_state_->GetContext();
  v8::Local<v8::Value> receiver = ToV8(element, context->Global(), isolate);
  if (receiver.IsEmpty())
    return;

  v8::Local<v8::Function> callback = attribute_changed_.NewLocal(isolate);
  if (callback.IsEmpty())
    return;

  v8::Local<v8::Value> argv[] = {
      V8String(isolate, name),
      old_value.IsNull() ? v8::Local<v8::Value>(v8::Null(isolate))
                         : v8::Local<v8::Value>(V8String(isolate, old_value)),
      new_value.IsNull() ? v8::Local<v8::Value>(v8::Null(isolate))
                         : v8::Local<v8::Value>(V8String(isolate, new_value))};

  v8::TryCatch exception_catcher(isolate);
  exception_catcher.SetVerbose(true);
  V8ScriptRunner::CallFunction(callback,
                               ExecutionContext::From(script_state_.get()),
                               receiver, WTF_ARRAY_LENGTH(argv), argv, isolate);
}

void V8V0CustomElementLifecycleCallbacks::Call(
    const ScopedPersistent<v8::Function>& weak_callback,
    Element* element) {
  // FIXME: callbacks while paused should be queued up for execution to
  // continue then be delivered in order rather than delivered immediately.
  // Bug 329665 tracks similar behavior for other synchronous events.
  if (!script_state_->ContextIsValid())
    return;
  ScriptState::Scope scope(script_state_.get());
  v8::Isolate* isolate = script_state_->GetIsolate();
  v8::Local<v8::Context> context = script_state_->GetContext();
  v8::Local<v8::Function> callback = weak_callback.NewLocal(isolate);
  if (callback.IsEmpty())
    return;

  v8::Local<v8::Value> receiver = ToV8(element, context->Global(), isolate);
  if (receiver.IsEmpty())
    return;

  v8::TryCatch exception_catcher(isolate);
  exception_catcher.SetVerbose(true);
  V8ScriptRunner::CallFunction(callback,
                               ExecutionContext::From(script_state_.get()),
                               receiver, 0, nullptr, isolate);
}

void V8V0CustomElementLifecycleCallbacks::Trace(blink::Visitor* visitor) {
  V0CustomElementLifecycleCallbacks::Trace(visitor);
}

}  // namespace blink
