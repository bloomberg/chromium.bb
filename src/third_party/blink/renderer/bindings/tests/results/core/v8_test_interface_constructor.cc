// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/interface.cc.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "third_party/blink/renderer/bindings/tests/results/core/v8_test_interface_constructor.h"

#include <algorithm>

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_configuration.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_test_dictionary.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_test_interface_empty.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_object_constructor.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/scheduler/public/cooperative_scheduling_manager.h"
#include "third_party/blink/renderer/platform/wtf/get_ptr.h"

namespace blink {

// Suppress warning: global constructors, because struct WrapperTypeInfo is trivial
// and does not depend on another global objects.
#if defined(COMPONENT_BUILD) && defined(WIN32) && defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#endif
const WrapperTypeInfo v8_test_interface_constructor_wrapper_type_info = {
    gin::kEmbedderBlink,
    V8TestInterfaceConstructor::DomTemplate,
    nullptr,
    "TestInterfaceConstructor",
    nullptr,
    WrapperTypeInfo::kWrapperTypeObjectPrototype,
    WrapperTypeInfo::kObjectClassId,
    WrapperTypeInfo::kNotInheritFromActiveScriptWrappable,
};
#if defined(COMPONENT_BUILD) && defined(WIN32) && defined(__clang__)
#pragma clang diagnostic pop
#endif

// This static member must be declared by DEFINE_WRAPPERTYPEINFO in TestInterfaceConstructor.h.
// For details, see the comment of DEFINE_WRAPPERTYPEINFO in
// platform/bindings/ScriptWrappable.h.
const WrapperTypeInfo& TestInterfaceConstructor::wrapper_type_info_ = v8_test_interface_constructor_wrapper_type_info;

// not [ActiveScriptWrappable]
static_assert(
    !std::is_base_of<ActiveScriptWrappableBase, TestInterfaceConstructor>::value,
    "TestInterfaceConstructor inherits from ActiveScriptWrappable<>, but is not specifying "
    "[ActiveScriptWrappable] extended attribute in the IDL file.  "
    "Be consistent.");
static_assert(
    std::is_same<decltype(&TestInterfaceConstructor::HasPendingActivity),
                 decltype(&ScriptWrappable::HasPendingActivity)>::value,
    "TestInterfaceConstructor is overriding hasPendingActivity(), but is not specifying "
    "[ActiveScriptWrappable] extended attribute in the IDL file.  "
    "Be consistent.");

namespace test_interface_constructor_v8_internal {

static void Constructor1(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceConstructor_ConstructorCallback");

  ExceptionState exception_state(info.GetIsolate(), ExceptionState::kConstructionContext, "TestInterfaceConstructor");
  ScriptState* script_state = ScriptState::From(
      info.NewTarget().As<v8::Object>()->CreationContext());

  ExecutionContext* execution_context = ToExecutionContext(
      info.NewTarget().As<v8::Object>()->CreationContext());
  Document& document = *ToLocalDOMWindow(
      info.NewTarget().As<v8::Object>()->CreationContext())->document();
  TestInterfaceConstructor* impl = TestInterfaceConstructor::Create(script_state, execution_context, document, exception_state);
  if (exception_state.HadException()) {
    return;
  }
  v8::Local<v8::Object> wrapper = info.Holder();
  wrapper = impl->AssociateWithWrapper(info.GetIsolate(), V8TestInterfaceConstructor::GetWrapperTypeInfo(), wrapper);
  V8SetReturnValue(info, wrapper);
}

static void Constructor2(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceConstructor_ConstructorCallback");

  ExceptionState exception_state(info.GetIsolate(), ExceptionState::kConstructionContext, "TestInterfaceConstructor");
  ScriptState* script_state = ScriptState::From(
      info.NewTarget().As<v8::Object>()->CreationContext());

  double double_arg;
  V8StringResource<> string_arg;
  TestInterfaceEmpty* test_interface_empty_arg;
  Vector<String> sequence_string_arg;
  HeapVector<LongOrTestDictionary> sequence_long_or_test_dictionary_arg;
  V8StringResource<kTreatNullAndUndefinedAsNullString> optional_usv_string_arg;
  TestInterfaceEmpty* optional_test_interface_empty_arg;
  int num_args_passed = info.Length();
  while (num_args_passed > 0) {
    if (!info[num_args_passed - 1]->IsUndefined())
      break;
    --num_args_passed;
  }
  double_arg = NativeValueTraits<IDLDouble>::NativeValue(info.GetIsolate(), info[0], exception_state);
  if (exception_state.HadException())
    return;

  string_arg = info[1];
  if (!string_arg.Prepare())
    return;

  test_interface_empty_arg = V8TestInterfaceEmpty::ToImplWithTypeCheck(info.GetIsolate(), info[2]);
  if (!test_interface_empty_arg) {
    exception_state.ThrowTypeError(ExceptionMessages::ArgumentNotOfType(2, "TestInterfaceEmpty"));
    return;
  }

  sequence_string_arg = NativeValueTraits<IDLSequence<IDLString>>::NativeValue(info.GetIsolate(), info[3], exception_state);
  if (exception_state.HadException())
    return;

  sequence_long_or_test_dictionary_arg = NativeValueTraits<IDLSequence<LongOrTestDictionary>>::NativeValue(info.GetIsolate(), info[4], exception_state);
  if (exception_state.HadException())
    return;

  if (UNLIKELY(num_args_passed <= 5)) {
    ExecutionContext* execution_context = ToExecutionContext(
        info.NewTarget().As<v8::Object>()->CreationContext());
    Document& document = *ToLocalDOMWindow(
        info.NewTarget().As<v8::Object>()->CreationContext())->document();
    TestInterfaceConstructor* impl = TestInterfaceConstructor::Create(script_state, execution_context, document, double_arg, string_arg, test_interface_empty_arg, sequence_string_arg, sequence_long_or_test_dictionary_arg, exception_state);
    if (exception_state.HadException()) {
      return;
    }
    v8::Local<v8::Object> wrapper = info.Holder();
    wrapper = impl->AssociateWithWrapper(info.GetIsolate(), V8TestInterfaceConstructor::GetWrapperTypeInfo(), wrapper);
    V8SetReturnValue(info, wrapper);
    return;
  }
  optional_usv_string_arg = NativeValueTraits<IDLUSVStringOrNull>::NativeValue(info.GetIsolate(), info[5], exception_state);
  if (exception_state.HadException())
    return;

  optional_test_interface_empty_arg = V8TestInterfaceEmpty::ToImplWithTypeCheck(info.GetIsolate(), info[6]);
  if (!optional_test_interface_empty_arg) {
    exception_state.ThrowTypeError(ExceptionMessages::ArgumentNotOfType(6, "TestInterfaceEmpty"));
    return;
  }

  ExecutionContext* execution_context = ToExecutionContext(
      info.NewTarget().As<v8::Object>()->CreationContext());
  Document& document = *ToLocalDOMWindow(
      info.NewTarget().As<v8::Object>()->CreationContext())->document();
  TestInterfaceConstructor* impl = TestInterfaceConstructor::Create(script_state, execution_context, document, double_arg, string_arg, test_interface_empty_arg, sequence_string_arg, sequence_long_or_test_dictionary_arg, optional_usv_string_arg, optional_test_interface_empty_arg, exception_state);
  if (exception_state.HadException()) {
    return;
  }
  v8::Local<v8::Object> wrapper = info.Holder();
  wrapper = impl->AssociateWithWrapper(info.GetIsolate(), V8TestInterfaceConstructor::GetWrapperTypeInfo(), wrapper);
  V8SetReturnValue(info, wrapper);
}

static void Constructor3(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceConstructor_ConstructorCallback");

  ExceptionState exception_state(info.GetIsolate(), ExceptionState::kConstructionContext, "TestInterfaceConstructor");
  ScriptState* script_state = ScriptState::From(
      info.NewTarget().As<v8::Object>()->CreationContext());

  V8StringResource<> arg;
  V8StringResource<> opt_arg;
  int num_args_passed = info.Length();
  while (num_args_passed > 0) {
    if (!info[num_args_passed - 1]->IsUndefined())
      break;
    --num_args_passed;
  }
  arg = info[0];
  if (!arg.Prepare())
    return;

  if (UNLIKELY(num_args_passed <= 1)) {
    ExecutionContext* execution_context = ToExecutionContext(
        info.NewTarget().As<v8::Object>()->CreationContext());
    Document& document = *ToLocalDOMWindow(
        info.NewTarget().As<v8::Object>()->CreationContext())->document();
    TestInterfaceConstructor* impl = TestInterfaceConstructor::Create(script_state, execution_context, document, arg, exception_state);
    if (exception_state.HadException()) {
      return;
    }
    v8::Local<v8::Object> wrapper = info.Holder();
    wrapper = impl->AssociateWithWrapper(info.GetIsolate(), V8TestInterfaceConstructor::GetWrapperTypeInfo(), wrapper);
    V8SetReturnValue(info, wrapper);
    return;
  }
  opt_arg = info[1];
  if (!opt_arg.Prepare())
    return;

  ExecutionContext* execution_context = ToExecutionContext(
      info.NewTarget().As<v8::Object>()->CreationContext());
  Document& document = *ToLocalDOMWindow(
      info.NewTarget().As<v8::Object>()->CreationContext())->document();
  TestInterfaceConstructor* impl = TestInterfaceConstructor::Create(script_state, execution_context, document, arg, opt_arg, exception_state);
  if (exception_state.HadException()) {
    return;
  }
  v8::Local<v8::Object> wrapper = info.Holder();
  wrapper = impl->AssociateWithWrapper(info.GetIsolate(), V8TestInterfaceConstructor::GetWrapperTypeInfo(), wrapper);
  V8SetReturnValue(info, wrapper);
}

static void Constructor4(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceConstructor_ConstructorCallback");

  ExceptionState exception_state(info.GetIsolate(), ExceptionState::kConstructionContext, "TestInterfaceConstructor");
  ScriptState* script_state = ScriptState::From(
      info.NewTarget().As<v8::Object>()->CreationContext());

  V8StringResource<> arg;
  V8StringResource<> arg_2;
  V8StringResource<> arg_3;
  arg = info[0];
  if (!arg.Prepare())
    return;

  arg_2 = info[1];
  if (!arg_2.Prepare())
    return;

  arg_3 = info[2];
  if (!arg_3.Prepare())
    return;

  ExecutionContext* execution_context = ToExecutionContext(
      info.NewTarget().As<v8::Object>()->CreationContext());
  Document& document = *ToLocalDOMWindow(
      info.NewTarget().As<v8::Object>()->CreationContext())->document();
  TestInterfaceConstructor* impl = TestInterfaceConstructor::Create(script_state, execution_context, document, arg, arg_2, arg_3, exception_state);
  if (exception_state.HadException()) {
    return;
  }
  v8::Local<v8::Object> wrapper = info.Holder();
  wrapper = impl->AssociateWithWrapper(info.GetIsolate(), V8TestInterfaceConstructor::GetWrapperTypeInfo(), wrapper);
  V8SetReturnValue(info, wrapper);
}

static void Constructor(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exception_state(info.GetIsolate(), ExceptionState::kConstructionContext, "TestInterfaceConstructor");
  switch (std::min(7, info.Length())) {
    case 0:
      if (true) {
        test_interface_constructor_v8_internal::Constructor1(info);
        return;
      }
      break;
    case 1:
      if (true) {
        test_interface_constructor_v8_internal::Constructor3(info);
        return;
      }
      break;
    case 2:
      if (true) {
        test_interface_constructor_v8_internal::Constructor3(info);
        return;
      }
      break;
    case 3:
      if (true) {
        test_interface_constructor_v8_internal::Constructor4(info);
        return;
      }
      break;
    case 5:
      if (true) {
        test_interface_constructor_v8_internal::Constructor2(info);
        return;
      }
      break;
    case 6:
      if (true) {
        test_interface_constructor_v8_internal::Constructor2(info);
        return;
      }
      break;
    case 7:
      if (true) {
        test_interface_constructor_v8_internal::Constructor2(info);
        return;
      }
      break;
    default:
      if (info.Length() >= 0) {
        exception_state.ThrowTypeError(ExceptionMessages::InvalidArity("[0, 1, 2, 3, 5, 6, 7]", info.Length()));
        return;
      }
      exception_state.ThrowTypeError(ExceptionMessages::NotEnoughArguments(0, info.Length()));
      return;
  }
  exception_state.ThrowTypeError("No matching constructor signature.");
}

CORE_EXPORT void ConstructorCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceConstructor_Constructor");

  ExecutionContext* execution_context_for_measurement = CurrentExecutionContext(info.GetIsolate());
  UseCounter::Count(execution_context_for_measurement, WebFeature::kTestFeature);
  if (!info.IsConstructCall()) {
    V8ThrowException::ThrowTypeError(
        info.GetIsolate(),
        ExceptionMessages::ConstructorNotCallableAsFunction("TestInterfaceConstructor"));
    return;
  }

  if (ConstructorMode::Current(info.GetIsolate()) == ConstructorMode::kWrapExistingObject) {
    V8SetReturnValue(info, info.Holder());
    return;
  }

  test_interface_constructor_v8_internal::Constructor(info);
}

}  // namespace test_interface_constructor_v8_internal

static void InstallV8TestInterfaceConstructorTemplate(
    v8::Isolate* isolate,
    const DOMWrapperWorld& world,
    v8::Local<v8::FunctionTemplate> interface_template) {
  // Initialize the interface object's template.
  V8DOMConfiguration::InitializeDOMInterfaceTemplate(isolate, interface_template, V8TestInterfaceConstructor::GetWrapperTypeInfo()->interface_name, v8::Local<v8::FunctionTemplate>(), V8TestInterfaceConstructor::kInternalFieldCount);
  interface_template->SetCallHandler(test_interface_constructor_v8_internal::ConstructorCallback);
  interface_template->SetLength(0);

  v8::Local<v8::Signature> signature = v8::Signature::New(isolate, interface_template);
  ALLOW_UNUSED_LOCAL(signature);
  v8::Local<v8::ObjectTemplate> instance_template = interface_template->InstanceTemplate();
  ALLOW_UNUSED_LOCAL(instance_template);
  v8::Local<v8::ObjectTemplate> prototype_template = interface_template->PrototypeTemplate();
  ALLOW_UNUSED_LOCAL(prototype_template);

  // Register IDL constants, attributes and operations.

  // Custom signature

  V8TestInterfaceConstructor::InstallRuntimeEnabledFeaturesOnTemplate(
      isolate, world, interface_template);
}

void V8TestInterfaceConstructor::InstallRuntimeEnabledFeaturesOnTemplate(
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

v8::Local<v8::FunctionTemplate> V8TestInterfaceConstructor::DomTemplate(
    v8::Isolate* isolate, const DOMWrapperWorld& world) {
  return V8DOMConfiguration::DomClassTemplate(
      isolate, world, const_cast<WrapperTypeInfo*>(V8TestInterfaceConstructor::GetWrapperTypeInfo()),
      InstallV8TestInterfaceConstructorTemplate);
}

bool V8TestInterfaceConstructor::HasInstance(v8::Local<v8::Value> v8_value, v8::Isolate* isolate) {
  return V8PerIsolateData::From(isolate)->HasInstance(V8TestInterfaceConstructor::GetWrapperTypeInfo(), v8_value);
}

v8::Local<v8::Object> V8TestInterfaceConstructor::FindInstanceInPrototypeChain(
    v8::Local<v8::Value> v8_value, v8::Isolate* isolate) {
  return V8PerIsolateData::From(isolate)->FindInstanceInPrototypeChain(
      V8TestInterfaceConstructor::GetWrapperTypeInfo(), v8_value);
}

TestInterfaceConstructor* V8TestInterfaceConstructor::ToImplWithTypeCheck(
    v8::Isolate* isolate, v8::Local<v8::Value> value) {
  return HasInstance(value, isolate) ? ToImpl(v8::Local<v8::Object>::Cast(value)) : nullptr;
}

}  // namespace blink
