// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/partial_interface.cc.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "third_party/blink/renderer/bindings/tests/results/modules/v8_test_interface_partial.h"

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_document.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_configuration.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_node.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_test_callback_interface.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_test_interface.h"
#include "third_party/blink/renderer/bindings/tests/idls/modules/test_interface_partial_3_implementation.h"
#include "third_party/blink/renderer/bindings/tests/idls/modules/test_interface_partial_4.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trials.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/runtime_call_stats.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_object_constructor.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_context_data.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/get_ptr.h"

namespace blink {

namespace test_interface_implementation_partial_v8_internal {

static void Partial4LongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  V8SetReturnValueInt(info, TestInterfacePartial4::partial4LongAttribute(*impl));
}

static void Partial4LongAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestInterface", "partial4LongAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  TestInterfacePartial4::setPartial4LongAttribute(*impl, cppValue);
}

static void Partial4StaticLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  V8SetReturnValueInt(info, TestInterfacePartial4::partial4StaticLongAttribute());
}

static void Partial4StaticLongAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestInterface", "partial4StaticLongAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  TestInterfacePartial4::setPartial4StaticLongAttribute(cppValue);
}

static void VoidMethodPartialOverload3Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  V8StringResource<> value;
  value = info[0];
  if (!value.Prepare())
    return;

  TestInterfacePartial3Implementation::voidMethodPartialOverload(*impl, value);
}

static void VoidMethodPartialOverloadMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  bool isArityError = false;

  switch (std::min(1, info.Length())) {
    case 0:
      break;
    case 1:
      if (true) {
        VoidMethodPartialOverload3Method(info);
        return;
      }
      break;
    default:
      isArityError = true;
  }

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterface", "voidMethodPartialOverload");
  if (isArityError) {
  }
  exceptionState.ThrowTypeError("No function was found that matched the signature provided.");
}

static void StaticVoidMethodPartialOverload2Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  V8StringResource<> value;
  value = info[0];
  if (!value.Prepare())
    return;

  TestInterfacePartial3Implementation::staticVoidMethodPartialOverload(value);
}

static void StaticVoidMethodPartialOverloadMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  bool isArityError = false;

  switch (std::min(1, info.Length())) {
    case 0:
      break;
    case 1:
      if (true) {
        StaticVoidMethodPartialOverload2Method(info);
        return;
      }
      break;
    default:
      isArityError = true;
  }

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterface", "staticVoidMethodPartialOverload");
  if (isArityError) {
  }
  exceptionState.ThrowTypeError("No function was found that matched the signature provided.");
}

static void PromiseMethodPartialOverload3Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterface", "promiseMethodPartialOverload");
  ExceptionToRejectPromiseScope rejectPromiseScope(info, exceptionState);

  // V8DOMConfiguration::kDoNotCheckHolder
  // Make sure that info.Holder() really points to an instance of the type.
  if (!V8TestInterface::HasInstance(info.Holder(), info.GetIsolate())) {
    exceptionState.ThrowTypeError("Illegal invocation");
    return;
  }
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  Document* document;
  document = V8Document::ToImplWithTypeCheck(info.GetIsolate(), info[0]);
  if (!document) {
    exceptionState.ThrowTypeError("parameter 1 is not of type 'Document'.");
    return;
  }

  V8SetReturnValue(info, TestInterfacePartial3Implementation::promiseMethodPartialOverload(*impl, document).V8Value());
}

static void PromiseMethodPartialOverloadMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  bool isArityError = false;

  switch (std::min(1, info.Length())) {
    case 0:
      break;
    case 1:
      if (V8Document::HasInstance(info[0], info.GetIsolate())) {
        PromiseMethodPartialOverload3Method(info);
        return;
      }
      break;
    default:
      isArityError = true;
  }

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterface", "promiseMethodPartialOverload");
  ExceptionToRejectPromiseScope rejectPromiseScope(info, exceptionState);
  if (isArityError) {
  }
  exceptionState.ThrowTypeError("No function was found that matched the signature provided.");
}

static void StaticPromiseMethodPartialOverload2Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterface", "staticPromiseMethodPartialOverload");
  ExceptionToRejectPromiseScope rejectPromiseScope(info, exceptionState);

  V8StringResource<> value;
  value = info[0];
  if (!value.Prepare(exceptionState))
    return;

  V8SetReturnValue(info, TestInterfacePartial3Implementation::staticPromiseMethodPartialOverload(value).V8Value());
}

static void StaticPromiseMethodPartialOverloadMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  bool isArityError = false;

  switch (std::min(1, info.Length())) {
    case 0:
      break;
    case 1:
      if (true) {
        StaticPromiseMethodPartialOverload2Method(info);
        return;
      }
      break;
    default:
      isArityError = true;
  }

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterface", "staticPromiseMethodPartialOverload");
  ExceptionToRejectPromiseScope rejectPromiseScope(info, exceptionState);
  if (isArityError) {
  }
  exceptionState.ThrowTypeError("No function was found that matched the signature provided.");
}

static void Partial2VoidMethod2Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  V8StringResource<> value;
  value = info[0];
  if (!value.Prepare())
    return;

  TestInterfacePartial3Implementation::partial2VoidMethod(*impl, value);
}

static void Partial2VoidMethod3Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  Node* node;
  node = V8Node::ToImplWithTypeCheck(info.GetIsolate(), info[0]);
  if (!node) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("partial2VoidMethod", "TestInterface", "parameter 1 is not of type 'Node'."));
    return;
  }

  TestInterfacePartial3Implementation::partial2VoidMethod(*impl, node);
}

static void Partial2VoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  bool isArityError = false;

  switch (std::min(1, info.Length())) {
    case 0:
      break;
    case 1:
      if (V8Node::HasInstance(info[0], info.GetIsolate())) {
        Partial2VoidMethod3Method(info);
        return;
      }
      if (true) {
        Partial2VoidMethod2Method(info);
        return;
      }
      break;
    default:
      isArityError = true;
  }

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterface", "partial2VoidMethod");
  if (isArityError) {
  }
  exceptionState.ThrowTypeError("No function was found that matched the signature provided.");
}

static void PartialVoidTestEnumModulesArgMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterface", "partialVoidTestEnumModulesArgMethod");

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  V8StringResource<> arg;
  arg = info[0];
  if (!arg.Prepare())
    return;
  const char* validArgValues[] = {
      "EnumModulesValue1",
      "EnumModulesValue2",
  };
  if (!IsValidEnum(arg, validArgValues, base::size(validArgValues), "TestEnumModules", exceptionState)) {
    return;
  }

  TestInterfacePartial3Implementation::partialVoidTestEnumModulesArgMethod(*impl, arg);
}

static void Partial2StaticVoidMethod2Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  V8StringResource<> value;
  value = info[0];
  if (!value.Prepare())
    return;

  TestInterfacePartial3Implementation::partial2StaticVoidMethod(value);
}

static void Partial2StaticVoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  bool isArityError = false;

  switch (std::min(1, info.Length())) {
    case 0:
      break;
    case 1:
      if (true) {
        Partial2StaticVoidMethod2Method(info);
        return;
      }
      break;
    default:
      isArityError = true;
  }

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterface", "partial2StaticVoidMethod");
  if (isArityError) {
  }
  exceptionState.ThrowTypeError("No function was found that matched the signature provided.");
}

static void Partial2VoidTestEnumModulesRecordMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterface", "partial2VoidTestEnumModulesRecordMethod");

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  Vector<std::pair<String, String>> arg;
  arg = NativeValueTraits<IDLRecord<IDLString, IDLString>>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  TestInterfacePartial3Implementation::partial2VoidTestEnumModulesRecordMethod(*impl, arg);
}

static void UnscopableVoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  TestInterfacePartial3Implementation::unscopableVoidMethod(*impl);
}

static void UnionWithTypedefMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  UnsignedLongLongOrBooleanOrTestCallbackInterface result;
  TestInterfacePartial3Implementation::unionWithTypedefMethod(*impl, result);
  V8SetReturnValue(info, result);
}

static void Partial4VoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  TestInterfacePartial4::partial4VoidMethod(*impl);
}

static void Partial4StaticVoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfacePartial4::partial4StaticVoidMethod();
}

}  // namespace test_interface_implementation_partial_v8_internal

void V8TestInterfacePartial::Partial4LongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partial4LongAttribute_Getter");

  test_interface_implementation_partial_v8_internal::Partial4LongAttributeAttributeGetter(info);
}

void V8TestInterfacePartial::Partial4LongAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partial4LongAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_interface_implementation_partial_v8_internal::Partial4LongAttributeAttributeSetter(v8Value, info);
}

void V8TestInterfacePartial::Partial4StaticLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partial4StaticLongAttribute_Getter");

  test_interface_implementation_partial_v8_internal::Partial4StaticLongAttributeAttributeGetter(info);
}

void V8TestInterfacePartial::Partial4StaticLongAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partial4StaticLongAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_interface_implementation_partial_v8_internal::Partial4StaticLongAttributeAttributeSetter(v8Value, info);
}

void V8TestInterfacePartial::PartialVoidTestEnumModulesArgMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partialVoidTestEnumModulesArgMethod");

  test_interface_implementation_partial_v8_internal::PartialVoidTestEnumModulesArgMethodMethod(info);
}

void V8TestInterfacePartial::Partial2VoidTestEnumModulesRecordMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partial2VoidTestEnumModulesRecordMethod");

  test_interface_implementation_partial_v8_internal::Partial2VoidTestEnumModulesRecordMethodMethod(info);
}

void V8TestInterfacePartial::UnscopableVoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_unscopableVoidMethod");

  test_interface_implementation_partial_v8_internal::UnscopableVoidMethodMethod(info);
}

void V8TestInterfacePartial::UnionWithTypedefMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_unionWithTypedefMethod");

  test_interface_implementation_partial_v8_internal::UnionWithTypedefMethodMethod(info);
}

void V8TestInterfacePartial::Partial4VoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partial4VoidMethod");

  test_interface_implementation_partial_v8_internal::Partial4VoidMethodMethod(info);
}

void V8TestInterfacePartial::Partial4StaticVoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partial4StaticVoidMethod");

  test_interface_implementation_partial_v8_internal::Partial4StaticVoidMethodMethod(info);
}

static const V8DOMConfiguration::MethodConfiguration V8TestInterfaceMethods[] = {
    {"partialVoidTestEnumModulesArgMethod", V8TestInterfacePartial::PartialVoidTestEnumModulesArgMethodMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"partial2VoidTestEnumModulesRecordMethod", V8TestInterfacePartial::Partial2VoidTestEnumModulesRecordMethodMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"unscopableVoidMethod", V8TestInterfacePartial::UnscopableVoidMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"unionWithTypedefMethod", V8TestInterfacePartial::UnionWithTypedefMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
};

void V8TestInterfacePartial::InstallV8TestInterfaceTemplate(
    v8::Isolate* isolate,
    const DOMWrapperWorld& world,
    v8::Local<v8::FunctionTemplate> interfaceTemplate) {
  // Initialize the interface object's template.
  V8TestInterface::InstallV8TestInterfaceTemplate(isolate, world, interfaceTemplate);

  v8::Local<v8::Signature> signature = v8::Signature::New(isolate, interfaceTemplate);
  ALLOW_UNUSED_LOCAL(signature);
  v8::Local<v8::ObjectTemplate> instanceTemplate = interfaceTemplate->InstanceTemplate();
  ALLOW_UNUSED_LOCAL(instanceTemplate);
  v8::Local<v8::ObjectTemplate> prototypeTemplate = interfaceTemplate->PrototypeTemplate();
  ALLOW_UNUSED_LOCAL(prototypeTemplate);

  // Register IDL constants, attributes and operations.
  static constexpr V8DOMConfiguration::ConstantConfiguration V8TestInterfaceConstants[] = {
      {"PARTIAL3_UNSIGNED_SHORT", V8DOMConfiguration::kConstantTypeUnsignedShort, static_cast<int>(0)},
  };
  V8DOMConfiguration::InstallConstants(
      isolate, interfaceTemplate, prototypeTemplate,
      V8TestInterfaceConstants, base::size(V8TestInterfaceConstants));
  V8DOMConfiguration::InstallMethods(
      isolate, world, instanceTemplate, prototypeTemplate, interfaceTemplate,
      signature, V8TestInterfaceMethods, base::size(V8TestInterfaceMethods));

  // Custom signature

  V8TestInterfacePartial::InstallRuntimeEnabledFeaturesOnTemplate(
      isolate, world, interfaceTemplate);
}

void V8TestInterfacePartial::InstallRuntimeEnabledFeaturesOnTemplate(
    v8::Isolate* isolate,
    const DOMWrapperWorld& world,
    v8::Local<v8::FunctionTemplate> interface_template) {
  V8TestInterface::InstallRuntimeEnabledFeaturesOnTemplate(isolate, world, interface_template);

  v8::Local<v8::Signature> signature = v8::Signature::New(isolate, interface_template);
  ALLOW_UNUSED_LOCAL(signature);
  v8::Local<v8::ObjectTemplate> instance_template = interface_template->InstanceTemplate();
  ALLOW_UNUSED_LOCAL(instance_template);
  v8::Local<v8::ObjectTemplate> prototype_template = interface_template->PrototypeTemplate();
  ALLOW_UNUSED_LOCAL(prototype_template);

  // Register IDL constants, attributes and operations.

  // Custom signature
}

void V8TestInterfacePartial::InstallOriginTrialPartialFeature(v8::Isolate* isolate, const DOMWrapperWorld& world, v8::Local<v8::Object> instance, v8::Local<v8::Object> prototype, v8::Local<v8::Function> interface) {
  v8::Local<v8::FunctionTemplate> interfaceTemplate = V8TestInterface::wrapperTypeInfo.domTemplate(isolate, world);
  v8::Local<v8::Signature> signature = v8::Signature::New(isolate, interfaceTemplate);
  ALLOW_UNUSED_LOCAL(signature);
  ExecutionContext* executionContext = ToExecutionContext(isolate->GetCurrentContext());
  bool isSecureContext = (executionContext && executionContext->IsSecureContext());
  if (isSecureContext) {
    static const V8DOMConfiguration::AccessorConfiguration accessorpartial4LongAttributeConfiguration[] = {
      { "partial4LongAttribute", V8TestInterfacePartial::Partial4LongAttributeAttributeGetterCallback, V8TestInterfacePartial::Partial4LongAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds }
    };
    for (const auto& accessorConfig : accessorpartial4LongAttributeConfiguration)
      V8DOMConfiguration::InstallAccessor(isolate, world, instance, prototype, interface, signature, accessorConfig);
  }
  if (isSecureContext) {
    static const V8DOMConfiguration::AccessorConfiguration accessorpartial4StaticLongAttributeConfiguration[] = {
      { "partial4StaticLongAttribute", V8TestInterfacePartial::Partial4StaticLongAttributeAttributeGetterCallback, V8TestInterfacePartial::Partial4StaticLongAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnInterface, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds }
    };
    for (const auto& accessorConfig : accessorpartial4StaticLongAttributeConfiguration)
      V8DOMConfiguration::InstallAccessor(isolate, world, instance, prototype, interface, signature, accessorConfig);
  }
  const V8DOMConfiguration::ConstantConfiguration constantPartial4UnsignedShortConfiguration = {"PARTIAL4_UNSIGNED_SHORT", V8DOMConfiguration::kConstantTypeUnsignedShort, static_cast<int>(4)};
  V8DOMConfiguration::InstallConstant(isolate, interface, prototype, constantPartial4UnsignedShortConfiguration);
  if (isSecureContext) {
    static const V8DOMConfiguration::MethodConfiguration methodPartial4StaticvoidmethodConfiguration[] = {
      {"partial4StaticVoidMethod", V8TestInterfacePartial::Partial4StaticVoidMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnInterface, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
    };
    for (const auto& methodConfig : methodPartial4StaticvoidmethodConfiguration)
      V8DOMConfiguration::InstallMethod(isolate, world, instance, prototype, interface, signature, methodConfig);
  }
  if (isSecureContext) {
    static const V8DOMConfiguration::MethodConfiguration methodPartial4VoidmethodConfiguration[] = {
      {"partial4VoidMethod", V8TestInterfacePartial::Partial4VoidMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
    };
    for (const auto& methodConfig : methodPartial4VoidmethodConfiguration)
      V8DOMConfiguration::InstallMethod(isolate, world, instance, prototype, interface, signature, methodConfig);
  }
}

void V8TestInterfacePartial::InstallOriginTrialPartialFeature(ScriptState* scriptState, v8::Local<v8::Object> instance) {
  V8PerContextData* perContextData = scriptState->PerContextData();
  v8::Local<v8::Object> prototype = perContextData->PrototypeForType(&V8TestInterface::wrapperTypeInfo);
  v8::Local<v8::Function> interface = perContextData->ConstructorForType(&V8TestInterface::wrapperTypeInfo);
  ALLOW_UNUSED_LOCAL(interface);
  InstallOriginTrialPartialFeature(scriptState->GetIsolate(), scriptState->World(), instance, prototype, interface);
}

void V8TestInterfacePartial::InstallOriginTrialPartialFeature(ScriptState* scriptState) {
  InstallOriginTrialPartialFeature(scriptState, v8::Local<v8::Object>());
}

void V8TestInterfacePartial::InstallConditionalFeatures(
    v8::Local<v8::Context> context,
    const DOMWrapperWorld& world,
    v8::Local<v8::Object> instanceObject,
    v8::Local<v8::Object> prototypeObject,
    v8::Local<v8::Function> interfaceObject,
    v8::Local<v8::FunctionTemplate> interfaceTemplate) {
  CHECK(!interfaceTemplate.IsEmpty());
  DCHECK((!prototypeObject.IsEmpty() && !interfaceObject.IsEmpty()) ||
         !instanceObject.IsEmpty());
  V8TestInterface::InstallConditionalFeatures(
      context, world, instanceObject, prototypeObject, interfaceObject, interfaceTemplate);

  v8::Isolate* isolate = context->GetIsolate();

  if (!prototypeObject.IsEmpty()) {
    v8::Local<v8::Name> unscopablesSymbol(v8::Symbol::GetUnscopables(isolate));
    v8::Local<v8::Object> unscopables;
    bool has_unscopables;
    if (prototypeObject->HasOwnProperty(context, unscopablesSymbol).To(&has_unscopables) && has_unscopables) {
      unscopables = prototypeObject->Get(context, unscopablesSymbol).ToLocalChecked().As<v8::Object>();
    } else {
      // Web IDL 3.6.3. Interface prototype object
      // https://heycam.github.io/webidl/#create-an-interface-prototype-object
      // step 8.1. Let unscopableObject be the result of performing
      //   ! ObjectCreate(null).
      unscopables = v8::Object::New(isolate);
      unscopables->SetPrototype(context, v8::Null(isolate)).ToChecked();
    }
    unscopables->CreateDataProperty(
        context, V8AtomicString(isolate, "unscopableVoidMethod"), v8::True(isolate))
        .FromJust();
    prototypeObject->CreateDataProperty(context, unscopablesSymbol, unscopables).FromJust();
  }
}

void V8TestInterfacePartial::Initialize() {
  // Should be invoked from ModulesInitializer.
  V8TestInterface::UpdateWrapperTypeInfo(
      &V8TestInterfacePartial::InstallV8TestInterfaceTemplate,
      nullptr,
      &V8TestInterfacePartial::InstallRuntimeEnabledFeaturesOnTemplate,
      V8TestInterfacePartial::InstallConditionalFeatures);
  V8TestInterface::RegisterVoidMethodPartialOverloadMethodForPartialInterface(&test_interface_implementation_partial_v8_internal::VoidMethodPartialOverloadMethod);
  V8TestInterface::RegisterStaticVoidMethodPartialOverloadMethodForPartialInterface(&test_interface_implementation_partial_v8_internal::StaticVoidMethodPartialOverloadMethod);
  V8TestInterface::RegisterPromiseMethodPartialOverloadMethodForPartialInterface(&test_interface_implementation_partial_v8_internal::PromiseMethodPartialOverloadMethod);
  V8TestInterface::RegisterStaticPromiseMethodPartialOverloadMethodForPartialInterface(&test_interface_implementation_partial_v8_internal::StaticPromiseMethodPartialOverloadMethod);
  V8TestInterface::RegisterPartial2VoidMethodMethodForPartialInterface(&test_interface_implementation_partial_v8_internal::Partial2VoidMethodMethod);
  V8TestInterface::RegisterPartial2StaticVoidMethodMethodForPartialInterface(&test_interface_implementation_partial_v8_internal::Partial2StaticVoidMethodMethod);
}

}  // namespace blink
