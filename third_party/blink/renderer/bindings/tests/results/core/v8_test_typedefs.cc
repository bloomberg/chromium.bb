// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/interface.cc.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "third_party/blink/renderer/bindings/tests/results/core/v8_test_typedefs.h"

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_configuration.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_event.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_node.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_node_list.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_test_callback_interface.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_test_interface.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_test_interface_empty.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_test_object.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_xml_http_request.h"
#include "third_party/blink/renderer/core/dom/name_node_list.h"
#include "third_party/blink/renderer/core/dom/node_list.h"
#include "third_party/blink/renderer/core/dom/static_node_list.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/html/forms/labels_node_list.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/runtime_call_stats.h"
#include "third_party/blink/renderer/platform/bindings/v8_object_constructor.h"
#include "third_party/blink/renderer/platform/wtf/get_ptr.h"

namespace blink {

// Suppress warning: global constructors, because struct WrapperTypeInfo is trivial
// and does not depend on another global objects.
#if defined(COMPONENT_BUILD) && defined(WIN32) && defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#endif
const WrapperTypeInfo V8TestTypedefs::wrapperTypeInfo = {
    gin::kEmbedderBlink,
    V8TestTypedefs::DomTemplate,
    nullptr,
    "TestTypedefs",
    nullptr,
    WrapperTypeInfo::kWrapperTypeObjectPrototype,
    WrapperTypeInfo::kObjectClassId,
    WrapperTypeInfo::kNotInheritFromActiveScriptWrappable,
};
#if defined(COMPONENT_BUILD) && defined(WIN32) && defined(__clang__)
#pragma clang diagnostic pop
#endif

// This static member must be declared by DEFINE_WRAPPERTYPEINFO in TestTypedefs.h.
// For details, see the comment of DEFINE_WRAPPERTYPEINFO in
// platform/bindings/ScriptWrappable.h.
const WrapperTypeInfo& TestTypedefs::wrapper_type_info_ = V8TestTypedefs::wrapperTypeInfo;

// not [ActiveScriptWrappable]
static_assert(
    !std::is_base_of<ActiveScriptWrappableBase, TestTypedefs>::value,
    "TestTypedefs inherits from ActiveScriptWrappable<>, but is not specifying "
    "[ActiveScriptWrappable] extended attribute in the IDL file.  "
    "Be consistent.");
static_assert(
    std::is_same<decltype(&TestTypedefs::HasPendingActivity),
                 decltype(&ScriptWrappable::HasPendingActivity)>::value,
    "TestTypedefs is overriding hasPendingActivity(), but is not specifying "
    "[ActiveScriptWrappable] extended attribute in the IDL file.  "
    "Be consistent.");

namespace test_typedefs_v8_internal {

static void ULongLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestTypedefs* impl = V8TestTypedefs::ToImpl(holder);

  V8SetReturnValue(info, static_cast<double>(impl->uLongLongAttribute()));
}

static void ULongLongAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestTypedefs* impl = V8TestTypedefs::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestTypedefs", "uLongLongAttribute");

  // Prepare the value to be set.
  uint64_t cppValue = NativeValueTraits<IDLUnsignedLongLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setULongLongAttribute(cppValue);
}

static void LongWithClampAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestTypedefs* impl = V8TestTypedefs::ToImpl(holder);

  V8SetReturnValueInt(info, impl->longWithClampAttribute());
}

static void LongWithClampAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestTypedefs* impl = V8TestTypedefs::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestTypedefs", "longWithClampAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLongClamp>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setLongWithClampAttribute(cppValue);
}

static void DOMStringOrDoubleOrNullAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestTypedefs* impl = V8TestTypedefs::ToImpl(holder);

  StringOrDouble result;
  impl->domStringOrDoubleOrNullAttribute(result);

  V8SetReturnValue(info, result);
}

static void DOMStringOrDoubleOrNullAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestTypedefs* impl = V8TestTypedefs::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestTypedefs", "domStringOrDoubleOrNullAttribute");

  // Prepare the value to be set.
  StringOrDouble cppValue;
  V8StringOrDouble::ToImpl(info.GetIsolate(), v8Value, cppValue, UnionTypeConversionMode::kNullable, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setDomStringOrDoubleOrNullAttribute(cppValue);
}

static void VoidMethodLongSequenceArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestTypedefs", "voidMethodLongSequenceArg");

  TestTypedefs* impl = V8TestTypedefs::ToImpl(info.Holder());

  Vector<int32_t> longSequenceArg;
  int numArgsPassed = info.Length();
  while (numArgsPassed > 0) {
    if (!info[numArgsPassed - 1]->IsUndefined())
      break;
    --numArgsPassed;
  }
  if (UNLIKELY(numArgsPassed <= 0)) {
    impl->voidMethodLongSequenceArg();
    return;
  }
  longSequenceArg = NativeValueTraits<IDLSequence<IDLLong>>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodLongSequenceArg(longSequenceArg);
}

static void VoidMethodFloatArgStringArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestTypedefs", "voidMethodFloatArgStringArg");

  TestTypedefs* impl = V8TestTypedefs::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 2)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(2, info.Length()));
    return;
  }

  float floatArg;
  V8StringResource<> stringArg;
  floatArg = NativeValueTraits<IDLFloat>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  stringArg = info[1];
  if (!stringArg.Prepare())
    return;

  impl->voidMethodFloatArgStringArg(floatArg, stringArg);
}

static void VoidMethodTestCallbackInterfaceTypeArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestTypedefs", "voidMethodTestCallbackInterfaceTypeArg");

  TestTypedefs* impl = V8TestTypedefs::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodTestCallbackInterfaceTypeArg", "TestTypedefs", ExceptionMessages::NotEnoughArguments(1, info.Length())));
    return;
  }

  V8TestCallbackInterface* testCallbackInterfaceTypeArg;
  if (info[0]->IsObject()) {
    testCallbackInterfaceTypeArg = V8TestCallbackInterface::CreateOrNull(info[0].As<v8::Object>());
    if (!testCallbackInterfaceTypeArg) {
      exceptionState.ThrowSecurityError("The callback provided as parameter 1 is a cross origin object.");
      return;
    }
  } else {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodTestCallbackInterfaceTypeArg", "TestTypedefs", "The callback provided as parameter 1 is not an object."));
    return;
  }

  impl->voidMethodTestCallbackInterfaceTypeArg(testCallbackInterfaceTypeArg);
}

static void ULongLongMethodTestInterfaceEmptyTypeSequenceArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestTypedefs", "uLongLongMethodTestInterfaceEmptyTypeSequenceArg");

  TestTypedefs* impl = V8TestTypedefs::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  HeapVector<Member<TestInterfaceEmpty>> testInterfaceEmptyTypeSequenceArg;
  testInterfaceEmptyTypeSequenceArg = NativeValueTraits<IDLSequence<TestInterfaceEmpty>>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  V8SetReturnValue(info, static_cast<double>(impl->uLongLongMethodTestInterfaceEmptyTypeSequenceArg(testInterfaceEmptyTypeSequenceArg)));
}

static void TestInterfaceOrTestInterfaceEmptyMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestTypedefs* impl = V8TestTypedefs::ToImpl(info.Holder());

  TestInterfaceOrTestInterfaceEmpty result;
  impl->testInterfaceOrTestInterfaceEmptyMethod(result);
  V8SetReturnValue(info, result);
}

static void DOMStringOrDoubleMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestTypedefs* impl = V8TestTypedefs::ToImpl(info.Holder());

  StringOrDouble result;
  impl->domStringOrDoubleMethod(result);
  V8SetReturnValue(info, result);
}

static void ArrayOfStringsMethodArrayOfStringsArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestTypedefs", "arrayOfStringsMethodArrayOfStringsArg");

  TestTypedefs* impl = V8TestTypedefs::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  Vector<String> frozenStringArrayArg;
  frozenStringArrayArg = NativeValueTraits<IDLSequence<IDLString>>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  V8SetReturnValue(info, FreezeV8Object(ToV8(impl->arrayOfStringsMethodArrayOfStringsArg(frozenStringArrayArg), info.Holder(), info.GetIsolate()), info.GetIsolate()));
}

static void MethodTakingRecordMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestTypedefs", "methodTakingRecord");

  TestTypedefs* impl = V8TestTypedefs::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  Vector<std::pair<String, int32_t>> arg;
  arg = NativeValueTraits<IDLRecord<IDLByteString, IDLLong>>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->methodTakingRecord(arg);
}

static void MethodTakingOilpanValueRecordMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestTypedefs", "methodTakingOilpanValueRecord");

  TestTypedefs* impl = V8TestTypedefs::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  HeapVector<std::pair<String, Member<TestObject>>> arg;
  arg = NativeValueTraits<IDLRecord<IDLUSVString, TestObject>>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->methodTakingOilpanValueRecord(arg);
}

static void UnionWithRecordMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestTypedefs", "unionWithRecordMethod");

  TestTypedefs* impl = V8TestTypedefs::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  ByteStringSequenceSequenceOrByteStringByteStringRecord arg;
  V8ByteStringSequenceSequenceOrByteStringByteStringRecord::ToImpl(info.GetIsolate(), info[0], arg, UnionTypeConversionMode::kNotNullable, exceptionState);
  if (exceptionState.HadException())
    return;

  V8SetReturnValue(info, impl->unionWithRecordMethod(arg));
}

static void MethodThatReturnsRecordMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestTypedefs* impl = V8TestTypedefs::ToImpl(info.Holder());

  V8SetReturnValue(info, ToV8(impl->methodThatReturnsRecord(), info.Holder(), info.GetIsolate()));
}

static void VoidMethodNestedUnionTypeMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestTypedefs", "voidMethodNestedUnionType");

  TestTypedefs* impl = V8TestTypedefs::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord arg;
  V8NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord::ToImpl(info.GetIsolate(), info[0], arg, UnionTypeConversionMode::kNullable, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodNestedUnionType(arg);
}

static void VoidMethodUnionWithTypedefMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestTypedefs", "voidMethodUnionWithTypedef");

  TestTypedefs* impl = V8TestTypedefs::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  UnsignedLongLongOrBooleanOrTestCallbackInterface arg;
  V8UnsignedLongLongOrBooleanOrTestCallbackInterface::ToImpl(info.GetIsolate(), info[0], arg, UnionTypeConversionMode::kNotNullable, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodUnionWithTypedef(arg);
}

static void Constructor(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestTypedefs_ConstructorCallback");

  if (UNLIKELY(info.Length() < 1)) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToConstruct("TestTypedefs", ExceptionMessages::NotEnoughArguments(1, info.Length())));
    return;
  }

  V8StringResource<> stringArg;
  stringArg = info[0];
  if (!stringArg.Prepare())
    return;

  TestTypedefs* impl = TestTypedefs::Create(stringArg);
  v8::Local<v8::Object> wrapper = info.Holder();
  wrapper = impl->AssociateWithWrapper(info.GetIsolate(), &V8TestTypedefs::wrapperTypeInfo, wrapper);
  V8SetReturnValue(info, wrapper);
}

CORE_EXPORT void ConstructorCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestTypedefs_Constructor");

  if (!info.IsConstructCall()) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::ConstructorNotCallableAsFunction("TestTypedefs"));
    return;
  }

  if (ConstructorMode::Current(info.GetIsolate()) == ConstructorMode::kWrapExistingObject) {
    V8SetReturnValue(info, info.Holder());
    return;
  }

  test_typedefs_v8_internal::Constructor(info);
}

}  // namespace test_typedefs_v8_internal

void V8TestTypedefs::ULongLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestTypedefs_uLongLongAttribute_Getter");

  test_typedefs_v8_internal::ULongLongAttributeAttributeGetter(info);
}

void V8TestTypedefs::ULongLongAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestTypedefs_uLongLongAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_typedefs_v8_internal::ULongLongAttributeAttributeSetter(v8Value, info);
}

void V8TestTypedefs::LongWithClampAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestTypedefs_longWithClampAttribute_Getter");

  test_typedefs_v8_internal::LongWithClampAttributeAttributeGetter(info);
}

void V8TestTypedefs::LongWithClampAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestTypedefs_longWithClampAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_typedefs_v8_internal::LongWithClampAttributeAttributeSetter(v8Value, info);
}

void V8TestTypedefs::TAttributeConstructorGetterCallback(v8::Local<v8::Name> property, const v8::PropertyCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestTypedefs_tAttribute_ConstructorGetterCallback");

  V8ConstructorAttributeGetter(property, info, &V8TestInterface::wrapperTypeInfo);
}

void V8TestTypedefs::DOMStringOrDoubleOrNullAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestTypedefs_domStringOrDoubleOrNullAttribute_Getter");

  test_typedefs_v8_internal::DOMStringOrDoubleOrNullAttributeAttributeGetter(info);
}

void V8TestTypedefs::DOMStringOrDoubleOrNullAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestTypedefs_domStringOrDoubleOrNullAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_typedefs_v8_internal::DOMStringOrDoubleOrNullAttributeAttributeSetter(v8Value, info);
}

void V8TestTypedefs::VoidMethodLongSequenceArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestTypedefs_voidMethodLongSequenceArg");

  test_typedefs_v8_internal::VoidMethodLongSequenceArgMethod(info);
}

void V8TestTypedefs::VoidMethodFloatArgStringArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestTypedefs_voidMethodFloatArgStringArg");

  test_typedefs_v8_internal::VoidMethodFloatArgStringArgMethod(info);
}

void V8TestTypedefs::VoidMethodTestCallbackInterfaceTypeArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestTypedefs_voidMethodTestCallbackInterfaceTypeArg");

  test_typedefs_v8_internal::VoidMethodTestCallbackInterfaceTypeArgMethod(info);
}

void V8TestTypedefs::ULongLongMethodTestInterfaceEmptyTypeSequenceArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestTypedefs_uLongLongMethodTestInterfaceEmptyTypeSequenceArg");

  test_typedefs_v8_internal::ULongLongMethodTestInterfaceEmptyTypeSequenceArgMethod(info);
}

void V8TestTypedefs::TestInterfaceOrTestInterfaceEmptyMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestTypedefs_testInterfaceOrTestInterfaceEmptyMethod");

  test_typedefs_v8_internal::TestInterfaceOrTestInterfaceEmptyMethodMethod(info);
}

void V8TestTypedefs::DOMStringOrDoubleMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestTypedefs_domStringOrDoubleMethod");

  test_typedefs_v8_internal::DOMStringOrDoubleMethodMethod(info);
}

void V8TestTypedefs::ArrayOfStringsMethodArrayOfStringsArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestTypedefs_arrayOfStringsMethodArrayOfStringsArg");

  test_typedefs_v8_internal::ArrayOfStringsMethodArrayOfStringsArgMethod(info);
}

void V8TestTypedefs::MethodTakingRecordMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestTypedefs_methodTakingRecord");

  test_typedefs_v8_internal::MethodTakingRecordMethod(info);
}

void V8TestTypedefs::MethodTakingOilpanValueRecordMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestTypedefs_methodTakingOilpanValueRecord");

  test_typedefs_v8_internal::MethodTakingOilpanValueRecordMethod(info);
}

void V8TestTypedefs::UnionWithRecordMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestTypedefs_unionWithRecordMethod");

  test_typedefs_v8_internal::UnionWithRecordMethodMethod(info);
}

void V8TestTypedefs::MethodThatReturnsRecordMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestTypedefs_methodThatReturnsRecord");

  test_typedefs_v8_internal::MethodThatReturnsRecordMethod(info);
}

void V8TestTypedefs::VoidMethodNestedUnionTypeMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestTypedefs_voidMethodNestedUnionType");

  test_typedefs_v8_internal::VoidMethodNestedUnionTypeMethod(info);
}

void V8TestTypedefs::VoidMethodUnionWithTypedefMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestTypedefs_voidMethodUnionWithTypedef");

  test_typedefs_v8_internal::VoidMethodUnionWithTypedefMethod(info);
}

// Suppress warning: global constructors, because AttributeConfiguration is trivial
// and does not depend on another global objects.
#if defined(COMPONENT_BUILD) && defined(WIN32) && defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#endif
static const V8DOMConfiguration::AttributeConfiguration V8TestTypedefsAttributes[] = {
    { "tAttribute", V8TestTypedefs::TAttributeConstructorGetterCallback, nullptr, static_cast<v8::PropertyAttribute>(v8::DontEnum), V8DOMConfiguration::kOnInstance, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kReplaceWithDataProperty, V8DOMConfiguration::kAllWorlds },
};
#if defined(COMPONENT_BUILD) && defined(WIN32) && defined(__clang__)
#pragma clang diagnostic pop
#endif

static const V8DOMConfiguration::AccessorConfiguration V8TestTypedefsAccessors[] = {
    { "uLongLongAttribute", V8TestTypedefs::ULongLongAttributeAttributeGetterCallback, V8TestTypedefs::ULongLongAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "longWithClampAttribute", V8TestTypedefs::LongWithClampAttributeAttributeGetterCallback, V8TestTypedefs::LongWithClampAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "domStringOrDoubleOrNullAttribute", V8TestTypedefs::DOMStringOrDoubleOrNullAttributeAttributeGetterCallback, V8TestTypedefs::DOMStringOrDoubleOrNullAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
};

static const V8DOMConfiguration::MethodConfiguration V8TestTypedefsMethods[] = {
    {"voidMethodLongSequenceArg", V8TestTypedefs::VoidMethodLongSequenceArgMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodFloatArgStringArg", V8TestTypedefs::VoidMethodFloatArgStringArgMethodCallback, 2, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodTestCallbackInterfaceTypeArg", V8TestTypedefs::VoidMethodTestCallbackInterfaceTypeArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"uLongLongMethodTestInterfaceEmptyTypeSequenceArg", V8TestTypedefs::ULongLongMethodTestInterfaceEmptyTypeSequenceArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"testInterfaceOrTestInterfaceEmptyMethod", V8TestTypedefs::TestInterfaceOrTestInterfaceEmptyMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"domStringOrDoubleMethod", V8TestTypedefs::DOMStringOrDoubleMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"arrayOfStringsMethodArrayOfStringsArg", V8TestTypedefs::ArrayOfStringsMethodArrayOfStringsArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"methodTakingRecord", V8TestTypedefs::MethodTakingRecordMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"methodTakingOilpanValueRecord", V8TestTypedefs::MethodTakingOilpanValueRecordMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"unionWithRecordMethod", V8TestTypedefs::UnionWithRecordMethodMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"methodThatReturnsRecord", V8TestTypedefs::MethodThatReturnsRecordMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodNestedUnionType", V8TestTypedefs::VoidMethodNestedUnionTypeMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodUnionWithTypedef", V8TestTypedefs::VoidMethodUnionWithTypedefMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
};

static void InstallV8TestTypedefsTemplate(
    v8::Isolate* isolate,
    const DOMWrapperWorld& world,
    v8::Local<v8::FunctionTemplate> interfaceTemplate) {
  // Initialize the interface object's template.
  V8DOMConfiguration::InitializeDOMInterfaceTemplate(isolate, interfaceTemplate, V8TestTypedefs::wrapperTypeInfo.interface_name, v8::Local<v8::FunctionTemplate>(), V8TestTypedefs::internalFieldCount);
  interfaceTemplate->SetCallHandler(test_typedefs_v8_internal::ConstructorCallback);
  interfaceTemplate->SetLength(1);

  v8::Local<v8::Signature> signature = v8::Signature::New(isolate, interfaceTemplate);
  ALLOW_UNUSED_LOCAL(signature);
  v8::Local<v8::ObjectTemplate> instanceTemplate = interfaceTemplate->InstanceTemplate();
  ALLOW_UNUSED_LOCAL(instanceTemplate);
  v8::Local<v8::ObjectTemplate> prototypeTemplate = interfaceTemplate->PrototypeTemplate();
  ALLOW_UNUSED_LOCAL(prototypeTemplate);

  // Register IDL constants, attributes and operations.
  V8DOMConfiguration::InstallAttributes(
      isolate, world, instanceTemplate, prototypeTemplate,
      V8TestTypedefsAttributes, base::size(V8TestTypedefsAttributes));
  V8DOMConfiguration::InstallAccessors(
      isolate, world, instanceTemplate, prototypeTemplate, interfaceTemplate,
      signature, V8TestTypedefsAccessors, base::size(V8TestTypedefsAccessors));
  V8DOMConfiguration::InstallMethods(
      isolate, world, instanceTemplate, prototypeTemplate, interfaceTemplate,
      signature, V8TestTypedefsMethods, base::size(V8TestTypedefsMethods));

  // Custom signature

  V8TestTypedefs::InstallRuntimeEnabledFeaturesOnTemplate(
      isolate, world, interfaceTemplate);
}

void V8TestTypedefs::InstallRuntimeEnabledFeaturesOnTemplate(
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

v8::Local<v8::FunctionTemplate> V8TestTypedefs::DomTemplate(v8::Isolate* isolate, const DOMWrapperWorld& world) {
  return V8DOMConfiguration::DomClassTemplate(isolate, world, const_cast<WrapperTypeInfo*>(&wrapperTypeInfo), InstallV8TestTypedefsTemplate);
}

bool V8TestTypedefs::HasInstance(v8::Local<v8::Value> v8Value, v8::Isolate* isolate) {
  return V8PerIsolateData::From(isolate)->HasInstance(&wrapperTypeInfo, v8Value);
}

v8::Local<v8::Object> V8TestTypedefs::FindInstanceInPrototypeChain(v8::Local<v8::Value> v8Value, v8::Isolate* isolate) {
  return V8PerIsolateData::From(isolate)->FindInstanceInPrototypeChain(&wrapperTypeInfo, v8Value);
}

TestTypedefs* V8TestTypedefs::ToImplWithTypeCheck(v8::Isolate* isolate, v8::Local<v8::Value> value) {
  return HasInstance(value, isolate) ? ToImpl(v8::Local<v8::Object>::Cast(value)) : nullptr;
}

TestTypedefs* NativeValueTraits<TestTypedefs>::NativeValue(v8::Isolate* isolate, v8::Local<v8::Value> value, ExceptionState& exceptionState) {
  TestTypedefs* nativeValue = V8TestTypedefs::ToImplWithTypeCheck(isolate, value);
  if (!nativeValue) {
    exceptionState.ThrowTypeError(ExceptionMessages::FailedToConvertJSValue(
        "TestTypedefs"));
  }
  return nativeValue;
}

}  // namespace blink
