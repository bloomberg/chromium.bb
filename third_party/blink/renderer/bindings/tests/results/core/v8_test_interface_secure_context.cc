// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/interface.cc.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "third_party/blink/renderer/bindings/tests/results/core/v8_test_interface_secure_context.h"

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
const WrapperTypeInfo V8TestInterfaceSecureContext::wrapperTypeInfo = {
    gin::kEmbedderBlink,
    V8TestInterfaceSecureContext::DomTemplate,
    V8TestInterfaceSecureContext::InstallConditionalFeatures,
    "TestInterfaceSecureContext",
    nullptr,
    WrapperTypeInfo::kWrapperTypeObjectPrototype,
    WrapperTypeInfo::kObjectClassId,
    WrapperTypeInfo::kNotInheritFromActiveScriptWrappable,
};
#if defined(COMPONENT_BUILD) && defined(WIN32) && defined(__clang__)
#pragma clang diagnostic pop
#endif

// This static member must be declared by DEFINE_WRAPPERTYPEINFO in TestInterfaceSecureContext.h.
// For details, see the comment of DEFINE_WRAPPERTYPEINFO in
// platform/bindings/ScriptWrappable.h.
const WrapperTypeInfo& TestInterfaceSecureContext::wrapper_type_info_ = V8TestInterfaceSecureContext::wrapperTypeInfo;

// not [ActiveScriptWrappable]
static_assert(
    !std::is_base_of<ActiveScriptWrappableBase, TestInterfaceSecureContext>::value,
    "TestInterfaceSecureContext inherits from ActiveScriptWrappable<>, but is not specifying "
    "[ActiveScriptWrappable] extended attribute in the IDL file.  "
    "Be consistent.");
static_assert(
    std::is_same<decltype(&TestInterfaceSecureContext::HasPendingActivity),
                 decltype(&ScriptWrappable::HasPendingActivity)>::value,
    "TestInterfaceSecureContext is overriding hasPendingActivity(), but is not specifying "
    "[ActiveScriptWrappable] extended attribute in the IDL file.  "
    "Be consistent.");

namespace test_interface_secure_context_v8_internal {

static void SecureContextAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceSecureContext* impl = V8TestInterfaceSecureContext::ToImpl(holder);

  V8SetReturnValueBool(info, impl->secureContextAttribute());
}

static void SecureContextAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceSecureContext* impl = V8TestInterfaceSecureContext::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestInterfaceSecureContext", "secureContextAttribute");

  // Prepare the value to be set.
  bool cppValue = NativeValueTraits<IDLBoolean>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setSecureContextAttribute(cppValue);
}

static void SecureContextRuntimeEnabledAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceSecureContext* impl = V8TestInterfaceSecureContext::ToImpl(holder);

  V8SetReturnValueBool(info, impl->secureContextRuntimeEnabledAttribute());
}

static void SecureContextRuntimeEnabledAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceSecureContext* impl = V8TestInterfaceSecureContext::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestInterfaceSecureContext", "secureContextRuntimeEnabledAttribute");

  // Prepare the value to be set.
  bool cppValue = NativeValueTraits<IDLBoolean>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setSecureContextRuntimeEnabledAttribute(cppValue);
}

static void SecureContextWindowExposedAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceSecureContext* impl = V8TestInterfaceSecureContext::ToImpl(holder);

  V8SetReturnValueBool(info, impl->secureContextWindowExposedAttribute());
}

static void SecureContextWindowExposedAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceSecureContext* impl = V8TestInterfaceSecureContext::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestInterfaceSecureContext", "secureContextWindowExposedAttribute");

  // Prepare the value to be set.
  bool cppValue = NativeValueTraits<IDLBoolean>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setSecureContextWindowExposedAttribute(cppValue);
}

static void SecureContextWorkerExposedAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceSecureContext* impl = V8TestInterfaceSecureContext::ToImpl(holder);

  V8SetReturnValueBool(info, impl->secureContextWorkerExposedAttribute());
}

static void SecureContextWorkerExposedAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceSecureContext* impl = V8TestInterfaceSecureContext::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestInterfaceSecureContext", "secureContextWorkerExposedAttribute");

  // Prepare the value to be set.
  bool cppValue = NativeValueTraits<IDLBoolean>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setSecureContextWorkerExposedAttribute(cppValue);
}

static void SecureContextWindowExposedRuntimeEnabledAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceSecureContext* impl = V8TestInterfaceSecureContext::ToImpl(holder);

  V8SetReturnValueBool(info, impl->secureContextWindowExposedRuntimeEnabledAttribute());
}

static void SecureContextWindowExposedRuntimeEnabledAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceSecureContext* impl = V8TestInterfaceSecureContext::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestInterfaceSecureContext", "secureContextWindowExposedRuntimeEnabledAttribute");

  // Prepare the value to be set.
  bool cppValue = NativeValueTraits<IDLBoolean>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setSecureContextWindowExposedRuntimeEnabledAttribute(cppValue);
}

static void SecureContextWorkerExposedRuntimeEnabledAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceSecureContext* impl = V8TestInterfaceSecureContext::ToImpl(holder);

  V8SetReturnValueBool(info, impl->secureContextWorkerExposedRuntimeEnabledAttribute());
}

static void SecureContextWorkerExposedRuntimeEnabledAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceSecureContext* impl = V8TestInterfaceSecureContext::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestInterfaceSecureContext", "secureContextWorkerExposedRuntimeEnabledAttribute");

  // Prepare the value to be set.
  bool cppValue = NativeValueTraits<IDLBoolean>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setSecureContextWorkerExposedRuntimeEnabledAttribute(cppValue);
}

static void SecureContextMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceSecureContext* impl = V8TestInterfaceSecureContext::ToImpl(info.Holder());

  impl->secureContextMethod();
}

static void SecureContextRuntimeEnabledMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceSecureContext* impl = V8TestInterfaceSecureContext::ToImpl(info.Holder());

  impl->secureContextRuntimeEnabledMethod();
}

static void SecureContextWindowExposedMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceSecureContext* impl = V8TestInterfaceSecureContext::ToImpl(info.Holder());

  impl->secureContextWindowExposedMethod();
}

static void SecureContextWorkerExposedMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceSecureContext* impl = V8TestInterfaceSecureContext::ToImpl(info.Holder());

  impl->secureContextWorkerExposedMethod();
}

static void SecureContextWindowExposedRuntimeEnabledMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceSecureContext* impl = V8TestInterfaceSecureContext::ToImpl(info.Holder());

  impl->secureContextWindowExposedRuntimeEnabledMethod();
}

static void SecureContextWorkerExposedRuntimeEnabledMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceSecureContext* impl = V8TestInterfaceSecureContext::ToImpl(info.Holder());

  impl->secureContextWorkerExposedRuntimeEnabledMethod();
}

}  // namespace test_interface_secure_context_v8_internal

void V8TestInterfaceSecureContext::SecureContextAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceSecureContext_secureContextAttribute_Getter");

  test_interface_secure_context_v8_internal::SecureContextAttributeAttributeGetter(info);
}

void V8TestInterfaceSecureContext::SecureContextAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceSecureContext_secureContextAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_interface_secure_context_v8_internal::SecureContextAttributeAttributeSetter(v8Value, info);
}

void V8TestInterfaceSecureContext::SecureContextRuntimeEnabledAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceSecureContext_secureContextRuntimeEnabledAttribute_Getter");

  test_interface_secure_context_v8_internal::SecureContextRuntimeEnabledAttributeAttributeGetter(info);
}

void V8TestInterfaceSecureContext::SecureContextRuntimeEnabledAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceSecureContext_secureContextRuntimeEnabledAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_interface_secure_context_v8_internal::SecureContextRuntimeEnabledAttributeAttributeSetter(v8Value, info);
}

void V8TestInterfaceSecureContext::SecureContextWindowExposedAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceSecureContext_secureContextWindowExposedAttribute_Getter");

  test_interface_secure_context_v8_internal::SecureContextWindowExposedAttributeAttributeGetter(info);
}

void V8TestInterfaceSecureContext::SecureContextWindowExposedAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceSecureContext_secureContextWindowExposedAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_interface_secure_context_v8_internal::SecureContextWindowExposedAttributeAttributeSetter(v8Value, info);
}

void V8TestInterfaceSecureContext::SecureContextWorkerExposedAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceSecureContext_secureContextWorkerExposedAttribute_Getter");

  test_interface_secure_context_v8_internal::SecureContextWorkerExposedAttributeAttributeGetter(info);
}

void V8TestInterfaceSecureContext::SecureContextWorkerExposedAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceSecureContext_secureContextWorkerExposedAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_interface_secure_context_v8_internal::SecureContextWorkerExposedAttributeAttributeSetter(v8Value, info);
}

void V8TestInterfaceSecureContext::SecureContextWindowExposedRuntimeEnabledAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceSecureContext_secureContextWindowExposedRuntimeEnabledAttribute_Getter");

  test_interface_secure_context_v8_internal::SecureContextWindowExposedRuntimeEnabledAttributeAttributeGetter(info);
}

void V8TestInterfaceSecureContext::SecureContextWindowExposedRuntimeEnabledAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceSecureContext_secureContextWindowExposedRuntimeEnabledAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_interface_secure_context_v8_internal::SecureContextWindowExposedRuntimeEnabledAttributeAttributeSetter(v8Value, info);
}

void V8TestInterfaceSecureContext::SecureContextWorkerExposedRuntimeEnabledAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceSecureContext_secureContextWorkerExposedRuntimeEnabledAttribute_Getter");

  test_interface_secure_context_v8_internal::SecureContextWorkerExposedRuntimeEnabledAttributeAttributeGetter(info);
}

void V8TestInterfaceSecureContext::SecureContextWorkerExposedRuntimeEnabledAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceSecureContext_secureContextWorkerExposedRuntimeEnabledAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_interface_secure_context_v8_internal::SecureContextWorkerExposedRuntimeEnabledAttributeAttributeSetter(v8Value, info);
}

void V8TestInterfaceSecureContext::SecureContextMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceSecureContext_secureContextMethod");

  test_interface_secure_context_v8_internal::SecureContextMethodMethod(info);
}

void V8TestInterfaceSecureContext::SecureContextRuntimeEnabledMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceSecureContext_secureContextRuntimeEnabledMethod");

  test_interface_secure_context_v8_internal::SecureContextRuntimeEnabledMethodMethod(info);
}

void V8TestInterfaceSecureContext::SecureContextWindowExposedMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceSecureContext_secureContextWindowExposedMethod");

  test_interface_secure_context_v8_internal::SecureContextWindowExposedMethodMethod(info);
}

void V8TestInterfaceSecureContext::SecureContextWorkerExposedMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceSecureContext_secureContextWorkerExposedMethod");

  test_interface_secure_context_v8_internal::SecureContextWorkerExposedMethodMethod(info);
}

void V8TestInterfaceSecureContext::SecureContextWindowExposedRuntimeEnabledMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceSecureContext_secureContextWindowExposedRuntimeEnabledMethod");

  test_interface_secure_context_v8_internal::SecureContextWindowExposedRuntimeEnabledMethodMethod(info);
}

void V8TestInterfaceSecureContext::SecureContextWorkerExposedRuntimeEnabledMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceSecureContext_secureContextWorkerExposedRuntimeEnabledMethod");

  test_interface_secure_context_v8_internal::SecureContextWorkerExposedRuntimeEnabledMethodMethod(info);
}

static void InstallV8TestInterfaceSecureContextTemplate(
    v8::Isolate* isolate,
    const DOMWrapperWorld& world,
    v8::Local<v8::FunctionTemplate> interfaceTemplate) {
  // Initialize the interface object's template.
  V8DOMConfiguration::InitializeDOMInterfaceTemplate(isolate, interfaceTemplate, V8TestInterfaceSecureContext::wrapperTypeInfo.interface_name, v8::Local<v8::FunctionTemplate>(), V8TestInterfaceSecureContext::internalFieldCount);

  v8::Local<v8::Signature> signature = v8::Signature::New(isolate, interfaceTemplate);
  ALLOW_UNUSED_LOCAL(signature);
  v8::Local<v8::ObjectTemplate> instanceTemplate = interfaceTemplate->InstanceTemplate();
  ALLOW_UNUSED_LOCAL(instanceTemplate);
  v8::Local<v8::ObjectTemplate> prototypeTemplate = interfaceTemplate->PrototypeTemplate();
  ALLOW_UNUSED_LOCAL(prototypeTemplate);

  // Register IDL constants, attributes and operations.

  // Custom signature

  V8TestInterfaceSecureContext::InstallRuntimeEnabledFeaturesOnTemplate(
      isolate, world, interfaceTemplate);
}

void V8TestInterfaceSecureContext::InstallRuntimeEnabledFeaturesOnTemplate(
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

v8::Local<v8::FunctionTemplate> V8TestInterfaceSecureContext::DomTemplate(v8::Isolate* isolate, const DOMWrapperWorld& world) {
  return V8DOMConfiguration::DomClassTemplate(isolate, world, const_cast<WrapperTypeInfo*>(&wrapperTypeInfo), InstallV8TestInterfaceSecureContextTemplate);
}

bool V8TestInterfaceSecureContext::HasInstance(v8::Local<v8::Value> v8Value, v8::Isolate* isolate) {
  return V8PerIsolateData::From(isolate)->HasInstance(&wrapperTypeInfo, v8Value);
}

v8::Local<v8::Object> V8TestInterfaceSecureContext::FindInstanceInPrototypeChain(v8::Local<v8::Value> v8Value, v8::Isolate* isolate) {
  return V8PerIsolateData::From(isolate)->FindInstanceInPrototypeChain(&wrapperTypeInfo, v8Value);
}

TestInterfaceSecureContext* V8TestInterfaceSecureContext::ToImplWithTypeCheck(v8::Isolate* isolate, v8::Local<v8::Value> value) {
  return HasInstance(value, isolate) ? ToImpl(v8::Local<v8::Object>::Cast(value)) : nullptr;
}

TestInterfaceSecureContext* NativeValueTraits<TestInterfaceSecureContext>::NativeValue(v8::Isolate* isolate, v8::Local<v8::Value> value, ExceptionState& exceptionState) {
  TestInterfaceSecureContext* nativeValue = V8TestInterfaceSecureContext::ToImplWithTypeCheck(isolate, value);
  if (!nativeValue) {
    exceptionState.ThrowTypeError(ExceptionMessages::FailedToConvertJSValue(
        "TestInterfaceSecureContext"));
  }
  return nativeValue;
}

void V8TestInterfaceSecureContext::InstallConditionalFeatures(
    v8::Local<v8::Context> context,
    const DOMWrapperWorld& world,
    v8::Local<v8::Object> instanceObject,
    v8::Local<v8::Object> prototypeObject,
    v8::Local<v8::Function> interfaceObject,
    v8::Local<v8::FunctionTemplate> interfaceTemplate) {
  CHECK(!interfaceTemplate.IsEmpty());
  DCHECK((!prototypeObject.IsEmpty() && !interfaceObject.IsEmpty()) ||
         !instanceObject.IsEmpty());

  v8::Isolate* isolate = context->GetIsolate();

  v8::Local<v8::Signature> signature = v8::Signature::New(isolate, interfaceTemplate);
  ExecutionContext* executionContext = ToExecutionContext(context);
  DCHECK(executionContext);
  bool isSecureContext = (executionContext && executionContext->IsSecureContext());

  if (!prototypeObject.IsEmpty() || !interfaceObject.IsEmpty()) {
    if (isSecureContext) {
      static const V8DOMConfiguration::AccessorConfiguration accessor_configurations[] = {
          { "secureContextAttribute", V8TestInterfaceSecureContext::SecureContextAttributeAttributeGetterCallback, V8TestInterfaceSecureContext::SecureContextAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
      };
      V8DOMConfiguration::InstallAccessors(
          isolate, world, instanceObject, prototypeObject, interfaceObject,
          signature, accessor_configurations,
          base::size(accessor_configurations));
      if (RuntimeEnabledFeatures::SecureFeatureEnabled()) {
        static const V8DOMConfiguration::AccessorConfiguration accessor_configurations[] = {
            { "secureContextRuntimeEnabledAttribute", V8TestInterfaceSecureContext::SecureContextRuntimeEnabledAttributeAttributeGetterCallback, V8TestInterfaceSecureContext::SecureContextRuntimeEnabledAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
        };
        V8DOMConfiguration::InstallAccessors(
            isolate, world, instanceObject, prototypeObject, interfaceObject,
            signature, accessor_configurations,
            base::size(accessor_configurations));
      }
    }
    if (executionContext && (executionContext->IsDocument())) {
      if (isSecureContext) {
        static const V8DOMConfiguration::AccessorConfiguration accessor_configurations[] = {
            { "secureContextWindowExposedAttribute", V8TestInterfaceSecureContext::SecureContextWindowExposedAttributeAttributeGetterCallback, V8TestInterfaceSecureContext::SecureContextWindowExposedAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
        };
        V8DOMConfiguration::InstallAccessors(
            isolate, world, instanceObject, prototypeObject, interfaceObject,
            signature, accessor_configurations,
            base::size(accessor_configurations));
        if (RuntimeEnabledFeatures::SecureFeatureEnabled()) {
          static const V8DOMConfiguration::AccessorConfiguration accessor_configurations[] = {
              { "secureContextWindowExposedRuntimeEnabledAttribute", V8TestInterfaceSecureContext::SecureContextWindowExposedRuntimeEnabledAttributeAttributeGetterCallback, V8TestInterfaceSecureContext::SecureContextWindowExposedRuntimeEnabledAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
          };
          V8DOMConfiguration::InstallAccessors(
              isolate, world, instanceObject, prototypeObject, interfaceObject,
              signature, accessor_configurations,
              base::size(accessor_configurations));
        }
      }
    }
    if (executionContext && (executionContext->IsWorkerGlobalScope())) {
      if (isSecureContext) {
        static const V8DOMConfiguration::AccessorConfiguration accessor_configurations[] = {
            { "secureContextWorkerExposedAttribute", V8TestInterfaceSecureContext::SecureContextWorkerExposedAttributeAttributeGetterCallback, V8TestInterfaceSecureContext::SecureContextWorkerExposedAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
        };
        V8DOMConfiguration::InstallAccessors(
            isolate, world, instanceObject, prototypeObject, interfaceObject,
            signature, accessor_configurations,
            base::size(accessor_configurations));
        if (RuntimeEnabledFeatures::SecureFeatureEnabled()) {
          static const V8DOMConfiguration::AccessorConfiguration accessor_configurations[] = {
              { "secureContextWorkerExposedRuntimeEnabledAttribute", V8TestInterfaceSecureContext::SecureContextWorkerExposedRuntimeEnabledAttributeAttributeGetterCallback, V8TestInterfaceSecureContext::SecureContextWorkerExposedRuntimeEnabledAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
          };
          V8DOMConfiguration::InstallAccessors(
              isolate, world, instanceObject, prototypeObject, interfaceObject,
              signature, accessor_configurations,
              base::size(accessor_configurations));
        }
      }
    }
    if (isSecureContext) {
      const V8DOMConfiguration::MethodConfiguration secureContextMethodMethodConfiguration[] = {
        {"secureContextMethod", V8TestInterfaceSecureContext::SecureContextMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
      };
      for (const auto& methodConfig : secureContextMethodMethodConfiguration)
        V8DOMConfiguration::InstallMethod(isolate, world, instanceObject, prototypeObject, interfaceObject, signature, methodConfig);
    }
    if (isSecureContext) {
      if (RuntimeEnabledFeatures::SecureFeatureEnabled()) {
        const V8DOMConfiguration::MethodConfiguration secureContextRuntimeEnabledMethodMethodConfiguration[] = {
          {"secureContextRuntimeEnabledMethod", V8TestInterfaceSecureContext::SecureContextRuntimeEnabledMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
        };
        for (const auto& methodConfig : secureContextRuntimeEnabledMethodMethodConfiguration)
          V8DOMConfiguration::InstallMethod(isolate, world, instanceObject, prototypeObject, interfaceObject, signature, methodConfig);
      }
    }
    if (isSecureContext) {
      if (executionContext && (executionContext->IsDocument())) {
        const V8DOMConfiguration::MethodConfiguration secureContextWindowExposedMethodMethodConfiguration[] = {
          {"secureContextWindowExposedMethod", V8TestInterfaceSecureContext::SecureContextWindowExposedMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
        };
        for (const auto& methodConfig : secureContextWindowExposedMethodMethodConfiguration)
          V8DOMConfiguration::InstallMethod(isolate, world, instanceObject, prototypeObject, interfaceObject, signature, methodConfig);
      }
    }
    if (isSecureContext) {
      if (executionContext && (executionContext->IsWorkerGlobalScope())) {
        const V8DOMConfiguration::MethodConfiguration secureContextWorkerExposedMethodMethodConfiguration[] = {
          {"secureContextWorkerExposedMethod", V8TestInterfaceSecureContext::SecureContextWorkerExposedMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
        };
        for (const auto& methodConfig : secureContextWorkerExposedMethodMethodConfiguration)
          V8DOMConfiguration::InstallMethod(isolate, world, instanceObject, prototypeObject, interfaceObject, signature, methodConfig);
      }
    }
    if (isSecureContext) {
      if (executionContext && (executionContext->IsDocument())) {
        if (RuntimeEnabledFeatures::SecureFeatureEnabled()) {
          const V8DOMConfiguration::MethodConfiguration secureContextWindowExposedRuntimeEnabledMethodMethodConfiguration[] = {
            {"secureContextWindowExposedRuntimeEnabledMethod", V8TestInterfaceSecureContext::SecureContextWindowExposedRuntimeEnabledMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
          };
          for (const auto& methodConfig : secureContextWindowExposedRuntimeEnabledMethodMethodConfiguration)
            V8DOMConfiguration::InstallMethod(isolate, world, instanceObject, prototypeObject, interfaceObject, signature, methodConfig);
        }
      }
    }
    if (isSecureContext) {
      if (executionContext && (executionContext->IsWorkerGlobalScope())) {
        if (RuntimeEnabledFeatures::SecureFeatureEnabled()) {
          const V8DOMConfiguration::MethodConfiguration secureContextWorkerExposedRuntimeEnabledMethodMethodConfiguration[] = {
            {"secureContextWorkerExposedRuntimeEnabledMethod", V8TestInterfaceSecureContext::SecureContextWorkerExposedRuntimeEnabledMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
          };
          for (const auto& methodConfig : secureContextWorkerExposedRuntimeEnabledMethodMethodConfiguration)
            V8DOMConfiguration::InstallMethod(isolate, world, instanceObject, prototypeObject, interfaceObject, signature, methodConfig);
        }
      }
    }
  }
}

}  // namespace blink
