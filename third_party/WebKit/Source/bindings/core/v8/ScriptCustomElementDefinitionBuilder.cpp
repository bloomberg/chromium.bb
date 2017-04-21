// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/ScriptCustomElementDefinitionBuilder.h"

#include "bindings/core/v8/DOMWrapperWorld.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/IDLTypes.h"
#include "bindings/core/v8/NativeValueTraitsImpl.h"
#include "bindings/core/v8/ScriptCustomElementDefinition.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8BindingMacros.h"
#include "core/dom/ExceptionCode.h"

namespace blink {

ScriptCustomElementDefinitionBuilder*
    ScriptCustomElementDefinitionBuilder::stack_ = nullptr;

ScriptCustomElementDefinitionBuilder::ScriptCustomElementDefinitionBuilder(
    ScriptState* script_state,
    CustomElementRegistry* registry,
    const ScriptValue& constructor,
    ExceptionState& exception_state)
    : prev_(stack_),
      script_state_(script_state),
      registry_(registry),
      constructor_value_(constructor.V8Value()),
      exception_state_(exception_state) {
  stack_ = this;
}

ScriptCustomElementDefinitionBuilder::~ScriptCustomElementDefinitionBuilder() {
  stack_ = prev_;
}

bool ScriptCustomElementDefinitionBuilder::CheckConstructorIntrinsics() {
  DCHECK(script_state_->World().IsMainWorld());

  // The signature of CustomElementRegistry.define says this is a
  // Function
  // https://html.spec.whatwg.org/multipage/scripting.html#customelementsregistry
  CHECK(constructor_value_->IsFunction());
  constructor_ = constructor_value_.As<v8::Object>();
  if (!constructor_->IsConstructor()) {
    exception_state_.ThrowTypeError(
        "constructor argument is not a constructor");
    return false;
  }
  return true;
}

bool ScriptCustomElementDefinitionBuilder::CheckConstructorNotRegistered() {
  if (ScriptCustomElementDefinition::ForConstructor(script_state_.Get(),
                                                    registry_, constructor_)) {
    // Constructor is already registered.
    exception_state_.ThrowDOMException(
        kNotSupportedError,
        "this constructor has already been used with this registry");
    return false;
  }
  for (auto builder = prev_; builder; builder = builder->prev_) {
    CHECK(!builder->constructor_.IsEmpty());
    if (registry_ != builder->registry_ ||
        constructor_ != builder->constructor_) {
      continue;
    }
    exception_state_.ThrowDOMException(
        kNotSupportedError,
        "this constructor is already being defined in this registry");
    return false;
  }
  return true;
}

bool ScriptCustomElementDefinitionBuilder::ValueForName(
    const v8::Local<v8::Object>& object,
    const StringView& name,
    v8::Local<v8::Value>& value) const {
  v8::Isolate* isolate = script_state_->GetIsolate();
  v8::Local<v8::Context> context = script_state_->GetContext();
  v8::Local<v8::String> name_string = V8AtomicString(isolate, name);
  v8::TryCatch try_catch(isolate);
  if (!V8Call(object->Get(context, name_string), value, try_catch)) {
    exception_state_.RethrowV8Exception(try_catch.Exception());
    return false;
  }
  return true;
}

bool ScriptCustomElementDefinitionBuilder::CheckPrototype() {
  v8::Local<v8::Value> prototype_value;
  if (!ValueForName(constructor_, "prototype", prototype_value))
    return false;
  if (!prototype_value->IsObject()) {
    exception_state_.ThrowTypeError("constructor prototype is not an object");
    return false;
  }
  prototype_ = prototype_value.As<v8::Object>();
  // If retrieving the prototype destroyed the context, indicate that
  // defining the element should not proceed.
  return true;
}

bool ScriptCustomElementDefinitionBuilder::CallableForName(
    const StringView& name,
    v8::Local<v8::Function>& callback) const {
  v8::Local<v8::Value> value;
  if (!ValueForName(prototype_, name, value))
    return false;
  // "undefined" means "omitted", so return true.
  if (value->IsUndefined())
    return true;
  if (!value->IsFunction()) {
    exception_state_.ThrowTypeError(String::Format(
        "\"%s\" is not a callable object", name.ToString().Ascii().data()));
    return false;
  }
  callback = value.As<v8::Function>();
  return true;
}

bool ScriptCustomElementDefinitionBuilder::RetrieveObservedAttributes() {
  v8::Local<v8::Value> observed_attributes_value;
  if (!ValueForName(constructor_, "observedAttributes",
                    observed_attributes_value))
    return false;
  if (observed_attributes_value->IsUndefined())
    return true;
  Vector<String> list = NativeValueTraits<IDLSequence<IDLString>>::NativeValue(
      script_state_->GetIsolate(), observed_attributes_value, exception_state_);
  if (exception_state_.HadException())
    return false;
  if (list.IsEmpty())
    return true;
  observed_attributes_.ReserveCapacityForSize(list.size());
  for (const auto& attribute : list)
    observed_attributes_.insert(AtomicString(attribute));
  return true;
}

bool ScriptCustomElementDefinitionBuilder::RememberOriginalProperties() {
  // Spec requires to use values of these properties at the point
  // CustomElementDefinition is built, even if JS changes them afterwards.
  return CallableForName("connectedCallback", connected_callback_) &&
         CallableForName("disconnectedCallback", disconnected_callback_) &&
         CallableForName("adoptedCallback", adopted_callback_) &&
         CallableForName("attributeChangedCallback",
                         attribute_changed_callback_) &&
         (attribute_changed_callback_.IsEmpty() ||
          RetrieveObservedAttributes());
}

CustomElementDefinition* ScriptCustomElementDefinitionBuilder::Build(
    const CustomElementDescriptor& descriptor) {
  return ScriptCustomElementDefinition::Create(
      script_state_.Get(), registry_, descriptor, constructor_,
      connected_callback_, disconnected_callback_, adopted_callback_,
      attribute_changed_callback_, observed_attributes_);
}

}  // namespace blink
