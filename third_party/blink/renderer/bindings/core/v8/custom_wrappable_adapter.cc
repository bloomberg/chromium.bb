// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/custom_wrappable_adapter.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_dom_configuration.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_dom_wrapper.h"
#include "third_party/blink/renderer/platform/bindings/wrapper_type_info.h"

namespace blink {

namespace {

v8::Local<v8::FunctionTemplate> CreateCustomWrappableTemplate(
    v8::Isolate* isolate,
    const DOMWrapperWorld& world);

const WrapperTypeInfo custom_wrappable_info = {
    gin::kEmbedderBlink,
    CreateCustomWrappableTemplate,
    nullptr,
    "CustomWrappableAdapter",
    nullptr,
    WrapperTypeInfo::kWrapperTypeNoPrototype,
    WrapperTypeInfo::kCustomWrappableId,
    WrapperTypeInfo::kNotInheritFromActiveScriptWrappable,
};

void InstallCustomWrappableTemplate(
    v8::Isolate* isolate,
    const DOMWrapperWorld& world,
    v8::Local<v8::FunctionTemplate> interfaceTemplate) {
  V8DOMConfiguration::InitializeDOMInterfaceTemplate(
      isolate, interfaceTemplate, custom_wrappable_info.interface_name,
      v8::Local<v8::FunctionTemplate>(), kV8DefaultWrapperInternalFieldCount);
}

v8::Local<v8::FunctionTemplate> CreateCustomWrappableTemplate(
    v8::Isolate* isolate,
    const DOMWrapperWorld& world) {
  return V8DOMConfiguration::DomClassTemplate(
      isolate, world, const_cast<WrapperTypeInfo*>(&custom_wrappable_info),
      InstallCustomWrappableTemplate);
}

}  // namespace

CustomWrappableAdapter* CustomWrappableAdapter::LookupInternal(
    v8::Local<v8::Object> object,
    const V8PrivateProperty::Symbol& property) {
  v8::Local<v8::Value> custom_wrappable_adapter_value;
  if (!property.GetOrUndefined(object).ToLocal(&custom_wrappable_adapter_value))
    return nullptr;

  if (!custom_wrappable_adapter_value->IsUndefined()) {
    return static_cast<CustomWrappableAdapter*>(
        ToCustomWrappable(custom_wrappable_adapter_value.As<v8::Object>()));
  }
  return nullptr;
}

void CustomWrappableAdapter::Attach(ScriptState* script_state,
                                    v8::Local<v8::Object> object,
                                    const V8PrivateProperty::Symbol& property) {
  v8::Local<v8::Object> wrapper_object =
      CreateAndInitializeWrapper(script_state);
  property.Set(object, wrapper_object);
}

v8::Local<v8::Object> CustomWrappableAdapter::CreateAndInitializeWrapper(
    ScriptState* script_state) {
  DCHECK(wrapper_.IsEmpty());
  v8::Isolate* isolate = script_state->GetIsolate();
  v8::Local<v8::Object> wrapper_object = V8DOMWrapper::CreateWrapper(
      isolate, script_state->GetContext()->Global(), &custom_wrappable_info);
  V8DOMWrapper::AssociateObjectWithWrapper(
      isolate, this, &custom_wrappable_info, wrapper_object);
  wrapper_.Set(isolate, wrapper_object);
  custom_wrappable_info.ConfigureWrapper(&wrapper_.Get());
  return wrapper_object;
}

}  // namespace blink
