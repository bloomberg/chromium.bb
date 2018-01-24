// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/WebKit/Source/bindings/templates/interface.cpp.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "V8TestInterfaceOriginTrialEnabled.h"

#include "base/memory/scoped_refptr.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/IDLTypes.h"
#include "bindings/core/v8/NativeValueTraitsImpl.h"
#include "bindings/core/v8/V8DOMConfiguration.h"
#include "core/dom/ExecutionContext.h"
#include "platform/bindings/RuntimeCallStats.h"
#include "platform/bindings/V8ObjectConstructor.h"
#include "platform/runtime_enabled_features.h"
#include "platform/wtf/GetPtr.h"

namespace blink {

// Suppress warning: global constructors, because struct WrapperTypeInfo is trivial
// and does not depend on another global objects.
#if defined(COMPONENT_BUILD) && defined(WIN32) && defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#endif
const WrapperTypeInfo V8TestInterfaceOriginTrialEnabled::wrapperTypeInfo = {
    gin::kEmbedderBlink,
    V8TestInterfaceOriginTrialEnabled::domTemplate,
    nullptr,
    "TestInterfaceOriginTrialEnabled",
    nullptr,
    WrapperTypeInfo::kWrapperTypeObjectPrototype,
    WrapperTypeInfo::kObjectClassId,
    WrapperTypeInfo::kNotInheritFromActiveScriptWrappable,
};
#if defined(COMPONENT_BUILD) && defined(WIN32) && defined(__clang__)
#pragma clang diagnostic pop
#endif

// This static member must be declared by DEFINE_WRAPPERTYPEINFO in TestInterfaceOriginTrialEnabled.h.
// For details, see the comment of DEFINE_WRAPPERTYPEINFO in
// platform/bindings/ScriptWrappable.h.
const WrapperTypeInfo& TestInterfaceOriginTrialEnabled::wrapper_type_info_ = V8TestInterfaceOriginTrialEnabled::wrapperTypeInfo;

// not [ActiveScriptWrappable]
static_assert(
    !std::is_base_of<ActiveScriptWrappableBase, TestInterfaceOriginTrialEnabled>::value,
    "TestInterfaceOriginTrialEnabled inherits from ActiveScriptWrappable<>, but is not specifying "
    "[ActiveScriptWrappable] extended attribute in the IDL file.  "
    "Be consistent.");
static_assert(
    std::is_same<decltype(&TestInterfaceOriginTrialEnabled::HasPendingActivity),
                 decltype(&ScriptWrappable::HasPendingActivity)>::value,
    "TestInterfaceOriginTrialEnabled is overriding hasPendingActivity(), but is not specifying "
    "[ActiveScriptWrappable] extended attribute in the IDL file.  "
    "Be consistent.");

namespace TestInterfaceOriginTrialEnabledV8Internal {

static void doubleAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceOriginTrialEnabled* impl = V8TestInterfaceOriginTrialEnabled::ToImpl(holder);

  V8SetReturnValue(info, impl->doubleAttribute());
}

static void doubleAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceOriginTrialEnabled* impl = V8TestInterfaceOriginTrialEnabled::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestInterfaceOriginTrialEnabled", "doubleAttribute");

  // Prepare the value to be set.
  double cppValue = NativeValueTraits<IDLDouble>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setDoubleAttribute(cppValue);
}

static void conditionalLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceOriginTrialEnabled* impl = V8TestInterfaceOriginTrialEnabled::ToImpl(holder);

  V8SetReturnValueInt(info, impl->conditionalLongAttribute());
}

static void conditionalLongAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceOriginTrialEnabled* impl = V8TestInterfaceOriginTrialEnabled::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestInterfaceOriginTrialEnabled", "conditionalLongAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState, kNormalConversion);
  if (exceptionState.HadException())
    return;

  impl->setConditionalLongAttribute(cppValue);
}

static void conditionalReadOnlyLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceOriginTrialEnabled* impl = V8TestInterfaceOriginTrialEnabled::ToImpl(holder);

  V8SetReturnValueInt(info, impl->conditionalReadOnlyLongAttribute());
}

static void staticStringAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  V8SetReturnValueString(info, TestInterfaceOriginTrialEnabled::staticStringAttribute(), info.GetIsolate());
}

static void staticStringAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  // Prepare the value to be set.
  V8StringResource<> cppValue = v8Value;
  if (!cppValue.Prepare())
    return;

  TestInterfaceOriginTrialEnabled::setStaticStringAttribute(cppValue);
}

static void staticConditionalReadOnlyLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  V8SetReturnValueInt(info, TestInterfaceOriginTrialEnabled::staticConditionalReadOnlyLongAttribute());
}

static void voidMethodDoubleArgFloatArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterfaceOriginTrialEnabled", "voidMethodDoubleArgFloatArg");

  TestInterfaceOriginTrialEnabled* impl = V8TestInterfaceOriginTrialEnabled::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 2)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(2, info.Length()));
    return;
  }

  double doubleArg;
  float floatArg;
  doubleArg = NativeValueTraits<IDLDouble>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  floatArg = NativeValueTraits<IDLFloat>::NativeValue(info.GetIsolate(), info[1], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodDoubleArgFloatArg(doubleArg, floatArg);
}

static void voidMethodPartialOverload1Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceOriginTrialEnabled* impl = V8TestInterfaceOriginTrialEnabled::ToImpl(info.Holder());

  impl->voidMethodPartialOverload();
}

static void voidMethodPartialOverload2Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterfaceOriginTrialEnabled", "voidMethodPartialOverload");

  TestInterfaceOriginTrialEnabled* impl = V8TestInterfaceOriginTrialEnabled::ToImpl(info.Holder());

  double doubleArg;
  doubleArg = NativeValueTraits<IDLDouble>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodPartialOverload(doubleArg);
}

static void voidMethodPartialOverloadMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  bool isArityError = false;
  switch (std::min(1, info.Length())) {
    case 0:
      if (true) {
        voidMethodPartialOverload1Method(info);
        return;
      }
      break;
    case 1:
      if (true) {
        voidMethodPartialOverload2Method(info);
        return;
      }
      break;
    default:
      isArityError = true;
  }

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterfaceOriginTrialEnabled", "voidMethodPartialOverload");

  if (isArityError) {
  }
  exceptionState.ThrowTypeError("No function was found that matched the signature provided.");
}

} // namespace TestInterfaceOriginTrialEnabledV8Internal

void V8TestInterfaceOriginTrialEnabled::doubleAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceOriginTrialEnabled_doubleAttribute_Getter");

  TestInterfaceOriginTrialEnabledV8Internal::doubleAttributeAttributeGetter(info);
}

void V8TestInterfaceOriginTrialEnabled::doubleAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceOriginTrialEnabled_doubleAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  TestInterfaceOriginTrialEnabledV8Internal::doubleAttributeAttributeSetter(v8Value, info);
}

void V8TestInterfaceOriginTrialEnabled::conditionalLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceOriginTrialEnabled_conditionalLongAttribute_Getter");

  TestInterfaceOriginTrialEnabledV8Internal::conditionalLongAttributeAttributeGetter(info);
}

void V8TestInterfaceOriginTrialEnabled::conditionalLongAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceOriginTrialEnabled_conditionalLongAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  TestInterfaceOriginTrialEnabledV8Internal::conditionalLongAttributeAttributeSetter(v8Value, info);
}

void V8TestInterfaceOriginTrialEnabled::conditionalReadOnlyLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceOriginTrialEnabled_conditionalReadOnlyLongAttribute_Getter");

  TestInterfaceOriginTrialEnabledV8Internal::conditionalReadOnlyLongAttributeAttributeGetter(info);
}

void V8TestInterfaceOriginTrialEnabled::staticStringAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceOriginTrialEnabled_staticStringAttribute_Getter");

  TestInterfaceOriginTrialEnabledV8Internal::staticStringAttributeAttributeGetter(info);
}

void V8TestInterfaceOriginTrialEnabled::staticStringAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceOriginTrialEnabled_staticStringAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  TestInterfaceOriginTrialEnabledV8Internal::staticStringAttributeAttributeSetter(v8Value, info);
}

void V8TestInterfaceOriginTrialEnabled::staticConditionalReadOnlyLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceOriginTrialEnabled_staticConditionalReadOnlyLongAttribute_Getter");

  TestInterfaceOriginTrialEnabledV8Internal::staticConditionalReadOnlyLongAttributeAttributeGetter(info);
}

void V8TestInterfaceOriginTrialEnabled::voidMethodDoubleArgFloatArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceOriginTrialEnabled_voidMethodDoubleArgFloatArg");

  TestInterfaceOriginTrialEnabledV8Internal::voidMethodDoubleArgFloatArgMethod(info);
}

void V8TestInterfaceOriginTrialEnabled::voidMethodPartialOverloadMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceOriginTrialEnabled_voidMethodPartialOverload");

  TestInterfaceOriginTrialEnabledV8Internal::voidMethodPartialOverloadMethod(info);
}

static const V8DOMConfiguration::AccessorConfiguration V8TestInterfaceOriginTrialEnabledAccessors[] = {
    { "doubleAttribute", V8TestInterfaceOriginTrialEnabled::doubleAttributeAttributeGetterCallback, V8TestInterfaceOriginTrialEnabled::doubleAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kAllWorlds },

    { "staticStringAttribute", V8TestInterfaceOriginTrialEnabled::staticStringAttributeAttributeGetterCallback, V8TestInterfaceOriginTrialEnabled::staticStringAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnInterface, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kAllWorlds },
};

static const V8DOMConfiguration::MethodConfiguration V8TestInterfaceOriginTrialEnabledMethods[] = {
    {"voidMethodDoubleArgFloatArg", V8TestInterfaceOriginTrialEnabled::voidMethodDoubleArgFloatArgMethodCallback, 2, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kAllWorlds},
    {"voidMethodPartialOverload", V8TestInterfaceOriginTrialEnabled::voidMethodPartialOverloadMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kAllWorlds},
};

static void installV8TestInterfaceOriginTrialEnabledTemplate(
    v8::Isolate* isolate,
    const DOMWrapperWorld& world,
    v8::Local<v8::FunctionTemplate> interfaceTemplate) {
  // Initialize the interface object's template.
  V8DOMConfiguration::InitializeDOMInterfaceTemplate(isolate, interfaceTemplate, V8TestInterfaceOriginTrialEnabled::wrapperTypeInfo.interface_name, v8::Local<v8::FunctionTemplate>(), V8TestInterfaceOriginTrialEnabled::internalFieldCount);

  v8::Local<v8::Signature> signature = v8::Signature::New(isolate, interfaceTemplate);
  ALLOW_UNUSED_LOCAL(signature);
  v8::Local<v8::ObjectTemplate> instanceTemplate = interfaceTemplate->InstanceTemplate();
  ALLOW_UNUSED_LOCAL(instanceTemplate);
  v8::Local<v8::ObjectTemplate> prototypeTemplate = interfaceTemplate->PrototypeTemplate();
  ALLOW_UNUSED_LOCAL(prototypeTemplate);

  // Register IDL constants, attributes and operations.
  static constexpr V8DOMConfiguration::ConstantConfiguration V8TestInterfaceOriginTrialEnabledConstants[] = {
      {"UNSIGNED_LONG", V8DOMConfiguration::kConstantTypeUnsignedLong, static_cast<int>(0)},
      {"CONST_JAVASCRIPT", V8DOMConfiguration::kConstantTypeShort, static_cast<int>(1)},
  };
  V8DOMConfiguration::InstallConstants(
      isolate, interfaceTemplate, prototypeTemplate,
      V8TestInterfaceOriginTrialEnabledConstants, WTF_ARRAY_LENGTH(V8TestInterfaceOriginTrialEnabledConstants));
  static_assert(0 == TestInterfaceOriginTrialEnabled::kUnsignedLong, "the value of TestInterfaceOriginTrialEnabled_kUnsignedLong does not match with implementation");
  static_assert(1 == TestInterfaceOriginTrialEnabled::kConstJavascript, "the value of TestInterfaceOriginTrialEnabled_kConstJavascript does not match with implementation");
  V8DOMConfiguration::InstallAccessors(
      isolate, world, instanceTemplate, prototypeTemplate, interfaceTemplate,
      signature, V8TestInterfaceOriginTrialEnabledAccessors, WTF_ARRAY_LENGTH(V8TestInterfaceOriginTrialEnabledAccessors));
  V8DOMConfiguration::InstallMethods(
      isolate, world, instanceTemplate, prototypeTemplate, interfaceTemplate,
      signature, V8TestInterfaceOriginTrialEnabledMethods, WTF_ARRAY_LENGTH(V8TestInterfaceOriginTrialEnabledMethods));

  // Custom signature

  V8TestInterfaceOriginTrialEnabled::InstallRuntimeEnabledFeaturesOnTemplate(
      isolate, world, interfaceTemplate);
}

void V8TestInterfaceOriginTrialEnabled::InstallRuntimeEnabledFeaturesOnTemplate(
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

  if (RuntimeEnabledFeatures::FeatureNameEnabled()) {
    static const V8DOMConfiguration::AccessorConfiguration accessor_configurations[] = {
        { "conditionalReadOnlyLongAttribute", V8TestInterfaceOriginTrialEnabled::conditionalReadOnlyLongAttributeAttributeGetterCallback, nullptr, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::ReadOnly), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kAllWorlds },

        { "staticConditionalReadOnlyLongAttribute", V8TestInterfaceOriginTrialEnabled::staticConditionalReadOnlyLongAttributeAttributeGetterCallback, nullptr, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::ReadOnly), V8DOMConfiguration::kOnInterface, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kAllWorlds },

        { "conditionalLongAttribute", V8TestInterfaceOriginTrialEnabled::conditionalLongAttributeAttributeGetterCallback, V8TestInterfaceOriginTrialEnabled::conditionalLongAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kAllWorlds },
    };
    V8DOMConfiguration::InstallAccessors(
        isolate, world, instance_template, prototype_template, interface_template,
        signature, accessor_configurations,
        WTF_ARRAY_LENGTH(accessor_configurations));
  }

  // Custom signature
}

v8::Local<v8::FunctionTemplate> V8TestInterfaceOriginTrialEnabled::domTemplate(v8::Isolate* isolate, const DOMWrapperWorld& world) {
  return V8DOMConfiguration::DomClassTemplate(isolate, world, const_cast<WrapperTypeInfo*>(&wrapperTypeInfo), installV8TestInterfaceOriginTrialEnabledTemplate);
}

bool V8TestInterfaceOriginTrialEnabled::hasInstance(v8::Local<v8::Value> v8Value, v8::Isolate* isolate) {
  return V8PerIsolateData::From(isolate)->HasInstance(&wrapperTypeInfo, v8Value);
}

v8::Local<v8::Object> V8TestInterfaceOriginTrialEnabled::findInstanceInPrototypeChain(v8::Local<v8::Value> v8Value, v8::Isolate* isolate) {
  return V8PerIsolateData::From(isolate)->FindInstanceInPrototypeChain(&wrapperTypeInfo, v8Value);
}

TestInterfaceOriginTrialEnabled* V8TestInterfaceOriginTrialEnabled::ToImplWithTypeCheck(v8::Isolate* isolate, v8::Local<v8::Value> value) {
  return hasInstance(value, isolate) ? ToImpl(v8::Local<v8::Object>::Cast(value)) : nullptr;
}

TestInterfaceOriginTrialEnabled* NativeValueTraits<TestInterfaceOriginTrialEnabled>::NativeValue(v8::Isolate* isolate, v8::Local<v8::Value> value, ExceptionState& exceptionState) {
  TestInterfaceOriginTrialEnabled* nativeValue = V8TestInterfaceOriginTrialEnabled::ToImplWithTypeCheck(isolate, value);
  if (!nativeValue) {
    exceptionState.ThrowTypeError(ExceptionMessages::FailedToConvertJSValue(
        "TestInterfaceOriginTrialEnabled"));
  }
  return nativeValue;
}

}  // namespace blink
