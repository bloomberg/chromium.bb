// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/WebKit/Source/bindings/templates/interface.cpp.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "V8SVGTestInterface.h"

#include "base/memory/scoped_refptr.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/IDLTypes.h"
#include "bindings/core/v8/NativeValueTraitsImpl.h"
#include "bindings/core/v8/V8DOMConfiguration.h"
#include "core/dom/ExecutionContext.h"
#include "core/html/custom/V0CustomElementProcessingStack.h"
#include "core/svg_names.h"
#include "platform/bindings/RuntimeCallStats.h"
#include "platform/bindings/V8ObjectConstructor.h"
#include "platform/wtf/GetPtr.h"

namespace blink {

// Suppress warning: global constructors, because struct WrapperTypeInfo is trivial
// and does not depend on another global objects.
#if defined(COMPONENT_BUILD) && defined(WIN32) && defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#endif
const WrapperTypeInfo V8SVGTestInterface::wrapperTypeInfo = {
    gin::kEmbedderBlink,
    V8SVGTestInterface::domTemplate,
    nullptr,
    "SVGTestInterface",
    nullptr,
    WrapperTypeInfo::kWrapperTypeObjectPrototype,
    WrapperTypeInfo::kObjectClassId,
    WrapperTypeInfo::kNotInheritFromActiveScriptWrappable,
};
#if defined(COMPONENT_BUILD) && defined(WIN32) && defined(__clang__)
#pragma clang diagnostic pop
#endif

// This static member must be declared by DEFINE_WRAPPERTYPEINFO in SVGTestInterface.h.
// For details, see the comment of DEFINE_WRAPPERTYPEINFO in
// platform/bindings/ScriptWrappable.h.
const WrapperTypeInfo& SVGTestInterface::wrapper_type_info_ = V8SVGTestInterface::wrapperTypeInfo;

// not [ActiveScriptWrappable]
static_assert(
    !std::is_base_of<ActiveScriptWrappableBase, SVGTestInterface>::value,
    "SVGTestInterface inherits from ActiveScriptWrappable<>, but is not specifying "
    "[ActiveScriptWrappable] extended attribute in the IDL file.  "
    "Be consistent.");
static_assert(
    std::is_same<decltype(&SVGTestInterface::HasPendingActivity),
                 decltype(&ScriptWrappable::HasPendingActivity)>::value,
    "SVGTestInterface is overriding hasPendingActivity(), but is not specifying "
    "[ActiveScriptWrappable] extended attribute in the IDL file.  "
    "Be consistent.");

namespace SVGTestInterfaceV8Internal {

static void typeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  SVGTestInterface* impl = V8SVGTestInterface::ToImpl(holder);

  V8SetReturnValueString(info, impl->FastGetAttribute(SVGNames::typeAttr), info.GetIsolate());
}

static void typeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  SVGTestInterface* impl = V8SVGTestInterface::ToImpl(holder);

  V0CustomElementProcessingStack::CallbackDeliveryScope deliveryScope;

  // Prepare the value to be set.
  V8StringResource<> cppValue = v8Value;
  if (!cppValue.Prepare())
    return;

  impl->setAttribute(SVGNames::typeAttr, cppValue);
}

} // namespace SVGTestInterfaceV8Internal

void V8SVGTestInterface::typeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_SVGTestInterface_type_Getter");

  SVGTestInterfaceV8Internal::typeAttributeGetter(info);
}

void V8SVGTestInterface::typeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_SVGTestInterface_type_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  SVGTestInterfaceV8Internal::typeAttributeSetter(v8Value, info);
}

static const V8DOMConfiguration::AccessorConfiguration V8SVGTestInterfaceAccessors[] = {
    { "type", V8SVGTestInterface::typeAttributeGetterCallback, V8SVGTestInterface::typeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kAllWorlds },
};

static void installV8SVGTestInterfaceTemplate(
    v8::Isolate* isolate,
    const DOMWrapperWorld& world,
    v8::Local<v8::FunctionTemplate> interfaceTemplate) {
  // Initialize the interface object's template.
  V8DOMConfiguration::InitializeDOMInterfaceTemplate(isolate, interfaceTemplate, V8SVGTestInterface::wrapperTypeInfo.interface_name, v8::Local<v8::FunctionTemplate>(), V8SVGTestInterface::internalFieldCount);

  v8::Local<v8::Signature> signature = v8::Signature::New(isolate, interfaceTemplate);
  ALLOW_UNUSED_LOCAL(signature);
  v8::Local<v8::ObjectTemplate> instanceTemplate = interfaceTemplate->InstanceTemplate();
  ALLOW_UNUSED_LOCAL(instanceTemplate);
  v8::Local<v8::ObjectTemplate> prototypeTemplate = interfaceTemplate->PrototypeTemplate();
  ALLOW_UNUSED_LOCAL(prototypeTemplate);

  // Register IDL constants, attributes and operations.
  V8DOMConfiguration::InstallAccessors(
      isolate, world, instanceTemplate, prototypeTemplate, interfaceTemplate,
      signature, V8SVGTestInterfaceAccessors, WTF_ARRAY_LENGTH(V8SVGTestInterfaceAccessors));

  // Custom signature

  V8SVGTestInterface::InstallRuntimeEnabledFeaturesOnTemplate(
      isolate, world, interfaceTemplate);
}

void V8SVGTestInterface::InstallRuntimeEnabledFeaturesOnTemplate(
    v8::Isolate* isolate,
    const DOMWrapperWorld& world,
    v8::Local<v8::FunctionTemplate> interface_template) {
  v8::Local<v8::Signature> signature = v8::Signature::New(isolate, interface_template);
  ALLOW_UNUSED_LOCAL(signature);
  v8::Local<v8::ObjectTemplate> instance_template = interface_template->InstanceTemplate();
  ALLOW_UNUSED_LOCAL(instance_template);
  v8::Local<v8::ObjectTemplate> prototype_template = interface_template->PrototypeTemplate();
  ALLOW_UNUSED_LOCAL(prototype_template);

  // Register IDL constants, attributes and operations.

  // Custom signature
}

v8::Local<v8::FunctionTemplate> V8SVGTestInterface::domTemplate(v8::Isolate* isolate, const DOMWrapperWorld& world) {
  return V8DOMConfiguration::DomClassTemplate(isolate, world, const_cast<WrapperTypeInfo*>(&wrapperTypeInfo), installV8SVGTestInterfaceTemplate);
}

bool V8SVGTestInterface::hasInstance(v8::Local<v8::Value> v8Value, v8::Isolate* isolate) {
  return V8PerIsolateData::From(isolate)->HasInstance(&wrapperTypeInfo, v8Value);
}

v8::Local<v8::Object> V8SVGTestInterface::findInstanceInPrototypeChain(v8::Local<v8::Value> v8Value, v8::Isolate* isolate) {
  return V8PerIsolateData::From(isolate)->FindInstanceInPrototypeChain(&wrapperTypeInfo, v8Value);
}

SVGTestInterface* V8SVGTestInterface::ToImplWithTypeCheck(v8::Isolate* isolate, v8::Local<v8::Value> value) {
  return hasInstance(value, isolate) ? ToImpl(v8::Local<v8::Object>::Cast(value)) : nullptr;
}

SVGTestInterface* NativeValueTraits<SVGTestInterface>::NativeValue(v8::Isolate* isolate, v8::Local<v8::Value> value, ExceptionState& exceptionState) {
  SVGTestInterface* nativeValue = V8SVGTestInterface::ToImplWithTypeCheck(isolate, value);
  if (!nativeValue) {
    exceptionState.ThrowTypeError(ExceptionMessages::FailedToConvertJSValue(
        "SVGTestInterface"));
  }
  return nativeValue;
}

}  // namespace blink
