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

#include "bindings/core/v8/V0CustomElementConstructorBuilder.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/StringOrDictionary.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "bindings/core/v8/V8Document.h"
#include "bindings/core/v8/V8HTMLElement.h"
#include "bindings/core/v8/V8SVGElement.h"
#include "core/HTMLNames.h"
#include "core/SVGNames.h"
#include "core/dom/Document.h"
#include "core/dom/ElementRegistrationOptions.h"
#include "core/dom/custom/V0CustomElementDefinition.h"
#include "core/dom/custom/V0CustomElementDescriptor.h"
#include "core/dom/custom/V0CustomElementException.h"
#include "core/dom/custom/V0CustomElementProcessingStack.h"
#include "core/frame/UseCounter.h"
#include "platform/bindings/DOMWrapperWorld.h"
#include "platform/bindings/V0CustomElementBinding.h"
#include "platform/bindings/V8PerContextData.h"
#include "platform/bindings/V8PrivateProperty.h"
#include "platform/wtf/Assertions.h"

namespace blink {

static void ConstructCustomElement(const v8::FunctionCallbackInfo<v8::Value>&);

V0CustomElementConstructorBuilder::V0CustomElementConstructorBuilder(
    ScriptState* script_state,
    const ElementRegistrationOptions& options)
    : script_state_(script_state), options_(options) {
  DCHECK(script_state_->GetContext() ==
         script_state_->GetIsolate()->GetCurrentContext());
}

bool V0CustomElementConstructorBuilder::IsFeatureAllowed() const {
  return script_state_->World().IsMainWorld();
}

bool V0CustomElementConstructorBuilder::ValidateOptions(
    const AtomicString& type,
    QualifiedName& tag_name,
    ExceptionState& exception_state) {
  DCHECK(prototype_.IsEmpty());

  v8::TryCatch try_catch(script_state_->GetIsolate());

  if (!script_state_->PerContextData()) {
    // FIXME: This should generate an InvalidContext exception at a later point.
    V0CustomElementException::ThrowException(
        V0CustomElementException::kContextDestroyedCheckingPrototype, type,
        exception_state);
    try_catch.ReThrow();
    return false;
  }

  if (options_.hasPrototype()) {
    DCHECK(options_.prototype().IsObject());
    prototype_ = options_.prototype().V8Value().As<v8::Object>();
  } else {
    prototype_ = v8::Object::New(script_state_->GetIsolate());
    v8::Local<v8::Object> base_prototype =
        script_state_->PerContextData()->PrototypeForType(
            &V8HTMLElement::wrapperTypeInfo);
    if (!base_prototype.IsEmpty()) {
      if (!V8CallBoolean(prototype_->SetPrototype(script_state_->GetContext(),
                                                  base_prototype)))
        return false;
    }
  }

  AtomicString namespace_uri = HTMLNames::xhtmlNamespaceURI;
  if (HasValidPrototypeChainFor(&V8SVGElement::wrapperTypeInfo))
    namespace_uri = SVGNames::svgNamespaceURI;

  DCHECK(!try_catch.HasCaught());

  AtomicString local_name;

  if (options_.hasExtends()) {
    local_name = AtomicString(options_.extends().DeprecatedLower());

    if (!Document::IsValidName(local_name)) {
      V0CustomElementException::ThrowException(
          V0CustomElementException::kExtendsIsInvalidName, type,
          exception_state);
      try_catch.ReThrow();
      return false;
    }
    if (V0CustomElement::IsValidName(local_name)) {
      V0CustomElementException::ThrowException(
          V0CustomElementException::kExtendsIsCustomElementName, type,
          exception_state);
      try_catch.ReThrow();
      return false;
    }
  } else {
    if (namespace_uri == SVGNames::svgNamespaceURI) {
      V0CustomElementException::ThrowException(
          V0CustomElementException::kExtendsIsInvalidName, type,
          exception_state);
      try_catch.ReThrow();
      return false;
    }
    local_name = type;
  }

  DCHECK(!try_catch.HasCaught());
  tag_name = QualifiedName(g_null_atom, local_name, namespace_uri);
  return true;
}

V0CustomElementLifecycleCallbacks*
V0CustomElementConstructorBuilder::CreateCallbacks() {
  DCHECK(!prototype_.IsEmpty());

  v8::TryCatch exception_catcher(script_state_->GetIsolate());
  exception_catcher.SetVerbose(true);

  v8::MaybeLocal<v8::Function> created = RetrieveCallback("createdCallback");
  v8::MaybeLocal<v8::Function> attached = RetrieveCallback("attachedCallback");
  v8::MaybeLocal<v8::Function> detached = RetrieveCallback("detachedCallback");
  v8::MaybeLocal<v8::Function> attribute_changed =
      RetrieveCallback("attributeChangedCallback");

  callbacks_ = V8V0CustomElementLifecycleCallbacks::Create(
      script_state_.Get(), prototype_, created, attached, detached,
      attribute_changed);
  return callbacks_.Get();
}

v8::MaybeLocal<v8::Function>
V0CustomElementConstructorBuilder::RetrieveCallback(const char* name) {
  v8::Local<v8::Value> value;
  if (!prototype_
           ->Get(script_state_->GetContext(),
                 V8String(script_state_->GetIsolate(), name))
           .ToLocal(&value) ||
      !value->IsFunction())
    return v8::MaybeLocal<v8::Function>();
  return v8::MaybeLocal<v8::Function>(value.As<v8::Function>());
}

bool V0CustomElementConstructorBuilder::CreateConstructor(
    Document* document,
    V0CustomElementDefinition* definition,
    ExceptionState& exception_state) {
  DCHECK(!prototype_.IsEmpty());
  DCHECK(constructor_.IsEmpty());
  DCHECK(document);

  v8::Isolate* isolate = script_state_->GetIsolate();
  v8::Local<v8::Context> context = script_state_->GetContext();

  if (!PrototypeIsValid(definition->Descriptor().GetType(), exception_state))
    return false;

  const V0CustomElementDescriptor& descriptor = definition->Descriptor();

  v8::Local<v8::String> v8_tag_name = V8String(isolate, descriptor.LocalName());
  v8::Local<v8::Value> v8_type;
  if (descriptor.IsTypeExtension())
    v8_type = V8String(isolate, descriptor.GetType());
  else
    v8_type = v8::Null(isolate);

  v8::Local<v8::Object> data = v8::Object::New(isolate);
  V8PrivateProperty::GetCustomElementDocument(isolate).Set(
      data, ToV8(document, context->Global(), isolate));
  V8PrivateProperty::GetCustomElementNamespaceURI(isolate).Set(
      data, V8String(isolate, descriptor.NamespaceURI()));
  V8PrivateProperty::GetCustomElementTagName(isolate).Set(data, v8_tag_name);
  V8PrivateProperty::GetCustomElementType(isolate).Set(data, v8_type);

  v8::Local<v8::FunctionTemplate> constructor_template =
      v8::FunctionTemplate::New(isolate);
  constructor_template->SetCallHandler(ConstructCustomElement, data);
  if (!constructor_template->GetFunction(context).ToLocal(&constructor_)) {
    V0CustomElementException::ThrowException(
        V0CustomElementException::kContextDestroyedRegisteringDefinition,
        definition->Descriptor().GetType(), exception_state);
    return false;
  }

  constructor_->SetName(v8_type->IsNull() ? v8_tag_name
                                          : v8_type.As<v8::String>());

  v8::Local<v8::String> prototype_key = V8String(isolate, "prototype");
  if (!V8CallBoolean(constructor_->HasOwnProperty(context, prototype_key)))
    return false;
  // This sets the property *value*; calling Set is safe because
  // "prototype" is a non-configurable data property so there can be
  // no side effects.
  if (!V8CallBoolean(constructor_->Set(context, prototype_key, prototype_)))
    return false;
  // This *configures* the property. DefineOwnProperty of a function's
  // "prototype" does not affect the value, but can reconfigure the
  // property.
  if (!V8CallBoolean(constructor_->DefineOwnProperty(
          context, prototype_key, prototype_,
          v8::PropertyAttribute(v8::ReadOnly | v8::DontEnum | v8::DontDelete))))
    return false;

  v8::Local<v8::String> constructor_key = V8String(isolate, "constructor");
  v8::Local<v8::Value> constructor_prototype;
  if (!prototype_->Get(context, constructor_key)
           .ToLocal(&constructor_prototype))
    return false;

  if (!V8CallBoolean(
          constructor_->SetPrototype(context, constructor_prototype)))
    return false;

  V8PrivateProperty::GetCustomElementIsInterfacePrototypeObject(isolate).Set(
      prototype_, v8::True(isolate));
  if (!V8CallBoolean(prototype_->DefineOwnProperty(
          context, V8String(isolate, "constructor"), constructor_,
          v8::DontEnum)))
    return false;

  return true;
}

bool V0CustomElementConstructorBuilder::PrototypeIsValid(
    const AtomicString& type,
    ExceptionState& exception_state) const {
  v8::Isolate* isolate = script_state_->GetIsolate();
  v8::Local<v8::Context> context = script_state_->GetContext();

  if (prototype_->InternalFieldCount() ||
      V8PrivateProperty::GetCustomElementIsInterfacePrototypeObject(isolate)
          .HasValue(prototype_)) {
    V0CustomElementException::ThrowException(
        V0CustomElementException::kPrototypeInUse, type, exception_state);
    return false;
  }

  v8::PropertyAttribute property_attribute;
  if (!prototype_
           ->GetPropertyAttributes(context, V8String(isolate, "constructor"))
           .To(&property_attribute) ||
      (property_attribute & v8::DontDelete)) {
    V0CustomElementException::ThrowException(
        V0CustomElementException::kConstructorPropertyNotConfigurable, type,
        exception_state);
    return false;
  }

  return true;
}

bool V0CustomElementConstructorBuilder::DidRegisterDefinition() const {
  DCHECK(!constructor_.IsEmpty());

  return callbacks_->SetBinding(
      V0CustomElementBinding::Create(script_state_->GetIsolate(), prototype_));
}

ScriptValue V0CustomElementConstructorBuilder::BindingsReturnValue() const {
  return ScriptValue(script_state_.Get(), constructor_);
}

bool V0CustomElementConstructorBuilder::HasValidPrototypeChainFor(
    const WrapperTypeInfo* type) const {
  v8::Local<v8::Object> element_prototype =
      script_state_->PerContextData()->PrototypeForType(type);
  if (element_prototype.IsEmpty())
    return false;

  v8::Local<v8::Value> chain = prototype_;
  while (!chain.IsEmpty() && chain->IsObject()) {
    if (chain == element_prototype)
      return true;
    chain = chain.As<v8::Object>()->GetPrototype();
  }

  return false;
}

static void ConstructCustomElement(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();

  if (!info.IsConstructCall()) {
    V8ThrowException::ThrowTypeError(
        isolate, "DOM object constructor cannot be called as a function.");
    return;
  }

  if (info.Length() > 0) {
    V8ThrowException::ThrowTypeError(
        isolate, "This constructor should be called without arguments.");
    return;
  }

  v8::Local<v8::Object> data = v8::Local<v8::Object>::Cast(info.Data());
  Document* document =
      V8Document::toImpl(V8PrivateProperty::GetCustomElementDocument(isolate)
                             .GetOrEmpty(data)
                             .As<v8::Object>());
  TOSTRING_VOID(
      V8StringResource<>, namespace_uri,
      V8PrivateProperty::GetCustomElementNamespaceURI(isolate).GetOrEmpty(
          data));
  TOSTRING_VOID(
      V8StringResource<>, tag_name,
      V8PrivateProperty::GetCustomElementTagName(isolate).GetOrEmpty(data));
  v8::Local<v8::Value> maybe_type =
      V8PrivateProperty::GetCustomElementType(isolate).GetOrEmpty(data);
  TOSTRING_VOID(V8StringResource<>, type, maybe_type);

  ExceptionState exception_state(isolate, ExceptionState::kConstructionContext,
                                 "CustomElement");
  V0CustomElementProcessingStack::CallbackDeliveryScope delivery_scope;
  Element* element = document->createElementNS(
      nullptr, namespace_uri, tag_name,
      StringOrDictionary::fromString(maybe_type->IsNull() ? g_null_atom : type),
      exception_state);
  if (element) {
    UseCounter::Count(document, UseCounter::kV0CustomElementsConstruct);
  }
  V8SetReturnValueFast(info, element, document);
}

}  // namespace blink
