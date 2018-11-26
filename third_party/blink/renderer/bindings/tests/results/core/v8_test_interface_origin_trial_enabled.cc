// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/interface.cc.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "third_party/blink/renderer/bindings/tests/results/core/v8_test_interface_origin_trial_enabled.h"

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_configuration.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/runtime_call_stats.h"
#include "third_party/blink/renderer/platform/bindings/v8_object_constructor.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/get_ptr.h"

namespace blink {

// Suppress warning: global constructors, because struct WrapperTypeInfo is trivial
// and does not depend on another global objects.
#if defined(COMPONENT_BUILD) && defined(WIN32) && defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#endif
const WrapperTypeInfo V8TestInterfaceOriginTrialEnabled::wrapperTypeInfo = {
    gin::kEmbedderBlink,
    V8TestInterfaceOriginTrialEnabled::DomTemplate,
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

namespace test_interface_origin_trial_enabled_v8_internal {

static void DoubleAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceOriginTrialEnabled* impl = V8TestInterfaceOriginTrialEnabled::ToImpl(holder);

  V8SetReturnValue(info, impl->doubleAttribute());
}

static void DoubleAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
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

static void ConditionalLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceOriginTrialEnabled* impl = V8TestInterfaceOriginTrialEnabled::ToImpl(holder);

  V8SetReturnValueInt(info, impl->conditionalLongAttribute());
}

static void ConditionalLongAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceOriginTrialEnabled* impl = V8TestInterfaceOriginTrialEnabled::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestInterfaceOriginTrialEnabled", "conditionalLongAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setConditionalLongAttribute(cppValue);
}

static void ConditionalReadOnlyLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceOriginTrialEnabled* impl = V8TestInterfaceOriginTrialEnabled::ToImpl(holder);

  V8SetReturnValueInt(info, impl->conditionalReadOnlyLongAttribute());
}

static void StaticStringAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  V8SetReturnValueString(info, TestInterfaceOriginTrialEnabled::staticStringAttribute(), info.GetIsolate());
}

static void StaticStringAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
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

static void StaticConditionalReadOnlyLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  V8SetReturnValueInt(info, TestInterfaceOriginTrialEnabled::staticConditionalReadOnlyLongAttribute());
}

static void VoidMethodDoubleArgFloatArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
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

static void VoidMethodPartialOverload1Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceOriginTrialEnabled* impl = V8TestInterfaceOriginTrialEnabled::ToImpl(info.Holder());

  impl->voidMethodPartialOverload();
}

static void VoidMethodPartialOverload2Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterfaceOriginTrialEnabled", "voidMethodPartialOverload");

  TestInterfaceOriginTrialEnabled* impl = V8TestInterfaceOriginTrialEnabled::ToImpl(info.Holder());

  double doubleArg;
  doubleArg = NativeValueTraits<IDLDouble>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodPartialOverload(doubleArg);
}

static void VoidMethodPartialOverloadMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  bool isArityError = false;

  switch (std::min(1, info.Length())) {
    case 0:
      if (true) {
        VoidMethodPartialOverload1Method(info);
        return;
      }
      break;
    case 1:
      if (true) {
        VoidMethodPartialOverload2Method(info);
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

}  // namespace test_interface_origin_trial_enabled_v8_internal

void V8TestInterfaceOriginTrialEnabled::DoubleAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceOriginTrialEnabled_doubleAttribute_Getter");

  test_interface_origin_trial_enabled_v8_internal::DoubleAttributeAttributeGetter(info);
}

void V8TestInterfaceOriginTrialEnabled::DoubleAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceOriginTrialEnabled_doubleAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_interface_origin_trial_enabled_v8_internal::DoubleAttributeAttributeSetter(v8Value, info);
}

void V8TestInterfaceOriginTrialEnabled::ConditionalLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceOriginTrialEnabled_conditionalLongAttribute_Getter");

  test_interface_origin_trial_enabled_v8_internal::ConditionalLongAttributeAttributeGetter(info);
}

void V8TestInterfaceOriginTrialEnabled::ConditionalLongAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceOriginTrialEnabled_conditionalLongAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_interface_origin_trial_enabled_v8_internal::ConditionalLongAttributeAttributeSetter(v8Value, info);
}

void V8TestInterfaceOriginTrialEnabled::ConditionalReadOnlyLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceOriginTrialEnabled_conditionalReadOnlyLongAttribute_Getter");

  test_interface_origin_trial_enabled_v8_internal::ConditionalReadOnlyLongAttributeAttributeGetter(info);
}

void V8TestInterfaceOriginTrialEnabled::StaticStringAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceOriginTrialEnabled_staticStringAttribute_Getter");

  test_interface_origin_trial_enabled_v8_internal::StaticStringAttributeAttributeGetter(info);
}

void V8TestInterfaceOriginTrialEnabled::StaticStringAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceOriginTrialEnabled_staticStringAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_interface_origin_trial_enabled_v8_internal::StaticStringAttributeAttributeSetter(v8Value, info);
}

void V8TestInterfaceOriginTrialEnabled::StaticConditionalReadOnlyLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceOriginTrialEnabled_staticConditionalReadOnlyLongAttribute_Getter");

  test_interface_origin_trial_enabled_v8_internal::StaticConditionalReadOnlyLongAttributeAttributeGetter(info);
}

void V8TestInterfaceOriginTrialEnabled::VoidMethodDoubleArgFloatArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceOriginTrialEnabled_voidMethodDoubleArgFloatArg");

  test_interface_origin_trial_enabled_v8_internal::VoidMethodDoubleArgFloatArgMethod(info);
}

void V8TestInterfaceOriginTrialEnabled::VoidMethodPartialOverloadMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceOriginTrialEnabled_voidMethodPartialOverload");

  test_interface_origin_trial_enabled_v8_internal::VoidMethodPartialOverloadMethod(info);
}

static const V8DOMConfiguration::AccessorConfiguration V8TestInterfaceOriginTrialEnabledAccessors[] = {
    { "doubleAttribute", V8TestInterfaceOriginTrialEnabled::DoubleAttributeAttributeGetterCallback, V8TestInterfaceOriginTrialEnabled::DoubleAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "staticStringAttribute", V8TestInterfaceOriginTrialEnabled::StaticStringAttributeAttributeGetterCallback, V8TestInterfaceOriginTrialEnabled::StaticStringAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnInterface, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
};

static const V8DOMConfiguration::MethodConfiguration V8TestInterfaceOriginTrialEnabledMethods[] = {
    {"voidMethodDoubleArgFloatArg", V8TestInterfaceOriginTrialEnabled::VoidMethodDoubleArgFloatArgMethodCallback, 2, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodPartialOverload", V8TestInterfaceOriginTrialEnabled::VoidMethodPartialOverloadMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
};

static void InstallV8TestInterfaceOriginTrialEnabledTemplate(
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
      V8TestInterfaceOriginTrialEnabledConstants, base::size(V8TestInterfaceOriginTrialEnabledConstants));
  static_assert(0 == TestInterfaceOriginTrialEnabled::kUnsignedLong, "the value of TestInterfaceOriginTrialEnabled_kUnsignedLong does not match with implementation");
  static_assert(1 == TestInterfaceOriginTrialEnabled::kConstJavascript, "the value of TestInterfaceOriginTrialEnabled_kConstJavascript does not match with implementation");
  V8DOMConfiguration::InstallAccessors(
      isolate, world, instanceTemplate, prototypeTemplate, interfaceTemplate,
      signature, V8TestInterfaceOriginTrialEnabledAccessors, base::size(V8TestInterfaceOriginTrialEnabledAccessors));
  V8DOMConfiguration::InstallMethods(
      isolate, world, instanceTemplate, prototypeTemplate, interfaceTemplate,
      signature, V8TestInterfaceOriginTrialEnabledMethods, base::size(V8TestInterfaceOriginTrialEnabledMethods));

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
        { "conditionalReadOnlyLongAttribute", V8TestInterfaceOriginTrialEnabled::ConditionalReadOnlyLongAttributeAttributeGetterCallback, nullptr, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::ReadOnly), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
        { "staticConditionalReadOnlyLongAttribute", V8TestInterfaceOriginTrialEnabled::StaticConditionalReadOnlyLongAttributeAttributeGetterCallback, nullptr, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::ReadOnly), V8DOMConfiguration::kOnInterface, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
        { "conditionalLongAttribute", V8TestInterfaceOriginTrialEnabled::ConditionalLongAttributeAttributeGetterCallback, V8TestInterfaceOriginTrialEnabled::ConditionalLongAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    };
    V8DOMConfiguration::InstallAccessors(
        isolate, world, instance_template, prototype_template, interface_template,
        signature, accessor_configurations,
        base::size(accessor_configurations));
  }

  // Custom signature
}

v8::Local<v8::FunctionTemplate> V8TestInterfaceOriginTrialEnabled::DomTemplate(v8::Isolate* isolate, const DOMWrapperWorld& world) {
  return V8DOMConfiguration::DomClassTemplate(isolate, world, const_cast<WrapperTypeInfo*>(&wrapperTypeInfo), InstallV8TestInterfaceOriginTrialEnabledTemplate);
}

bool V8TestInterfaceOriginTrialEnabled::HasInstance(v8::Local<v8::Value> v8Value, v8::Isolate* isolate) {
  return V8PerIsolateData::From(isolate)->HasInstance(&wrapperTypeInfo, v8Value);
}

v8::Local<v8::Object> V8TestInterfaceOriginTrialEnabled::FindInstanceInPrototypeChain(v8::Local<v8::Value> v8Value, v8::Isolate* isolate) {
  return V8PerIsolateData::From(isolate)->FindInstanceInPrototypeChain(&wrapperTypeInfo, v8Value);
}

TestInterfaceOriginTrialEnabled* V8TestInterfaceOriginTrialEnabled::ToImplWithTypeCheck(v8::Isolate* isolate, v8::Local<v8::Value> value) {
  return HasInstance(value, isolate) ? ToImpl(v8::Local<v8::Object>::Cast(value)) : nullptr;
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
