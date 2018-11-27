// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/interface.cc.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "third_party/blink/renderer/bindings/tests/results/core/v8_test_object.h"

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/bindings/core/v8/binding_security.h"
#include "third_party/blink/renderer/bindings/core/v8/dictionary.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/script_call_stack.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value_factory.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_array_buffer.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_array_buffer_view.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_attr.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_document.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_document_fragment.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_document_type.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_configuration.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_element.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_event_listener_helper.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_event_target.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_float32_array.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_html_collection.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_html_element.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_int32_array.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_iterator.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_node.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_node_filter.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_shadow_root.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_test_callback_interface.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_test_dictionary.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_test_interface.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_test_interface_empty.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_test_node.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_test_object.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_uint8_array.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_window.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_xpath_ns_resolver.h"
#include "third_party/blink/renderer/core/dom/class_collection.h"
#include "third_party/blink/renderer/core/dom/tag_collection.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/deprecation.h"
#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/core/html/custom/ce_reactions_scope.h"
#include "third_party/blink/renderer/core/html/custom/v0_custom_element_processing_stack.h"
#include "third_party/blink/renderer/core/html/forms/html_data_list_options_collection.h"
#include "third_party/blink/renderer/core/html/forms/html_form_controls_collection.h"
#include "third_party/blink/renderer/core/html/html_collection.h"
#include "third_party/blink/renderer/core/html/html_table_rows_collection.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/inspector/script_arguments.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trials.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer_view_helpers.h"
#include "third_party/blink/renderer/core/typed_arrays/flexible_array_buffer_view.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/runtime_call_stats.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_dom_activity_logger.h"
#include "third_party/blink/renderer/platform/bindings/v8_object_constructor.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_context_data.h"
#include "third_party/blink/renderer/platform/bindings/v8_private_property.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/get_ptr.h"

namespace blink {

// Suppress warning: global constructors, because struct WrapperTypeInfo is trivial
// and does not depend on another global objects.
#if defined(COMPONENT_BUILD) && defined(WIN32) && defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#endif
const WrapperTypeInfo V8TestObject::wrapperTypeInfo = {
    gin::kEmbedderBlink,
    V8TestObject::DomTemplate,
    V8TestObject::InstallConditionalFeatures,
    "TestObject",
    nullptr,
    WrapperTypeInfo::kWrapperTypeObjectPrototype,
    WrapperTypeInfo::kObjectClassId,
    WrapperTypeInfo::kNotInheritFromActiveScriptWrappable,
};
#if defined(COMPONENT_BUILD) && defined(WIN32) && defined(__clang__)
#pragma clang diagnostic pop
#endif

// This static member must be declared by DEFINE_WRAPPERTYPEINFO in TestObject.h.
// For details, see the comment of DEFINE_WRAPPERTYPEINFO in
// platform/bindings/ScriptWrappable.h.
const WrapperTypeInfo& TestObject::wrapper_type_info_ = V8TestObject::wrapperTypeInfo;

// not [ActiveScriptWrappable]
static_assert(
    !std::is_base_of<ActiveScriptWrappableBase, TestObject>::value,
    "TestObject inherits from ActiveScriptWrappable<>, but is not specifying "
    "[ActiveScriptWrappable] extended attribute in the IDL file.  "
    "Be consistent.");
static_assert(
    std::is_same<decltype(&TestObject::HasPendingActivity),
                 decltype(&ScriptWrappable::HasPendingActivity)>::value,
    "TestObject is overriding hasPendingActivity(), but is not specifying "
    "[ActiveScriptWrappable] extended attribute in the IDL file.  "
    "Be consistent.");

namespace test_object_v8_internal {

static void StringifierAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueString(info, impl->stringifierAttribute(), info.GetIsolate());
}

static void StringifierAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  // Prepare the value to be set.
  V8StringResource<> cppValue = v8Value;
  if (!cppValue.Prepare())
    return;

  impl->setStringifierAttribute(cppValue);
}

static void ReadonlyStringAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueString(info, impl->readonlyStringAttribute(), info.GetIsolate());
}

static void ReadonlyTestInterfaceEmptyAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  TestInterfaceEmpty* cppValue(WTF::GetPtr(impl->readonlyTestInterfaceEmptyAttribute()));

  // Keep the wrapper object for the return value alive as long as |this|
  // object is alive in order to save creation time of the wrapper object.
  if (cppValue && DOMDataStore::SetReturnValue(info.GetReturnValue(), cppValue))
    return;
  v8::Local<v8::Value> v8Value(ToV8(cppValue, holder, info.GetIsolate()));
  V8PrivateProperty::GetSymbol(
      info.GetIsolate(), "KeepAlive#TestObject#readonlyTestInterfaceEmptyAttribute")
      .Set(holder, v8Value);

  V8SetReturnValue(info, v8Value);
}

static void ReadonlyLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueInt(info, impl->readonlyLongAttribute());
}

static void DateAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValue(info, v8::Date::New(info.GetIsolate()->GetCurrentContext(), impl->dateAttribute()));
}

static void DateAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "dateAttribute");

  // Prepare the value to be set.
  double cppValue = NativeValueTraits<IDLDate>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setDateAttribute(cppValue);
}

static void StringAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueString(info, impl->stringAttribute(), info.GetIsolate());
}

static void StringAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  // Prepare the value to be set.
  V8StringResource<> cppValue = v8Value;
  if (!cppValue.Prepare())
    return;

  impl->setStringAttribute(cppValue);
}

static void ByteStringAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueString(info, impl->byteStringAttribute(), info.GetIsolate());
}

static void ByteStringAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "byteStringAttribute");

  // Prepare the value to be set.
  V8StringResource<> cppValue = NativeValueTraits<IDLByteString>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setByteStringAttribute(cppValue);
}

static void UsvStringAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueString(info, impl->usvStringAttribute(), info.GetIsolate());
}

static void UsvStringAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "usvStringAttribute");

  // Prepare the value to be set.
  V8StringResource<> cppValue = NativeValueTraits<IDLUSVString>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setUsvStringAttribute(cppValue);
}

static void DOMTimeStampAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValue(info, static_cast<double>(impl->domTimeStampAttribute()));
}

static void DOMTimeStampAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "domTimeStampAttribute");

  // Prepare the value to be set.
  uint64_t cppValue = NativeValueTraits<IDLUnsignedLongLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setDomTimeStampAttribute(cppValue);
}

static void BooleanAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueBool(info, impl->booleanAttribute());
}

static void BooleanAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "booleanAttribute");

  // Prepare the value to be set.
  bool cppValue = NativeValueTraits<IDLBoolean>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setBooleanAttribute(cppValue);
}

static void ByteAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueInt(info, impl->byteAttribute());
}

static void ByteAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "byteAttribute");

  // Prepare the value to be set.
  int8_t cppValue = NativeValueTraits<IDLByte>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setByteAttribute(cppValue);
}

static void DoubleAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValue(info, impl->doubleAttribute());
}

static void DoubleAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "doubleAttribute");

  // Prepare the value to be set.
  double cppValue = NativeValueTraits<IDLDouble>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setDoubleAttribute(cppValue);
}

static void FloatAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValue(info, impl->floatAttribute());
}

static void FloatAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "floatAttribute");

  // Prepare the value to be set.
  float cppValue = NativeValueTraits<IDLFloat>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setFloatAttribute(cppValue);
}

static void LongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueInt(info, impl->longAttribute());
}

static void LongAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "longAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setLongAttribute(cppValue);
}

static void LongLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValue(info, static_cast<double>(impl->longLongAttribute()));
}

static void LongLongAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "longLongAttribute");

  // Prepare the value to be set.
  int64_t cppValue = NativeValueTraits<IDLLongLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setLongLongAttribute(cppValue);
}

static void OctetAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueUnsigned(info, impl->octetAttribute());
}

static void OctetAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "octetAttribute");

  // Prepare the value to be set.
  uint8_t cppValue = NativeValueTraits<IDLOctet>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setOctetAttribute(cppValue);
}

static void ShortAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueInt(info, impl->shortAttribute());
}

static void ShortAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "shortAttribute");

  // Prepare the value to be set.
  int16_t cppValue = NativeValueTraits<IDLShort>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setShortAttribute(cppValue);
}

static void UnrestrictedDoubleAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValue(info, impl->unrestrictedDoubleAttribute());
}

static void UnrestrictedDoubleAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "unrestrictedDoubleAttribute");

  // Prepare the value to be set.
  double cppValue = NativeValueTraits<IDLUnrestrictedDouble>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setUnrestrictedDoubleAttribute(cppValue);
}

static void UnrestrictedFloatAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValue(info, impl->unrestrictedFloatAttribute());
}

static void UnrestrictedFloatAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "unrestrictedFloatAttribute");

  // Prepare the value to be set.
  float cppValue = NativeValueTraits<IDLUnrestrictedFloat>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setUnrestrictedFloatAttribute(cppValue);
}

static void UnsignedLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueUnsigned(info, impl->unsignedLongAttribute());
}

static void UnsignedLongAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "unsignedLongAttribute");

  // Prepare the value to be set.
  uint32_t cppValue = NativeValueTraits<IDLUnsignedLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setUnsignedLongAttribute(cppValue);
}

static void UnsignedLongLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValue(info, static_cast<double>(impl->unsignedLongLongAttribute()));
}

static void UnsignedLongLongAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "unsignedLongLongAttribute");

  // Prepare the value to be set.
  uint64_t cppValue = NativeValueTraits<IDLUnsignedLongLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setUnsignedLongLongAttribute(cppValue);
}

static void UnsignedShortAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueUnsigned(info, impl->unsignedShortAttribute());
}

static void UnsignedShortAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "unsignedShortAttribute");

  // Prepare the value to be set.
  uint16_t cppValue = NativeValueTraits<IDLUnsignedShort>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setUnsignedShortAttribute(cppValue);
}

static void TestInterfaceEmptyAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueFast(info, WTF::GetPtr(impl->testInterfaceEmptyAttribute()), impl);
}

static void TestInterfaceEmptyAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "testInterfaceEmptyAttribute");

  // Prepare the value to be set.
  TestInterfaceEmpty* cppValue = V8TestInterfaceEmpty::ToImplWithTypeCheck(info.GetIsolate(), v8Value);

  // Type check per: http://heycam.github.io/webidl/#es-interface
  if (!cppValue) {
    exceptionState.ThrowTypeError("The provided value is not of type 'TestInterfaceEmpty'.");
    return;
  }

  impl->setTestInterfaceEmptyAttribute(cppValue);
}

static void TestObjectAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueFast(info, WTF::GetPtr(impl->testObjectAttribute()), impl);
}

static void TestObjectAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "testObjectAttribute");

  // Prepare the value to be set.
  TestObject* cppValue = V8TestObject::ToImplWithTypeCheck(info.GetIsolate(), v8Value);

  // Type check per: http://heycam.github.io/webidl/#es-interface
  if (!cppValue) {
    exceptionState.ThrowTypeError("The provided value is not of type 'TestObject'.");
    return;
  }

  impl->setTestObjectAttribute(cppValue);
}

static void CSSAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueInt(info, impl->cssAttribute());
}

static void CSSAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "cssAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setCSSAttribute(cppValue);
}

static void ImeAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueInt(info, impl->imeAttribute());
}

static void ImeAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "imeAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setIMEAttribute(cppValue);
}

static void SVGAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueInt(info, impl->svgAttribute());
}

static void SVGAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "svgAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setSVGAttribute(cppValue);
}

static void XmlAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueInt(info, impl->xmlAttribute());
}

static void XmlAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "xmlAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setXMLAttribute(cppValue);
}

static void NodeFilterAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValue(info, ToV8(WTF::GetPtr(impl->nodeFilterAttribute()), info.Holder(), info.GetIsolate()));
}

static void SerializedScriptValueAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValue(info, V8Deserialize(info.GetIsolate(), WTF::GetPtr(impl->serializedScriptValueAttribute()).get()));
}

static void SerializedScriptValueAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "serializedScriptValueAttribute");

  // Prepare the value to be set.
  scoped_refptr<SerializedScriptValue> cppValue = NativeValueTraits<SerializedScriptValue>::NativeValue(info.GetIsolate(), v8Value, SerializedScriptValue::SerializeOptions(SerializedScriptValue::kNotForStorage), exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setSerializedScriptValueAttribute(std::move(cppValue));
}

static void AnyAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValue(info, impl->anyAttribute().V8Value());
}

static void AnyAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  // Prepare the value to be set.
  ScriptValue cppValue = ScriptValue(ScriptState::Current(info.GetIsolate()), v8Value);

  impl->setAnyAttribute(cppValue);
}

static void PromiseAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  // This attribute returns a Promise.
  // Per https://heycam.github.io/webidl/#dfn-attribute-getter, all exceptions
  // must be turned into a Promise rejection.
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kGetterContext, "TestObject", "promiseAttribute");
  ExceptionToRejectPromiseScope rejectPromiseScope(info, exceptionState);

  // Returning a Promise type requires us to disable some of V8's type checks,
  // so we have to manually check that info.Holder() really points to an
  // instance of the type.
  if (!V8TestObject::HasInstance(info.Holder(), info.GetIsolate())) {
    exceptionState.ThrowTypeError("Illegal invocation");
    return;
  }

  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValue(info, impl->promiseAttribute().V8Value());
}

static void PromiseAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  // Prepare the value to be set.
  ScriptPromise cppValue = ScriptPromise::Cast(ScriptState::Current(info.GetIsolate()), v8Value);

  impl->setPromiseAttribute(cppValue);
}

static void WindowAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueFast(info, WTF::GetPtr(impl->windowAttribute()), impl);
}

static void WindowAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "windowAttribute");

  // Prepare the value to be set.
  DOMWindow* cppValue = ToDOMWindow(info.GetIsolate(), v8Value);

  // Type check per: http://heycam.github.io/webidl/#es-interface
  if (!cppValue) {
    exceptionState.ThrowTypeError("The provided value is not of type 'Window'.");
    return;
  }

  impl->setWindowAttribute(cppValue);
}

static void DocumentAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueFast(info, WTF::GetPtr(impl->documentAttribute()), impl);
}

static void DocumentAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "documentAttribute");

  // Prepare the value to be set.
  Document* cppValue = V8Document::ToImplWithTypeCheck(info.GetIsolate(), v8Value);

  // Type check per: http://heycam.github.io/webidl/#es-interface
  if (!cppValue) {
    exceptionState.ThrowTypeError("The provided value is not of type 'Document'.");
    return;
  }

  impl->setDocumentAttribute(cppValue);
}

static void DocumentFragmentAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueFast(info, WTF::GetPtr(impl->documentFragmentAttribute()), impl);
}

static void DocumentFragmentAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "documentFragmentAttribute");

  // Prepare the value to be set.
  DocumentFragment* cppValue = V8DocumentFragment::ToImplWithTypeCheck(info.GetIsolate(), v8Value);

  // Type check per: http://heycam.github.io/webidl/#es-interface
  if (!cppValue) {
    exceptionState.ThrowTypeError("The provided value is not of type 'DocumentFragment'.");
    return;
  }

  impl->setDocumentFragmentAttribute(cppValue);
}

static void DocumentTypeAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueFast(info, WTF::GetPtr(impl->documentTypeAttribute()), impl);
}

static void DocumentTypeAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "documentTypeAttribute");

  // Prepare the value to be set.
  DocumentType* cppValue = V8DocumentType::ToImplWithTypeCheck(info.GetIsolate(), v8Value);

  // Type check per: http://heycam.github.io/webidl/#es-interface
  if (!cppValue) {
    exceptionState.ThrowTypeError("The provided value is not of type 'DocumentType'.");
    return;
  }

  impl->setDocumentTypeAttribute(cppValue);
}

static void ElementAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueFast(info, WTF::GetPtr(impl->elementAttribute()), impl);
}

static void ElementAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "elementAttribute");

  // Prepare the value to be set.
  Element* cppValue = V8Element::ToImplWithTypeCheck(info.GetIsolate(), v8Value);

  // Type check per: http://heycam.github.io/webidl/#es-interface
  if (!cppValue) {
    exceptionState.ThrowTypeError("The provided value is not of type 'Element'.");
    return;
  }

  impl->setElementAttribute(cppValue);
}

static void NodeAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueFast(info, WTF::GetPtr(impl->nodeAttribute()), impl);
}

static void NodeAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "nodeAttribute");

  // Prepare the value to be set.
  Node* cppValue = V8Node::ToImplWithTypeCheck(info.GetIsolate(), v8Value);

  // Type check per: http://heycam.github.io/webidl/#es-interface
  if (!cppValue) {
    exceptionState.ThrowTypeError("The provided value is not of type 'Node'.");
    return;
  }

  impl->setNodeAttribute(cppValue);
}

static void ShadowRootAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueFast(info, WTF::GetPtr(impl->shadowRootAttribute()), impl);
}

static void ShadowRootAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "shadowRootAttribute");

  // Prepare the value to be set.
  ShadowRoot* cppValue = V8ShadowRoot::ToImplWithTypeCheck(info.GetIsolate(), v8Value);

  // Type check per: http://heycam.github.io/webidl/#es-interface
  if (!cppValue) {
    exceptionState.ThrowTypeError("The provided value is not of type 'ShadowRoot'.");
    return;
  }

  impl->setShadowRootAttribute(cppValue);
}

static void ArrayBufferAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueFast(info, WTF::GetPtr(impl->arrayBufferAttribute()), impl);
}

static void ArrayBufferAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "arrayBufferAttribute");

  // Prepare the value to be set.
  TestArrayBuffer* cppValue = v8Value->IsArrayBuffer() ? V8ArrayBuffer::ToImpl(v8::Local<v8::ArrayBuffer>::Cast(v8Value)) : 0;

  // Type check per: http://heycam.github.io/webidl/#es-interface
  if (!cppValue) {
    exceptionState.ThrowTypeError("The provided value is not of type 'ArrayBuffer'.");
    return;
  }

  impl->setArrayBufferAttribute(cppValue);
}

static void Float32ArrayAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueFast(info, impl->float32ArrayAttribute(), impl);
}

static void Float32ArrayAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "float32ArrayAttribute");

  // Prepare the value to be set.
  NotShared<DOMFloat32Array> cppValue = ToNotShared<NotShared<DOMFloat32Array>>(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  // Type check per: http://heycam.github.io/webidl/#es-interface
  if (!cppValue) {
    exceptionState.ThrowTypeError("The provided value is not of type 'Float32Array'.");
    return;
  }

  impl->setFloat32ArrayAttribute(cppValue);
}

static void Uint8ArrayAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueFast(info, impl->uint8ArrayAttribute(), impl);
}

static void Uint8ArrayAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "uint8ArrayAttribute");

  // Prepare the value to be set.
  NotShared<DOMUint8Array> cppValue = ToNotShared<NotShared<DOMUint8Array>>(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  // Type check per: http://heycam.github.io/webidl/#es-interface
  if (!cppValue) {
    exceptionState.ThrowTypeError("The provided value is not of type 'Uint8Array'.");
    return;
  }

  impl->setUint8ArrayAttribute(cppValue);
}

static void SelfAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueFast(info, WTF::GetPtr(impl->self()), impl);
}

static void ReadonlyEventTargetAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueFast(info, WTF::GetPtr(impl->readonlyEventTargetAttribute()), impl);
}

static void ReadonlyEventTargetOrNullAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueFast(info, WTF::GetPtr(impl->readonlyEventTargetOrNullAttribute()), impl);
}

static void ReadonlyWindowAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueFast(info, WTF::GetPtr(impl->readonlyWindowAttribute()), impl);
}

static void HTMLCollectionAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueFast(info, WTF::GetPtr(impl->htmlCollectionAttribute()), impl);
}

static void HTMLElementAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueFast(info, WTF::GetPtr(impl->htmlElementAttribute()), impl);
}

static void StringFrozenArrayAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValue(info, FreezeV8Object(ToV8(impl->stringFrozenArrayAttribute(), info.Holder(), info.GetIsolate()), info.GetIsolate()));
}

static void StringFrozenArrayAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "stringFrozenArrayAttribute");

  // Prepare the value to be set.
  Vector<String> cppValue = NativeValueTraits<IDLSequence<IDLString>>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setStringFrozenArrayAttribute(cppValue);
}

static void TestInterfaceEmptyFrozenArrayAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValue(info, FreezeV8Object(ToV8(impl->testInterfaceEmptyFrozenArrayAttribute(), info.Holder(), info.GetIsolate()), info.GetIsolate()));
}

static void TestInterfaceEmptyFrozenArrayAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "testInterfaceEmptyFrozenArrayAttribute");

  // Prepare the value to be set.
  HeapVector<Member<TestInterfaceEmpty>> cppValue = NativeValueTraits<IDLSequence<TestInterfaceEmpty>>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setTestInterfaceEmptyFrozenArrayAttribute(cppValue);
}

static void BooleanOrNullAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  bool isNull = false;

  bool cppValue(impl->booleanOrNullAttribute(isNull));

  if (isNull) {
    V8SetReturnValueNull(info);
    return;
  }

  V8SetReturnValueBool(info, cppValue);
}

static void BooleanOrNullAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "booleanOrNullAttribute");

  // Prepare the value to be set.
  bool cppValue = NativeValueTraits<IDLBoolean>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  bool isNull = IsUndefinedOrNull(v8Value);
  impl->setBooleanOrNullAttribute(cppValue, isNull);
}

static void StringOrNullAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueStringOrNull(info, impl->stringOrNullAttribute(), info.GetIsolate());
}

static void StringOrNullAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  // Prepare the value to be set.
  V8StringResource<kTreatNullAndUndefinedAsNullString> cppValue = v8Value;
  if (!cppValue.Prepare())
    return;

  impl->setStringOrNullAttribute(cppValue);
}

static void LongOrNullAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  bool isNull = false;

  int32_t cppValue(impl->longOrNullAttribute(isNull));

  if (isNull) {
    V8SetReturnValueNull(info);
    return;
  }

  V8SetReturnValueInt(info, cppValue);
}

static void LongOrNullAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "longOrNullAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  bool isNull = IsUndefinedOrNull(v8Value);
  impl->setLongOrNullAttribute(cppValue, isNull);
}

static void TestInterfaceOrNullAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueFast(info, WTF::GetPtr(impl->testInterfaceOrNullAttribute()), impl);
}

static void TestInterfaceOrNullAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "testInterfaceOrNullAttribute");

  // Prepare the value to be set.
  TestInterfaceImplementation* cppValue = V8TestInterface::ToImplWithTypeCheck(info.GetIsolate(), v8Value);

  // Type check per: http://heycam.github.io/webidl/#es-interface
  if (!cppValue && !IsUndefinedOrNull(v8Value)) {
    exceptionState.ThrowTypeError("The provided value is not of type 'TestInterface'.");
    return;
  }

  impl->setTestInterfaceOrNullAttribute(cppValue);
}

static void TestEnumAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueString(info, impl->testEnumAttribute(), info.GetIsolate());
}

static void TestEnumAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "testEnumAttribute");

  // Prepare the value to be set.
  V8StringResource<> cppValue = v8Value;
  if (!cppValue.Prepare())
    return;

  // Type check per: http://heycam.github.io/webidl/#dfn-attribute-setter
  // Returns undefined without setting the value if the value is invalid.
  DummyExceptionStateForTesting dummyExceptionState;
  const char* validValues[] = {
      "",
      "EnumValue1",
      "EnumValue2",
      "EnumValue3",
  };
  if (!IsValidEnum(cppValue, validValues, base::size(validValues), "TestEnum", dummyExceptionState)) {
    ExecutionContext::ForCurrentRealm(info)->AddConsoleMessage(
        ConsoleMessage::Create(kJSMessageSource, kWarningMessageLevel,
                               dummyExceptionState.Message()));
    return;
  }

  impl->setTestEnumAttribute(cppValue);
}

static void TestEnumOrNullAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueStringOrNull(info, impl->testEnumOrNullAttribute(), info.GetIsolate());
}

static void TestEnumOrNullAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "testEnumOrNullAttribute");

  // Prepare the value to be set.
  V8StringResource<kTreatNullAndUndefinedAsNullString> cppValue = v8Value;
  if (!cppValue.Prepare())
    return;

  // Type check per: http://heycam.github.io/webidl/#dfn-attribute-setter
  // Returns undefined without setting the value if the value is invalid.
  DummyExceptionStateForTesting dummyExceptionState;
  const char* validValues[] = {
      nullptr,
      "",
      "EnumValue1",
      "EnumValue2",
      "EnumValue3",
  };
  if (!IsValidEnum(cppValue, validValues, base::size(validValues), "TestEnum", dummyExceptionState)) {
    ExecutionContext::ForCurrentRealm(info)->AddConsoleMessage(
        ConsoleMessage::Create(kJSMessageSource, kWarningMessageLevel,
                               dummyExceptionState.Message()));
    return;
  }

  impl->setTestEnumOrNullAttribute(cppValue);
}

static void StaticStringAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  V8SetReturnValueString(info, TestObject::staticStringAttribute(), info.GetIsolate());
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

  TestObject::setStaticStringAttribute(cppValue);
}

static void StaticLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  V8SetReturnValueInt(info, TestObject::staticLongAttribute());
}

static void StaticLongAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "staticLongAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  TestObject::setStaticLongAttribute(cppValue);
}

static void EventHandlerAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  EventListener* cppValue(WTF::GetPtr(impl->eventHandlerAttribute()));

  V8SetReturnValue(info, JSBasedEventListener::GetListenerOrNull(info.GetIsolate(), impl, cppValue));
}

static void EventHandlerAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  // Prepare the value to be set.

  impl->setEventHandlerAttribute(V8EventListenerHelper::GetEventHandler(ScriptState::ForRelevantRealm(info), v8Value, JSEventHandler::HandlerType::kEventHandler, kListenerFindOrCreate));
}

static void DoubleOrStringAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  DoubleOrString result;
  impl->doubleOrStringAttribute(result);

  V8SetReturnValue(info, result);
}

static void DoubleOrStringAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "doubleOrStringAttribute");

  // Prepare the value to be set.
  DoubleOrString cppValue;
  V8DoubleOrString::ToImpl(info.GetIsolate(), v8Value, cppValue, UnionTypeConversionMode::kNotNullable, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setDoubleOrStringAttribute(cppValue);
}

static void DoubleOrStringOrNullAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  DoubleOrString result;
  impl->doubleOrStringOrNullAttribute(result);

  V8SetReturnValue(info, result);
}

static void DoubleOrStringOrNullAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "doubleOrStringOrNullAttribute");

  // Prepare the value to be set.
  DoubleOrString cppValue;
  V8DoubleOrString::ToImpl(info.GetIsolate(), v8Value, cppValue, UnionTypeConversionMode::kNullable, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setDoubleOrStringOrNullAttribute(cppValue);
}

static void DoubleOrNullStringAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  DoubleOrString result;
  impl->doubleOrNullStringAttribute(result);

  V8SetReturnValue(info, result);
}

static void DoubleOrNullStringAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "doubleOrNullStringAttribute");

  // Prepare the value to be set.
  DoubleOrString cppValue;
  V8DoubleOrString::ToImpl(info.GetIsolate(), v8Value, cppValue, UnionTypeConversionMode::kNullable, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setDoubleOrNullStringAttribute(cppValue);
}

static void StringOrStringSequenceAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  StringOrStringSequence result;
  impl->stringOrStringSequenceAttribute(result);

  V8SetReturnValue(info, result);
}

static void StringOrStringSequenceAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "stringOrStringSequenceAttribute");

  // Prepare the value to be set.
  StringOrStringSequence cppValue;
  V8StringOrStringSequence::ToImpl(info.GetIsolate(), v8Value, cppValue, UnionTypeConversionMode::kNotNullable, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setStringOrStringSequenceAttribute(cppValue);
}

static void TestEnumOrDoubleAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  TestEnumOrDouble result;
  impl->testEnumOrDoubleAttribute(result);

  V8SetReturnValue(info, result);
}

static void TestEnumOrDoubleAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "testEnumOrDoubleAttribute");

  // Prepare the value to be set.
  TestEnumOrDouble cppValue;
  V8TestEnumOrDouble::ToImpl(info.GetIsolate(), v8Value, cppValue, UnionTypeConversionMode::kNotNullable, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setTestEnumOrDoubleAttribute(cppValue);
}

static void UnrestrictedDoubleOrStringAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  UnrestrictedDoubleOrString result;
  impl->unrestrictedDoubleOrStringAttribute(result);

  V8SetReturnValue(info, result);
}

static void UnrestrictedDoubleOrStringAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "unrestrictedDoubleOrStringAttribute");

  // Prepare the value to be set.
  UnrestrictedDoubleOrString cppValue;
  V8UnrestrictedDoubleOrString::ToImpl(info.GetIsolate(), v8Value, cppValue, UnionTypeConversionMode::kNotNullable, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setUnrestrictedDoubleOrStringAttribute(cppValue);
}

static void NestedUnionAtributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  DoubleOrStringOrDoubleOrStringSequence result;
  impl->nestedUnionAtribute(result);

  V8SetReturnValue(info, result);
}

static void NestedUnionAtributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "nestedUnionAtribute");

  // Prepare the value to be set.
  DoubleOrStringOrDoubleOrStringSequence cppValue;
  V8DoubleOrStringOrDoubleOrStringSequence::ToImpl(info.GetIsolate(), v8Value, cppValue, UnionTypeConversionMode::kNotNullable, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setNestedUnionAtribute(cppValue);
}

static void ActivityLoggingAccessForAllWorldsLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueInt(info, impl->activityLoggingAccessForAllWorldsLongAttribute());
}

static void ActivityLoggingAccessForAllWorldsLongAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "activityLoggingAccessForAllWorldsLongAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setActivityLoggingAccessForAllWorldsLongAttribute(cppValue);
}

static void ActivityLoggingGetterForAllWorldsLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueInt(info, impl->activityLoggingGetterForAllWorldsLongAttribute());
}

static void ActivityLoggingGetterForAllWorldsLongAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "activityLoggingGetterForAllWorldsLongAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setActivityLoggingGetterForAllWorldsLongAttribute(cppValue);
}

static void ActivityLoggingSetterForAllWorldsLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueInt(info, impl->activityLoggingSetterForAllWorldsLongAttribute());
}

static void ActivityLoggingSetterForAllWorldsLongAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "activityLoggingSetterForAllWorldsLongAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setActivityLoggingSetterForAllWorldsLongAttribute(cppValue);
}

static void CachedAttributeAnyAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  // [CachedAttribute]
  V8PrivateProperty::Symbol propertySymbol =
      V8PrivateProperty::GetSymbol(info.GetIsolate(),
          "TestObject#CachedAttributeAnyAttribute");
  if (!static_cast<const TestObject*>(impl)->isValueDirty()) {
    v8::Local<v8::Value> v8Value;
    if (propertySymbol.GetOrUndefined(holder).ToLocal(&v8Value) && !v8Value->IsUndefined()) {
      V8SetReturnValue(info, v8Value);
      return;
    }
  }

  ScriptValue cppValue(impl->cachedAttributeAnyAttribute());

  // [CachedAttribute]
  v8::Local<v8::Value> v8Value(cppValue.V8Value());
  propertySymbol.Set(holder, v8Value);

  V8SetReturnValue(info, v8Value);
}

static void CachedAttributeAnyAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  // Prepare the value to be set.
  ScriptValue cppValue = ScriptValue(ScriptState::Current(info.GetIsolate()), v8Value);

  impl->setCachedAttributeAnyAttribute(cppValue);

  // [CachedAttribute]
  // Invalidate the cached value.
  V8PrivateProperty::GetSymbol(
      isolate, "TestObject#CachedAttributeAnyAttribute")
      .DeleteProperty(holder, v8::Undefined(isolate));
}

static void CachedArrayAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  // [CachedAttribute]
  V8PrivateProperty::Symbol propertySymbol =
      V8PrivateProperty::GetSymbol(info.GetIsolate(),
          "TestObject#CachedArrayAttribute");
  if (!static_cast<const TestObject*>(impl)->isFrozenArrayDirty()) {
    v8::Local<v8::Value> v8Value;
    if (propertySymbol.GetOrUndefined(holder).ToLocal(&v8Value) && !v8Value->IsUndefined()) {
      V8SetReturnValue(info, v8Value);
      return;
    }
  }

  Vector<String> cppValue(impl->cachedArrayAttribute());

  // [CachedAttribute]
  v8::Local<v8::Value> v8Value(FreezeV8Object(ToV8(cppValue, holder, info.GetIsolate()), info.GetIsolate()));
  propertySymbol.Set(holder, v8Value);

  V8SetReturnValue(info, v8Value);
}

static void CachedArrayAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "cachedArrayAttribute");

  // Prepare the value to be set.
  Vector<String> cppValue = NativeValueTraits<IDLSequence<IDLString>>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setCachedArrayAttribute(cppValue);

  // [CachedAttribute]
  // Invalidate the cached value.
  V8PrivateProperty::GetSymbol(
      isolate, "TestObject#CachedArrayAttribute")
      .DeleteProperty(holder, v8::Undefined(isolate));
}

static void CachedStringOrNoneAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  // [CachedAttribute]
  V8PrivateProperty::Symbol propertySymbol =
      V8PrivateProperty::GetSymbol(info.GetIsolate(),
          "TestObject#CachedStringOrNoneAttribute");
  if (!static_cast<const TestObject*>(impl)->isStringDirty()) {
    v8::Local<v8::Value> v8Value;
    if (propertySymbol.GetOrUndefined(holder).ToLocal(&v8Value) && !v8Value->IsUndefined()) {
      V8SetReturnValue(info, v8Value);
      return;
    }
  }

  String cppValue(impl->cachedStringOrNoneAttribute());

  // [CachedAttribute]
  v8::Local<v8::Value> v8Value((cppValue.IsNull() ? v8::Null(info.GetIsolate()).As<v8::Value>() : V8String(info.GetIsolate(), cppValue).As<v8::Value>()));
  propertySymbol.Set(holder, v8Value);

  V8SetReturnValue(info, v8Value);
}

static void CachedStringOrNoneAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  // Prepare the value to be set.
  V8StringResource<kTreatNullAndUndefinedAsNullString> cppValue = v8Value;
  if (!cppValue.Prepare())
    return;

  impl->setCachedStringOrNoneAttribute(cppValue);

  // [CachedAttribute]
  // Invalidate the cached value.
  V8PrivateProperty::GetSymbol(
      isolate, "TestObject#CachedStringOrNoneAttribute")
      .DeleteProperty(holder, v8::Undefined(isolate));
}

static void CallWithExecutionContextAnyAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExecutionContext* executionContext = ExecutionContext::ForRelevantRealm(info);

  V8SetReturnValue(info, impl->callWithExecutionContextAnyAttribute(executionContext).V8Value());
}

static void CallWithExecutionContextAnyAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  // Prepare the value to be set.
  ScriptValue cppValue = ScriptValue(ScriptState::Current(info.GetIsolate()), v8Value);

  ExecutionContext* executionContext = ExecutionContext::ForRelevantRealm(info);

  impl->setCallWithExecutionContextAnyAttribute(executionContext, cppValue);
}

static void CallWithScriptStateAnyAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);

  V8SetReturnValue(info, impl->callWithScriptStateAnyAttribute(scriptState).V8Value());
}

static void CallWithScriptStateAnyAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  // Prepare the value to be set.
  ScriptValue cppValue = ScriptValue(ScriptState::Current(info.GetIsolate()), v8Value);

  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);

  impl->setCallWithScriptStateAnyAttribute(scriptState, cppValue);
}

static void CallWithExecutionContextAndScriptStateAnyAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExecutionContext* executionContext = ExecutionContext::ForRelevantRealm(info);

  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);

  V8SetReturnValue(info, impl->callWithExecutionContextAndScriptStateAnyAttribute(scriptState, executionContext).V8Value());
}

static void CallWithExecutionContextAndScriptStateAnyAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  // Prepare the value to be set.
  ScriptValue cppValue = ScriptValue(ScriptState::Current(info.GetIsolate()), v8Value);

  ExecutionContext* executionContext = ExecutionContext::ForRelevantRealm(info);

  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);

  impl->setCallWithExecutionContextAndScriptStateAnyAttribute(scriptState, executionContext, cppValue);
}

static void CheckSecurityForNodeReadonlyDocumentAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  // Perform a security check for the returned object.
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kGetterContext, "TestObject", "checkSecurityForNodeReadonlyDocumentAttribute");
  if (!BindingSecurity::ShouldAllowAccessTo(CurrentDOMWindow(info.GetIsolate()), WTF::GetPtr(impl->checkSecurityForNodeReadonlyDocumentAttribute()), BindingSecurity::ErrorReportOption::kDoNotReport)) {
    UseCounter::Count(CurrentExecutionContext(info.GetIsolate()),
                      WebFeature::kCrossOriginTestObjectCheckSecurityForNodeReadonlyDocumentAttribute);
    V8SetReturnValueNull(info);
    return;
  }

  V8SetReturnValue(info, ToV8(WTF::GetPtr(impl->checkSecurityForNodeReadonlyDocumentAttribute()), ToV8(impl->contentWindow(), v8::Local<v8::Object>(), info.GetIsolate()).As<v8::Object>(), info.GetIsolate()));
}

static void CustomGetterLongAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "customGetterLongAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setCustomGetterLongAttribute(cppValue);
}

static void CustomSetterLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueInt(info, impl->customSetterLongAttribute());
}

static void DeprecatedLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueInt(info, impl->deprecatedLongAttribute());
}

static void DeprecatedLongAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "deprecatedLongAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setDeprecatedLongAttribute(cppValue);
}

static void EnforceRangeLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueInt(info, impl->enforceRangeLongAttribute());
}

static void EnforceRangeLongAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "enforceRangeLongAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLongEnforceRange>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setEnforceRangeLongAttribute(cppValue);
}

static void ImplementedAsLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueInt(info, impl->implementedAsName());
}

static void ImplementedAsLongAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "implementedAsLongAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setImplementedAsName(cppValue);
}

static void CustomGetterImplementedAsLongAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "customGetterImplementedAsLongAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setImplementedAsNameWithCustomGetter(cppValue);
}

static void CustomSetterImplementedAsLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueInt(info, impl->implementedAsNameWithCustomGetter());
}

static void MeasureAsLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueInt(info, impl->measureAsLongAttribute());
}

static void MeasureAsLongAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "measureAsLongAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setMeasureAsLongAttribute(cppValue);
}

static void NotEnumerableLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueInt(info, impl->notEnumerableLongAttribute());
}

static void NotEnumerableLongAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "notEnumerableLongAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setNotEnumerableLongAttribute(cppValue);
}

static void OriginTrialEnabledLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueInt(info, impl->originTrialEnabledLongAttribute());
}

static void OriginTrialEnabledLongAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "originTrialEnabledLongAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setOriginTrialEnabledLongAttribute(cppValue);
}

static void PerWorldBindingsReadonlyTestInterfaceEmptyAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  TestInterfaceEmpty* cppValue(WTF::GetPtr(impl->perWorldBindingsReadonlyTestInterfaceEmptyAttribute()));

  // Keep the wrapper object for the return value alive as long as |this|
  // object is alive in order to save creation time of the wrapper object.
  if (cppValue && DOMDataStore::SetReturnValue(info.GetReturnValue(), cppValue))
    return;
  v8::Local<v8::Value> v8Value(ToV8(cppValue, holder, info.GetIsolate()));
  V8PrivateProperty::GetSymbol(
      info.GetIsolate(), "KeepAlive#TestObject#perWorldBindingsReadonlyTestInterfaceEmptyAttribute")
      .Set(holder, v8Value);

  V8SetReturnValue(info, v8Value);
}

static void PerWorldBindingsReadonlyTestInterfaceEmptyAttributeAttributeGetterForMainWorld(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  TestInterfaceEmpty* cppValue(WTF::GetPtr(impl->perWorldBindingsReadonlyTestInterfaceEmptyAttribute()));

  // Keep the wrapper object for the return value alive as long as |this|
  // object is alive in order to save creation time of the wrapper object.
  if (cppValue && DOMDataStore::SetReturnValueForMainWorld(info.GetReturnValue(), cppValue))
    return;
  v8::Local<v8::Value> v8Value(ToV8(cppValue, holder, info.GetIsolate()));
  V8PrivateProperty::GetSymbol(
      info.GetIsolate(), "KeepAlive#TestObject#perWorldBindingsReadonlyTestInterfaceEmptyAttribute")
      .Set(holder, v8Value);

  V8SetReturnValue(info, v8Value);
}

static void ActivityLoggingAccessPerWorldBindingsLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueInt(info, impl->activityLoggingAccessPerWorldBindingsLongAttribute());
}

static void ActivityLoggingAccessPerWorldBindingsLongAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "activityLoggingAccessPerWorldBindingsLongAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setActivityLoggingAccessPerWorldBindingsLongAttribute(cppValue);
}

static void ActivityLoggingAccessPerWorldBindingsLongAttributeAttributeGetterForMainWorld(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueInt(info, impl->activityLoggingAccessPerWorldBindingsLongAttribute());
}

static void ActivityLoggingAccessPerWorldBindingsLongAttributeAttributeSetterForMainWorld(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "activityLoggingAccessPerWorldBindingsLongAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setActivityLoggingAccessPerWorldBindingsLongAttribute(cppValue);
}

static void ActivityLoggingAccessForIsolatedWorldsPerWorldBindingsLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueInt(info, impl->activityLoggingAccessForIsolatedWorldsPerWorldBindingsLongAttribute());
}

static void ActivityLoggingAccessForIsolatedWorldsPerWorldBindingsLongAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "activityLoggingAccessForIsolatedWorldsPerWorldBindingsLongAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setActivityLoggingAccessForIsolatedWorldsPerWorldBindingsLongAttribute(cppValue);
}

static void ActivityLoggingAccessForIsolatedWorldsPerWorldBindingsLongAttributeAttributeGetterForMainWorld(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueInt(info, impl->activityLoggingAccessForIsolatedWorldsPerWorldBindingsLongAttribute());
}

static void ActivityLoggingAccessForIsolatedWorldsPerWorldBindingsLongAttributeAttributeSetterForMainWorld(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "activityLoggingAccessForIsolatedWorldsPerWorldBindingsLongAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setActivityLoggingAccessForIsolatedWorldsPerWorldBindingsLongAttribute(cppValue);
}

static void ActivityLoggingGetterPerWorldBindingsLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueInt(info, impl->activityLoggingGetterPerWorldBindingsLongAttribute());
}

static void ActivityLoggingGetterPerWorldBindingsLongAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "activityLoggingGetterPerWorldBindingsLongAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setActivityLoggingGetterPerWorldBindingsLongAttribute(cppValue);
}

static void ActivityLoggingGetterPerWorldBindingsLongAttributeAttributeGetterForMainWorld(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueInt(info, impl->activityLoggingGetterPerWorldBindingsLongAttribute());
}

static void ActivityLoggingGetterPerWorldBindingsLongAttributeAttributeSetterForMainWorld(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "activityLoggingGetterPerWorldBindingsLongAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setActivityLoggingGetterPerWorldBindingsLongAttribute(cppValue);
}

static void ActivityLoggingGetterForIsolatedWorldsPerWorldBindingsLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueInt(info, impl->activityLoggingGetterForIsolatedWorldsPerWorldBindingsLongAttribute());
}

static void ActivityLoggingGetterForIsolatedWorldsPerWorldBindingsLongAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "activityLoggingGetterForIsolatedWorldsPerWorldBindingsLongAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setActivityLoggingGetterForIsolatedWorldsPerWorldBindingsLongAttribute(cppValue);
}

static void ActivityLoggingGetterForIsolatedWorldsPerWorldBindingsLongAttributeAttributeGetterForMainWorld(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueInt(info, impl->activityLoggingGetterForIsolatedWorldsPerWorldBindingsLongAttribute());
}

static void ActivityLoggingGetterForIsolatedWorldsPerWorldBindingsLongAttributeAttributeSetterForMainWorld(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "activityLoggingGetterForIsolatedWorldsPerWorldBindingsLongAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setActivityLoggingGetterForIsolatedWorldsPerWorldBindingsLongAttribute(cppValue);
}

static void LocationAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueFast(info, WTF::GetPtr(impl->location()), impl);
}

static void LocationAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  // [PutForwards] => location.href
  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "location");
  v8::Local<v8::Value> target;
  if (!holder->Get(isolate->GetCurrentContext(), V8AtomicString(isolate, "location")).ToLocal(&target))
    return;
  if (!target->IsObject()) {
    exceptionState.ThrowTypeError("The attribute value is not an object");
    return;
  }
  bool result;
  if (!target.As<v8::Object>()->Set(isolate->GetCurrentContext(), V8AtomicString(isolate, "href"), v8Value).To(&result))
    return;
  if (!result)
    return;
}

static void LocationWithExceptionAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueFast(info, WTF::GetPtr(impl->locationWithException()), impl);
}

static void LocationWithExceptionAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  // [PutForwards] => locationWithException.hrefThrows
  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "locationWithException");
  v8::Local<v8::Value> target;
  if (!holder->Get(isolate->GetCurrentContext(), V8AtomicString(isolate, "locationWithException")).ToLocal(&target))
    return;
  if (!target->IsObject()) {
    exceptionState.ThrowTypeError("The attribute value is not an object");
    return;
  }
  bool result;
  if (!target.As<v8::Object>()->Set(isolate->GetCurrentContext(), V8AtomicString(isolate, "hrefThrows"), v8Value).To(&result))
    return;
  if (!result)
    return;
}

static void LocationWithCallWithAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueFast(info, WTF::GetPtr(impl->locationWithCallWith()), impl);
}

static void LocationWithCallWithAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  // [PutForwards] => locationWithCallWith.hrefCallWith
  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "locationWithCallWith");
  v8::Local<v8::Value> target;
  if (!holder->Get(isolate->GetCurrentContext(), V8AtomicString(isolate, "locationWithCallWith")).ToLocal(&target))
    return;
  if (!target->IsObject()) {
    exceptionState.ThrowTypeError("The attribute value is not an object");
    return;
  }
  bool result;
  if (!target.As<v8::Object>()->Set(isolate->GetCurrentContext(), V8AtomicString(isolate, "hrefCallWith"), v8Value).To(&result))
    return;
  if (!result)
    return;
}

static void LocationByteStringAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueFast(info, WTF::GetPtr(impl->locationByteString()), impl);
}

static void LocationByteStringAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  // [PutForwards] => locationByteString.hrefByteString
  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "locationByteString");
  v8::Local<v8::Value> target;
  if (!holder->Get(isolate->GetCurrentContext(), V8AtomicString(isolate, "locationByteString")).ToLocal(&target))
    return;
  if (!target->IsObject()) {
    exceptionState.ThrowTypeError("The attribute value is not an object");
    return;
  }
  bool result;
  if (!target.As<v8::Object>()->Set(isolate->GetCurrentContext(), V8AtomicString(isolate, "hrefByteString"), v8Value).To(&result))
    return;
  if (!result)
    return;
}

static void LocationWithPerWorldBindingsAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueFast(info, WTF::GetPtr(impl->locationWithPerWorldBindings()), impl);
}

static void LocationWithPerWorldBindingsAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  // [PutForwards] => locationWithPerWorldBindings.href
  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "locationWithPerWorldBindings");
  v8::Local<v8::Value> target;
  if (!holder->Get(isolate->GetCurrentContext(), V8AtomicString(isolate, "locationWithPerWorldBindings")).ToLocal(&target))
    return;
  if (!target->IsObject()) {
    exceptionState.ThrowTypeError("The attribute value is not an object");
    return;
  }
  bool result;
  if (!target.As<v8::Object>()->Set(isolate->GetCurrentContext(), V8AtomicString(isolate, "href"), v8Value).To(&result))
    return;
  if (!result)
    return;
}

static void LocationWithPerWorldBindingsAttributeGetterForMainWorld(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueForMainWorld(info, WTF::GetPtr(impl->locationWithPerWorldBindings()));
}

static void LocationWithPerWorldBindingsAttributeSetterForMainWorld(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  // [PutForwards] => locationWithPerWorldBindings.href
  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "locationWithPerWorldBindings");
  v8::Local<v8::Value> target;
  if (!holder->Get(isolate->GetCurrentContext(), V8AtomicString(isolate, "locationWithPerWorldBindings")).ToLocal(&target))
    return;
  if (!target->IsObject()) {
    exceptionState.ThrowTypeError("The attribute value is not an object");
    return;
  }
  bool result;
  if (!target.As<v8::Object>()->Set(isolate->GetCurrentContext(), V8AtomicString(isolate, "href"), v8Value).To(&result))
    return;
  if (!result)
    return;
}

static void LocationLegacyInterfaceTypeCheckingAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueFast(info, WTF::GetPtr(impl->locationLegacyInterfaceTypeChecking()), impl);
}

static void LocationLegacyInterfaceTypeCheckingAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  // [PutForwards] => locationLegacyInterfaceTypeChecking.href
  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "locationLegacyInterfaceTypeChecking");
  v8::Local<v8::Value> target;
  if (!holder->Get(isolate->GetCurrentContext(), V8AtomicString(isolate, "locationLegacyInterfaceTypeChecking")).ToLocal(&target))
    return;
  if (!target->IsObject()) {
    exceptionState.ThrowTypeError("The attribute value is not an object");
    return;
  }
  bool result;
  if (!target.As<v8::Object>()->Set(isolate->GetCurrentContext(), V8AtomicString(isolate, "href"), v8Value).To(&result))
    return;
  if (!result)
    return;
}

static void RaisesExceptionLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kGetterContext, "TestObject", "raisesExceptionLongAttribute");

  int32_t cppValue(impl->raisesExceptionLongAttribute(exceptionState));

  if (UNLIKELY(exceptionState.HadException()))
    return;

  V8SetReturnValueInt(info, cppValue);
}

static void RaisesExceptionLongAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "raisesExceptionLongAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setRaisesExceptionLongAttribute(cppValue, exceptionState);
}

static void RaisesExceptionGetterLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kGetterContext, "TestObject", "raisesExceptionGetterLongAttribute");

  int32_t cppValue(impl->raisesExceptionGetterLongAttribute(exceptionState));

  if (UNLIKELY(exceptionState.HadException()))
    return;

  V8SetReturnValueInt(info, cppValue);
}

static void RaisesExceptionGetterLongAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "raisesExceptionGetterLongAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setRaisesExceptionGetterLongAttribute(cppValue);
}

static void SetterRaisesExceptionLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueInt(info, impl->setterRaisesExceptionLongAttribute());
}

static void SetterRaisesExceptionLongAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "setterRaisesExceptionLongAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setSetterRaisesExceptionLongAttribute(cppValue, exceptionState);
}

static void RaisesExceptionTestInterfaceEmptyAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kGetterContext, "TestObject", "raisesExceptionTestInterfaceEmptyAttribute");

  TestInterfaceEmpty* cppValue(impl->raisesExceptionTestInterfaceEmptyAttribute(exceptionState));

  if (UNLIKELY(exceptionState.HadException()))
    return;

  V8SetReturnValueFast(info, cppValue, impl);
}

static void RaisesExceptionTestInterfaceEmptyAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "raisesExceptionTestInterfaceEmptyAttribute");

  // Prepare the value to be set.
  TestInterfaceEmpty* cppValue = V8TestInterfaceEmpty::ToImplWithTypeCheck(info.GetIsolate(), v8Value);

  // Type check per: http://heycam.github.io/webidl/#es-interface
  if (!cppValue) {
    exceptionState.ThrowTypeError("The provided value is not of type 'TestInterfaceEmpty'.");
    return;
  }

  impl->setRaisesExceptionTestInterfaceEmptyAttribute(cppValue, exceptionState);
}

static void CachedAttributeRaisesExceptionGetterAnyAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  // [CachedAttribute]
  V8PrivateProperty::Symbol propertySymbol =
      V8PrivateProperty::GetSymbol(info.GetIsolate(),
          "TestObject#CachedAttributeRaisesExceptionGetterAnyAttribute");
  if (!static_cast<const TestObject*>(impl)->isValueDirty()) {
    v8::Local<v8::Value> v8Value;
    if (propertySymbol.GetOrUndefined(holder).ToLocal(&v8Value) && !v8Value->IsUndefined()) {
      V8SetReturnValue(info, v8Value);
      return;
    }
  }

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kGetterContext, "TestObject", "cachedAttributeRaisesExceptionGetterAnyAttribute");

  ScriptValue cppValue(impl->cachedAttributeRaisesExceptionGetterAnyAttribute(exceptionState));

  if (UNLIKELY(exceptionState.HadException()))
    return;

  // [CachedAttribute]
  v8::Local<v8::Value> v8Value(cppValue.V8Value());
  propertySymbol.Set(holder, v8Value);

  V8SetReturnValue(info, v8Value);
}

static void CachedAttributeRaisesExceptionGetterAnyAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "cachedAttributeRaisesExceptionGetterAnyAttribute");

  // Prepare the value to be set.
  ScriptValue cppValue = ScriptValue(ScriptState::Current(info.GetIsolate()), v8Value);

  impl->setCachedAttributeRaisesExceptionGetterAnyAttribute(cppValue, exceptionState);

  // [CachedAttribute]
  // Invalidate the cached value.
  V8PrivateProperty::GetSymbol(
      isolate, "TestObject#CachedAttributeRaisesExceptionGetterAnyAttribute")
      .DeleteProperty(holder, v8::Undefined(isolate));
}

static void ReflectTestInterfaceAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueFast(info, impl->FastGetAttribute(html_names::kReflecttestinterfaceattributeAttr), impl);
}

static void ReflectTestInterfaceAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  V0CustomElementProcessingStack::CallbackDeliveryScope deliveryScope;

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "reflectTestInterfaceAttribute");

  // Prepare the value to be set.
  TestInterfaceImplementation* cppValue = V8TestInterface::ToImplWithTypeCheck(info.GetIsolate(), v8Value);

  // Type check per: http://heycam.github.io/webidl/#es-interface
  if (!cppValue) {
    exceptionState.ThrowTypeError("The provided value is not of type 'TestInterface'.");
    return;
  }

  impl->setAttribute(html_names::kReflecttestinterfaceattributeAttr, cppValue);
}

static void ReflectReflectedNameAttributeTestAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueFast(info, impl->FastGetAttribute(html_names::kReflectedNameAttributeAttr), impl);
}

static void ReflectReflectedNameAttributeTestAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  V0CustomElementProcessingStack::CallbackDeliveryScope deliveryScope;

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "reflectReflectedNameAttributeTestAttribute");

  // Prepare the value to be set.
  TestInterfaceImplementation* cppValue = V8TestInterface::ToImplWithTypeCheck(info.GetIsolate(), v8Value);

  // Type check per: http://heycam.github.io/webidl/#es-interface
  if (!cppValue) {
    exceptionState.ThrowTypeError("The provided value is not of type 'TestInterface'.");
    return;
  }

  impl->setAttribute(html_names::kReflectedNameAttributeAttr, cppValue);
}

static void ReflectBooleanAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueBool(info, impl->FastHasAttribute(html_names::kReflectbooleanattributeAttr));
}

static void ReflectBooleanAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  V0CustomElementProcessingStack::CallbackDeliveryScope deliveryScope;

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "reflectBooleanAttribute");

  // Prepare the value to be set.
  bool cppValue = NativeValueTraits<IDLBoolean>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->SetBooleanAttribute(html_names::kReflectbooleanattributeAttr, cppValue);
}

static void ReflectLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueInt(info, impl->GetIntegralAttribute(html_names::kReflectlongattributeAttr));
}

static void ReflectLongAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  V0CustomElementProcessingStack::CallbackDeliveryScope deliveryScope;

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "reflectLongAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->SetIntegralAttribute(html_names::kReflectlongattributeAttr, cppValue);
}

static void ReflectUnsignedShortAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueUnsigned(info, std::max(0, static_cast<int>(impl->FastGetAttribute(html_names::kReflectunsignedshortattributeAttr))));
}

static void ReflectUnsignedShortAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  V0CustomElementProcessingStack::CallbackDeliveryScope deliveryScope;

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "reflectUnsignedShortAttribute");

  // Prepare the value to be set.
  uint16_t cppValue = NativeValueTraits<IDLUnsignedShort>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setAttribute(html_names::kReflectunsignedshortattributeAttr, cppValue);
}

static void ReflectUnsignedLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueUnsigned(info, std::max(0, static_cast<int>(impl->GetIntegralAttribute(html_names::kReflectunsignedlongattributeAttr))));
}

static void ReflectUnsignedLongAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  V0CustomElementProcessingStack::CallbackDeliveryScope deliveryScope;

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "reflectUnsignedLongAttribute");

  // Prepare the value to be set.
  uint32_t cppValue = NativeValueTraits<IDLUnsignedLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->SetUnsignedIntegralAttribute(html_names::kReflectunsignedlongattributeAttr, cppValue);
}

static void IdAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueString(info, impl->GetIdAttribute(), info.GetIsolate());
}

static void IdAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  V0CustomElementProcessingStack::CallbackDeliveryScope deliveryScope;

  // Prepare the value to be set.
  V8StringResource<> cppValue = v8Value;
  if (!cppValue.Prepare())
    return;

  impl->setAttribute(html_names::kIdAttr, cppValue);
}

static void NameAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueString(info, impl->GetNameAttribute(), info.GetIsolate());
}

static void NameAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  V0CustomElementProcessingStack::CallbackDeliveryScope deliveryScope;

  // Prepare the value to be set.
  V8StringResource<> cppValue = v8Value;
  if (!cppValue.Prepare())
    return;

  impl->setAttribute(html_names::kNameAttr, cppValue);
}

static void ClassAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueString(info, impl->GetClassAttribute(), info.GetIsolate());
}

static void ClassAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  V0CustomElementProcessingStack::CallbackDeliveryScope deliveryScope;

  // Prepare the value to be set.
  V8StringResource<> cppValue = v8Value;
  if (!cppValue.Prepare())
    return;

  impl->setAttribute(html_names::kClassAttr, cppValue);
}

static void ReflectedIdAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueString(info, impl->GetIdAttribute(), info.GetIsolate());
}

static void ReflectedIdAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  V0CustomElementProcessingStack::CallbackDeliveryScope deliveryScope;

  // Prepare the value to be set.
  V8StringResource<> cppValue = v8Value;
  if (!cppValue.Prepare())
    return;

  impl->setAttribute(html_names::kIdAttr, cppValue);
}

static void ReflectedNameAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueString(info, impl->GetNameAttribute(), info.GetIsolate());
}

static void ReflectedNameAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  V0CustomElementProcessingStack::CallbackDeliveryScope deliveryScope;

  // Prepare the value to be set.
  V8StringResource<> cppValue = v8Value;
  if (!cppValue.Prepare())
    return;

  impl->setAttribute(html_names::kNameAttr, cppValue);
}

static void ReflectedClassAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueString(info, impl->GetClassAttribute(), info.GetIsolate());
}

static void ReflectedClassAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  V0CustomElementProcessingStack::CallbackDeliveryScope deliveryScope;

  // Prepare the value to be set.
  V8StringResource<> cppValue = v8Value;
  if (!cppValue.Prepare())
    return;

  impl->setAttribute(html_names::kClassAttr, cppValue);
}

static void LimitedToOnlyOneAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  String cppValue(impl->FastGetAttribute(html_names::kLimitedtoonlyoneattributeAttr));

  if (cppValue.IsEmpty()) {
    ;
  } else if (EqualIgnoringASCIICase(cppValue, "unique")) {
    cppValue = "unique";
  } else {
    cppValue = "";
  }

  V8SetReturnValueString(info, cppValue, info.GetIsolate());
}

static void LimitedToOnlyOneAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  V0CustomElementProcessingStack::CallbackDeliveryScope deliveryScope;

  // Prepare the value to be set.
  V8StringResource<> cppValue = v8Value;
  if (!cppValue.Prepare())
    return;

  impl->setAttribute(html_names::kLimitedtoonlyoneattributeAttr, cppValue);
}

static void LimitedToOnlyAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  String cppValue(impl->FastGetAttribute(html_names::kLimitedtoonlyattributeAttr));

  if (cppValue.IsEmpty()) {
    ;
  } else if (EqualIgnoringASCIICase(cppValue, "Per")) {
    cppValue = "Per";
  } else if (EqualIgnoringASCIICase(cppValue, "Paal")) {
    cppValue = "Paal";
  } else if (EqualIgnoringASCIICase(cppValue, "Espen")) {
    cppValue = "Espen";
  } else {
    cppValue = "";
  }

  V8SetReturnValueString(info, cppValue, info.GetIsolate());
}

static void LimitedToOnlyAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  V0CustomElementProcessingStack::CallbackDeliveryScope deliveryScope;

  // Prepare the value to be set.
  V8StringResource<> cppValue = v8Value;
  if (!cppValue.Prepare())
    return;

  impl->setAttribute(html_names::kLimitedtoonlyattributeAttr, cppValue);
}

static void LimitedToOnlyOtherAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  String cppValue(impl->FastGetAttribute(html_names::kOtherAttr));

  if (cppValue.IsEmpty()) {
    ;
  } else if (EqualIgnoringASCIICase(cppValue, "Value1")) {
    cppValue = "Value1";
  } else if (EqualIgnoringASCIICase(cppValue, "Value2")) {
    cppValue = "Value2";
  } else {
    cppValue = "";
  }

  V8SetReturnValueString(info, cppValue, info.GetIsolate());
}

static void LimitedToOnlyOtherAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  V0CustomElementProcessingStack::CallbackDeliveryScope deliveryScope;

  // Prepare the value to be set.
  V8StringResource<> cppValue = v8Value;
  if (!cppValue.Prepare())
    return;

  impl->setAttribute(html_names::kOtherAttr, cppValue);
}

static void LimitedWithMissingDefaultAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  String cppValue(impl->FastGetAttribute(html_names::kLimitedwithmissingdefaultattributeAttr));

  if (cppValue.IsEmpty()) {
    cppValue = "rsa";
  } else if (EqualIgnoringASCIICase(cppValue, "rsa")) {
    cppValue = "rsa";
  } else if (EqualIgnoringASCIICase(cppValue, "dsa")) {
    cppValue = "dsa";
  } else {
    cppValue = "";
  }

  V8SetReturnValueString(info, cppValue, info.GetIsolate());
}

static void LimitedWithMissingDefaultAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  V0CustomElementProcessingStack::CallbackDeliveryScope deliveryScope;

  // Prepare the value to be set.
  V8StringResource<> cppValue = v8Value;
  if (!cppValue.Prepare())
    return;

  impl->setAttribute(html_names::kLimitedwithmissingdefaultattributeAttr, cppValue);
}

static void LimitedWithInvalidMissingDefaultAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  String cppValue(impl->FastGetAttribute(html_names::kLimitedwithinvalidmissingdefaultattributeAttr));

  if (cppValue.IsEmpty()) {
    cppValue = "auto";
  } else if (EqualIgnoringASCIICase(cppValue, "ltr")) {
    cppValue = "ltr";
  } else if (EqualIgnoringASCIICase(cppValue, "rtl")) {
    cppValue = "rtl";
  } else if (EqualIgnoringASCIICase(cppValue, "auto")) {
    cppValue = "auto";
  } else {
    cppValue = "ltr";
  }

  V8SetReturnValueString(info, cppValue, info.GetIsolate());
}

static void LimitedWithInvalidMissingDefaultAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  V0CustomElementProcessingStack::CallbackDeliveryScope deliveryScope;

  // Prepare the value to be set.
  V8StringResource<> cppValue = v8Value;
  if (!cppValue.Prepare())
    return;

  impl->setAttribute(html_names::kLimitedwithinvalidmissingdefaultattributeAttr, cppValue);
}

static void CorsSettingAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  String cppValue(impl->FastGetAttribute(html_names::kCorssettingattributeAttr));

  if (cppValue.IsNull()) {
    ;
  } else if (cppValue.IsEmpty()) {
    cppValue = "anonymous";
  } else if (EqualIgnoringASCIICase(cppValue, "anonymous")) {
    cppValue = "anonymous";
  } else if (EqualIgnoringASCIICase(cppValue, "use-credentials")) {
    cppValue = "use-credentials";
  } else {
    cppValue = "anonymous";
  }

  V8SetReturnValueString(info, cppValue, info.GetIsolate());
}

static void LimitedWithEmptyMissingInvalidAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  String cppValue(impl->FastGetAttribute(html_names::kLimitedwithemptymissinginvalidattributeAttr));

  if (cppValue.IsNull()) {
    cppValue = "missing";
  } else if (cppValue.IsEmpty()) {
    cppValue = "empty";
  } else if (EqualIgnoringASCIICase(cppValue, "empty")) {
    cppValue = "empty";
  } else if (EqualIgnoringASCIICase(cppValue, "missing")) {
    cppValue = "missing";
  } else if (EqualIgnoringASCIICase(cppValue, "invalid")) {
    cppValue = "invalid";
  } else if (EqualIgnoringASCIICase(cppValue, "a-normal")) {
    cppValue = "a-normal";
  } else {
    cppValue = "invalid";
  }

  V8SetReturnValueString(info, cppValue, info.GetIsolate());
}

static void RuntimeCallStatsCounterAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueInt(info, impl->runtimeCallStatsCounterAttribute());
}

static void RuntimeCallStatsCounterAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "RuntimeCallStatsCounterAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setRuntimeCallStatsCounterAttribute(cppValue);
}

static void RuntimeCallStatsCounterReadOnlyAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValue(info, impl->runtimeCallStatsCounterReadOnlyAttribute());
}

static void ReplaceableReadonlyLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueInt(info, impl->replaceableReadonlyLongAttribute());
}

static void ReplaceableReadonlyLongAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  // Prepare the value to be set.

  v8::Local<v8::String> propertyName = V8AtomicString(isolate, "replaceableReadonlyLongAttribute");
  if (info.Holder()->CreateDataProperty(info.GetIsolate()->GetCurrentContext(), propertyName, v8Value).IsNothing())
    return;
}

static void LocationPutForwardsAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueFast(info, WTF::GetPtr(impl->locationPutForwards()), impl);
}

static void LocationPutForwardsAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  // [PutForwards] => locationPutForwards.href
  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "locationPutForwards");
  v8::Local<v8::Value> target;
  if (!holder->Get(isolate->GetCurrentContext(), V8AtomicString(isolate, "locationPutForwards")).ToLocal(&target))
    return;
  if (!target->IsObject()) {
    exceptionState.ThrowTypeError("The attribute value is not an object");
    return;
  }
  bool result;
  if (!target.As<v8::Object>()->Set(isolate->GetCurrentContext(), V8AtomicString(isolate, "href"), v8Value).To(&result))
    return;
  if (!result)
    return;
}

static void RuntimeEnabledLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueInt(info, impl->runtimeEnabledLongAttribute());
}

static void RuntimeEnabledLongAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "runtimeEnabledLongAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setRuntimeEnabledLongAttribute(cppValue);
}

static void SetterCallWithCurrentWindowAndEnteredWindowStringAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueString(info, impl->setterCallWithCurrentWindowAndEnteredWindowStringAttribute(), info.GetIsolate());
}

static void SetterCallWithCurrentWindowAndEnteredWindowStringAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  // Prepare the value to be set.
  V8StringResource<> cppValue = v8Value;
  if (!cppValue.Prepare())
    return;

  impl->setSetterCallWithCurrentWindowAndEnteredWindowStringAttribute(CurrentDOMWindow(info.GetIsolate()), EnteredDOMWindow(info.GetIsolate()), cppValue);
}

static void SetterCallWithExecutionContextStringAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueString(info, impl->setterCallWithExecutionContextStringAttribute(), info.GetIsolate());
}

static void SetterCallWithExecutionContextStringAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  // Prepare the value to be set.
  V8StringResource<> cppValue = v8Value;
  if (!cppValue.Prepare())
    return;

  ExecutionContext* executionContext = ExecutionContext::ForRelevantRealm(info);

  impl->setSetterCallWithExecutionContextStringAttribute(executionContext, cppValue);
}

static void TreatNullAsEmptyStringStringAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueString(info, impl->treatNullAsEmptyStringStringAttribute(), info.GetIsolate());
}

static void TreatNullAsEmptyStringStringAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  // Prepare the value to be set.
  V8StringResource<kTreatNullAsEmptyString> cppValue = v8Value;
  if (!cppValue.Prepare())
    return;

  impl->setTreatNullAsEmptyStringStringAttribute(cppValue);
}

static void LegacyInterfaceTypeCheckingFloatAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValue(info, impl->legacyInterfaceTypeCheckingFloatAttribute());
}

static void LegacyInterfaceTypeCheckingFloatAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "legacyInterfaceTypeCheckingFloatAttribute");

  // Prepare the value to be set.
  float cppValue = NativeValueTraits<IDLFloat>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setLegacyInterfaceTypeCheckingFloatAttribute(cppValue);
}

static void LegacyInterfaceTypeCheckingTestInterfaceAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueFast(info, WTF::GetPtr(impl->legacyInterfaceTypeCheckingTestInterfaceAttribute()), impl);
}

static void LegacyInterfaceTypeCheckingTestInterfaceAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  // Prepare the value to be set.
  TestInterfaceImplementation* cppValue = V8TestInterface::ToImplWithTypeCheck(info.GetIsolate(), v8Value);

  impl->setLegacyInterfaceTypeCheckingTestInterfaceAttribute(cppValue);
}

static void LegacyInterfaceTypeCheckingTestInterfaceOrNullAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueFast(info, WTF::GetPtr(impl->legacyInterfaceTypeCheckingTestInterfaceOrNullAttribute()), impl);
}

static void LegacyInterfaceTypeCheckingTestInterfaceOrNullAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  // Prepare the value to be set.
  TestInterfaceImplementation* cppValue = V8TestInterface::ToImplWithTypeCheck(info.GetIsolate(), v8Value);

  impl->setLegacyInterfaceTypeCheckingTestInterfaceOrNullAttribute(cppValue);
}

static void UrlStringAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueString(info, impl->GetURLAttribute(html_names::kUrlstringattributeAttr), info.GetIsolate());
}

static void UrlStringAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  V0CustomElementProcessingStack::CallbackDeliveryScope deliveryScope;

  // Prepare the value to be set.
  V8StringResource<> cppValue = v8Value;
  if (!cppValue.Prepare())
    return;

  impl->setAttribute(html_names::kUrlstringattributeAttr, cppValue);
}

static void UrlStringAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueString(info, impl->GetURLAttribute(html_names::kReflectUrlAttributeAttr), info.GetIsolate());
}

static void UrlStringAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  V0CustomElementProcessingStack::CallbackDeliveryScope deliveryScope;

  // Prepare the value to be set.
  V8StringResource<> cppValue = v8Value;
  if (!cppValue.Prepare())
    return;

  impl->setAttribute(html_names::kReflectUrlAttributeAttr, cppValue);
}

static void UnforgeableLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueInt(info, impl->unforgeableLongAttribute());
}

static void UnforgeableLongAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "unforgeableLongAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setUnforgeableLongAttribute(cppValue);
}

static void MeasuredLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueInt(info, impl->measuredLongAttribute());
}

static void MeasuredLongAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "measuredLongAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setMeasuredLongAttribute(cppValue);
}

static void SameObjectAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  TestInterfaceImplementation* cppValue(WTF::GetPtr(impl->sameObjectAttribute()));

  // Keep the wrapper object for the return value alive as long as |this|
  // object is alive in order to save creation time of the wrapper object.
  if (cppValue && DOMDataStore::SetReturnValue(info.GetReturnValue(), cppValue))
    return;
  v8::Local<v8::Value> v8Value(ToV8(cppValue, holder, info.GetIsolate()));
  V8PrivateProperty::GetSymbol(
      info.GetIsolate(), "KeepAlive#TestObject#sameObjectAttribute")
      .Set(holder, v8Value);

  V8SetReturnValue(info, v8Value);
}

static void SaveSameObjectAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  // [SaveSameObject]
  // If you see a compile error that
  //   V8PrivateProperty::GetSameObjectTestObjectSaveSameObjectAttribute
  // is not defined, then you need to register your attribute at
  // V8_PRIVATE_PROPERTY_FOR_EACH defined in V8PrivateProperty.h as
  //   X(SameObject, TestObjectSaveSameObjectAttribute)
  auto privateSameObject = V8PrivateProperty::GetSameObjectTestObjectSaveSameObjectAttribute(info.GetIsolate());
  {
    v8::Local<v8::Value> v8Value;
    if (privateSameObject.GetOrUndefined(holder).ToLocal(&v8Value) && !v8Value->IsUndefined()) {
      V8SetReturnValue(info, v8Value);
      return;
    }
  }

  TestObject* impl = V8TestObject::ToImpl(holder);

  TestInterfaceImplementation* cppValue(WTF::GetPtr(impl->saveSameObjectAttribute()));

  // Keep the wrapper object for the return value alive as long as |this|
  // object is alive in order to save creation time of the wrapper object.
  if (cppValue && DOMDataStore::SetReturnValue(info.GetReturnValue(), cppValue))
    return;
  v8::Local<v8::Value> v8Value(ToV8(cppValue, holder, info.GetIsolate()));
  V8PrivateProperty::GetSymbol(
      info.GetIsolate(), "KeepAlive#TestObject#saveSameObjectAttribute")
      .Set(holder, v8Value);

  V8SetReturnValue(info, v8Value);

  // [SaveSameObject]
  privateSameObject.Set(holder, info.GetReturnValue().Get());
}

static void StaticSaveSameObjectAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  // [SaveSameObject]
  // If you see a compile error that
  //   V8PrivateProperty::GetSameObjectTestObjectStaticSaveSameObjectAttribute
  // is not defined, then you need to register your attribute at
  // V8_PRIVATE_PROPERTY_FOR_EACH defined in V8PrivateProperty.h as
  //   X(SameObject, TestObjectStaticSaveSameObjectAttribute)
  auto privateSameObject = V8PrivateProperty::GetSameObjectTestObjectStaticSaveSameObjectAttribute(info.GetIsolate());
  {
    v8::Local<v8::Value> v8Value;
    if (privateSameObject.GetOrUndefined(holder).ToLocal(&v8Value) && !v8Value->IsUndefined()) {
      V8SetReturnValue(info, v8Value);
      return;
    }
  }

  V8SetReturnValue(info, WTF::GetPtr(TestObject::staticSaveSameObjectAttribute()), info.GetIsolate()->GetCurrentContext()->Global());

  // [SaveSameObject]
  privateSameObject.Set(holder, info.GetReturnValue().Get());
}

static void UnscopableLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueInt(info, impl->unscopableLongAttribute());
}

static void UnscopableLongAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "unscopableLongAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setUnscopableLongAttribute(cppValue);
}

static void UnscopableOriginTrialEnabledLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueInt(info, impl->unscopableOriginTrialEnabledLongAttribute());
}

static void UnscopableOriginTrialEnabledLongAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "unscopableOriginTrialEnabledLongAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setUnscopableOriginTrialEnabledLongAttribute(cppValue);
}

static void UnscopableRuntimeEnabledLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueInt(info, impl->unscopableRuntimeEnabledLongAttribute());
}

static void UnscopableRuntimeEnabledLongAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "unscopableRuntimeEnabledLongAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setUnscopableRuntimeEnabledLongAttribute(cppValue);
}

static void TestInterfaceAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueFast(info, WTF::GetPtr(impl->testInterfaceAttribute()), impl);
}

static void TestInterfaceAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestObject* impl = V8TestObject::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestObject", "testInterfaceAttribute");

  // Prepare the value to be set.
  TestInterfaceImplementation* cppValue = V8TestInterface::ToImplWithTypeCheck(info.GetIsolate(), v8Value);

  // Type check per: http://heycam.github.io/webidl/#es-interface
  if (!cppValue) {
    exceptionState.ThrowTypeError("The provided value is not of type 'TestInterface'.");
    return;
  }

  impl->setTestInterfaceAttribute(cppValue);
}

static void SizeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestObject* impl = V8TestObject::ToImpl(holder);

  V8SetReturnValueUnsigned(info, impl->size());
}

static void UnscopableVoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  impl->unscopableVoidMethod();
}

static void UnscopableRuntimeEnabledVoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  impl->unscopableRuntimeEnabledVoidMethod();
}

static void VoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  impl->voidMethod();
}

static void StaticVoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject::staticVoidMethod();
}

static void DateMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8SetReturnValue(info, v8::Date::New(info.GetIsolate()->GetCurrentContext(), impl->dateMethod()));
}

static void StringMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8SetReturnValueString(info, impl->stringMethod(), info.GetIsolate());
}

static void ByteStringMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8SetReturnValueString(info, impl->byteStringMethod(), info.GetIsolate());
}

static void UsvStringMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8SetReturnValueString(info, impl->usvStringMethod(), info.GetIsolate());
}

static void ReadonlyDOMTimeStampMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8SetReturnValue(info, static_cast<double>(impl->readonlyDOMTimeStampMethod()));
}

static void BooleanMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8SetReturnValueBool(info, impl->booleanMethod());
}

static void ByteMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8SetReturnValueInt(info, impl->byteMethod());
}

static void DoubleMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8SetReturnValue(info, impl->doubleMethod());
}

static void FloatMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8SetReturnValue(info, impl->floatMethod());
}

static void LongMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8SetReturnValueInt(info, impl->longMethod());
}

static void LongLongMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8SetReturnValue(info, static_cast<double>(impl->longLongMethod()));
}

static void OctetMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8SetReturnValueUnsigned(info, impl->octetMethod());
}

static void ShortMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8SetReturnValueInt(info, impl->shortMethod());
}

static void UnsignedLongMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8SetReturnValueUnsigned(info, impl->unsignedLongMethod());
}

static void UnsignedLongLongMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8SetReturnValue(info, static_cast<double>(impl->unsignedLongLongMethod()));
}

static void UnsignedShortMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8SetReturnValueUnsigned(info, impl->unsignedShortMethod());
}

static void VoidMethodDateArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodDateArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  double dateArg;
  dateArg = NativeValueTraits<IDLDate>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodDateArg(dateArg);
}

static void VoidMethodStringArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodStringArg", "TestObject", ExceptionMessages::NotEnoughArguments(1, info.Length())));
    return;
  }

  V8StringResource<> stringArg;
  stringArg = info[0];
  if (!stringArg.Prepare())
    return;

  impl->voidMethodStringArg(stringArg);
}

static void VoidMethodByteStringArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodByteStringArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  V8StringResource<> stringArg;
  stringArg = NativeValueTraits<IDLByteString>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodByteStringArg(stringArg);
}

static void VoidMethodUSVStringArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodUSVStringArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  V8StringResource<> usvStringArg;
  usvStringArg = NativeValueTraits<IDLUSVString>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodUSVStringArg(usvStringArg);
}

static void VoidMethodDOMTimeStampArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodDOMTimeStampArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  uint64_t domTimeStampArg;
  domTimeStampArg = NativeValueTraits<IDLUnsignedLongLong>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodDOMTimeStampArg(domTimeStampArg);
}

static void VoidMethodBooleanArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodBooleanArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  bool booleanArg;
  booleanArg = NativeValueTraits<IDLBoolean>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodBooleanArg(booleanArg);
}

static void VoidMethodByteArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodByteArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  int8_t byteArg;
  byteArg = NativeValueTraits<IDLByte>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodByteArg(byteArg);
}

static void VoidMethodDoubleArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodDoubleArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  double doubleArg;
  doubleArg = NativeValueTraits<IDLDouble>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodDoubleArg(doubleArg);
}

static void VoidMethodFloatArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodFloatArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  float floatArg;
  floatArg = NativeValueTraits<IDLFloat>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodFloatArg(floatArg);
}

static void VoidMethodLongArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodLongArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  int32_t longArg;
  longArg = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodLongArg(longArg);
}

static void VoidMethodLongLongArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodLongLongArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  int64_t longLongArg;
  longLongArg = NativeValueTraits<IDLLongLong>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodLongLongArg(longLongArg);
}

static void VoidMethodOctetArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodOctetArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  uint8_t octetArg;
  octetArg = NativeValueTraits<IDLOctet>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodOctetArg(octetArg);
}

static void VoidMethodShortArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodShortArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  int16_t shortArg;
  shortArg = NativeValueTraits<IDLShort>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodShortArg(shortArg);
}

static void VoidMethodUnsignedLongArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodUnsignedLongArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  uint32_t unsignedLongArg;
  unsignedLongArg = NativeValueTraits<IDLUnsignedLong>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodUnsignedLongArg(unsignedLongArg);
}

static void VoidMethodUnsignedLongLongArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodUnsignedLongLongArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  uint64_t unsignedLongLongArg;
  unsignedLongLongArg = NativeValueTraits<IDLUnsignedLongLong>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodUnsignedLongLongArg(unsignedLongLongArg);
}

static void VoidMethodUnsignedShortArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodUnsignedShortArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  uint16_t unsignedShortArg;
  unsignedShortArg = NativeValueTraits<IDLUnsignedShort>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodUnsignedShortArg(unsignedShortArg);
}

static void TestInterfaceEmptyMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8SetReturnValue(info, impl->testInterfaceEmptyMethod());
}

static void VoidMethodTestInterfaceEmptyArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodTestInterfaceEmptyArg", "TestObject", ExceptionMessages::NotEnoughArguments(1, info.Length())));
    return;
  }

  TestInterfaceEmpty* testInterfaceEmptyArg;
  testInterfaceEmptyArg = V8TestInterfaceEmpty::ToImplWithTypeCheck(info.GetIsolate(), info[0]);
  if (!testInterfaceEmptyArg) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodTestInterfaceEmptyArg", "TestObject", "parameter 1 is not of type 'TestInterfaceEmpty'."));
    return;
  }

  impl->voidMethodTestInterfaceEmptyArg(testInterfaceEmptyArg);
}

static void VoidMethodLongArgTestInterfaceEmptyArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodLongArgTestInterfaceEmptyArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 2)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(2, info.Length()));
    return;
  }

  int32_t longArg;
  TestInterfaceEmpty* testInterfaceEmptyArg;
  longArg = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  testInterfaceEmptyArg = V8TestInterfaceEmpty::ToImplWithTypeCheck(info.GetIsolate(), info[1]);
  if (!testInterfaceEmptyArg) {
    exceptionState.ThrowTypeError("parameter 2 is not of type 'TestInterfaceEmpty'.");
    return;
  }

  impl->voidMethodLongArgTestInterfaceEmptyArg(longArg, testInterfaceEmptyArg);
}

static void AnyMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8SetReturnValue(info, impl->anyMethod().V8Value());
}

static void VoidMethodEventTargetArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodEventTargetArg", "TestObject", ExceptionMessages::NotEnoughArguments(1, info.Length())));
    return;
  }

  EventTarget* eventTargetArg;
  eventTargetArg = V8EventTarget::ToImplWithTypeCheck(info.GetIsolate(), info[0]);
  if (!eventTargetArg) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodEventTargetArg", "TestObject", "parameter 1 is not of type 'EventTarget'."));
    return;
  }

  impl->voidMethodEventTargetArg(eventTargetArg);
}

static void VoidMethodAnyArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodAnyArg", "TestObject", ExceptionMessages::NotEnoughArguments(1, info.Length())));
    return;
  }

  ScriptValue anyArg;
  anyArg = ScriptValue(ScriptState::Current(info.GetIsolate()), info[0]);

  impl->voidMethodAnyArg(anyArg);
}

static void VoidMethodAttrArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodAttrArg", "TestObject", ExceptionMessages::NotEnoughArguments(1, info.Length())));
    return;
  }

  Attr* attrArg;
  attrArg = V8Attr::ToImplWithTypeCheck(info.GetIsolate(), info[0]);
  if (!attrArg) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodAttrArg", "TestObject", "parameter 1 is not of type 'Attr'."));
    return;
  }

  impl->voidMethodAttrArg(attrArg);
}

static void VoidMethodDocumentArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodDocumentArg", "TestObject", ExceptionMessages::NotEnoughArguments(1, info.Length())));
    return;
  }

  Document* documentArg;
  documentArg = V8Document::ToImplWithTypeCheck(info.GetIsolate(), info[0]);
  if (!documentArg) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodDocumentArg", "TestObject", "parameter 1 is not of type 'Document'."));
    return;
  }

  impl->voidMethodDocumentArg(documentArg);
}

static void VoidMethodDocumentTypeArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodDocumentTypeArg", "TestObject", ExceptionMessages::NotEnoughArguments(1, info.Length())));
    return;
  }

  DocumentType* documentTypeArg;
  documentTypeArg = V8DocumentType::ToImplWithTypeCheck(info.GetIsolate(), info[0]);
  if (!documentTypeArg) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodDocumentTypeArg", "TestObject", "parameter 1 is not of type 'DocumentType'."));
    return;
  }

  impl->voidMethodDocumentTypeArg(documentTypeArg);
}

static void VoidMethodElementArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodElementArg", "TestObject", ExceptionMessages::NotEnoughArguments(1, info.Length())));
    return;
  }

  Element* elementArg;
  elementArg = V8Element::ToImplWithTypeCheck(info.GetIsolate(), info[0]);
  if (!elementArg) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodElementArg", "TestObject", "parameter 1 is not of type 'Element'."));
    return;
  }

  impl->voidMethodElementArg(elementArg);
}

static void VoidMethodNodeArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodNodeArg", "TestObject", ExceptionMessages::NotEnoughArguments(1, info.Length())));
    return;
  }

  Node* nodeArg;
  nodeArg = V8Node::ToImplWithTypeCheck(info.GetIsolate(), info[0]);
  if (!nodeArg) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodNodeArg", "TestObject", "parameter 1 is not of type 'Node'."));
    return;
  }

  impl->voidMethodNodeArg(nodeArg);
}

static void ArrayBufferMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8SetReturnValue(info, impl->arrayBufferMethod());
}

static void ArrayBufferViewMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8SetReturnValue(info, impl->arrayBufferViewMethod());
}

static void ArrayBufferViewMethodRaisesExceptionMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "arrayBufferViewMethodRaisesException");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  NotShared<TestArrayBufferView> result = impl->arrayBufferViewMethodRaisesException(exceptionState);
  if (exceptionState.HadException()) {
    return;
  }
  V8SetReturnValue(info, result);
}

static void Float32ArrayMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8SetReturnValue(info, impl->float32ArrayMethod());
}

static void Int32ArrayMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8SetReturnValue(info, impl->int32ArrayMethod());
}

static void Uint8ArrayMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8SetReturnValue(info, impl->uint8ArrayMethod());
}

static void VoidMethodArrayBufferArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodArrayBufferArg", "TestObject", ExceptionMessages::NotEnoughArguments(1, info.Length())));
    return;
  }

  TestArrayBuffer* arrayBufferArg;
  arrayBufferArg = info[0]->IsArrayBuffer() ? V8ArrayBuffer::ToImpl(v8::Local<v8::ArrayBuffer>::Cast(info[0])) : 0;
  if (!arrayBufferArg) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodArrayBufferArg", "TestObject", "parameter 1 is not of type 'ArrayBuffer'."));
    return;
  }

  impl->voidMethodArrayBufferArg(arrayBufferArg);
}

static void VoidMethodArrayBufferOrNullArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodArrayBufferOrNullArg", "TestObject", ExceptionMessages::NotEnoughArguments(1, info.Length())));
    return;
  }

  TestArrayBuffer* arrayBufferArg;
  arrayBufferArg = V8ArrayBuffer::ToImplWithTypeCheck(info.GetIsolate(), info[0]);
  if (!arrayBufferArg && !IsUndefinedOrNull(info[0])) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodArrayBufferOrNullArg", "TestObject", "parameter 1 is not of type 'ArrayBuffer'."));
    return;
  }

  impl->voidMethodArrayBufferOrNullArg(arrayBufferArg);
}

static void VoidMethodArrayBufferViewArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodArrayBufferViewArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  NotShared<TestArrayBufferView> arrayBufferViewArg;
  arrayBufferViewArg = ToNotShared<NotShared<TestArrayBufferView>>(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;
  if (!arrayBufferViewArg) {
    exceptionState.ThrowTypeError("parameter 1 is not of type 'ArrayBufferView'.");
    return;
  }

  impl->voidMethodArrayBufferViewArg(arrayBufferViewArg);
}

static void VoidMethodFlexibleArrayBufferViewArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodFlexibleArrayBufferViewArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  FlexibleArrayBufferView arrayBufferViewArg;
  ToFlexibleArrayBufferView(info.GetIsolate(), info[0], arrayBufferViewArg, allocateFlexibleArrayBufferViewStorage(info[0]));
  if (!arrayBufferViewArg) {
    exceptionState.ThrowTypeError("parameter 1 is not of type 'ArrayBufferView'.");
    return;
  }

  impl->voidMethodFlexibleArrayBufferViewArg(arrayBufferViewArg);
}

static void VoidMethodFlexibleArrayBufferViewTypedArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodFlexibleArrayBufferViewTypedArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  FlexibleFloat32ArrayView typedArrayBufferViewArg;
  ToFlexibleArrayBufferView(info.GetIsolate(), info[0], typedArrayBufferViewArg, allocateFlexibleArrayBufferViewStorage(info[0]));
  if (!typedArrayBufferViewArg) {
    exceptionState.ThrowTypeError("parameter 1 is not of type 'Float32Array'.");
    return;
  }

  impl->voidMethodFlexibleArrayBufferViewTypedArg(typedArrayBufferViewArg);
}

static void VoidMethodFloat32ArrayArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodFloat32ArrayArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  NotShared<DOMFloat32Array> float32ArrayArg;
  float32ArrayArg = ToNotShared<NotShared<DOMFloat32Array>>(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;
  if (!float32ArrayArg) {
    exceptionState.ThrowTypeError("parameter 1 is not of type 'Float32Array'.");
    return;
  }

  impl->voidMethodFloat32ArrayArg(float32ArrayArg);
}

static void VoidMethodInt32ArrayArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodInt32ArrayArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  NotShared<DOMInt32Array> int32ArrayArg;
  int32ArrayArg = ToNotShared<NotShared<DOMInt32Array>>(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;
  if (!int32ArrayArg) {
    exceptionState.ThrowTypeError("parameter 1 is not of type 'Int32Array'.");
    return;
  }

  impl->voidMethodInt32ArrayArg(int32ArrayArg);
}

static void VoidMethodUint8ArrayArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodUint8ArrayArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  NotShared<DOMUint8Array> uint8ArrayArg;
  uint8ArrayArg = ToNotShared<NotShared<DOMUint8Array>>(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;
  if (!uint8ArrayArg) {
    exceptionState.ThrowTypeError("parameter 1 is not of type 'Uint8Array'.");
    return;
  }

  impl->voidMethodUint8ArrayArg(uint8ArrayArg);
}

static void VoidMethodAllowSharedArrayBufferViewArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodAllowSharedArrayBufferViewArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  MaybeShared<TestArrayBufferView> arrayBufferViewArg;
  arrayBufferViewArg = ToMaybeShared<MaybeShared<TestArrayBufferView>>(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;
  if (!arrayBufferViewArg) {
    exceptionState.ThrowTypeError("parameter 1 is not of type 'ArrayBufferView'.");
    return;
  }

  impl->voidMethodAllowSharedArrayBufferViewArg(arrayBufferViewArg);
}

static void VoidMethodAllowSharedUint8ArrayArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodAllowSharedUint8ArrayArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  MaybeShared<DOMUint8Array> uint8ArrayArg;
  uint8ArrayArg = ToMaybeShared<MaybeShared<DOMUint8Array>>(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;
  if (!uint8ArrayArg) {
    exceptionState.ThrowTypeError("parameter 1 is not of type 'Uint8Array'.");
    return;
  }

  impl->voidMethodAllowSharedUint8ArrayArg(uint8ArrayArg);
}

static void LongSequenceMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8SetReturnValue(info, ToV8(impl->longSequenceMethod(), info.Holder(), info.GetIsolate()));
}

static void StringSequenceMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8SetReturnValue(info, ToV8(impl->stringSequenceMethod(), info.Holder(), info.GetIsolate()));
}

static void TestInterfaceEmptySequenceMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8SetReturnValue(info, ToV8(impl->testInterfaceEmptySequenceMethod(), info.Holder(), info.GetIsolate()));
}

static void VoidMethodSequenceLongArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodSequenceLongArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  Vector<int32_t> longSequenceArg;
  longSequenceArg = NativeValueTraits<IDLSequence<IDLLong>>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodSequenceLongArg(longSequenceArg);
}

static void VoidMethodSequenceStringArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodSequenceStringArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  Vector<String> stringSequenceArg;
  stringSequenceArg = NativeValueTraits<IDLSequence<IDLString>>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodSequenceStringArg(stringSequenceArg);
}

static void VoidMethodSequenceTestInterfaceEmptyArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodSequenceTestInterfaceEmptyArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  HeapVector<Member<TestInterfaceEmpty>> testInterfaceEmptySequenceArg;
  testInterfaceEmptySequenceArg = NativeValueTraits<IDLSequence<TestInterfaceEmpty>>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodSequenceTestInterfaceEmptyArg(testInterfaceEmptySequenceArg);
}

static void VoidMethodSequenceSequenceDOMStringArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodSequenceSequenceDOMStringArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  Vector<Vector<String>> stringSequenceSequenceArg;
  stringSequenceSequenceArg = NativeValueTraits<IDLSequence<IDLSequence<IDLString>>>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodSequenceSequenceDOMStringArg(stringSequenceSequenceArg);
}

static void VoidMethodNullableSequenceLongArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodNullableSequenceLongArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  base::Optional<Vector<int32_t>> longSequenceArg;
  if (!info[0]->IsNullOrUndefined()) {
    longSequenceArg = NativeValueTraits<IDLSequence<IDLLong>>::NativeValue(info.GetIsolate(), info[0], exceptionState);
    if (exceptionState.HadException())
      return;
  }

  impl->voidMethodNullableSequenceLongArg(longSequenceArg);
}

static void LongFrozenArrayMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8SetReturnValue(info, FreezeV8Object(ToV8(impl->longFrozenArrayMethod(), info.Holder(), info.GetIsolate()), info.GetIsolate()));
}

static void VoidMethodStringFrozenArrayMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodStringFrozenArrayMethod");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  Vector<String> stringFrozenArrayArg;
  stringFrozenArrayArg = NativeValueTraits<IDLSequence<IDLString>>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodStringFrozenArrayMethod(stringFrozenArrayArg);
}

static void VoidMethodTestInterfaceEmptyFrozenArrayMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodTestInterfaceEmptyFrozenArrayMethod");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  HeapVector<Member<TestInterfaceEmpty>> testInterfaceEmptyFrozenArrayArg;
  testInterfaceEmptyFrozenArrayArg = NativeValueTraits<IDLSequence<TestInterfaceEmpty>>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodTestInterfaceEmptyFrozenArrayMethod(testInterfaceEmptyFrozenArrayArg);
}

static void NullableLongMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  base::Optional<int32_t> result = impl->nullableLongMethod();
  if (!result)
    V8SetReturnValueNull(info);
  else
    V8SetReturnValueInt(info, result.value());
}

static void NullableStringMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8SetReturnValueStringOrNull(info, impl->nullableStringMethod(), info.GetIsolate());
}

static void NullableTestInterfaceMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8SetReturnValue(info, impl->nullableTestInterfaceMethod());
}

static void NullableLongSequenceMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  base::Optional<Vector<int32_t>> result = impl->nullableLongSequenceMethod();
  if (!result)
    V8SetReturnValueNull(info);
  else
    V8SetReturnValue(info, ToV8(result.value(), info.Holder(), info.GetIsolate()));
}

static void BooleanOrDOMStringOrUnrestrictedDoubleMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  BooleanOrStringOrUnrestrictedDouble result;
  impl->booleanOrDOMStringOrUnrestrictedDoubleMethod(result);
  V8SetReturnValue(info, result);
}

static void TestInterfaceOrLongMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  TestInterfaceOrLong result;
  impl->testInterfaceOrLongMethod(result);
  V8SetReturnValue(info, result);
}

static void VoidMethodDoubleOrDOMStringArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodDoubleOrDOMStringArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  DoubleOrString arg;
  V8DoubleOrString::ToImpl(info.GetIsolate(), info[0], arg, UnionTypeConversionMode::kNotNullable, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodDoubleOrDOMStringArg(arg);
}

static void VoidMethodDoubleOrDOMStringOrNullArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodDoubleOrDOMStringOrNullArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  DoubleOrString arg;
  V8DoubleOrString::ToImpl(info.GetIsolate(), info[0], arg, UnionTypeConversionMode::kNullable, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodDoubleOrDOMStringOrNullArg(arg);
}

static void VoidMethodDoubleOrNullOrDOMStringArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodDoubleOrNullOrDOMStringArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  DoubleOrString arg;
  V8DoubleOrString::ToImpl(info.GetIsolate(), info[0], arg, UnionTypeConversionMode::kNullable, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodDoubleOrNullOrDOMStringArg(arg);
}

static void VoidMethodDOMStringOrArrayBufferOrArrayBufferViewArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodDOMStringOrArrayBufferOrArrayBufferViewArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  StringOrArrayBufferOrArrayBufferView arg;
  V8StringOrArrayBufferOrArrayBufferView::ToImpl(info.GetIsolate(), info[0], arg, UnionTypeConversionMode::kNotNullable, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodDOMStringOrArrayBufferOrArrayBufferViewArg(arg);
}

static void VoidMethodArrayBufferOrArrayBufferViewOrDictionaryArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodArrayBufferOrArrayBufferViewOrDictionaryArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  ArrayBufferOrArrayBufferViewOrDictionary arg;
  V8ArrayBufferOrArrayBufferViewOrDictionary::ToImpl(info.GetIsolate(), info[0], arg, UnionTypeConversionMode::kNotNullable, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodArrayBufferOrArrayBufferViewOrDictionaryArg(arg);
}

static void VoidMethodBooleanOrElementSequenceArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodBooleanOrElementSequenceArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  BooleanOrElementSequence arg;
  V8BooleanOrElementSequence::ToImpl(info.GetIsolate(), info[0], arg, UnionTypeConversionMode::kNotNullable, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodBooleanOrElementSequenceArg(arg);
}

static void VoidMethodDoubleOrLongOrBooleanSequenceArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodDoubleOrLongOrBooleanSequenceArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  DoubleOrLongOrBooleanSequence arg;
  V8DoubleOrLongOrBooleanSequence::ToImpl(info.GetIsolate(), info[0], arg, UnionTypeConversionMode::kNotNullable, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodDoubleOrLongOrBooleanSequenceArg(arg);
}

static void VoidMethodElementSequenceOrByteStringDoubleOrStringRecordMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodElementSequenceOrByteStringDoubleOrStringRecord");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  ElementSequenceOrByteStringDoubleOrStringRecord arg;
  V8ElementSequenceOrByteStringDoubleOrStringRecord::ToImpl(info.GetIsolate(), info[0], arg, UnionTypeConversionMode::kNotNullable, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodElementSequenceOrByteStringDoubleOrStringRecord(arg);
}

static void VoidMethodArrayOfDoubleOrDOMStringArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodArrayOfDoubleOrDOMStringArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  HeapVector<DoubleOrString> arg;
  arg = ToImplArguments<DoubleOrString>(info, 0, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodArrayOfDoubleOrDOMStringArg(arg);
}

static void VoidMethodTestInterfaceEmptyOrNullArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodTestInterfaceEmptyOrNullArg", "TestObject", ExceptionMessages::NotEnoughArguments(1, info.Length())));
    return;
  }

  TestInterfaceEmpty* nullableTestInterfaceEmptyArg;
  nullableTestInterfaceEmptyArg = V8TestInterfaceEmpty::ToImplWithTypeCheck(info.GetIsolate(), info[0]);
  if (!nullableTestInterfaceEmptyArg && !IsUndefinedOrNull(info[0])) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodTestInterfaceEmptyOrNullArg", "TestObject", "parameter 1 is not of type 'TestInterfaceEmpty'."));
    return;
  }

  impl->voidMethodTestInterfaceEmptyOrNullArg(nullableTestInterfaceEmptyArg);
}

static void VoidMethodTestCallbackInterfaceArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodTestCallbackInterfaceArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodTestCallbackInterfaceArg", "TestObject", ExceptionMessages::NotEnoughArguments(1, info.Length())));
    return;
  }

  V8TestCallbackInterface* testCallbackInterfaceArg;
  if (info[0]->IsObject()) {
    testCallbackInterfaceArg = V8TestCallbackInterface::CreateOrNull(info[0].As<v8::Object>());
    if (!testCallbackInterfaceArg) {
      exceptionState.ThrowSecurityError("The callback provided as parameter 1 is a cross origin object.");
      return;
    }
  } else {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodTestCallbackInterfaceArg", "TestObject", "The callback provided as parameter 1 is not an object."));
    return;
  }

  impl->voidMethodTestCallbackInterfaceArg(testCallbackInterfaceArg);
}

static void VoidMethodOptionalTestCallbackInterfaceArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodOptionalTestCallbackInterfaceArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8TestCallbackInterface* optionalTestCallbackInterfaceArg;
  if (info[0]->IsObject()) {
    optionalTestCallbackInterfaceArg = V8TestCallbackInterface::CreateOrNull(info[0].As<v8::Object>());
    if (!optionalTestCallbackInterfaceArg) {
      exceptionState.ThrowSecurityError("The callback provided as parameter 1 is a cross origin object.");
      return;
    }
  } else if (info[0]->IsUndefined()) {
    optionalTestCallbackInterfaceArg = nullptr;
  } else {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodOptionalTestCallbackInterfaceArg", "TestObject", "The callback provided as parameter 1 is not an object."));
    return;
  }

  impl->voidMethodOptionalTestCallbackInterfaceArg(optionalTestCallbackInterfaceArg);
}

static void VoidMethodTestCallbackInterfaceOrNullArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodTestCallbackInterfaceOrNullArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodTestCallbackInterfaceOrNullArg", "TestObject", ExceptionMessages::NotEnoughArguments(1, info.Length())));
    return;
  }

  V8TestCallbackInterface* testCallbackInterfaceArg;
  if (info[0]->IsObject()) {
    testCallbackInterfaceArg = V8TestCallbackInterface::CreateOrNull(info[0].As<v8::Object>());
    if (!testCallbackInterfaceArg) {
      exceptionState.ThrowSecurityError("The callback provided as parameter 1 is a cross origin object.");
      return;
    }
  } else if (info[0]->IsNullOrUndefined()) {
    testCallbackInterfaceArg = nullptr;
  } else {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodTestCallbackInterfaceOrNullArg", "TestObject", "The callback provided as parameter 1 is not an object."));
    return;
  }

  impl->voidMethodTestCallbackInterfaceOrNullArg(testCallbackInterfaceArg);
}

static void TestEnumMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8SetReturnValueString(info, impl->testEnumMethod(), info.GetIsolate());
}

static void VoidMethodTestEnumArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodTestEnumArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  V8StringResource<> testEnumTypeArg;
  testEnumTypeArg = info[0];
  if (!testEnumTypeArg.Prepare())
    return;
  const char* validTestEnumTypeArgValues[] = {
      "",
      "EnumValue1",
      "EnumValue2",
      "EnumValue3",
  };
  if (!IsValidEnum(testEnumTypeArg, validTestEnumTypeArgValues, base::size(validTestEnumTypeArgValues), "TestEnum", exceptionState)) {
    return;
  }

  impl->voidMethodTestEnumArg(testEnumTypeArg);
}

static void VoidMethodTestMultipleEnumArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodTestMultipleEnumArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 2)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(2, info.Length()));
    return;
  }

  V8StringResource<> testEnumTypeArg;
  V8StringResource<> testEnumTypeArg2;
  testEnumTypeArg = info[0];
  if (!testEnumTypeArg.Prepare())
    return;
  const char* validTestEnumTypeArgValues[] = {
      "",
      "EnumValue1",
      "EnumValue2",
      "EnumValue3",
  };
  if (!IsValidEnum(testEnumTypeArg, validTestEnumTypeArgValues, base::size(validTestEnumTypeArgValues), "TestEnum", exceptionState)) {
    return;
  }

  testEnumTypeArg2 = info[1];
  if (!testEnumTypeArg2.Prepare())
    return;
  const char* validTestEnumTypeArg2Values[] = {
      "",
      "EnumValue1",
      "EnumValue2",
      "EnumValue3",
  };
  if (!IsValidEnum(testEnumTypeArg2, validTestEnumTypeArg2Values, base::size(validTestEnumTypeArg2Values), "TestEnum2", exceptionState)) {
    return;
  }

  impl->voidMethodTestMultipleEnumArg(testEnumTypeArg, testEnumTypeArg2);
}

static void DictionaryMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8SetReturnValue(info, impl->dictionaryMethod());
}

static void TestDictionaryMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  TestDictionary* result = impl->testDictionaryMethod();
  V8SetReturnValue(info, result);
}

static void NullableTestDictionaryMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  TestDictionary* result = impl->nullableTestDictionaryMethod();
  V8SetReturnValue(info, result);
}

static void StaticTestDictionaryMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestDictionary* result = TestObject::staticTestDictionaryMethod();
  V8SetReturnValue(info, result, info.GetIsolate()->GetCurrentContext()->Global());
}

static void StaticNullableTestDictionaryMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestDictionary* result = TestObject::staticNullableTestDictionaryMethod();
  V8SetReturnValue(info, result, info.GetIsolate()->GetCurrentContext()->Global());
}

static void PassPermissiveDictionaryMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "passPermissiveDictionaryMethod");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  TestDictionary* arg;
  arg = NativeValueTraits<TestDictionary>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->passPermissiveDictionaryMethod(arg);
}

static void NodeFilterMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8SetReturnValue(info, ToV8(impl->nodeFilterMethod(), info.Holder(), info.GetIsolate()));
}

static void PromiseMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "promiseMethod");
  ExceptionToRejectPromiseScope rejectPromiseScope(info, exceptionState);

  // V8DOMConfiguration::kDoNotCheckHolder
  // Make sure that info.Holder() really points to an instance of the type.
  if (!V8TestObject::HasInstance(info.Holder(), info.GetIsolate())) {
    exceptionState.ThrowTypeError("Illegal invocation");
    return;
  }
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 3)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(3, info.Length()));
    return;
  }

  int32_t arg1;
  Dictionary arg2;
  V8StringResource<> arg3;
  Vector<String> variadic;
  arg1 = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  if (!info[1]->IsNullOrUndefined() && !info[1]->IsObject()) {
    exceptionState.ThrowTypeError("parameter 2 ('arg2') is not an object.");
    return;
  }
  arg2 = NativeValueTraits<Dictionary>::NativeValue(info.GetIsolate(), info[1], exceptionState);
  if (exceptionState.HadException())
    return;

  arg3 = info[2];
  if (!arg3.Prepare(exceptionState))
    return;

  variadic = ToImplArguments<IDLString>(info, 3, exceptionState);
  if (exceptionState.HadException())
    return;

  V8SetReturnValue(info, impl->promiseMethod(arg1, arg2, arg3, variadic).V8Value());
}

static void PromiseMethodWithoutExceptionStateMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "promiseMethodWithoutExceptionState");
  ExceptionToRejectPromiseScope rejectPromiseScope(info, exceptionState);

  // V8DOMConfiguration::kDoNotCheckHolder
  // Make sure that info.Holder() really points to an instance of the type.
  if (!V8TestObject::HasInstance(info.Holder(), info.GetIsolate())) {
    exceptionState.ThrowTypeError("Illegal invocation");
    return;
  }
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  Dictionary arg1;
  if (!info[0]->IsNullOrUndefined() && !info[0]->IsObject()) {
    exceptionState.ThrowTypeError("parameter 1 ('arg1') is not an object.");
    return;
  }
  arg1 = NativeValueTraits<Dictionary>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  V8SetReturnValue(info, impl->promiseMethodWithoutExceptionState(arg1).V8Value());
}

static void SerializedScriptValueMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8SetReturnValue(info, V8Deserialize(info.GetIsolate(), impl->serializedScriptValueMethod().get()));
}

static void XPathNSResolverMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8SetReturnValue(info, impl->xPathNSResolverMethod());
}

static void VoidMethodDictionaryArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodDictionaryArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  Dictionary dictionaryArg;
  if (!info[0]->IsNullOrUndefined() && !info[0]->IsObject()) {
    exceptionState.ThrowTypeError("parameter 1 ('dictionaryArg') is not an object.");
    return;
  }
  dictionaryArg = NativeValueTraits<Dictionary>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodDictionaryArg(dictionaryArg);
}

static void VoidMethodNodeFilterArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodNodeFilterArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodNodeFilterArg", "TestObject", ExceptionMessages::NotEnoughArguments(1, info.Length())));
    return;
  }

  V8NodeFilter* nodeFilterArg;
  if (info[0]->IsObject()) {
    nodeFilterArg = V8NodeFilter::CreateOrNull(info[0].As<v8::Object>());
    if (!nodeFilterArg) {
      exceptionState.ThrowSecurityError("The callback provided as parameter 1 is a cross origin object.");
      return;
    }
  } else {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodNodeFilterArg", "TestObject", "The callback provided as parameter 1 is not an object."));
    return;
  }

  impl->voidMethodNodeFilterArg(nodeFilterArg);
}

static void VoidMethodPromiseArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodPromiseArg", "TestObject", ExceptionMessages::NotEnoughArguments(1, info.Length())));
    return;
  }

  ScriptPromise promiseArg;
  promiseArg = ScriptPromise::Cast(ScriptState::Current(info.GetIsolate()), info[0]);
  if (!promiseArg.IsUndefinedOrNull() && !promiseArg.IsObject()) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodPromiseArg", "TestObject", "parameter 1 ('promiseArg') is not an object."));
    return;
  }

  impl->voidMethodPromiseArg(promiseArg);
}

static void VoidMethodSerializedScriptValueArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodSerializedScriptValueArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  scoped_refptr<SerializedScriptValue> serializedScriptValueArg;
  serializedScriptValueArg = NativeValueTraits<SerializedScriptValue>::NativeValue(info.GetIsolate(), info[0], SerializedScriptValue::SerializeOptions(SerializedScriptValue::kNotForStorage), exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodSerializedScriptValueArg(std::move(serializedScriptValueArg));
}

static void VoidMethodXPathNSResolverArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodXPathNSResolverArg", "TestObject", ExceptionMessages::NotEnoughArguments(1, info.Length())));
    return;
  }

  XPathNSResolver* xPathNSResolverArg;
  xPathNSResolverArg = ToXPathNSResolver(ScriptState::Current(info.GetIsolate()), info[0]);
  if (!xPathNSResolverArg) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodXPathNSResolverArg", "TestObject", "parameter 1 is not of type 'XPathNSResolver'."));
    return;
  }

  impl->voidMethodXPathNSResolverArg(xPathNSResolverArg);
}

static void VoidMethodDictionarySequenceArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodDictionarySequenceArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  Vector<Dictionary> dictionarySequenceArg;
  dictionarySequenceArg = NativeValueTraits<IDLSequence<Dictionary>>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodDictionarySequenceArg(dictionarySequenceArg);
}

static void VoidMethodStringArgLongArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodStringArgLongArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 2)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(2, info.Length()));
    return;
  }

  V8StringResource<> stringArg;
  int32_t longArg;
  stringArg = info[0];
  if (!stringArg.Prepare())
    return;

  longArg = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[1], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodStringArgLongArg(stringArg, longArg);
}

static void VoidMethodByteStringOrNullOptionalUSVStringArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodByteStringOrNullOptionalUSVStringArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  V8StringResource<kTreatNullAndUndefinedAsNullString> byteStringArg;
  V8StringResource<> usvStringArg;
  int numArgsPassed = info.Length();
  while (numArgsPassed > 0) {
    if (!info[numArgsPassed - 1]->IsUndefined())
      break;
    --numArgsPassed;
  }
  byteStringArg = NativeValueTraits<IDLByteStringOrNull>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  if (UNLIKELY(numArgsPassed <= 1)) {
    impl->voidMethodByteStringOrNullOptionalUSVStringArg(byteStringArg);
    return;
  }
  usvStringArg = NativeValueTraits<IDLUSVString>::NativeValue(info.GetIsolate(), info[1], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodByteStringOrNullOptionalUSVStringArg(byteStringArg, usvStringArg);
}

static void VoidMethodOptionalStringArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8StringResource<> optionalStringArg;
  int numArgsPassed = info.Length();
  while (numArgsPassed > 0) {
    if (!info[numArgsPassed - 1]->IsUndefined())
      break;
    --numArgsPassed;
  }
  if (UNLIKELY(numArgsPassed <= 0)) {
    impl->voidMethodOptionalStringArg();
    return;
  }
  optionalStringArg = info[0];
  if (!optionalStringArg.Prepare())
    return;

  impl->voidMethodOptionalStringArg(optionalStringArg);
}

static void VoidMethodOptionalTestInterfaceEmptyArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  TestInterfaceEmpty* optionalTestInterfaceEmptyArg;
  int numArgsPassed = info.Length();
  while (numArgsPassed > 0) {
    if (!info[numArgsPassed - 1]->IsUndefined())
      break;
    --numArgsPassed;
  }
  if (UNLIKELY(numArgsPassed <= 0)) {
    impl->voidMethodOptionalTestInterfaceEmptyArg();
    return;
  }
  optionalTestInterfaceEmptyArg = V8TestInterfaceEmpty::ToImplWithTypeCheck(info.GetIsolate(), info[0]);
  if (!optionalTestInterfaceEmptyArg) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodOptionalTestInterfaceEmptyArg", "TestObject", "parameter 1 is not of type 'TestInterfaceEmpty'."));
    return;
  }

  impl->voidMethodOptionalTestInterfaceEmptyArg(optionalTestInterfaceEmptyArg);
}

static void VoidMethodOptionalLongArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodOptionalLongArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  int32_t optionalLongArg;
  int numArgsPassed = info.Length();
  while (numArgsPassed > 0) {
    if (!info[numArgsPassed - 1]->IsUndefined())
      break;
    --numArgsPassed;
  }
  if (UNLIKELY(numArgsPassed <= 0)) {
    impl->voidMethodOptionalLongArg();
    return;
  }
  optionalLongArg = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodOptionalLongArg(optionalLongArg);
}

static void StringMethodOptionalLongArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "stringMethodOptionalLongArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  int32_t optionalLongArg;
  int numArgsPassed = info.Length();
  while (numArgsPassed > 0) {
    if (!info[numArgsPassed - 1]->IsUndefined())
      break;
    --numArgsPassed;
  }
  if (UNLIKELY(numArgsPassed <= 0)) {
    V8SetReturnValueString(info, impl->stringMethodOptionalLongArg(), info.GetIsolate());
    return;
  }
  optionalLongArg = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  V8SetReturnValueString(info, impl->stringMethodOptionalLongArg(optionalLongArg), info.GetIsolate());
}

static void TestInterfaceEmptyMethodOptionalLongArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "testInterfaceEmptyMethodOptionalLongArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  int32_t optionalLongArg;
  int numArgsPassed = info.Length();
  while (numArgsPassed > 0) {
    if (!info[numArgsPassed - 1]->IsUndefined())
      break;
    --numArgsPassed;
  }
  if (UNLIKELY(numArgsPassed <= 0)) {
    V8SetReturnValue(info, impl->testInterfaceEmptyMethodOptionalLongArg());
    return;
  }
  optionalLongArg = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  V8SetReturnValue(info, impl->testInterfaceEmptyMethodOptionalLongArg(optionalLongArg));
}

static void LongMethodOptionalLongArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "longMethodOptionalLongArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  int32_t optionalLongArg;
  int numArgsPassed = info.Length();
  while (numArgsPassed > 0) {
    if (!info[numArgsPassed - 1]->IsUndefined())
      break;
    --numArgsPassed;
  }
  if (UNLIKELY(numArgsPassed <= 0)) {
    V8SetReturnValueInt(info, impl->longMethodOptionalLongArg());
    return;
  }
  optionalLongArg = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  V8SetReturnValueInt(info, impl->longMethodOptionalLongArg(optionalLongArg));
}

static void VoidMethodLongArgOptionalLongArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodLongArgOptionalLongArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  int32_t longArg;
  int32_t optionalLongArg;
  int numArgsPassed = info.Length();
  while (numArgsPassed > 0) {
    if (!info[numArgsPassed - 1]->IsUndefined())
      break;
    --numArgsPassed;
  }
  longArg = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  if (UNLIKELY(numArgsPassed <= 1)) {
    impl->voidMethodLongArgOptionalLongArg(longArg);
    return;
  }
  optionalLongArg = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[1], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodLongArgOptionalLongArg(longArg, optionalLongArg);
}

static void VoidMethodLongArgOptionalLongArgOptionalLongArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodLongArgOptionalLongArgOptionalLongArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  int32_t longArg;
  int32_t optionalLongArg1;
  int32_t optionalLongArg2;
  int numArgsPassed = info.Length();
  while (numArgsPassed > 0) {
    if (!info[numArgsPassed - 1]->IsUndefined())
      break;
    --numArgsPassed;
  }
  longArg = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  if (UNLIKELY(numArgsPassed <= 1)) {
    impl->voidMethodLongArgOptionalLongArgOptionalLongArg(longArg);
    return;
  }
  optionalLongArg1 = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[1], exceptionState);
  if (exceptionState.HadException())
    return;

  if (UNLIKELY(numArgsPassed <= 2)) {
    impl->voidMethodLongArgOptionalLongArgOptionalLongArg(longArg, optionalLongArg1);
    return;
  }
  optionalLongArg2 = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[2], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodLongArgOptionalLongArgOptionalLongArg(longArg, optionalLongArg1, optionalLongArg2);
}

static void VoidMethodLongArgOptionalTestInterfaceEmptyArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodLongArgOptionalTestInterfaceEmptyArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  int32_t longArg;
  TestInterfaceEmpty* optionalTestInterfaceEmpty;
  int numArgsPassed = info.Length();
  while (numArgsPassed > 0) {
    if (!info[numArgsPassed - 1]->IsUndefined())
      break;
    --numArgsPassed;
  }
  longArg = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  if (UNLIKELY(numArgsPassed <= 1)) {
    impl->voidMethodLongArgOptionalTestInterfaceEmptyArg(longArg);
    return;
  }
  optionalTestInterfaceEmpty = V8TestInterfaceEmpty::ToImplWithTypeCheck(info.GetIsolate(), info[1]);
  if (!optionalTestInterfaceEmpty) {
    exceptionState.ThrowTypeError("parameter 2 is not of type 'TestInterfaceEmpty'.");
    return;
  }

  impl->voidMethodLongArgOptionalTestInterfaceEmptyArg(longArg, optionalTestInterfaceEmpty);
}

static void VoidMethodTestInterfaceEmptyArgOptionalLongArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodTestInterfaceEmptyArgOptionalLongArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  TestInterfaceEmpty* optionalTestInterfaceEmpty;
  int32_t longArg;
  int numArgsPassed = info.Length();
  while (numArgsPassed > 0) {
    if (!info[numArgsPassed - 1]->IsUndefined())
      break;
    --numArgsPassed;
  }
  optionalTestInterfaceEmpty = V8TestInterfaceEmpty::ToImplWithTypeCheck(info.GetIsolate(), info[0]);
  if (!optionalTestInterfaceEmpty) {
    exceptionState.ThrowTypeError("parameter 1 is not of type 'TestInterfaceEmpty'.");
    return;
  }

  if (UNLIKELY(numArgsPassed <= 1)) {
    impl->voidMethodTestInterfaceEmptyArgOptionalLongArg(optionalTestInterfaceEmpty);
    return;
  }
  longArg = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[1], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodTestInterfaceEmptyArgOptionalLongArg(optionalTestInterfaceEmpty, longArg);
}

static void VoidMethodOptionalDictionaryArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodOptionalDictionaryArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  Dictionary optionalDictionaryArg;
  if (!info[0]->IsNullOrUndefined() && !info[0]->IsObject()) {
    exceptionState.ThrowTypeError("parameter 1 ('optionalDictionaryArg') is not an object.");
    return;
  }
  optionalDictionaryArg = NativeValueTraits<Dictionary>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodOptionalDictionaryArg(optionalDictionaryArg);
}

static void VoidMethodDefaultByteStringArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodDefaultByteStringArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8StringResource<> defaultByteStringArg;
  if (!info[0]->IsUndefined()) {
    defaultByteStringArg = NativeValueTraits<IDLByteString>::NativeValue(info.GetIsolate(), info[0], exceptionState);
    if (exceptionState.HadException())
      return;
  } else {
    defaultByteStringArg = "foo";
  }

  impl->voidMethodDefaultByteStringArg(defaultByteStringArg);
}

static void VoidMethodDefaultStringArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8StringResource<> defaultStringArg;
  if (!info[0]->IsUndefined()) {
    defaultStringArg = info[0];
    if (!defaultStringArg.Prepare())
      return;
  } else {
    defaultStringArg = "foo";
  }

  impl->voidMethodDefaultStringArg(defaultStringArg);
}

static void VoidMethodDefaultIntegerArgsMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodDefaultIntegerArgs");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  int32_t defaultLongArg;
  int64_t defaultLongLongArg;
  uint32_t defaultUnsignedArg;
  if (!info[0]->IsUndefined()) {
    defaultLongArg = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[0], exceptionState);
    if (exceptionState.HadException())
      return;
  } else {
    defaultLongArg = 10;
  }
  if (!info[1]->IsUndefined()) {
    defaultLongLongArg = NativeValueTraits<IDLLongLong>::NativeValue(info.GetIsolate(), info[1], exceptionState);
    if (exceptionState.HadException())
      return;
  } else {
    defaultLongLongArg = -10;
  }
  if (!info[2]->IsUndefined()) {
    defaultUnsignedArg = NativeValueTraits<IDLUnsignedLong>::NativeValue(info.GetIsolate(), info[2], exceptionState);
    if (exceptionState.HadException())
      return;
  } else {
    defaultUnsignedArg = 4294967295u;
  }

  impl->voidMethodDefaultIntegerArgs(defaultLongArg, defaultLongLongArg, defaultUnsignedArg);
}

static void VoidMethodDefaultDoubleArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodDefaultDoubleArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  double defaultDoubleArg;
  if (!info[0]->IsUndefined()) {
    defaultDoubleArg = NativeValueTraits<IDLDouble>::NativeValue(info.GetIsolate(), info[0], exceptionState);
    if (exceptionState.HadException())
      return;
  } else {
    defaultDoubleArg = 0.5;
  }

  impl->voidMethodDefaultDoubleArg(defaultDoubleArg);
}

static void VoidMethodDefaultTrueBooleanArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodDefaultTrueBooleanArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  bool defaultBooleanArg;
  if (!info[0]->IsUndefined()) {
    defaultBooleanArg = NativeValueTraits<IDLBoolean>::NativeValue(info.GetIsolate(), info[0], exceptionState);
    if (exceptionState.HadException())
      return;
  } else {
    defaultBooleanArg = true;
  }

  impl->voidMethodDefaultTrueBooleanArg(defaultBooleanArg);
}

static void VoidMethodDefaultFalseBooleanArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodDefaultFalseBooleanArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  bool defaultBooleanArg;
  if (!info[0]->IsUndefined()) {
    defaultBooleanArg = NativeValueTraits<IDLBoolean>::NativeValue(info.GetIsolate(), info[0], exceptionState);
    if (exceptionState.HadException())
      return;
  } else {
    defaultBooleanArg = false;
  }

  impl->voidMethodDefaultFalseBooleanArg(defaultBooleanArg);
}

static void VoidMethodDefaultNullableByteStringArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodDefaultNullableByteStringArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8StringResource<kTreatNullAndUndefinedAsNullString> defaultStringArg;
  if (!info[0]->IsUndefined()) {
    defaultStringArg = NativeValueTraits<IDLByteStringOrNull>::NativeValue(info.GetIsolate(), info[0], exceptionState);
    if (exceptionState.HadException())
      return;
  } else {
    defaultStringArg = nullptr;
  }

  impl->voidMethodDefaultNullableByteStringArg(defaultStringArg);
}

static void VoidMethodDefaultNullableStringArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8StringResource<kTreatNullAndUndefinedAsNullString> defaultStringArg;
  if (!info[0]->IsUndefined()) {
    defaultStringArg = info[0];
    if (!defaultStringArg.Prepare())
      return;
  } else {
    defaultStringArg = nullptr;
  }

  impl->voidMethodDefaultNullableStringArg(defaultStringArg);
}

static void VoidMethodDefaultNullableTestInterfaceArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  TestInterfaceImplementation* defaultTestInterfaceArg;
  if (!info[0]->IsUndefined()) {
    defaultTestInterfaceArg = V8TestInterface::ToImplWithTypeCheck(info.GetIsolate(), info[0]);
    if (!defaultTestInterfaceArg && !IsUndefinedOrNull(info[0])) {
      V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodDefaultNullableTestInterfaceArg", "TestObject", "parameter 1 is not of type 'TestInterface'."));
      return;
    }
  } else {
    defaultTestInterfaceArg = nullptr;
  }

  impl->voidMethodDefaultNullableTestInterfaceArg(defaultTestInterfaceArg);
}

static void VoidMethodDefaultDoubleOrStringArgsMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodDefaultDoubleOrStringArgs");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  DoubleOrString defaultLongArg;
  DoubleOrString defaultStringArg;
  DoubleOrString defaultNullArg;
  if (!info[0]->IsUndefined()) {
    V8DoubleOrString::ToImpl(info.GetIsolate(), info[0], defaultLongArg, UnionTypeConversionMode::kNotNullable, exceptionState);
    if (exceptionState.HadException())
      return;
  } else {
    defaultLongArg.SetDouble(10);
  }
  if (!info[1]->IsUndefined()) {
    V8DoubleOrString::ToImpl(info.GetIsolate(), info[1], defaultStringArg, UnionTypeConversionMode::kNullable, exceptionState);
    if (exceptionState.HadException())
      return;
  } else {
    defaultStringArg.SetString("foo");
  }
  if (!info[2]->IsUndefined()) {
    V8DoubleOrString::ToImpl(info.GetIsolate(), info[2], defaultNullArg, UnionTypeConversionMode::kNullable, exceptionState);
    if (exceptionState.HadException())
      return;
  } else {
    /* null default value */;
  }

  impl->voidMethodDefaultDoubleOrStringArgs(defaultLongArg, defaultStringArg, defaultNullArg);
}

static void VoidMethodDefaultStringSequenceArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodDefaultStringSequenceArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  Vector<String> defaultStringSequenceArg;
  if (!info[0]->IsUndefined()) {
    defaultStringSequenceArg = NativeValueTraits<IDLSequence<IDLString>>::NativeValue(info.GetIsolate(), info[0], exceptionState);
    if (exceptionState.HadException())
      return;
  } else {
    /* Nothing to do */;
  }

  impl->voidMethodDefaultStringSequenceArg(defaultStringSequenceArg);
}

static void VoidMethodVariadicStringArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodVariadicStringArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  Vector<String> variadicStringArgs;
  variadicStringArgs = ToImplArguments<IDLString>(info, 0, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodVariadicStringArg(variadicStringArgs);
}

static void VoidMethodStringArgVariadicStringArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodStringArgVariadicStringArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  V8StringResource<> stringArg;
  Vector<String> variadicStringArgs;
  stringArg = info[0];
  if (!stringArg.Prepare())
    return;

  variadicStringArgs = ToImplArguments<IDLString>(info, 1, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodStringArgVariadicStringArg(stringArg, variadicStringArgs);
}

static void VoidMethodVariadicTestInterfaceEmptyArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodVariadicTestInterfaceEmptyArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  HeapVector<Member<TestInterfaceEmpty>> variadicTestInterfaceEmptyArgs;
  for (int i = 0; i < info.Length(); ++i) {
    if (!V8TestInterfaceEmpty::HasInstance(info[i], info.GetIsolate())) {
      exceptionState.ThrowTypeError("parameter 1 is not of type 'TestInterfaceEmpty'.");
      return;
    }
    variadicTestInterfaceEmptyArgs.push_back(V8TestInterfaceEmpty::ToImpl(v8::Local<v8::Object>::Cast(info[i])));
  }

  impl->voidMethodVariadicTestInterfaceEmptyArg(variadicTestInterfaceEmptyArgs);
}

static void VoidMethodTestInterfaceEmptyArgVariadicTestInterfaceEmptyArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodTestInterfaceEmptyArgVariadicTestInterfaceEmptyArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  TestInterfaceEmpty* testInterfaceEmptyArg;
  HeapVector<Member<TestInterfaceEmpty>> variadicTestInterfaceEmptyArgs;
  testInterfaceEmptyArg = V8TestInterfaceEmpty::ToImplWithTypeCheck(info.GetIsolate(), info[0]);
  if (!testInterfaceEmptyArg) {
    exceptionState.ThrowTypeError("parameter 1 is not of type 'TestInterfaceEmpty'.");
    return;
  }

  for (int i = 1; i < info.Length(); ++i) {
    if (!V8TestInterfaceEmpty::HasInstance(info[i], info.GetIsolate())) {
      exceptionState.ThrowTypeError("parameter 2 is not of type 'TestInterfaceEmpty'.");
      return;
    }
    variadicTestInterfaceEmptyArgs.push_back(V8TestInterfaceEmpty::ToImpl(v8::Local<v8::Object>::Cast(info[i])));
  }

  impl->voidMethodTestInterfaceEmptyArgVariadicTestInterfaceEmptyArg(testInterfaceEmptyArg, variadicTestInterfaceEmptyArgs);
}

static void OverloadedMethodA1Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "overloadedMethodA");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  int32_t longArg;
  longArg = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->overloadedMethodA(longArg);
}

static void OverloadedMethodA2Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "overloadedMethodA");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  int32_t longArg1;
  int32_t longArg2;
  longArg1 = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  longArg2 = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[1], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->overloadedMethodA(longArg1, longArg2);
}

static void OverloadedMethodAMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  bool isArityError = false;

  switch (std::min(2, info.Length())) {
    case 1:
      if (true) {
        OverloadedMethodA1Method(info);
        return;
      }
      break;
    case 2:
      if (true) {
        OverloadedMethodA2Method(info);
        return;
      }
      break;
    default:
      isArityError = true;
  }

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "overloadedMethodA");
  if (isArityError) {
    if (info.Length() < 1) {
      exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
      return;
    }
  }
  exceptionState.ThrowTypeError("No function was found that matched the signature provided.");
}

static void OverloadedMethodB1Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "overloadedMethodB");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  int32_t longArg;
  longArg = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->overloadedMethodB(longArg);
}

static void OverloadedMethodB2Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "overloadedMethodB");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8StringResource<> stringArg;
  int32_t longArg;
  int numArgsPassed = info.Length();
  while (numArgsPassed > 0) {
    if (!info[numArgsPassed - 1]->IsUndefined())
      break;
    --numArgsPassed;
  }
  stringArg = info[0];
  if (!stringArg.Prepare())
    return;

  if (UNLIKELY(numArgsPassed <= 1)) {
    impl->overloadedMethodB(stringArg);
    return;
  }
  longArg = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[1], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->overloadedMethodB(stringArg, longArg);
}

static void OverloadedMethodBMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  bool isArityError = false;

  switch (std::min(2, info.Length())) {
    case 1:
      if (info[0]->IsNumber()) {
        OverloadedMethodB1Method(info);
        return;
      }
      if (true) {
        OverloadedMethodB2Method(info);
        return;
      }
      if (true) {
        OverloadedMethodB1Method(info);
        return;
      }
      break;
    case 2:
      if (true) {
        OverloadedMethodB2Method(info);
        return;
      }
      break;
    default:
      isArityError = true;
  }

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "overloadedMethodB");
  if (isArityError) {
    if (info.Length() < 1) {
      exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
      return;
    }
  }
  exceptionState.ThrowTypeError("No function was found that matched the signature provided.");
}

static void OverloadedMethodC1Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "overloadedMethodC");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  int32_t longArg;
  longArg = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->overloadedMethodC(longArg);
}

static void OverloadedMethodC2Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  TestInterfaceEmpty* testInterfaceEmptyArg;
  testInterfaceEmptyArg = V8TestInterfaceEmpty::ToImplWithTypeCheck(info.GetIsolate(), info[0]);
  if (!testInterfaceEmptyArg) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("overloadedMethodC", "TestObject", "parameter 1 is not of type 'TestInterfaceEmpty'."));
    return;
  }

  impl->overloadedMethodC(testInterfaceEmptyArg);
}

static void OverloadedMethodCMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  bool isArityError = false;

  switch (std::min(1, info.Length())) {
    case 1:
      if (V8TestInterfaceEmpty::HasInstance(info[0], info.GetIsolate())) {
        OverloadedMethodC2Method(info);
        return;
      }
      if (info[0]->IsNumber()) {
        OverloadedMethodC1Method(info);
        return;
      }
      if (true) {
        OverloadedMethodC1Method(info);
        return;
      }
      break;
    default:
      isArityError = true;
  }

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "overloadedMethodC");
  if (isArityError) {
    if (info.Length() < 1) {
      exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
      return;
    }
  }
  exceptionState.ThrowTypeError("No function was found that matched the signature provided.");
}

static void OverloadedMethodD1Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "overloadedMethodD");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  int32_t longArg;
  longArg = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->overloadedMethodD(longArg);
}

static void OverloadedMethodD2Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "overloadedMethodD");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  Vector<int32_t> longArraySequence;
  longArraySequence = NativeValueTraits<IDLSequence<IDLLong>>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->overloadedMethodD(longArraySequence);
}

static void OverloadedMethodDMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  bool isArityError = false;

  switch (std::min(1, info.Length())) {
    case 1:
      if (info[0]->IsArray()) {
        OverloadedMethodD2Method(info);
        return;
      }
      {
        ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext,
                                      "TestObject", "overloadedMethodD");
        if (HasCallableIteratorSymbol(info.GetIsolate(), info[0], exceptionState)) {
          OverloadedMethodD2Method(info);
          return;
        }
        if (exceptionState.HadException()) {
          exceptionState.RethrowV8Exception(exceptionState.GetException());
          return;
        }
      }
      if (info[0]->IsNumber()) {
        OverloadedMethodD1Method(info);
        return;
      }
      if (true) {
        OverloadedMethodD1Method(info);
        return;
      }
      break;
    default:
      isArityError = true;
  }

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "overloadedMethodD");
  if (isArityError) {
    if (info.Length() < 1) {
      exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
      return;
    }
  }
  exceptionState.ThrowTypeError("No function was found that matched the signature provided.");
}

static void OverloadedMethodE1Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "overloadedMethodE");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  int32_t longArg;
  longArg = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->overloadedMethodE(longArg);
}

static void OverloadedMethodE2Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  TestInterfaceEmpty* testInterfaceEmptyOrNullArg;
  testInterfaceEmptyOrNullArg = V8TestInterfaceEmpty::ToImplWithTypeCheck(info.GetIsolate(), info[0]);
  if (!testInterfaceEmptyOrNullArg && !IsUndefinedOrNull(info[0])) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("overloadedMethodE", "TestObject", "parameter 1 is not of type 'TestInterfaceEmpty'."));
    return;
  }

  impl->overloadedMethodE(testInterfaceEmptyOrNullArg);
}

static void OverloadedMethodEMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  bool isArityError = false;

  switch (std::min(1, info.Length())) {
    case 1:
      if (IsUndefinedOrNull(info[0])) {
        OverloadedMethodE2Method(info);
        return;
      }
      if (V8TestInterfaceEmpty::HasInstance(info[0], info.GetIsolate())) {
        OverloadedMethodE2Method(info);
        return;
      }
      if (info[0]->IsNumber()) {
        OverloadedMethodE1Method(info);
        return;
      }
      if (true) {
        OverloadedMethodE1Method(info);
        return;
      }
      break;
    default:
      isArityError = true;
  }

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "overloadedMethodE");
  if (isArityError) {
    if (info.Length() < 1) {
      exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
      return;
    }
  }
  exceptionState.ThrowTypeError("No function was found that matched the signature provided.");
}

static void OverloadedMethodF1Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8StringResource<> stringArg;
  int numArgsPassed = info.Length();
  while (numArgsPassed > 0) {
    if (!info[numArgsPassed - 1]->IsUndefined())
      break;
    --numArgsPassed;
  }
  if (UNLIKELY(numArgsPassed <= 0)) {
    impl->overloadedMethodF();
    return;
  }
  stringArg = info[0];
  if (!stringArg.Prepare())
    return;

  impl->overloadedMethodF(stringArg);
}

static void OverloadedMethodF2Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "overloadedMethodF");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  double doubleArg;
  doubleArg = NativeValueTraits<IDLDouble>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->overloadedMethodF(doubleArg);
}

static void OverloadedMethodFMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  bool isArityError = false;

  switch (std::min(1, info.Length())) {
    case 0:
      if (true) {
        OverloadedMethodF1Method(info);
        return;
      }
      break;
    case 1:
      if (info[0]->IsUndefined()) {
        OverloadedMethodF1Method(info);
        return;
      }
      if (info[0]->IsNumber()) {
        OverloadedMethodF2Method(info);
        return;
      }
      if (true) {
        OverloadedMethodF1Method(info);
        return;
      }
      if (true) {
        OverloadedMethodF2Method(info);
        return;
      }
      break;
    default:
      isArityError = true;
  }

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "overloadedMethodF");
  if (isArityError) {
  }
  exceptionState.ThrowTypeError("No function was found that matched the signature provided.");
}

static void OverloadedMethodG1Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "overloadedMethodG");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  int32_t longArg;
  longArg = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->overloadedMethodG(longArg);
}

static void OverloadedMethodG2Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  TestInterfaceEmpty* testInterfaceEmptyOrNullArg;
  if (!info[0]->IsUndefined()) {
    testInterfaceEmptyOrNullArg = V8TestInterfaceEmpty::ToImplWithTypeCheck(info.GetIsolate(), info[0]);
    if (!testInterfaceEmptyOrNullArg && !IsUndefinedOrNull(info[0])) {
      V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("overloadedMethodG", "TestObject", "parameter 1 is not of type 'TestInterfaceEmpty'."));
      return;
    }
  } else {
    testInterfaceEmptyOrNullArg = nullptr;
  }

  impl->overloadedMethodG(testInterfaceEmptyOrNullArg);
}

static void OverloadedMethodGMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  bool isArityError = false;

  switch (std::min(1, info.Length())) {
    case 0:
      if (true) {
        OverloadedMethodG2Method(info);
        return;
      }
      break;
    case 1:
      if (info[0]->IsUndefined()) {
        OverloadedMethodG2Method(info);
        return;
      }
      if (IsUndefinedOrNull(info[0])) {
        OverloadedMethodG2Method(info);
        return;
      }
      if (V8TestInterfaceEmpty::HasInstance(info[0], info.GetIsolate())) {
        OverloadedMethodG2Method(info);
        return;
      }
      if (info[0]->IsNumber()) {
        OverloadedMethodG1Method(info);
        return;
      }
      if (true) {
        OverloadedMethodG1Method(info);
        return;
      }
      break;
    default:
      isArityError = true;
  }

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "overloadedMethodG");
  if (isArityError) {
  }
  exceptionState.ThrowTypeError("No function was found that matched the signature provided.");
}

static void OverloadedMethodH1Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  TestInterfaceImplementation* testInterfaceArg;
  testInterfaceArg = V8TestInterface::ToImplWithTypeCheck(info.GetIsolate(), info[0]);
  if (!testInterfaceArg) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("overloadedMethodH", "TestObject", "parameter 1 is not of type 'TestInterface'."));
    return;
  }

  impl->overloadedMethodH(testInterfaceArg);
}

static void OverloadedMethodH2Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  TestInterfaceEmpty* testInterfaceEmptyArg;
  testInterfaceEmptyArg = V8TestInterfaceEmpty::ToImplWithTypeCheck(info.GetIsolate(), info[0]);
  if (!testInterfaceEmptyArg) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("overloadedMethodH", "TestObject", "parameter 1 is not of type 'TestInterfaceEmpty'."));
    return;
  }

  impl->overloadedMethodH(testInterfaceEmptyArg);
}

static void OverloadedMethodHMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  bool isArityError = false;

  switch (std::min(1, info.Length())) {
    case 1:
      if (V8TestInterface::HasInstance(info[0], info.GetIsolate())) {
        OverloadedMethodH1Method(info);
        return;
      }
      if (V8TestInterfaceEmpty::HasInstance(info[0], info.GetIsolate())) {
        OverloadedMethodH2Method(info);
        return;
      }
      break;
    default:
      isArityError = true;
  }

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "overloadedMethodH");
  if (isArityError) {
    if (info.Length() < 1) {
      exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
      return;
    }
  }
  exceptionState.ThrowTypeError("No function was found that matched the signature provided.");
}

static void OverloadedMethodI1Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8StringResource<> stringArg;
  stringArg = info[0];
  if (!stringArg.Prepare())
    return;

  impl->overloadedMethodI(stringArg);
}

static void OverloadedMethodI2Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "overloadedMethodI");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  double doubleArg;
  doubleArg = NativeValueTraits<IDLDouble>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->overloadedMethodI(doubleArg);
}

static void OverloadedMethodIMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  bool isArityError = false;

  switch (std::min(1, info.Length())) {
    case 1:
      if (info[0]->IsNumber()) {
        OverloadedMethodI2Method(info);
        return;
      }
      if (true) {
        OverloadedMethodI1Method(info);
        return;
      }
      if (true) {
        OverloadedMethodI2Method(info);
        return;
      }
      break;
    default:
      isArityError = true;
  }

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "overloadedMethodI");
  if (isArityError) {
    if (info.Length() < 1) {
      exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
      return;
    }
  }
  exceptionState.ThrowTypeError("No function was found that matched the signature provided.");
}

static void OverloadedMethodJ1Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8StringResource<> stringArg;
  stringArg = info[0];
  if (!stringArg.Prepare())
    return;

  impl->overloadedMethodJ(stringArg);
}

static void OverloadedMethodJ2Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "overloadedMethodJ");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  TestDictionary* testDictionaryArg;
  if (!info[0]->IsNullOrUndefined() && !info[0]->IsObject()) {
    exceptionState.ThrowTypeError("parameter 1 ('testDictionaryArg') is not an object.");
    return;
  }
  testDictionaryArg = NativeValueTraits<TestDictionary>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->overloadedMethodJ(testDictionaryArg);
}

static void OverloadedMethodJMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  bool isArityError = false;

  switch (std::min(1, info.Length())) {
    case 1:
      if (IsUndefinedOrNull(info[0])) {
        OverloadedMethodJ2Method(info);
        return;
      }
      if (info[0]->IsObject()) {
        OverloadedMethodJ2Method(info);
        return;
      }
      if (true) {
        OverloadedMethodJ1Method(info);
        return;
      }
      break;
    default:
      isArityError = true;
  }

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "overloadedMethodJ");
  if (isArityError) {
    if (info.Length() < 1) {
      exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
      return;
    }
  }
  exceptionState.ThrowTypeError("No function was found that matched the signature provided.");
}

static void OverloadedMethodK1Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  ScriptValue functionArg;
  if (info[0]->IsFunction()) {
    functionArg = ScriptValue(ScriptState::Current(info.GetIsolate()), info[0]);
  } else {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("overloadedMethodK", "TestObject", "The callback provided as parameter 1 is not a function."));
    return;
  }

  impl->overloadedMethodK(functionArg);
}

static void OverloadedMethodK2Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8StringResource<> stringArg;
  stringArg = info[0];
  if (!stringArg.Prepare())
    return;

  impl->overloadedMethodK(stringArg);
}

static void OverloadedMethodKMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  bool isArityError = false;

  switch (std::min(1, info.Length())) {
    case 1:
      if (info[0]->IsFunction()) {
        OverloadedMethodK1Method(info);
        return;
      }
      if (true) {
        OverloadedMethodK2Method(info);
        return;
      }
      break;
    default:
      isArityError = true;
  }

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "overloadedMethodK");
  if (isArityError) {
    if (info.Length() < 1) {
      exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
      return;
    }
  }
  exceptionState.ThrowTypeError("No function was found that matched the signature provided.");
}

static void OverloadedMethodL1Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "overloadedMethodL");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  int32_t longArg;
  Vector<ScriptValue> restArgs;
  longArg = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  restArgs = ToImplArguments<ScriptValue>(info, 1, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->overloadedMethodL(longArg, restArgs);
}

static void OverloadedMethodL2Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "overloadedMethodL");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8StringResource<> stringArg;
  Vector<ScriptValue> restArgs;
  stringArg = info[0];
  if (!stringArg.Prepare())
    return;

  restArgs = ToImplArguments<ScriptValue>(info, 1, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->overloadedMethodL(stringArg, restArgs);
}

static void OverloadedMethodLMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  bool isArityError = false;

  switch (std::min(2, info.Length())) {
    case 1:
      if (info[0]->IsNumber()) {
        OverloadedMethodL1Method(info);
        return;
      }
      if (true) {
        OverloadedMethodL2Method(info);
        return;
      }
      if (true) {
        OverloadedMethodL1Method(info);
        return;
      }
      break;
    case 2:
      if (info[0]->IsNumber()) {
        OverloadedMethodL1Method(info);
        return;
      }
      if (true) {
        OverloadedMethodL2Method(info);
        return;
      }
      if (true) {
        OverloadedMethodL1Method(info);
        return;
      }
      break;
    default:
      isArityError = true;
  }

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "overloadedMethodL");
  if (isArityError) {
    if (info.Length() < 1) {
      exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
      return;
    }
  }
  exceptionState.ThrowTypeError("No function was found that matched the signature provided.");
}

static void OverloadedMethodN1Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  TestInterfaceImplementation* testInterfaceArg;
  testInterfaceArg = V8TestInterface::ToImplWithTypeCheck(info.GetIsolate(), info[0]);
  if (!testInterfaceArg) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("overloadedMethodN", "TestObject", "parameter 1 is not of type 'TestInterface'."));
    return;
  }

  impl->overloadedMethodN(testInterfaceArg);
}

static void OverloadedMethodN2Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "overloadedMethodN");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8TestCallbackInterface* testCallbackInterfaceArg;
  if (info[0]->IsObject()) {
    testCallbackInterfaceArg = V8TestCallbackInterface::CreateOrNull(info[0].As<v8::Object>());
    if (!testCallbackInterfaceArg) {
      exceptionState.ThrowSecurityError("The callback provided as parameter 1 is a cross origin object.");
      return;
    }
  } else {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("overloadedMethodN", "TestObject", "The callback provided as parameter 1 is not an object."));
    return;
  }

  impl->overloadedMethodN(testCallbackInterfaceArg);
}

static void OverloadedMethodNMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  bool isArityError = false;

  switch (std::min(1, info.Length())) {
    case 1:
      if (V8TestInterface::HasInstance(info[0], info.GetIsolate())) {
        OverloadedMethodN1Method(info);
        return;
      }
      if (info[0]->IsObject()) {
        OverloadedMethodN2Method(info);
        return;
      }
      break;
    default:
      isArityError = true;
  }

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "overloadedMethodN");
  if (isArityError) {
    if (info.Length() < 1) {
      exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
      return;
    }
  }
  exceptionState.ThrowTypeError("No function was found that matched the signature provided.");
}

static void PromiseOverloadMethod1Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "promiseOverloadMethod");
  ExceptionToRejectPromiseScope rejectPromiseScope(info, exceptionState);

  // V8DOMConfiguration::kDoNotCheckHolder
  // Make sure that info.Holder() really points to an instance of the type.
  if (!V8TestObject::HasInstance(info.Holder(), info.GetIsolate())) {
    exceptionState.ThrowTypeError("Illegal invocation");
    return;
  }
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8SetReturnValue(info, impl->promiseOverloadMethod().V8Value());
}

static void PromiseOverloadMethod2Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "promiseOverloadMethod");
  ExceptionToRejectPromiseScope rejectPromiseScope(info, exceptionState);

  // V8DOMConfiguration::kDoNotCheckHolder
  // Make sure that info.Holder() really points to an instance of the type.
  if (!V8TestObject::HasInstance(info.Holder(), info.GetIsolate())) {
    exceptionState.ThrowTypeError("Illegal invocation");
    return;
  }
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  DOMWindow* arg1;
  double arg2;
  arg1 = ToDOMWindow(info.GetIsolate(), info[0]);
  if (!arg1) {
    exceptionState.ThrowTypeError("parameter 1 is not of type 'Window'.");
    return;
  }

  arg2 = NativeValueTraits<IDLDouble>::NativeValue(info.GetIsolate(), info[1], exceptionState);
  if (exceptionState.HadException())
    return;

  V8SetReturnValue(info, impl->promiseOverloadMethod(arg1, arg2).V8Value());
}

static void PromiseOverloadMethod3Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "promiseOverloadMethod");
  ExceptionToRejectPromiseScope rejectPromiseScope(info, exceptionState);

  // V8DOMConfiguration::kDoNotCheckHolder
  // Make sure that info.Holder() really points to an instance of the type.
  if (!V8TestObject::HasInstance(info.Holder(), info.GetIsolate())) {
    exceptionState.ThrowTypeError("Illegal invocation");
    return;
  }
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  Document* arg1;
  double arg2;
  arg1 = V8Document::ToImplWithTypeCheck(info.GetIsolate(), info[0]);
  if (!arg1) {
    exceptionState.ThrowTypeError("parameter 1 is not of type 'Document'.");
    return;
  }

  arg2 = NativeValueTraits<IDLDouble>::NativeValue(info.GetIsolate(), info[1], exceptionState);
  if (exceptionState.HadException())
    return;

  V8SetReturnValue(info, impl->promiseOverloadMethod(arg1, arg2).V8Value());
}

static void PromiseOverloadMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  bool isArityError = false;

  switch (std::min(2, info.Length())) {
    case 0:
      if (true) {
        PromiseOverloadMethod1Method(info);
        return;
      }
      break;
    case 2:
      if (V8Window::HasInstance(info[0], info.GetIsolate())) {
        PromiseOverloadMethod2Method(info);
        return;
      }
      if (V8Document::HasInstance(info[0], info.GetIsolate())) {
        PromiseOverloadMethod3Method(info);
        return;
      }
      break;
    default:
      isArityError = true;
  }

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "promiseOverloadMethod");
  ExceptionToRejectPromiseScope rejectPromiseScope(info, exceptionState);
  if (isArityError) {
    if (info.Length() >= 0) {
      exceptionState.ThrowTypeError(ExceptionMessages::InvalidArity("[0, 2]", info.Length()));
      return;
    }
  }
  exceptionState.ThrowTypeError("No function was found that matched the signature provided.");
}

static void OverloadedPerWorldBindingsMethod1Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  impl->overloadedPerWorldBindingsMethod();
}

static void OverloadedPerWorldBindingsMethod1MethodForMainWorld(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  impl->overloadedPerWorldBindingsMethod();
}

static void OverloadedPerWorldBindingsMethod2Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "overloadedPerWorldBindingsMethod");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  int32_t longArg;
  longArg = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->overloadedPerWorldBindingsMethod(longArg);
}

static void OverloadedPerWorldBindingsMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  bool isArityError = false;

  switch (std::min(1, info.Length())) {
    case 0:
      if (true) {
        OverloadedPerWorldBindingsMethod1Method(info);
        return;
      }
      break;
    case 1:
      if (true) {
        OverloadedPerWorldBindingsMethod2Method(info);
        return;
      }
      break;
    default:
      isArityError = true;
  }

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "overloadedPerWorldBindingsMethod");
  if (isArityError) {
  }
  exceptionState.ThrowTypeError("No function was found that matched the signature provided.");
}

static void OverloadedPerWorldBindingsMethod2MethodForMainWorld(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "overloadedPerWorldBindingsMethod");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  int32_t longArg;
  longArg = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->overloadedPerWorldBindingsMethod(longArg);
}

static void OverloadedPerWorldBindingsMethodMethodForMainWorld(const v8::FunctionCallbackInfo<v8::Value>& info) {
  bool isArityError = false;

  switch (std::min(1, info.Length())) {
    case 0:
      if (true) {
        OverloadedPerWorldBindingsMethod1MethodForMainWorld(info);
        return;
      }
      break;
    case 1:
      if (true) {
        OverloadedPerWorldBindingsMethod2MethodForMainWorld(info);
        return;
      }
      break;
    default:
      isArityError = true;
  }

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "overloadedPerWorldBindingsMethod");
  if (isArityError) {
  }
  exceptionState.ThrowTypeError("No function was found that matched the signature provided.");
}

static void OverloadedStaticMethod1Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "overloadedStaticMethod");

  int32_t longArg;
  longArg = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  TestObject::overloadedStaticMethod(longArg);
}

static void OverloadedStaticMethod2Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "overloadedStaticMethod");

  int32_t longArg1;
  int32_t longArg2;
  longArg1 = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  longArg2 = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[1], exceptionState);
  if (exceptionState.HadException())
    return;

  TestObject::overloadedStaticMethod(longArg1, longArg2);
}

static void OverloadedStaticMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  bool isArityError = false;

  switch (std::min(2, info.Length())) {
    case 1:
      if (true) {
        OverloadedStaticMethod1Method(info);
        return;
      }
      break;
    case 2:
      if (true) {
        OverloadedStaticMethod2Method(info);
        return;
      }
      break;
    default:
      isArityError = true;
  }

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "overloadedStaticMethod");
  if (isArityError) {
    if (info.Length() < 1) {
      exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
      return;
    }
  }
  exceptionState.ThrowTypeError("No function was found that matched the signature provided.");
}

static void ItemMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "item");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  uint32_t index;
  index = NativeValueTraits<IDLUnsignedLong>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  ScriptValue result = impl->item(scriptState, index);
  V8SetReturnValue(info, result.V8Value());
}

static void SetItemMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "setItem");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);

  if (UNLIKELY(info.Length() < 2)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(2, info.Length()));
    return;
  }

  uint32_t index;
  V8StringResource<> value;
  index = NativeValueTraits<IDLUnsignedLong>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  value = info[1];
  if (!value.Prepare())
    return;

  String result = impl->setItem(scriptState, index, value);
  V8SetReturnValueString(info, result, info.GetIsolate());
}

static void VoidMethodClampUnsignedShortArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodClampUnsignedShortArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  uint16_t clampUnsignedShortArg;
  clampUnsignedShortArg = NativeValueTraits<IDLUnsignedShortClamp>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodClampUnsignedShortArg(clampUnsignedShortArg);
}

static void VoidMethodClampUnsignedLongArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodClampUnsignedLongArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  uint32_t clampUnsignedLongArg;
  clampUnsignedLongArg = NativeValueTraits<IDLUnsignedLongClamp>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodClampUnsignedLongArg(clampUnsignedLongArg);
}

static void VoidMethodDefaultUndefinedTestInterfaceEmptyArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  TestInterfaceEmpty* defaultUndefinedTestInterfaceEmptyArg;
  defaultUndefinedTestInterfaceEmptyArg = V8TestInterfaceEmpty::ToImplWithTypeCheck(info.GetIsolate(), info[0]);
  if (!defaultUndefinedTestInterfaceEmptyArg) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodDefaultUndefinedTestInterfaceEmptyArg", "TestObject", "parameter 1 is not of type 'TestInterfaceEmpty'."));
    return;
  }

  impl->voidMethodDefaultUndefinedTestInterfaceEmptyArg(defaultUndefinedTestInterfaceEmptyArg);
}

static void VoidMethodDefaultUndefinedLongArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodDefaultUndefinedLongArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  int32_t defaultUndefinedLongArg;
  defaultUndefinedLongArg = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodDefaultUndefinedLongArg(defaultUndefinedLongArg);
}

static void VoidMethodDefaultUndefinedStringArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8StringResource<> defaultUndefinedStringArg;
  defaultUndefinedStringArg = info[0];
  if (!defaultUndefinedStringArg.Prepare())
    return;

  impl->voidMethodDefaultUndefinedStringArg(defaultUndefinedStringArg);
}

static void VoidMethodEnforceRangeLongArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "voidMethodEnforceRangeLongArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  int32_t enforceRangeLongArg;
  enforceRangeLongArg = NativeValueTraits<IDLLongEnforceRange>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodEnforceRangeLongArg(enforceRangeLongArg);
}

static void VoidMethodTreatNullAsEmptyStringStringArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodTreatNullAsEmptyStringStringArg", "TestObject", ExceptionMessages::NotEnoughArguments(1, info.Length())));
    return;
  }

  V8StringResource<kTreatNullAsEmptyString> treatNullAsEmptyStringStringArg;
  treatNullAsEmptyStringStringArg = info[0];
  if (!treatNullAsEmptyStringStringArg.Prepare())
    return;

  impl->voidMethodTreatNullAsEmptyStringStringArg(treatNullAsEmptyStringStringArg);
}

static void ActivityLoggingAccessForAllWorldsMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  impl->activityLoggingAccessForAllWorldsMethod();
}

static void CallWithExecutionContextVoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  ExecutionContext* executionContext = ExecutionContext::ForRelevantRealm(info);
  impl->callWithExecutionContextVoidMethod(executionContext);
}

static void CallWithScriptStateVoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);

  impl->callWithScriptStateVoidMethod(scriptState);
}

static void CallWithScriptStateLongMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);

  int32_t result = impl->callWithScriptStateLongMethod(scriptState);
  V8SetReturnValueInt(info, result);
}

static void CallWithScriptStateExecutionContextVoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);

  ExecutionContext* executionContext = ExecutionContext::ForRelevantRealm(info);
  impl->callWithScriptStateExecutionContextVoidMethod(scriptState, executionContext);
}

static void CallWithScriptStateScriptArgumentsVoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);

  ScriptArguments* scriptArguments(ScriptArguments::Create(scriptState, info, 0));
  impl->callWithScriptStateScriptArgumentsVoidMethod(scriptState, scriptArguments);
}

static void CallWithScriptStateScriptArgumentsVoidMethodOptionalBooleanArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "callWithScriptStateScriptArgumentsVoidMethodOptionalBooleanArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);

  bool optionalBooleanArg;
  int numArgsPassed = info.Length();
  while (numArgsPassed > 0) {
    if (!info[numArgsPassed - 1]->IsUndefined())
      break;
    --numArgsPassed;
  }
  if (UNLIKELY(numArgsPassed <= 0)) {
    ScriptArguments* scriptArguments(ScriptArguments::Create(scriptState, info, 1));
    impl->callWithScriptStateScriptArgumentsVoidMethodOptionalBooleanArg(scriptState, scriptArguments);
    return;
  }
  optionalBooleanArg = NativeValueTraits<IDLBoolean>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  ScriptArguments* scriptArguments(ScriptArguments::Create(scriptState, info, 1));
  impl->callWithScriptStateScriptArgumentsVoidMethodOptionalBooleanArg(scriptState, scriptArguments, optionalBooleanArg);
}

static void CallWithCurrentWindowMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  impl->callWithCurrentWindow(CurrentDOMWindow(info.GetIsolate()));
}

static void CallWithCurrentWindowScriptWindowMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  impl->callWithCurrentWindowScriptWindow(CurrentDOMWindow(info.GetIsolate()), EnteredDOMWindow(info.GetIsolate()));
}

static void CallWithThisValueMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);

  impl->callWithThisValue(ScriptValue(scriptState, info.Holder()));
}

static void CheckSecurityForNodeVoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "checkSecurityForNodeVoidMethod");
  if (!BindingSecurity::ShouldAllowAccessTo(CurrentDOMWindow(info.GetIsolate()), impl->checkSecurityForNodeVoidMethod(), BindingSecurity::ErrorReportOption::kDoNotReport)) {
    UseCounter::Count(CurrentExecutionContext(info.GetIsolate()),
                      WebFeature::kCrossOriginTestObjectCheckSecurityForNodeVoidMethod);
    V8SetReturnValueNull(info);
    return;
  }

  impl->checkSecurityForNodeVoidMethod();
}

static void CustomCallPrologueVoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8TestObject::CustomCallPrologueVoidMethodMethodPrologueCustom(info, impl);
  impl->customCallPrologueVoidMethod();
}

static void CustomCallEpilogueVoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  impl->customCallEpilogueVoidMethod();
  V8TestObject::CustomCallEpilogueVoidMethodMethodEpilogueCustom(info, impl);
}

static void DeprecatedVoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  impl->deprecatedVoidMethod();
}

static void ImplementedAsVoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  impl->implementedAsMethodName();
}

static void MeasureAsVoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  impl->measureAsVoidMethod();
}

static void MeasureMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  impl->measureMethod();
}

static void MeasureOverloadedMethod1Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  impl->measureOverloadedMethod();
}

static void MeasureOverloadedMethod2Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "measureOverloadedMethod");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  int32_t arg;
  arg = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->measureOverloadedMethod(arg);
}

static void MeasureOverloadedMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  bool isArityError = false;

  switch (std::min(1, info.Length())) {
    case 0:
      if (true) {
        UseCounter::Count(CurrentExecutionContext(info.GetIsolate()), WebFeature::kV8TestObject_MeasureOverloadedMethod_Method);
        MeasureOverloadedMethod1Method(info);
        return;
      }
      break;
    case 1:
      if (true) {
        UseCounter::Count(CurrentExecutionContext(info.GetIsolate()), WebFeature::kV8TestObject_MeasureOverloadedMethod_Method);
        MeasureOverloadedMethod2Method(info);
        return;
      }
      break;
    default:
      isArityError = true;
  }

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "measureOverloadedMethod");
  if (isArityError) {
  }
  exceptionState.ThrowTypeError("No function was found that matched the signature provided.");
}

static void DeprecateAsOverloadedMethod1Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  impl->DeprecateAsOverloadedMethod();
}

static void DeprecateAsOverloadedMethod2Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "DeprecateAsOverloadedMethod");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  int32_t arg;
  arg = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->DeprecateAsOverloadedMethod(arg);
}

static void DeprecateAsOverloadedMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  bool isArityError = false;

  switch (std::min(1, info.Length())) {
    case 0:
      if (true) {
        Deprecation::CountDeprecation(CurrentExecutionContext(info.GetIsolate()), WebFeature::kTestFeatureA);
        DeprecateAsOverloadedMethod1Method(info);
        return;
      }
      break;
    case 1:
      if (true) {
        Deprecation::CountDeprecation(CurrentExecutionContext(info.GetIsolate()), WebFeature::kTestFeatureB);
        DeprecateAsOverloadedMethod2Method(info);
        return;
      }
      break;
    default:
      isArityError = true;
  }

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "DeprecateAsOverloadedMethod");
  if (isArityError) {
  }
  exceptionState.ThrowTypeError("No function was found that matched the signature provided.");
}

static void DeprecateAsSameValueOverloadedMethod1Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  impl->DeprecateAsSameValueOverloadedMethod();
}

static void DeprecateAsSameValueOverloadedMethod2Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "DeprecateAsSameValueOverloadedMethod");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  int32_t arg;
  arg = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->DeprecateAsSameValueOverloadedMethod(arg);
}

static void DeprecateAsSameValueOverloadedMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  Deprecation::CountDeprecation(CurrentExecutionContext(info.GetIsolate()), WebFeature::kTestFeature);

  bool isArityError = false;

  switch (std::min(1, info.Length())) {
    case 0:
      if (true) {
        DeprecateAsSameValueOverloadedMethod1Method(info);
        return;
      }
      break;
    case 1:
      if (true) {
        DeprecateAsSameValueOverloadedMethod2Method(info);
        return;
      }
      break;
    default:
      isArityError = true;
  }

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "DeprecateAsSameValueOverloadedMethod");
  if (isArityError) {
  }
  exceptionState.ThrowTypeError("No function was found that matched the signature provided.");
}

static void MeasureAsOverloadedMethod1Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  impl->measureAsOverloadedMethod();
}

static void MeasureAsOverloadedMethod2Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "measureAsOverloadedMethod");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  int32_t arg;
  arg = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->measureAsOverloadedMethod(arg);
}

static void MeasureAsOverloadedMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  bool isArityError = false;

  switch (std::min(1, info.Length())) {
    case 0:
      if (true) {
        UseCounter::Count(CurrentExecutionContext(info.GetIsolate()), WebFeature::kTestFeatureA);
        MeasureAsOverloadedMethod1Method(info);
        return;
      }
      break;
    case 1:
      if (true) {
        UseCounter::Count(CurrentExecutionContext(info.GetIsolate()), WebFeature::kTestFeatureB);
        MeasureAsOverloadedMethod2Method(info);
        return;
      }
      break;
    default:
      isArityError = true;
  }

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "measureAsOverloadedMethod");
  if (isArityError) {
  }
  exceptionState.ThrowTypeError("No function was found that matched the signature provided.");
}

static void MeasureAsSameValueOverloadedMethod1Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  impl->measureAsSameValueOverloadedMethod();
}

static void MeasureAsSameValueOverloadedMethod2Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "measureAsSameValueOverloadedMethod");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  int32_t arg;
  arg = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->measureAsSameValueOverloadedMethod(arg);
}

static void MeasureAsSameValueOverloadedMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  bool isArityError = false;

  switch (std::min(1, info.Length())) {
    case 0:
      if (true) {
        UseCounter::Count(CurrentExecutionContext(info.GetIsolate()), WebFeature::kTestFeature);
        MeasureAsSameValueOverloadedMethod1Method(info);
        return;
      }
      break;
    case 1:
      if (true) {
        UseCounter::Count(CurrentExecutionContext(info.GetIsolate()), WebFeature::kTestFeature);
        MeasureAsSameValueOverloadedMethod2Method(info);
        return;
      }
      break;
    default:
      isArityError = true;
  }

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "measureAsSameValueOverloadedMethod");
  if (isArityError) {
  }
  exceptionState.ThrowTypeError("No function was found that matched the signature provided.");
}

static void CeReactionsNotOverloadedMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "ceReactionsNotOverloadedMethod");
  CEReactionsScope ce_reactions_scope;

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  bool arg;
  arg = NativeValueTraits<IDLBoolean>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->ceReactionsNotOverloadedMethod(arg);
}

static void CeReactionsOverloadedMethod1Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  impl->ceReactionsOverloadedMethod();
}

static void CeReactionsOverloadedMethod2Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "ceReactionsOverloadedMethod");
  CEReactionsScope ce_reactions_scope;

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  int32_t arg;
  arg = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->ceReactionsOverloadedMethod(arg);
}

static void CeReactionsOverloadedMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  bool isArityError = false;

  switch (std::min(1, info.Length())) {
    case 0:
      if (true) {
        CeReactionsOverloadedMethod1Method(info);
        return;
      }
      break;
    case 1:
      if (true) {
        CeReactionsOverloadedMethod2Method(info);
        return;
      }
      break;
    default:
      isArityError = true;
  }

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "ceReactionsOverloadedMethod");
  if (isArityError) {
  }
  exceptionState.ThrowTypeError("No function was found that matched the signature provided.");
}

static void DeprecateAsMeasureAsSameValueOverloadedMethod1Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  impl->deprecateAsMeasureAsSameValueOverloadedMethod();
}

static void DeprecateAsMeasureAsSameValueOverloadedMethod2Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "deprecateAsMeasureAsSameValueOverloadedMethod");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  int32_t arg;
  arg = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->deprecateAsMeasureAsSameValueOverloadedMethod(arg);
}

static void DeprecateAsMeasureAsSameValueOverloadedMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  bool isArityError = false;

  switch (std::min(1, info.Length())) {
    case 0:
      if (true) {
        UseCounter::Count(CurrentExecutionContext(info.GetIsolate()), WebFeature::kTestFeature);
        Deprecation::CountDeprecation(CurrentExecutionContext(info.GetIsolate()), WebFeature::kTestFeatureA);
        DeprecateAsMeasureAsSameValueOverloadedMethod1Method(info);
        return;
      }
      break;
    case 1:
      if (true) {
        UseCounter::Count(CurrentExecutionContext(info.GetIsolate()), WebFeature::kTestFeature);
        Deprecation::CountDeprecation(CurrentExecutionContext(info.GetIsolate()), WebFeature::kTestFeatureB);
        DeprecateAsMeasureAsSameValueOverloadedMethod2Method(info);
        return;
      }
      break;
    default:
      isArityError = true;
  }

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "deprecateAsMeasureAsSameValueOverloadedMethod");
  if (isArityError) {
  }
  exceptionState.ThrowTypeError("No function was found that matched the signature provided.");
}

static void DeprecateAsSameValueMeasureAsOverloadedMethod1Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  impl->deprecateAsSameValueMeasureAsOverloadedMethod();
}

static void DeprecateAsSameValueMeasureAsOverloadedMethod2Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "deprecateAsSameValueMeasureAsOverloadedMethod");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  int32_t arg;
  arg = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->deprecateAsSameValueMeasureAsOverloadedMethod(arg);
}

static void DeprecateAsSameValueMeasureAsOverloadedMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  Deprecation::CountDeprecation(CurrentExecutionContext(info.GetIsolate()), WebFeature::kTestFeature);

  bool isArityError = false;

  switch (std::min(1, info.Length())) {
    case 0:
      if (true) {
        UseCounter::Count(CurrentExecutionContext(info.GetIsolate()), WebFeature::kTestFeatureA);
        DeprecateAsSameValueMeasureAsOverloadedMethod1Method(info);
        return;
      }
      break;
    case 1:
      if (true) {
        UseCounter::Count(CurrentExecutionContext(info.GetIsolate()), WebFeature::kTestFeatureB);
        DeprecateAsSameValueMeasureAsOverloadedMethod2Method(info);
        return;
      }
      break;
    default:
      isArityError = true;
  }

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "deprecateAsSameValueMeasureAsOverloadedMethod");
  if (isArityError) {
  }
  exceptionState.ThrowTypeError("No function was found that matched the signature provided.");
}

static void DeprecateAsSameValueMeasureAsSameValueOverloadedMethod1Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  impl->deprecateAsSameValueMeasureAsSameValueOverloadedMethod();
}

static void DeprecateAsSameValueMeasureAsSameValueOverloadedMethod2Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "deprecateAsSameValueMeasureAsSameValueOverloadedMethod");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  int32_t arg;
  arg = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->deprecateAsSameValueMeasureAsSameValueOverloadedMethod(arg);
}

static void DeprecateAsSameValueMeasureAsSameValueOverloadedMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  Deprecation::CountDeprecation(CurrentExecutionContext(info.GetIsolate()), WebFeature::kTestFeatureA);

  bool isArityError = false;

  switch (std::min(1, info.Length())) {
    case 0:
      if (true) {
        UseCounter::Count(CurrentExecutionContext(info.GetIsolate()), WebFeature::kTestFeatureB);
        DeprecateAsSameValueMeasureAsSameValueOverloadedMethod1Method(info);
        return;
      }
      break;
    case 1:
      if (true) {
        UseCounter::Count(CurrentExecutionContext(info.GetIsolate()), WebFeature::kTestFeatureB);
        DeprecateAsSameValueMeasureAsSameValueOverloadedMethod2Method(info);
        return;
      }
      break;
    default:
      isArityError = true;
  }

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "deprecateAsSameValueMeasureAsSameValueOverloadedMethod");
  if (isArityError) {
  }
  exceptionState.ThrowTypeError("No function was found that matched the signature provided.");
}

static void NotEnumerableVoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  impl->notEnumerableVoidMethod();
}

static void OriginTrialEnabledVoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  impl->originTrialEnabledVoidMethod();
}

static void PerWorldBindingsOriginTrialEnabledVoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  impl->perWorldBindingsOriginTrialEnabledVoidMethod();
}

static void PerWorldBindingsOriginTrialEnabledVoidMethodMethodForMainWorld(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  impl->perWorldBindingsOriginTrialEnabledVoidMethod();
}

static void PerWorldBindingsVoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  impl->perWorldBindingsVoidMethod();
}

static void PerWorldBindingsVoidMethodMethodForMainWorld(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  impl->perWorldBindingsVoidMethod();
}

static void PerWorldBindingsVoidMethodTestInterfaceEmptyArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("perWorldBindingsVoidMethodTestInterfaceEmptyArg", "TestObject", ExceptionMessages::NotEnoughArguments(1, info.Length())));
    return;
  }

  TestInterfaceEmpty* testInterfaceEmptyArg;
  testInterfaceEmptyArg = V8TestInterfaceEmpty::ToImplWithTypeCheck(info.GetIsolate(), info[0]);
  if (!testInterfaceEmptyArg) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("perWorldBindingsVoidMethodTestInterfaceEmptyArg", "TestObject", "parameter 1 is not of type 'TestInterfaceEmpty'."));
    return;
  }

  impl->perWorldBindingsVoidMethodTestInterfaceEmptyArg(testInterfaceEmptyArg);
}

static void PerWorldBindingsVoidMethodTestInterfaceEmptyArgMethodForMainWorld(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("perWorldBindingsVoidMethodTestInterfaceEmptyArg", "TestObject", ExceptionMessages::NotEnoughArguments(1, info.Length())));
    return;
  }

  TestInterfaceEmpty* testInterfaceEmptyArg;
  testInterfaceEmptyArg = V8TestInterfaceEmpty::ToImplWithTypeCheck(info.GetIsolate(), info[0]);
  if (!testInterfaceEmptyArg) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("perWorldBindingsVoidMethodTestInterfaceEmptyArg", "TestObject", "parameter 1 is not of type 'TestInterfaceEmpty'."));
    return;
  }

  impl->perWorldBindingsVoidMethodTestInterfaceEmptyArg(testInterfaceEmptyArg);
}

static void ActivityLoggingForAllWorldsPerWorldBindingsVoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  impl->activityLoggingForAllWorldsPerWorldBindingsVoidMethod();
}

static void ActivityLoggingForAllWorldsPerWorldBindingsVoidMethodMethodForMainWorld(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  impl->activityLoggingForAllWorldsPerWorldBindingsVoidMethod();
}

static void ActivityLoggingForIsolatedWorldsPerWorldBindingsVoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  impl->activityLoggingForIsolatedWorldsPerWorldBindingsVoidMethod();
}

static void ActivityLoggingForIsolatedWorldsPerWorldBindingsVoidMethodMethodForMainWorld(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  impl->activityLoggingForIsolatedWorldsPerWorldBindingsVoidMethod();
}

static void RaisesExceptionVoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "raisesExceptionVoidMethod");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  impl->raisesExceptionVoidMethod(exceptionState);
  if (exceptionState.HadException()) {
    return;
  }
}

static void RaisesExceptionStringMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "raisesExceptionStringMethod");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  String result = impl->raisesExceptionStringMethod(exceptionState);
  if (exceptionState.HadException()) {
    return;
  }
  V8SetReturnValueString(info, result, info.GetIsolate());
}

static void RaisesExceptionVoidMethodOptionalLongArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "raisesExceptionVoidMethodOptionalLongArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  int32_t optionalLongArg;
  int numArgsPassed = info.Length();
  while (numArgsPassed > 0) {
    if (!info[numArgsPassed - 1]->IsUndefined())
      break;
    --numArgsPassed;
  }
  if (UNLIKELY(numArgsPassed <= 0)) {
    impl->raisesExceptionVoidMethodOptionalLongArg(exceptionState);
    if (exceptionState.HadException()) {
      return;
    }
    return;
  }
  optionalLongArg = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->raisesExceptionVoidMethodOptionalLongArg(optionalLongArg, exceptionState);
  if (exceptionState.HadException()) {
    return;
  }
}

static void RaisesExceptionVoidMethodTestCallbackInterfaceArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "raisesExceptionVoidMethodTestCallbackInterfaceArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  V8TestCallbackInterface* testCallbackInterfaceArg;
  if (info[0]->IsObject()) {
    testCallbackInterfaceArg = V8TestCallbackInterface::CreateOrNull(info[0].As<v8::Object>());
    if (!testCallbackInterfaceArg) {
      exceptionState.ThrowSecurityError("The callback provided as parameter 1 is a cross origin object.");
      return;
    }
  } else {
    exceptionState.ThrowTypeError("The callback provided as parameter 1 is not an object.");
    return;
  }

  impl->raisesExceptionVoidMethodTestCallbackInterfaceArg(testCallbackInterfaceArg, exceptionState);
  if (exceptionState.HadException()) {
    return;
  }
}

static void RaisesExceptionVoidMethodOptionalTestCallbackInterfaceArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "raisesExceptionVoidMethodOptionalTestCallbackInterfaceArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8TestCallbackInterface* optionalTestCallbackInterfaceArg;
  if (info[0]->IsObject()) {
    optionalTestCallbackInterfaceArg = V8TestCallbackInterface::CreateOrNull(info[0].As<v8::Object>());
    if (!optionalTestCallbackInterfaceArg) {
      exceptionState.ThrowSecurityError("The callback provided as parameter 1 is a cross origin object.");
      return;
    }
  } else if (info[0]->IsUndefined()) {
    optionalTestCallbackInterfaceArg = nullptr;
  } else {
    exceptionState.ThrowTypeError("The callback provided as parameter 1 is not an object.");
    return;
  }

  impl->raisesExceptionVoidMethodOptionalTestCallbackInterfaceArg(optionalTestCallbackInterfaceArg, exceptionState);
  if (exceptionState.HadException()) {
    return;
  }
}

static void RaisesExceptionTestInterfaceEmptyVoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "raisesExceptionTestInterfaceEmptyVoidMethod");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  TestInterfaceEmpty* result = impl->raisesExceptionTestInterfaceEmptyVoidMethod(exceptionState);
  if (exceptionState.HadException()) {
    return;
  }
  V8SetReturnValue(info, result);
}

static void RaisesExceptionXPathNSResolverVoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "raisesExceptionXPathNSResolverVoidMethod");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  XPathNSResolver* result = impl->raisesExceptionXPathNSResolverVoidMethod(exceptionState);
  if (exceptionState.HadException()) {
    return;
  }
  V8SetReturnValue(info, result);
}

static void CallWithExecutionContextRaisesExceptionVoidMethodLongArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "callWithExecutionContextRaisesExceptionVoidMethodLongArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  int32_t longArg;
  longArg = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  ExecutionContext* executionContext = ExecutionContext::ForRelevantRealm(info);
  impl->callWithExecutionContextRaisesExceptionVoidMethodLongArg(executionContext, longArg, exceptionState);
  if (exceptionState.HadException()) {
    return;
  }
}

static void RuntimeEnabledVoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  impl->runtimeEnabledVoidMethod();
}

static void PerWorldBindingsRuntimeEnabledVoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  impl->perWorldBindingsRuntimeEnabledVoidMethod();
}

static void PerWorldBindingsRuntimeEnabledVoidMethodMethodForMainWorld(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  impl->perWorldBindingsRuntimeEnabledVoidMethod();
}

static void RuntimeEnabledOverloadedVoidMethod1Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8StringResource<> stringArg;
  stringArg = info[0];
  if (!stringArg.Prepare())
    return;

  impl->runtimeEnabledOverloadedVoidMethod(stringArg);
}

static void RuntimeEnabledOverloadedVoidMethod2Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "runtimeEnabledOverloadedVoidMethod");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  int32_t longArg;
  longArg = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->runtimeEnabledOverloadedVoidMethod(longArg);
}

static void RuntimeEnabledOverloadedVoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  bool isArityError = false;

  switch (std::min(1, info.Length())) {
    case 1:
      if (info[0]->IsNumber()) {
        RuntimeEnabledOverloadedVoidMethod2Method(info);
        return;
      }
      if (true) {
        RuntimeEnabledOverloadedVoidMethod1Method(info);
        return;
      }
      if (true) {
        RuntimeEnabledOverloadedVoidMethod2Method(info);
        return;
      }
      break;
    default:
      isArityError = true;
  }

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "runtimeEnabledOverloadedVoidMethod");
  if (isArityError) {
    if (info.Length() < 1) {
      exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
      return;
    }
  }
  exceptionState.ThrowTypeError("No function was found that matched the signature provided.");
}

static void PartiallyRuntimeEnabledOverloadedVoidMethod1Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8StringResource<> stringArg;
  stringArg = info[0];
  if (!stringArg.Prepare())
    return;

  impl->partiallyRuntimeEnabledOverloadedVoidMethod(stringArg);
}

static void PartiallyRuntimeEnabledOverloadedVoidMethod2Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  TestInterfaceImplementation* testInterfaceArg;
  testInterfaceArg = V8TestInterface::ToImplWithTypeCheck(info.GetIsolate(), info[0]);
  if (!testInterfaceArg) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("partiallyRuntimeEnabledOverloadedVoidMethod", "TestObject", "parameter 1 is not of type 'TestInterface'."));
    return;
  }

  impl->partiallyRuntimeEnabledOverloadedVoidMethod(testInterfaceArg);
}

static void PartiallyRuntimeEnabledOverloadedVoidMethod3Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "partiallyRuntimeEnabledOverloadedVoidMethod");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  int32_t longArg;
  V8StringResource<> stringArg;
  longArg = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  stringArg = info[1];
  if (!stringArg.Prepare())
    return;

  impl->partiallyRuntimeEnabledOverloadedVoidMethod(longArg, stringArg);
}

static void PartiallyRuntimeEnabledOverloadedVoidMethod4Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "partiallyRuntimeEnabledOverloadedVoidMethod");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  int32_t longArg;
  V8StringResource<> stringArg;
  TestInterfaceImplementation* testInterfaceArg;
  longArg = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  stringArg = info[1];
  if (!stringArg.Prepare())
    return;

  testInterfaceArg = V8TestInterface::ToImplWithTypeCheck(info.GetIsolate(), info[2]);
  if (!testInterfaceArg) {
    exceptionState.ThrowTypeError("parameter 3 is not of type 'TestInterface'.");
    return;
  }

  impl->partiallyRuntimeEnabledOverloadedVoidMethod(longArg, stringArg, testInterfaceArg);
}

static int PartiallyRuntimeEnabledOverloadedVoidMethodMethodLength() {
  if (RuntimeEnabledFeatures::FeatureName1Enabled()) {
    return 1;
  }
  if (RuntimeEnabledFeatures::FeatureName2Enabled()) {
    return 1;
  }
  return 2;
}

static int PartiallyRuntimeEnabledOverloadedVoidMethodMethodMaxArg() {
  if (RuntimeEnabledFeatures::FeatureName3Enabled()) {
    return 3;
  }
  return 2;
}

static void PartiallyRuntimeEnabledOverloadedVoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  bool isArityError = false;

  switch (std::min(test_object_v8_internal::PartiallyRuntimeEnabledOverloadedVoidMethodMethodMaxArg(), info.Length())) {
    case 1:
      if (RuntimeEnabledFeatures::FeatureName2Enabled()) {
        if (V8TestInterface::HasInstance(info[0], info.GetIsolate())) {
          PartiallyRuntimeEnabledOverloadedVoidMethod2Method(info);
          return;
        }
      }
      if (RuntimeEnabledFeatures::FeatureName1Enabled()) {
        if (true) {
          PartiallyRuntimeEnabledOverloadedVoidMethod1Method(info);
          return;
        }
      }
      break;
    case 2:
      if (true) {
        PartiallyRuntimeEnabledOverloadedVoidMethod3Method(info);
        return;
      }
      break;
    case 3:
      if (RuntimeEnabledFeatures::FeatureName3Enabled()) {
        if (true) {
          PartiallyRuntimeEnabledOverloadedVoidMethod4Method(info);
          return;
        }
      }
      break;
    default:
      isArityError = true;
  }

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "partiallyRuntimeEnabledOverloadedVoidMethod");
  if (isArityError) {
    if (info.Length() < test_object_v8_internal::PartiallyRuntimeEnabledOverloadedVoidMethodMethodLength()) {
      exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(test_object_v8_internal::PartiallyRuntimeEnabledOverloadedVoidMethodMethodLength(), info.Length()));
      return;
    }
  }
  exceptionState.ThrowTypeError("No function was found that matched the signature provided.");
}

static void LegacyInterfaceTypeCheckingVoidMethodTestInterfaceEmptyArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("legacyInterfaceTypeCheckingVoidMethodTestInterfaceEmptyArg", "TestObject", ExceptionMessages::NotEnoughArguments(1, info.Length())));
    return;
  }

  TestInterfaceEmpty* testInterfaceEmptyArg;
  testInterfaceEmptyArg = V8TestInterfaceEmpty::ToImplWithTypeCheck(info.GetIsolate(), info[0]);

  impl->legacyInterfaceTypeCheckingVoidMethodTestInterfaceEmptyArg(testInterfaceEmptyArg);
}

static void LegacyInterfaceTypeCheckingVoidMethodTestInterfaceEmptyVariadicArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "legacyInterfaceTypeCheckingVoidMethodTestInterfaceEmptyVariadicArg");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  HeapVector<Member<TestInterfaceEmpty>> testInterfaceEmptyArg;
  for (int i = 0; i < info.Length(); ++i) {
    if (!V8TestInterfaceEmpty::HasInstance(info[i], info.GetIsolate())) {
      exceptionState.ThrowTypeError("parameter 1 is not of type 'TestInterfaceEmpty'.");
      return;
    }
    testInterfaceEmptyArg.push_back(V8TestInterfaceEmpty::ToImpl(v8::Local<v8::Object>::Cast(info[i])));
  }

  impl->legacyInterfaceTypeCheckingVoidMethodTestInterfaceEmptyVariadicArg(testInterfaceEmptyArg);
}

static void UseToImpl4ArgumentsCheckingIfPossibleWithOptionalArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("useToImpl4ArgumentsCheckingIfPossibleWithOptionalArg", "TestObject", ExceptionMessages::NotEnoughArguments(1, info.Length())));
    return;
  }

  Node* node1;
  Node* node2;
  int numArgsPassed = info.Length();
  while (numArgsPassed > 0) {
    if (!info[numArgsPassed - 1]->IsUndefined())
      break;
    --numArgsPassed;
  }
  node1 = V8Node::ToImplWithTypeCheck(info.GetIsolate(), info[0]);
  if (!node1) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("useToImpl4ArgumentsCheckingIfPossibleWithOptionalArg", "TestObject", "parameter 1 is not of type 'Node'."));
    return;
  }

  if (UNLIKELY(numArgsPassed <= 1)) {
    impl->useToImpl4ArgumentsCheckingIfPossibleWithOptionalArg(node1);
    return;
  }
  node2 = V8Node::ToImplWithTypeCheck(info.GetIsolate(), info[1]);
  if (!node2) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("useToImpl4ArgumentsCheckingIfPossibleWithOptionalArg", "TestObject", "parameter 2 is not of type 'Node'."));
    return;
  }

  impl->useToImpl4ArgumentsCheckingIfPossibleWithOptionalArg(node1, node2);
}

static void UseToImpl4ArgumentsCheckingIfPossibleWithNullableArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 2)) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("useToImpl4ArgumentsCheckingIfPossibleWithNullableArg", "TestObject", ExceptionMessages::NotEnoughArguments(2, info.Length())));
    return;
  }

  Node* node1;
  Node* node2;
  node1 = V8Node::ToImplWithTypeCheck(info.GetIsolate(), info[0]);
  if (!node1) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("useToImpl4ArgumentsCheckingIfPossibleWithNullableArg", "TestObject", "parameter 1 is not of type 'Node'."));
    return;
  }

  node2 = V8Node::ToImplWithTypeCheck(info.GetIsolate(), info[1]);
  if (!node2 && !IsUndefinedOrNull(info[1])) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("useToImpl4ArgumentsCheckingIfPossibleWithNullableArg", "TestObject", "parameter 2 is not of type 'Node'."));
    return;
  }

  impl->useToImpl4ArgumentsCheckingIfPossibleWithNullableArg(node1, node2);
}

static void UseToImpl4ArgumentsCheckingIfPossibleWithUndefinedArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("useToImpl4ArgumentsCheckingIfPossibleWithUndefinedArg", "TestObject", ExceptionMessages::NotEnoughArguments(1, info.Length())));
    return;
  }

  Node* node1;
  Node* node2;
  node1 = V8Node::ToImplWithTypeCheck(info.GetIsolate(), info[0]);
  if (!node1) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("useToImpl4ArgumentsCheckingIfPossibleWithUndefinedArg", "TestObject", "parameter 1 is not of type 'Node'."));
    return;
  }

  node2 = V8Node::ToImplWithTypeCheck(info.GetIsolate(), info[1]);
  if (!node2) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("useToImpl4ArgumentsCheckingIfPossibleWithUndefinedArg", "TestObject", "parameter 2 is not of type 'Node'."));
    return;
  }

  impl->useToImpl4ArgumentsCheckingIfPossibleWithUndefinedArg(node1, node2);
}

static void UnforgeableVoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  impl->unforgeableVoidMethod();
}

static void NewObjectTestInterfaceMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  TestInterfaceImplementation* result = impl->newObjectTestInterfaceMethod();
  // [NewObject] must always create a new wrapper.  Check that a wrapper
  // does not exist yet.
  DCHECK(!result || DOMDataStore::GetWrapper(result, info.GetIsolate()).IsEmpty());
  V8SetReturnValue(info, result);
}

static void NewObjectTestInterfacePromiseMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "newObjectTestInterfacePromiseMethod");
  ExceptionToRejectPromiseScope rejectPromiseScope(info, exceptionState);

  // V8DOMConfiguration::kDoNotCheckHolder
  // Make sure that info.Holder() really points to an instance of the type.
  if (!V8TestObject::HasInstance(info.Holder(), info.GetIsolate())) {
    exceptionState.ThrowTypeError("Illegal invocation");
    return;
  }
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  ScriptPromise result = impl->newObjectTestInterfacePromiseMethod();
  V8SetReturnValue(info, result.V8Value());
}

static void RuntimeCallStatsCounterMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  impl->RuntimeCallStatsCounterMethod();
}

static void ClearMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "clear");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);

  bool result = impl->myMaplikeClear(scriptState, exceptionState);
  if (exceptionState.HadException()) {
    return;
  }
  V8SetReturnValueBool(info, result);
}

static void KeysMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "keys");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);

  Iterator* result = impl->keysForBinding(scriptState, exceptionState);
  if (exceptionState.HadException()) {
    return;
  }
  V8SetReturnValue(info, result);
}

static void ValuesMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "values");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);

  Iterator* result = impl->valuesForBinding(scriptState, exceptionState);
  if (exceptionState.HadException()) {
    return;
  }
  V8SetReturnValue(info, result);
}

static void ForEachMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "forEach");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  ScriptValue callback;
  ScriptValue thisArg;
  if (info[0]->IsFunction()) {
    callback = ScriptValue(ScriptState::Current(info.GetIsolate()), info[0]);
  } else {
    exceptionState.ThrowTypeError("The callback provided as parameter 1 is not a function.");
    return;
  }

  thisArg = ScriptValue(ScriptState::Current(info.GetIsolate()), info[1]);

  impl->forEachForBinding(scriptState, ScriptValue(scriptState, info.Holder()), callback, thisArg, exceptionState);
  if (exceptionState.HadException()) {
    return;
  }
}

static void HasMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "has");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  int32_t key;
  key = NativeValueTraits<IDLLongEnforceRange>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  bool result = impl->hasForBinding(scriptState, key, exceptionState);
  if (exceptionState.HadException()) {
    return;
  }
  V8SetReturnValueBool(info, result);
}

static void GetMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "get");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  int32_t key;
  key = NativeValueTraits<IDLLongEnforceRange>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  ScriptValue result = impl->getForBinding(scriptState, key, exceptionState);
  if (exceptionState.HadException()) {
    return;
  }
  V8SetReturnValue(info, result.V8Value());
}

static void DeleteMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "delete");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  int32_t key;
  key = NativeValueTraits<IDLLongEnforceRange>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  bool result = impl->deleteForBinding(scriptState, key, exceptionState);
  if (exceptionState.HadException()) {
    return;
  }
  V8SetReturnValueBool(info, result);
}

static void SetMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "set");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);

  if (UNLIKELY(info.Length() < 2)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(2, info.Length()));
    return;
  }

  int32_t key;
  StringOrDouble value;
  key = NativeValueTraits<IDLLongEnforceRange>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  V8StringOrDouble::ToImpl(info.GetIsolate(), info[1], value, UnionTypeConversionMode::kNotNullable, exceptionState);
  if (exceptionState.HadException())
    return;

  TestObject* result = impl->setForBinding(scriptState, key, value, exceptionState);
  if (exceptionState.HadException()) {
    return;
  }
  V8SetReturnValue(info, result);
}

static void ToJSONMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);

  ScriptValue result = impl->toJSONForBinding(scriptState);
  V8SetReturnValue(info, result.V8Value());
}

static void ToStringMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  V8SetReturnValueString(info, impl->stringifierAttribute(), info.GetIsolate());
}

static void IteratorMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestObject", "iterator");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);

  Iterator* result = impl->GetIterator(scriptState, exceptionState);
  if (exceptionState.HadException()) {
    return;
  }
  V8SetReturnValue(info, result);
}

static void NamedPropertyGetter(const AtomicString& name, const v8::PropertyCallbackInfo<v8::Value>& info) {
  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);

  TestObject* impl = V8TestObject::ToImpl(info.Holder());
  ScriptValue result = impl->AnonymousNamedGetter(scriptState, name);
  if (result.IsEmpty())
    return;
  V8SetReturnValue(info, result.V8Value());
}

static void NamedPropertySetter(const AtomicString& name, v8::Local<v8::Value> v8Value, const v8::PropertyCallbackInfo<v8::Value>& info) {
  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);

  TestObject* impl = V8TestObject::ToImpl(info.Holder());
  V8StringResource<> propertyValue = v8Value;
  if (!propertyValue.Prepare())
    return;

  bool result = impl->AnonymousNamedSetter(scriptState, name, propertyValue);
  if (!result)
    return;
  V8SetReturnValue(info, v8Value);
}

static void NamedPropertyDeleter(const AtomicString& name, const v8::PropertyCallbackInfo<v8::Boolean>& info) {
  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  DeleteResult result = impl->AnonymousNamedDeleter(scriptState, name);
  if (result == kDeleteUnknownProperty)
    return;
  V8SetReturnValue(info, result == kDeleteSuccess);
}

static void NamedPropertyQuery(const AtomicString& name, const v8::PropertyCallbackInfo<v8::Integer>& info) {
  const CString& nameInUtf8 = name.Utf8();
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kGetterContext, "TestObject", nameInUtf8.data());
  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  bool result = impl->NamedPropertyQuery(scriptState, name, exceptionState);
  if (!result)
    return;
  // https://heycam.github.io/webidl/#LegacyPlatformObjectGetOwnProperty
  // 2.7. If |O| implements an interface with a named property setter, then set
  //      desc.[[Writable]] to true, otherwise set it to false.
  // 2.8. If |O| implements an interface with the
  //      [LegacyUnenumerableNamedProperties] extended attribute, then set
  //      desc.[[Enumerable]] to false, otherwise set it to true.
  V8SetReturnValueInt(info, v8::None);
}

static void NamedPropertyEnumerator(const v8::PropertyCallbackInfo<v8::Array>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kEnumerationContext, "TestObject");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  Vector<String> names;
  impl->NamedPropertyEnumerator(names, exceptionState);
  if (exceptionState.HadException())
    return;
  V8SetReturnValue(info, ToV8(names, info.Holder(), info.GetIsolate()).As<v8::Array>());
}

static void IndexedPropertyGetter(uint32_t index, const v8::PropertyCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  // We assume that all the implementations support length() method, although
  // the spec doesn't require that length() must exist.  It's okay that
  // the interface does not have length attribute as long as the
  // implementation supports length() member function.
  if (index >= impl->length())
    return;  // Returns undefined due to out-of-range.

  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);
  ScriptValue result = impl->item(scriptState, index);
  V8SetReturnValue(info, result.V8Value());
}

static void IndexedPropertyDescriptor(uint32_t index, const v8::PropertyCallbackInfo<v8::Value>& info) {
  // https://heycam.github.io/webidl/#LegacyPlatformObjectGetOwnProperty
  // Steps 1.1 to 1.2.4 are covered here: we rely on indexedPropertyGetter() to
  // call the getter function and check that |index| is a valid property index,
  // in which case it will have set info.GetReturnValue() to something other
  // than undefined.
  V8TestObject::IndexedPropertyGetterCallback(index, info);
  v8::Local<v8::Value> getterValue = info.GetReturnValue().Get();
  if (!getterValue->IsUndefined()) {
    // 1.2.5. Let |desc| be a newly created Property Descriptor with no fields.
    // 1.2.6. Set desc.[[Value]] to the result of converting value to an
    //        ECMAScript value.
    // 1.2.7. If O implements an interface with an indexed property setter,
    //        then set desc.[[Writable]] to true, otherwise set it to false.
    v8::PropertyDescriptor desc(getterValue, true);
    // 1.2.8. Set desc.[[Enumerable]] and desc.[[Configurable]] to true.
    desc.set_enumerable(true);
    desc.set_configurable(true);
    // 1.2.9. Return |desc|.
    V8SetReturnValue(info, desc);
  }
}

static void IndexedPropertySetter(uint32_t index, v8::Local<v8::Value> v8Value, const v8::PropertyCallbackInfo<v8::Value>& info) {
  TestObject* impl = V8TestObject::ToImpl(info.Holder());
  V8StringResource<> propertyValue = v8Value;
  if (!propertyValue.Prepare())
    return;

  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);
  bool result = impl->setItem(scriptState, index, propertyValue);
  if (!result)
    return;
  V8SetReturnValue(info, v8Value);
}

static void IndexedPropertyDeleter(uint32_t index, const v8::PropertyCallbackInfo<v8::Boolean>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kIndexedDeletionContext, "TestObject");

  TestObject* impl = V8TestObject::ToImpl(info.Holder());

  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);
  DeleteResult result = impl->AnonymousIndexedDeleter(scriptState, index, exceptionState);
  if (exceptionState.HadException())
    return;
  if (result == kDeleteUnknownProperty)
    return;
  V8SetReturnValue(info, result == kDeleteSuccess);
}

}  // namespace test_object_v8_internal

void V8TestObject::StringifierAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_stringifierAttribute_Getter");

  test_object_v8_internal::StringifierAttributeAttributeGetter(info);
}

void V8TestObject::StringifierAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_stringifierAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::StringifierAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::ReadonlyStringAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_readonlyStringAttribute_Getter");

  test_object_v8_internal::ReadonlyStringAttributeAttributeGetter(info);
}

void V8TestObject::ReadonlyTestInterfaceEmptyAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_readonlyTestInterfaceEmptyAttribute_Getter");

  test_object_v8_internal::ReadonlyTestInterfaceEmptyAttributeAttributeGetter(info);
}

void V8TestObject::ReadonlyLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_readonlyLongAttribute_Getter");

  test_object_v8_internal::ReadonlyLongAttributeAttributeGetter(info);
}

void V8TestObject::DateAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_dateAttribute_Getter");

  test_object_v8_internal::DateAttributeAttributeGetter(info);
}

void V8TestObject::DateAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_dateAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::DateAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::StringAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_stringAttribute_Getter");

  test_object_v8_internal::StringAttributeAttributeGetter(info);
}

void V8TestObject::StringAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_stringAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::StringAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::ByteStringAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_byteStringAttribute_Getter");

  test_object_v8_internal::ByteStringAttributeAttributeGetter(info);
}

void V8TestObject::ByteStringAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_byteStringAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::ByteStringAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::UsvStringAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_usvStringAttribute_Getter");

  test_object_v8_internal::UsvStringAttributeAttributeGetter(info);
}

void V8TestObject::UsvStringAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_usvStringAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::UsvStringAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::DOMTimeStampAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_domTimeStampAttribute_Getter");

  test_object_v8_internal::DOMTimeStampAttributeAttributeGetter(info);
}

void V8TestObject::DOMTimeStampAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_domTimeStampAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::DOMTimeStampAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::BooleanAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_booleanAttribute_Getter");

  test_object_v8_internal::BooleanAttributeAttributeGetter(info);
}

void V8TestObject::BooleanAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_booleanAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::BooleanAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::ByteAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_byteAttribute_Getter");

  test_object_v8_internal::ByteAttributeAttributeGetter(info);
}

void V8TestObject::ByteAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_byteAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::ByteAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::DoubleAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_doubleAttribute_Getter");

  test_object_v8_internal::DoubleAttributeAttributeGetter(info);
}

void V8TestObject::DoubleAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_doubleAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::DoubleAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::FloatAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_floatAttribute_Getter");

  test_object_v8_internal::FloatAttributeAttributeGetter(info);
}

void V8TestObject::FloatAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_floatAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::FloatAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::LongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_longAttribute_Getter");

  test_object_v8_internal::LongAttributeAttributeGetter(info);
}

void V8TestObject::LongAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_longAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::LongAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::LongLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_longLongAttribute_Getter");

  test_object_v8_internal::LongLongAttributeAttributeGetter(info);
}

void V8TestObject::LongLongAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_longLongAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::LongLongAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::OctetAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_octetAttribute_Getter");

  test_object_v8_internal::OctetAttributeAttributeGetter(info);
}

void V8TestObject::OctetAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_octetAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::OctetAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::ShortAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_shortAttribute_Getter");

  test_object_v8_internal::ShortAttributeAttributeGetter(info);
}

void V8TestObject::ShortAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_shortAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::ShortAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::UnrestrictedDoubleAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_unrestrictedDoubleAttribute_Getter");

  test_object_v8_internal::UnrestrictedDoubleAttributeAttributeGetter(info);
}

void V8TestObject::UnrestrictedDoubleAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_unrestrictedDoubleAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::UnrestrictedDoubleAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::UnrestrictedFloatAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_unrestrictedFloatAttribute_Getter");

  test_object_v8_internal::UnrestrictedFloatAttributeAttributeGetter(info);
}

void V8TestObject::UnrestrictedFloatAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_unrestrictedFloatAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::UnrestrictedFloatAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::UnsignedLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_unsignedLongAttribute_Getter");

  test_object_v8_internal::UnsignedLongAttributeAttributeGetter(info);
}

void V8TestObject::UnsignedLongAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_unsignedLongAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::UnsignedLongAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::UnsignedLongLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_unsignedLongLongAttribute_Getter");

  test_object_v8_internal::UnsignedLongLongAttributeAttributeGetter(info);
}

void V8TestObject::UnsignedLongLongAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_unsignedLongLongAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::UnsignedLongLongAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::UnsignedShortAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_unsignedShortAttribute_Getter");

  test_object_v8_internal::UnsignedShortAttributeAttributeGetter(info);
}

void V8TestObject::UnsignedShortAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_unsignedShortAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::UnsignedShortAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::TestInterfaceEmptyAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_testInterfaceEmptyAttribute_Getter");

  test_object_v8_internal::TestInterfaceEmptyAttributeAttributeGetter(info);
}

void V8TestObject::TestInterfaceEmptyAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_testInterfaceEmptyAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::TestInterfaceEmptyAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::TestObjectAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_testObjectAttribute_Getter");

  test_object_v8_internal::TestObjectAttributeAttributeGetter(info);
}

void V8TestObject::TestObjectAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_testObjectAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::TestObjectAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::CSSAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_cssAttribute_Getter");

  test_object_v8_internal::CSSAttributeAttributeGetter(info);
}

void V8TestObject::CSSAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_cssAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::CSSAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::ImeAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_imeAttribute_Getter");

  test_object_v8_internal::ImeAttributeAttributeGetter(info);
}

void V8TestObject::ImeAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_imeAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::ImeAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::SVGAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_svgAttribute_Getter");

  test_object_v8_internal::SVGAttributeAttributeGetter(info);
}

void V8TestObject::SVGAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_svgAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::SVGAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::XmlAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_xmlAttribute_Getter");

  test_object_v8_internal::XmlAttributeAttributeGetter(info);
}

void V8TestObject::XmlAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_xmlAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::XmlAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::NodeFilterAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_nodeFilterAttribute_Getter");

  test_object_v8_internal::NodeFilterAttributeAttributeGetter(info);
}

void V8TestObject::SerializedScriptValueAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_serializedScriptValueAttribute_Getter");

  test_object_v8_internal::SerializedScriptValueAttributeAttributeGetter(info);
}

void V8TestObject::SerializedScriptValueAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_serializedScriptValueAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::SerializedScriptValueAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::AnyAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_anyAttribute_Getter");

  test_object_v8_internal::AnyAttributeAttributeGetter(info);
}

void V8TestObject::AnyAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_anyAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::AnyAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::PromiseAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_promiseAttribute_Getter");

  test_object_v8_internal::PromiseAttributeAttributeGetter(info);
}

void V8TestObject::PromiseAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_promiseAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::PromiseAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::WindowAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_windowAttribute_Getter");

  test_object_v8_internal::WindowAttributeAttributeGetter(info);
}

void V8TestObject::WindowAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_windowAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::WindowAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::DocumentAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_documentAttribute_Getter");

  test_object_v8_internal::DocumentAttributeAttributeGetter(info);
}

void V8TestObject::DocumentAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_documentAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::DocumentAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::DocumentFragmentAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_documentFragmentAttribute_Getter");

  test_object_v8_internal::DocumentFragmentAttributeAttributeGetter(info);
}

void V8TestObject::DocumentFragmentAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_documentFragmentAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::DocumentFragmentAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::DocumentTypeAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_documentTypeAttribute_Getter");

  test_object_v8_internal::DocumentTypeAttributeAttributeGetter(info);
}

void V8TestObject::DocumentTypeAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_documentTypeAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::DocumentTypeAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::ElementAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_elementAttribute_Getter");

  test_object_v8_internal::ElementAttributeAttributeGetter(info);
}

void V8TestObject::ElementAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_elementAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::ElementAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::NodeAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_nodeAttribute_Getter");

  test_object_v8_internal::NodeAttributeAttributeGetter(info);
}

void V8TestObject::NodeAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_nodeAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::NodeAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::ShadowRootAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_shadowRootAttribute_Getter");

  test_object_v8_internal::ShadowRootAttributeAttributeGetter(info);
}

void V8TestObject::ShadowRootAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_shadowRootAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::ShadowRootAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::ArrayBufferAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_arrayBufferAttribute_Getter");

  test_object_v8_internal::ArrayBufferAttributeAttributeGetter(info);
}

void V8TestObject::ArrayBufferAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_arrayBufferAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::ArrayBufferAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::Float32ArrayAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_float32ArrayAttribute_Getter");

  test_object_v8_internal::Float32ArrayAttributeAttributeGetter(info);
}

void V8TestObject::Float32ArrayAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_float32ArrayAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::Float32ArrayAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::Uint8ArrayAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_uint8ArrayAttribute_Getter");

  test_object_v8_internal::Uint8ArrayAttributeAttributeGetter(info);
}

void V8TestObject::Uint8ArrayAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_uint8ArrayAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::Uint8ArrayAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::SelfAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_self_Getter");

  test_object_v8_internal::SelfAttributeGetter(info);
}

void V8TestObject::ReadonlyEventTargetAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_readonlyEventTargetAttribute_Getter");

  test_object_v8_internal::ReadonlyEventTargetAttributeAttributeGetter(info);
}

void V8TestObject::ReadonlyEventTargetOrNullAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_readonlyEventTargetOrNullAttribute_Getter");

  test_object_v8_internal::ReadonlyEventTargetOrNullAttributeAttributeGetter(info);
}

void V8TestObject::ReadonlyWindowAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_readonlyWindowAttribute_Getter");

  test_object_v8_internal::ReadonlyWindowAttributeAttributeGetter(info);
}

void V8TestObject::HTMLCollectionAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_htmlCollectionAttribute_Getter");

  test_object_v8_internal::HTMLCollectionAttributeAttributeGetter(info);
}

void V8TestObject::HTMLElementAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_htmlElementAttribute_Getter");

  test_object_v8_internal::HTMLElementAttributeAttributeGetter(info);
}

void V8TestObject::StringFrozenArrayAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_stringFrozenArrayAttribute_Getter");

  test_object_v8_internal::StringFrozenArrayAttributeAttributeGetter(info);
}

void V8TestObject::StringFrozenArrayAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_stringFrozenArrayAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::StringFrozenArrayAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::TestInterfaceEmptyFrozenArrayAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_testInterfaceEmptyFrozenArrayAttribute_Getter");

  test_object_v8_internal::TestInterfaceEmptyFrozenArrayAttributeAttributeGetter(info);
}

void V8TestObject::TestInterfaceEmptyFrozenArrayAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_testInterfaceEmptyFrozenArrayAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::TestInterfaceEmptyFrozenArrayAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::BooleanOrNullAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_booleanOrNullAttribute_Getter");

  test_object_v8_internal::BooleanOrNullAttributeAttributeGetter(info);
}

void V8TestObject::BooleanOrNullAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_booleanOrNullAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::BooleanOrNullAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::StringOrNullAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_stringOrNullAttribute_Getter");

  test_object_v8_internal::StringOrNullAttributeAttributeGetter(info);
}

void V8TestObject::StringOrNullAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_stringOrNullAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::StringOrNullAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::LongOrNullAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_longOrNullAttribute_Getter");

  test_object_v8_internal::LongOrNullAttributeAttributeGetter(info);
}

void V8TestObject::LongOrNullAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_longOrNullAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::LongOrNullAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::TestInterfaceOrNullAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_testInterfaceOrNullAttribute_Getter");

  test_object_v8_internal::TestInterfaceOrNullAttributeAttributeGetter(info);
}

void V8TestObject::TestInterfaceOrNullAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_testInterfaceOrNullAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::TestInterfaceOrNullAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::TestEnumAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_testEnumAttribute_Getter");

  test_object_v8_internal::TestEnumAttributeAttributeGetter(info);
}

void V8TestObject::TestEnumAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_testEnumAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::TestEnumAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::TestEnumOrNullAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_testEnumOrNullAttribute_Getter");

  test_object_v8_internal::TestEnumOrNullAttributeAttributeGetter(info);
}

void V8TestObject::TestEnumOrNullAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_testEnumOrNullAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::TestEnumOrNullAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::StaticStringAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_staticStringAttribute_Getter");

  test_object_v8_internal::StaticStringAttributeAttributeGetter(info);
}

void V8TestObject::StaticStringAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_staticStringAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::StaticStringAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::StaticLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_staticLongAttribute_Getter");

  test_object_v8_internal::StaticLongAttributeAttributeGetter(info);
}

void V8TestObject::StaticLongAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_staticLongAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::StaticLongAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::EventHandlerAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_eventHandlerAttribute_Getter");

  test_object_v8_internal::EventHandlerAttributeAttributeGetter(info);
}

void V8TestObject::EventHandlerAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_eventHandlerAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::EventHandlerAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::DoubleOrStringAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_doubleOrStringAttribute_Getter");

  test_object_v8_internal::DoubleOrStringAttributeAttributeGetter(info);
}

void V8TestObject::DoubleOrStringAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_doubleOrStringAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::DoubleOrStringAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::DoubleOrStringOrNullAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_doubleOrStringOrNullAttribute_Getter");

  test_object_v8_internal::DoubleOrStringOrNullAttributeAttributeGetter(info);
}

void V8TestObject::DoubleOrStringOrNullAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_doubleOrStringOrNullAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::DoubleOrStringOrNullAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::DoubleOrNullStringAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_doubleOrNullStringAttribute_Getter");

  test_object_v8_internal::DoubleOrNullStringAttributeAttributeGetter(info);
}

void V8TestObject::DoubleOrNullStringAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_doubleOrNullStringAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::DoubleOrNullStringAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::StringOrStringSequenceAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_stringOrStringSequenceAttribute_Getter");

  test_object_v8_internal::StringOrStringSequenceAttributeAttributeGetter(info);
}

void V8TestObject::StringOrStringSequenceAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_stringOrStringSequenceAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::StringOrStringSequenceAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::TestEnumOrDoubleAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_testEnumOrDoubleAttribute_Getter");

  test_object_v8_internal::TestEnumOrDoubleAttributeAttributeGetter(info);
}

void V8TestObject::TestEnumOrDoubleAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_testEnumOrDoubleAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::TestEnumOrDoubleAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::UnrestrictedDoubleOrStringAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_unrestrictedDoubleOrStringAttribute_Getter");

  test_object_v8_internal::UnrestrictedDoubleOrStringAttributeAttributeGetter(info);
}

void V8TestObject::UnrestrictedDoubleOrStringAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_unrestrictedDoubleOrStringAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::UnrestrictedDoubleOrStringAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::NestedUnionAtributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_nestedUnionAtribute_Getter");

  test_object_v8_internal::NestedUnionAtributeAttributeGetter(info);
}

void V8TestObject::NestedUnionAtributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_nestedUnionAtribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::NestedUnionAtributeAttributeSetter(v8Value, info);
}

void V8TestObject::ActivityLoggingAccessForAllWorldsLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_activityLoggingAccessForAllWorldsLongAttribute_Getter");

  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);
  V8PerContextData* contextData = scriptState->PerContextData();
  if (contextData && contextData->ActivityLogger()) {
    contextData->ActivityLogger()->LogGetter("TestObject.activityLoggingAccessForAllWorldsLongAttribute");
  }

  test_object_v8_internal::ActivityLoggingAccessForAllWorldsLongAttributeAttributeGetter(info);
}

void V8TestObject::ActivityLoggingAccessForAllWorldsLongAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_activityLoggingAccessForAllWorldsLongAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);
  V8PerContextData* contextData = scriptState->PerContextData();
  if (contextData && contextData->ActivityLogger()) {
    contextData->ActivityLogger()->LogSetter("TestObject.activityLoggingAccessForAllWorldsLongAttribute", v8Value);
  }

  test_object_v8_internal::ActivityLoggingAccessForAllWorldsLongAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::ActivityLoggingGetterForAllWorldsLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_activityLoggingGetterForAllWorldsLongAttribute_Getter");

  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);
  V8PerContextData* contextData = scriptState->PerContextData();
  if (contextData && contextData->ActivityLogger()) {
    contextData->ActivityLogger()->LogGetter("TestObject.activityLoggingGetterForAllWorldsLongAttribute");
  }

  test_object_v8_internal::ActivityLoggingGetterForAllWorldsLongAttributeAttributeGetter(info);
}

void V8TestObject::ActivityLoggingGetterForAllWorldsLongAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_activityLoggingGetterForAllWorldsLongAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::ActivityLoggingGetterForAllWorldsLongAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::ActivityLoggingSetterForAllWorldsLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_activityLoggingSetterForAllWorldsLongAttribute_Getter");

  test_object_v8_internal::ActivityLoggingSetterForAllWorldsLongAttributeAttributeGetter(info);
}

void V8TestObject::ActivityLoggingSetterForAllWorldsLongAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_activityLoggingSetterForAllWorldsLongAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);
  V8PerContextData* contextData = scriptState->PerContextData();
  if (contextData && contextData->ActivityLogger()) {
    contextData->ActivityLogger()->LogSetter("TestObject.activityLoggingSetterForAllWorldsLongAttribute", v8Value);
  }

  test_object_v8_internal::ActivityLoggingSetterForAllWorldsLongAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::CachedAttributeAnyAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_cachedAttributeAnyAttribute_Getter");

  test_object_v8_internal::CachedAttributeAnyAttributeAttributeGetter(info);
}

void V8TestObject::CachedAttributeAnyAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_cachedAttributeAnyAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::CachedAttributeAnyAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::CachedArrayAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_cachedArrayAttribute_Getter");

  test_object_v8_internal::CachedArrayAttributeAttributeGetter(info);
}

void V8TestObject::CachedArrayAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_cachedArrayAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::CachedArrayAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::CachedStringOrNoneAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_cachedStringOrNoneAttribute_Getter");

  test_object_v8_internal::CachedStringOrNoneAttributeAttributeGetter(info);
}

void V8TestObject::CachedStringOrNoneAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_cachedStringOrNoneAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::CachedStringOrNoneAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::CallWithExecutionContextAnyAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_callWithExecutionContextAnyAttribute_Getter");

  test_object_v8_internal::CallWithExecutionContextAnyAttributeAttributeGetter(info);
}

void V8TestObject::CallWithExecutionContextAnyAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_callWithExecutionContextAnyAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::CallWithExecutionContextAnyAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::CallWithScriptStateAnyAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_callWithScriptStateAnyAttribute_Getter");

  test_object_v8_internal::CallWithScriptStateAnyAttributeAttributeGetter(info);
}

void V8TestObject::CallWithScriptStateAnyAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_callWithScriptStateAnyAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::CallWithScriptStateAnyAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::CallWithExecutionContextAndScriptStateAnyAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_callWithExecutionContextAndScriptStateAnyAttribute_Getter");

  test_object_v8_internal::CallWithExecutionContextAndScriptStateAnyAttributeAttributeGetter(info);
}

void V8TestObject::CallWithExecutionContextAndScriptStateAnyAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_callWithExecutionContextAndScriptStateAnyAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::CallWithExecutionContextAndScriptStateAnyAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::CheckSecurityForNodeReadonlyDocumentAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_checkSecurityForNodeReadonlyDocumentAttribute_Getter");

  test_object_v8_internal::CheckSecurityForNodeReadonlyDocumentAttributeAttributeGetter(info);
}

void V8TestObject::TestInterfaceEmptyConstructorAttributeConstructorGetterCallback(v8::Local<v8::Name> property, const v8::PropertyCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_testInterfaceEmptyConstructorAttribute_ConstructorGetterCallback");

  V8ConstructorAttributeGetter(property, info, &V8TestInterfaceEmpty::wrapperTypeInfo);
}

void V8TestObject::TestInterfaceEmptyConstructorAttributeConstructorGetterCallback(v8::Local<v8::Name> property, const v8::PropertyCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_testInterfaceEmptyConstructorAttribute_ConstructorGetterCallback");

  Deprecation::CountDeprecation(CurrentExecutionContext(info.GetIsolate()), WebFeature::kdeprecatedTestInterfaceEmptyConstructorAttribute);

  V8ConstructorAttributeGetter(property, info, &V8TestInterfaceEmpty::wrapperTypeInfo);
}

void V8TestObject::MeasureAsFeatureNameTestInterfaceEmptyConstructorAttributeConstructorGetterCallback(v8::Local<v8::Name> property, const v8::PropertyCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_measureAsFeatureNameTestInterfaceEmptyConstructorAttribute_ConstructorGetterCallback");

  UseCounter::Count(CurrentExecutionContext(info.GetIsolate()), WebFeature::kFeatureName);

  V8ConstructorAttributeGetter(property, info, &V8TestInterfaceEmpty::wrapperTypeInfo);
}

void V8TestObject::CustomObjectAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_customObjectAttribute_Getter");

  V8TestObject::CustomObjectAttributeAttributeGetterCustom(info);
}

void V8TestObject::CustomObjectAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_customObjectAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  V8TestObject::CustomObjectAttributeAttributeSetterCustom(v8Value, info);
}

void V8TestObject::CustomGetterLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_customGetterLongAttribute_Getter");

  V8TestObject::CustomGetterLongAttributeAttributeGetterCustom(info);
}

void V8TestObject::CustomGetterLongAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_customGetterLongAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::CustomGetterLongAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::CustomGetterReadonlyObjectAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_customGetterReadonlyObjectAttribute_Getter");

  V8TestObject::CustomGetterReadonlyObjectAttributeAttributeGetterCustom(info);
}

void V8TestObject::CustomSetterLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_customSetterLongAttribute_Getter");

  test_object_v8_internal::CustomSetterLongAttributeAttributeGetter(info);
}

void V8TestObject::CustomSetterLongAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_customSetterLongAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  V8TestObject::CustomSetterLongAttributeAttributeSetterCustom(v8Value, info);
}

void V8TestObject::DeprecatedLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_deprecatedLongAttribute_Getter");

  Deprecation::CountDeprecation(CurrentExecutionContext(info.GetIsolate()), WebFeature::kLongAttribute);

  test_object_v8_internal::DeprecatedLongAttributeAttributeGetter(info);
}

void V8TestObject::DeprecatedLongAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_deprecatedLongAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  Deprecation::CountDeprecation(CurrentExecutionContext(info.GetIsolate()), WebFeature::kLongAttribute);

  test_object_v8_internal::DeprecatedLongAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::EnforceRangeLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_enforceRangeLongAttribute_Getter");

  test_object_v8_internal::EnforceRangeLongAttributeAttributeGetter(info);
}

void V8TestObject::EnforceRangeLongAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_enforceRangeLongAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::EnforceRangeLongAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::ImplementedAsLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_implementedAsLongAttribute_Getter");

  test_object_v8_internal::ImplementedAsLongAttributeAttributeGetter(info);
}

void V8TestObject::ImplementedAsLongAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_implementedAsLongAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::ImplementedAsLongAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::CustomImplementedAsLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_customImplementedAsLongAttribute_Getter");

  V8TestObject::CustomImplementedAsLongAttributeAttributeGetterCustom(info);
}

void V8TestObject::CustomImplementedAsLongAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_customImplementedAsLongAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  V8TestObject::CustomImplementedAsLongAttributeAttributeSetterCustom(v8Value, info);
}

void V8TestObject::CustomGetterImplementedAsLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_customGetterImplementedAsLongAttribute_Getter");

  V8TestObject::CustomGetterImplementedAsLongAttributeAttributeGetterCustom(info);
}

void V8TestObject::CustomGetterImplementedAsLongAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_customGetterImplementedAsLongAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::CustomGetterImplementedAsLongAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::CustomSetterImplementedAsLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_customSetterImplementedAsLongAttribute_Getter");

  test_object_v8_internal::CustomSetterImplementedAsLongAttributeAttributeGetter(info);
}

void V8TestObject::CustomSetterImplementedAsLongAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_customSetterImplementedAsLongAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  V8TestObject::CustomSetterImplementedAsLongAttributeAttributeSetterCustom(v8Value, info);
}

void V8TestObject::MeasureAsLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_measureAsLongAttribute_Getter");

  UseCounter::Count(CurrentExecutionContext(info.GetIsolate()), WebFeature::kTestFeature);

  test_object_v8_internal::MeasureAsLongAttributeAttributeGetter(info);
}

void V8TestObject::MeasureAsLongAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_measureAsLongAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  UseCounter::Count(CurrentExecutionContext(info.GetIsolate()), WebFeature::kTestFeature);

  test_object_v8_internal::MeasureAsLongAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::NotEnumerableLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_notEnumerableLongAttribute_Getter");

  test_object_v8_internal::NotEnumerableLongAttributeAttributeGetter(info);
}

void V8TestObject::NotEnumerableLongAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_notEnumerableLongAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::NotEnumerableLongAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::OriginTrialEnabledLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_originTrialEnabledLongAttribute_Getter");

  test_object_v8_internal::OriginTrialEnabledLongAttributeAttributeGetter(info);
}

void V8TestObject::OriginTrialEnabledLongAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_originTrialEnabledLongAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::OriginTrialEnabledLongAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::PerWorldBindingsReadonlyTestInterfaceEmptyAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_perWorldBindingsReadonlyTestInterfaceEmptyAttribute_Getter");

  test_object_v8_internal::PerWorldBindingsReadonlyTestInterfaceEmptyAttributeAttributeGetter(info);
}

void V8TestObject::PerWorldBindingsReadonlyTestInterfaceEmptyAttributeAttributeGetterCallbackForMainWorld(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_perWorldBindingsReadonlyTestInterfaceEmptyAttribute_Getter");

  test_object_v8_internal::PerWorldBindingsReadonlyTestInterfaceEmptyAttributeAttributeGetterForMainWorld(info);
}

void V8TestObject::ActivityLoggingAccessPerWorldBindingsLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_activityLoggingAccessPerWorldBindingsLongAttribute_Getter");

  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);
  V8PerContextData* contextData = scriptState->PerContextData();
  if (contextData && contextData->ActivityLogger()) {
    contextData->ActivityLogger()->LogGetter("TestObject.activityLoggingAccessPerWorldBindingsLongAttribute");
  }

  test_object_v8_internal::ActivityLoggingAccessPerWorldBindingsLongAttributeAttributeGetter(info);
}

void V8TestObject::ActivityLoggingAccessPerWorldBindingsLongAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_activityLoggingAccessPerWorldBindingsLongAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);
  V8PerContextData* contextData = scriptState->PerContextData();
  if (contextData && contextData->ActivityLogger()) {
    contextData->ActivityLogger()->LogSetter("TestObject.activityLoggingAccessPerWorldBindingsLongAttribute", v8Value);
  }

  test_object_v8_internal::ActivityLoggingAccessPerWorldBindingsLongAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::ActivityLoggingAccessPerWorldBindingsLongAttributeAttributeGetterCallbackForMainWorld(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_activityLoggingAccessPerWorldBindingsLongAttribute_Getter");

  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);
  V8PerContextData* contextData = scriptState->PerContextData();
  if (contextData && contextData->ActivityLogger()) {
    contextData->ActivityLogger()->LogGetter("TestObject.activityLoggingAccessPerWorldBindingsLongAttribute");
  }

  test_object_v8_internal::ActivityLoggingAccessPerWorldBindingsLongAttributeAttributeGetterForMainWorld(info);
}

void V8TestObject::ActivityLoggingAccessPerWorldBindingsLongAttributeAttributeSetterCallbackForMainWorld(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_activityLoggingAccessPerWorldBindingsLongAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);
  V8PerContextData* contextData = scriptState->PerContextData();
  if (contextData && contextData->ActivityLogger()) {
    contextData->ActivityLogger()->LogSetter("TestObject.activityLoggingAccessPerWorldBindingsLongAttribute", v8Value);
  }

  test_object_v8_internal::ActivityLoggingAccessPerWorldBindingsLongAttributeAttributeSetterForMainWorld(v8Value, info);
}

void V8TestObject::ActivityLoggingAccessForIsolatedWorldsPerWorldBindingsLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_activityLoggingAccessForIsolatedWorldsPerWorldBindingsLongAttribute_Getter");

  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);
  V8PerContextData* contextData = scriptState->PerContextData();
  if (contextData && contextData->ActivityLogger()) {
    contextData->ActivityLogger()->LogGetter("TestObject.activityLoggingAccessForIsolatedWorldsPerWorldBindingsLongAttribute");
  }

  test_object_v8_internal::ActivityLoggingAccessForIsolatedWorldsPerWorldBindingsLongAttributeAttributeGetter(info);
}

void V8TestObject::ActivityLoggingAccessForIsolatedWorldsPerWorldBindingsLongAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_activityLoggingAccessForIsolatedWorldsPerWorldBindingsLongAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);
  V8PerContextData* contextData = scriptState->PerContextData();
  if (contextData && contextData->ActivityLogger()) {
    contextData->ActivityLogger()->LogSetter("TestObject.activityLoggingAccessForIsolatedWorldsPerWorldBindingsLongAttribute", v8Value);
  }

  test_object_v8_internal::ActivityLoggingAccessForIsolatedWorldsPerWorldBindingsLongAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::ActivityLoggingAccessForIsolatedWorldsPerWorldBindingsLongAttributeAttributeGetterCallbackForMainWorld(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_activityLoggingAccessForIsolatedWorldsPerWorldBindingsLongAttribute_Getter");

  test_object_v8_internal::ActivityLoggingAccessForIsolatedWorldsPerWorldBindingsLongAttributeAttributeGetterForMainWorld(info);
}

void V8TestObject::ActivityLoggingAccessForIsolatedWorldsPerWorldBindingsLongAttributeAttributeSetterCallbackForMainWorld(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_activityLoggingAccessForIsolatedWorldsPerWorldBindingsLongAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::ActivityLoggingAccessForIsolatedWorldsPerWorldBindingsLongAttributeAttributeSetterForMainWorld(v8Value, info);
}

void V8TestObject::ActivityLoggingGetterPerWorldBindingsLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_activityLoggingGetterPerWorldBindingsLongAttribute_Getter");

  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);
  V8PerContextData* contextData = scriptState->PerContextData();
  if (contextData && contextData->ActivityLogger()) {
    contextData->ActivityLogger()->LogGetter("TestObject.activityLoggingGetterPerWorldBindingsLongAttribute");
  }

  test_object_v8_internal::ActivityLoggingGetterPerWorldBindingsLongAttributeAttributeGetter(info);
}

void V8TestObject::ActivityLoggingGetterPerWorldBindingsLongAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_activityLoggingGetterPerWorldBindingsLongAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::ActivityLoggingGetterPerWorldBindingsLongAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::ActivityLoggingGetterPerWorldBindingsLongAttributeAttributeGetterCallbackForMainWorld(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_activityLoggingGetterPerWorldBindingsLongAttribute_Getter");

  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);
  V8PerContextData* contextData = scriptState->PerContextData();
  if (contextData && contextData->ActivityLogger()) {
    contextData->ActivityLogger()->LogGetter("TestObject.activityLoggingGetterPerWorldBindingsLongAttribute");
  }

  test_object_v8_internal::ActivityLoggingGetterPerWorldBindingsLongAttributeAttributeGetterForMainWorld(info);
}

void V8TestObject::ActivityLoggingGetterPerWorldBindingsLongAttributeAttributeSetterCallbackForMainWorld(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_activityLoggingGetterPerWorldBindingsLongAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::ActivityLoggingGetterPerWorldBindingsLongAttributeAttributeSetterForMainWorld(v8Value, info);
}

void V8TestObject::ActivityLoggingGetterForIsolatedWorldsPerWorldBindingsLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_activityLoggingGetterForIsolatedWorldsPerWorldBindingsLongAttribute_Getter");

  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);
  V8PerContextData* contextData = scriptState->PerContextData();
  if (contextData && contextData->ActivityLogger()) {
    contextData->ActivityLogger()->LogGetter("TestObject.activityLoggingGetterForIsolatedWorldsPerWorldBindingsLongAttribute");
  }

  test_object_v8_internal::ActivityLoggingGetterForIsolatedWorldsPerWorldBindingsLongAttributeAttributeGetter(info);
}

void V8TestObject::ActivityLoggingGetterForIsolatedWorldsPerWorldBindingsLongAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_activityLoggingGetterForIsolatedWorldsPerWorldBindingsLongAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::ActivityLoggingGetterForIsolatedWorldsPerWorldBindingsLongAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::ActivityLoggingGetterForIsolatedWorldsPerWorldBindingsLongAttributeAttributeGetterCallbackForMainWorld(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_activityLoggingGetterForIsolatedWorldsPerWorldBindingsLongAttribute_Getter");

  test_object_v8_internal::ActivityLoggingGetterForIsolatedWorldsPerWorldBindingsLongAttributeAttributeGetterForMainWorld(info);
}

void V8TestObject::ActivityLoggingGetterForIsolatedWorldsPerWorldBindingsLongAttributeAttributeSetterCallbackForMainWorld(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_activityLoggingGetterForIsolatedWorldsPerWorldBindingsLongAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::ActivityLoggingGetterForIsolatedWorldsPerWorldBindingsLongAttributeAttributeSetterForMainWorld(v8Value, info);
}

void V8TestObject::LocationAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_location_Getter");

  test_object_v8_internal::LocationAttributeGetter(info);
}

void V8TestObject::LocationAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_location_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::LocationAttributeSetter(v8Value, info);
}

void V8TestObject::LocationWithExceptionAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_locationWithException_Getter");

  test_object_v8_internal::LocationWithExceptionAttributeGetter(info);
}

void V8TestObject::LocationWithExceptionAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_locationWithException_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::LocationWithExceptionAttributeSetter(v8Value, info);
}

void V8TestObject::LocationWithCallWithAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_locationWithCallWith_Getter");

  test_object_v8_internal::LocationWithCallWithAttributeGetter(info);
}

void V8TestObject::LocationWithCallWithAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_locationWithCallWith_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::LocationWithCallWithAttributeSetter(v8Value, info);
}

void V8TestObject::LocationByteStringAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_locationByteString_Getter");

  test_object_v8_internal::LocationByteStringAttributeGetter(info);
}

void V8TestObject::LocationByteStringAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_locationByteString_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::LocationByteStringAttributeSetter(v8Value, info);
}

void V8TestObject::LocationWithPerWorldBindingsAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_locationWithPerWorldBindings_Getter");

  test_object_v8_internal::LocationWithPerWorldBindingsAttributeGetter(info);
}

void V8TestObject::LocationWithPerWorldBindingsAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_locationWithPerWorldBindings_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::LocationWithPerWorldBindingsAttributeSetter(v8Value, info);
}

void V8TestObject::LocationWithPerWorldBindingsAttributeGetterCallbackForMainWorld(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_locationWithPerWorldBindings_Getter");

  test_object_v8_internal::LocationWithPerWorldBindingsAttributeGetterForMainWorld(info);
}

void V8TestObject::LocationWithPerWorldBindingsAttributeSetterCallbackForMainWorld(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_locationWithPerWorldBindings_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::LocationWithPerWorldBindingsAttributeSetterForMainWorld(v8Value, info);
}

void V8TestObject::LocationLegacyInterfaceTypeCheckingAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_locationLegacyInterfaceTypeChecking_Getter");

  test_object_v8_internal::LocationLegacyInterfaceTypeCheckingAttributeGetter(info);
}

void V8TestObject::LocationLegacyInterfaceTypeCheckingAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_locationLegacyInterfaceTypeChecking_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::LocationLegacyInterfaceTypeCheckingAttributeSetter(v8Value, info);
}

void V8TestObject::RaisesExceptionLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_raisesExceptionLongAttribute_Getter");

  test_object_v8_internal::RaisesExceptionLongAttributeAttributeGetter(info);
}

void V8TestObject::RaisesExceptionLongAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_raisesExceptionLongAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::RaisesExceptionLongAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::RaisesExceptionGetterLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_raisesExceptionGetterLongAttribute_Getter");

  test_object_v8_internal::RaisesExceptionGetterLongAttributeAttributeGetter(info);
}

void V8TestObject::RaisesExceptionGetterLongAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_raisesExceptionGetterLongAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::RaisesExceptionGetterLongAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::SetterRaisesExceptionLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_setterRaisesExceptionLongAttribute_Getter");

  test_object_v8_internal::SetterRaisesExceptionLongAttributeAttributeGetter(info);
}

void V8TestObject::SetterRaisesExceptionLongAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_setterRaisesExceptionLongAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::SetterRaisesExceptionLongAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::RaisesExceptionTestInterfaceEmptyAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_raisesExceptionTestInterfaceEmptyAttribute_Getter");

  test_object_v8_internal::RaisesExceptionTestInterfaceEmptyAttributeAttributeGetter(info);
}

void V8TestObject::RaisesExceptionTestInterfaceEmptyAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_raisesExceptionTestInterfaceEmptyAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::RaisesExceptionTestInterfaceEmptyAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::CachedAttributeRaisesExceptionGetterAnyAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_cachedAttributeRaisesExceptionGetterAnyAttribute_Getter");

  test_object_v8_internal::CachedAttributeRaisesExceptionGetterAnyAttributeAttributeGetter(info);
}

void V8TestObject::CachedAttributeRaisesExceptionGetterAnyAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_cachedAttributeRaisesExceptionGetterAnyAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::CachedAttributeRaisesExceptionGetterAnyAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::ReflectTestInterfaceAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_reflectTestInterfaceAttribute_Getter");

  test_object_v8_internal::ReflectTestInterfaceAttributeAttributeGetter(info);
}

void V8TestObject::ReflectTestInterfaceAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_reflectTestInterfaceAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::ReflectTestInterfaceAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::ReflectReflectedNameAttributeTestAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_reflectReflectedNameAttributeTestAttribute_Getter");

  test_object_v8_internal::ReflectReflectedNameAttributeTestAttributeAttributeGetter(info);
}

void V8TestObject::ReflectReflectedNameAttributeTestAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_reflectReflectedNameAttributeTestAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::ReflectReflectedNameAttributeTestAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::ReflectBooleanAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_reflectBooleanAttribute_Getter");

  test_object_v8_internal::ReflectBooleanAttributeAttributeGetter(info);
}

void V8TestObject::ReflectBooleanAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_reflectBooleanAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::ReflectBooleanAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::ReflectLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_reflectLongAttribute_Getter");

  test_object_v8_internal::ReflectLongAttributeAttributeGetter(info);
}

void V8TestObject::ReflectLongAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_reflectLongAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::ReflectLongAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::ReflectUnsignedShortAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_reflectUnsignedShortAttribute_Getter");

  test_object_v8_internal::ReflectUnsignedShortAttributeAttributeGetter(info);
}

void V8TestObject::ReflectUnsignedShortAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_reflectUnsignedShortAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::ReflectUnsignedShortAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::ReflectUnsignedLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_reflectUnsignedLongAttribute_Getter");

  test_object_v8_internal::ReflectUnsignedLongAttributeAttributeGetter(info);
}

void V8TestObject::ReflectUnsignedLongAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_reflectUnsignedLongAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::ReflectUnsignedLongAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::IdAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_id_Getter");

  test_object_v8_internal::IdAttributeGetter(info);
}

void V8TestObject::IdAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_id_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::IdAttributeSetter(v8Value, info);
}

void V8TestObject::NameAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_name_Getter");

  test_object_v8_internal::NameAttributeGetter(info);
}

void V8TestObject::NameAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_name_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::NameAttributeSetter(v8Value, info);
}

void V8TestObject::ClassAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_class_Getter");

  test_object_v8_internal::ClassAttributeGetter(info);
}

void V8TestObject::ClassAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_class_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::ClassAttributeSetter(v8Value, info);
}

void V8TestObject::ReflectedIdAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_reflectedId_Getter");

  test_object_v8_internal::ReflectedIdAttributeGetter(info);
}

void V8TestObject::ReflectedIdAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_reflectedId_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::ReflectedIdAttributeSetter(v8Value, info);
}

void V8TestObject::ReflectedNameAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_reflectedName_Getter");

  test_object_v8_internal::ReflectedNameAttributeGetter(info);
}

void V8TestObject::ReflectedNameAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_reflectedName_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::ReflectedNameAttributeSetter(v8Value, info);
}

void V8TestObject::ReflectedClassAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_reflectedClass_Getter");

  test_object_v8_internal::ReflectedClassAttributeGetter(info);
}

void V8TestObject::ReflectedClassAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_reflectedClass_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::ReflectedClassAttributeSetter(v8Value, info);
}

void V8TestObject::LimitedToOnlyOneAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_limitedToOnlyOneAttribute_Getter");

  test_object_v8_internal::LimitedToOnlyOneAttributeAttributeGetter(info);
}

void V8TestObject::LimitedToOnlyOneAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_limitedToOnlyOneAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::LimitedToOnlyOneAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::LimitedToOnlyAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_limitedToOnlyAttribute_Getter");

  test_object_v8_internal::LimitedToOnlyAttributeAttributeGetter(info);
}

void V8TestObject::LimitedToOnlyAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_limitedToOnlyAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::LimitedToOnlyAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::LimitedToOnlyOtherAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_limitedToOnlyOtherAttribute_Getter");

  test_object_v8_internal::LimitedToOnlyOtherAttributeAttributeGetter(info);
}

void V8TestObject::LimitedToOnlyOtherAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_limitedToOnlyOtherAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::LimitedToOnlyOtherAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::LimitedWithMissingDefaultAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_limitedWithMissingDefaultAttribute_Getter");

  test_object_v8_internal::LimitedWithMissingDefaultAttributeAttributeGetter(info);
}

void V8TestObject::LimitedWithMissingDefaultAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_limitedWithMissingDefaultAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::LimitedWithMissingDefaultAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::LimitedWithInvalidMissingDefaultAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_limitedWithInvalidMissingDefaultAttribute_Getter");

  test_object_v8_internal::LimitedWithInvalidMissingDefaultAttributeAttributeGetter(info);
}

void V8TestObject::LimitedWithInvalidMissingDefaultAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_limitedWithInvalidMissingDefaultAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::LimitedWithInvalidMissingDefaultAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::CorsSettingAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_corsSettingAttribute_Getter");

  test_object_v8_internal::CorsSettingAttributeAttributeGetter(info);
}

void V8TestObject::LimitedWithEmptyMissingInvalidAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_limitedWithEmptyMissingInvalidAttribute_Getter");

  test_object_v8_internal::LimitedWithEmptyMissingInvalidAttributeAttributeGetter(info);
}

void V8TestObject::RuntimeCallStatsCounterAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE(info.GetIsolate(), RuntimeCallStats::CounterId::kRuntimeCallStatsCounterAttribute_Getter);

  test_object_v8_internal::RuntimeCallStatsCounterAttributeAttributeGetter(info);
}

void V8TestObject::RuntimeCallStatsCounterAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE(info.GetIsolate(), RuntimeCallStats::CounterId::kRuntimeCallStatsCounterAttribute_Setter);

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::RuntimeCallStatsCounterAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::RuntimeCallStatsCounterReadOnlyAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE(info.GetIsolate(), RuntimeCallStats::CounterId::kRuntimeCallStatsCounterReadOnlyAttribute_Getter);

  test_object_v8_internal::RuntimeCallStatsCounterReadOnlyAttributeAttributeGetter(info);
}

void V8TestObject::ReplaceableReadonlyLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_replaceableReadonlyLongAttribute_Getter");

  test_object_v8_internal::ReplaceableReadonlyLongAttributeAttributeGetter(info);
}

void V8TestObject::ReplaceableReadonlyLongAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_replaceableReadonlyLongAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::ReplaceableReadonlyLongAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::LocationPutForwardsAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_locationPutForwards_Getter");

  test_object_v8_internal::LocationPutForwardsAttributeGetter(info);
}

void V8TestObject::LocationPutForwardsAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_locationPutForwards_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::LocationPutForwardsAttributeSetter(v8Value, info);
}

void V8TestObject::RuntimeEnabledLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_runtimeEnabledLongAttribute_Getter");

  test_object_v8_internal::RuntimeEnabledLongAttributeAttributeGetter(info);
}

void V8TestObject::RuntimeEnabledLongAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_runtimeEnabledLongAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::RuntimeEnabledLongAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::SetterCallWithCurrentWindowAndEnteredWindowStringAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_setterCallWithCurrentWindowAndEnteredWindowStringAttribute_Getter");

  test_object_v8_internal::SetterCallWithCurrentWindowAndEnteredWindowStringAttributeAttributeGetter(info);
}

void V8TestObject::SetterCallWithCurrentWindowAndEnteredWindowStringAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_setterCallWithCurrentWindowAndEnteredWindowStringAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::SetterCallWithCurrentWindowAndEnteredWindowStringAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::SetterCallWithExecutionContextStringAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_setterCallWithExecutionContextStringAttribute_Getter");

  test_object_v8_internal::SetterCallWithExecutionContextStringAttributeAttributeGetter(info);
}

void V8TestObject::SetterCallWithExecutionContextStringAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_setterCallWithExecutionContextStringAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::SetterCallWithExecutionContextStringAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::TreatNullAsEmptyStringStringAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_treatNullAsEmptyStringStringAttribute_Getter");

  test_object_v8_internal::TreatNullAsEmptyStringStringAttributeAttributeGetter(info);
}

void V8TestObject::TreatNullAsEmptyStringStringAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_treatNullAsEmptyStringStringAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::TreatNullAsEmptyStringStringAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::LegacyInterfaceTypeCheckingFloatAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_legacyInterfaceTypeCheckingFloatAttribute_Getter");

  test_object_v8_internal::LegacyInterfaceTypeCheckingFloatAttributeAttributeGetter(info);
}

void V8TestObject::LegacyInterfaceTypeCheckingFloatAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_legacyInterfaceTypeCheckingFloatAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::LegacyInterfaceTypeCheckingFloatAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::LegacyInterfaceTypeCheckingTestInterfaceAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_legacyInterfaceTypeCheckingTestInterfaceAttribute_Getter");

  test_object_v8_internal::LegacyInterfaceTypeCheckingTestInterfaceAttributeAttributeGetter(info);
}

void V8TestObject::LegacyInterfaceTypeCheckingTestInterfaceAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_legacyInterfaceTypeCheckingTestInterfaceAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::LegacyInterfaceTypeCheckingTestInterfaceAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::LegacyInterfaceTypeCheckingTestInterfaceOrNullAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_legacyInterfaceTypeCheckingTestInterfaceOrNullAttribute_Getter");

  test_object_v8_internal::LegacyInterfaceTypeCheckingTestInterfaceOrNullAttributeAttributeGetter(info);
}

void V8TestObject::LegacyInterfaceTypeCheckingTestInterfaceOrNullAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_legacyInterfaceTypeCheckingTestInterfaceOrNullAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::LegacyInterfaceTypeCheckingTestInterfaceOrNullAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::UrlStringAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_urlStringAttribute_Getter");

  test_object_v8_internal::UrlStringAttributeAttributeGetter(info);
}

void V8TestObject::UrlStringAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_urlStringAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::UrlStringAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::UrlStringAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_urlStringAttribute_Getter");

  test_object_v8_internal::UrlStringAttributeAttributeGetter(info);
}

void V8TestObject::UrlStringAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_urlStringAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::UrlStringAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::UnforgeableLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_unforgeableLongAttribute_Getter");

  test_object_v8_internal::UnforgeableLongAttributeAttributeGetter(info);
}

void V8TestObject::UnforgeableLongAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_unforgeableLongAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::UnforgeableLongAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::MeasuredLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_measuredLongAttribute_Getter");

  UseCounter::Count(CurrentExecutionContext(info.GetIsolate()), WebFeature::kV8TestObject_MeasuredLongAttribute_AttributeGetter);

  test_object_v8_internal::MeasuredLongAttributeAttributeGetter(info);
}

void V8TestObject::MeasuredLongAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_measuredLongAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  UseCounter::Count(CurrentExecutionContext(info.GetIsolate()), WebFeature::kV8TestObject_MeasuredLongAttribute_AttributeSetter);

  test_object_v8_internal::MeasuredLongAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::SameObjectAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_sameObjectAttribute_Getter");

  test_object_v8_internal::SameObjectAttributeAttributeGetter(info);
}

void V8TestObject::SaveSameObjectAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_saveSameObjectAttribute_Getter");

  test_object_v8_internal::SaveSameObjectAttributeAttributeGetter(info);
}

void V8TestObject::StaticSaveSameObjectAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_staticSaveSameObjectAttribute_Getter");

  test_object_v8_internal::StaticSaveSameObjectAttributeAttributeGetter(info);
}

void V8TestObject::UnscopableLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_unscopableLongAttribute_Getter");

  test_object_v8_internal::UnscopableLongAttributeAttributeGetter(info);
}

void V8TestObject::UnscopableLongAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_unscopableLongAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::UnscopableLongAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::UnscopableOriginTrialEnabledLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_unscopableOriginTrialEnabledLongAttribute_Getter");

  test_object_v8_internal::UnscopableOriginTrialEnabledLongAttributeAttributeGetter(info);
}

void V8TestObject::UnscopableOriginTrialEnabledLongAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_unscopableOriginTrialEnabledLongAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::UnscopableOriginTrialEnabledLongAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::UnscopableRuntimeEnabledLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_unscopableRuntimeEnabledLongAttribute_Getter");

  test_object_v8_internal::UnscopableRuntimeEnabledLongAttributeAttributeGetter(info);
}

void V8TestObject::UnscopableRuntimeEnabledLongAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_unscopableRuntimeEnabledLongAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::UnscopableRuntimeEnabledLongAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::TestInterfaceAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_testInterfaceAttribute_Getter");

  test_object_v8_internal::TestInterfaceAttributeAttributeGetter(info);
}

void V8TestObject::TestInterfaceAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_testInterfaceAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_object_v8_internal::TestInterfaceAttributeAttributeSetter(v8Value, info);
}

void V8TestObject::SizeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_size_Getter");

  test_object_v8_internal::SizeAttributeGetter(info);
}

void V8TestObject::UnscopableVoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_unscopableVoidMethod");

  test_object_v8_internal::UnscopableVoidMethodMethod(info);
}

void V8TestObject::UnscopableRuntimeEnabledVoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_unscopableRuntimeEnabledVoidMethod");

  test_object_v8_internal::UnscopableRuntimeEnabledVoidMethodMethod(info);
}

void V8TestObject::VoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethod");

  test_object_v8_internal::VoidMethodMethod(info);
}

void V8TestObject::StaticVoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_staticVoidMethod");

  test_object_v8_internal::StaticVoidMethodMethod(info);
}

void V8TestObject::DateMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_dateMethod");

  test_object_v8_internal::DateMethodMethod(info);
}

void V8TestObject::StringMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_stringMethod");

  test_object_v8_internal::StringMethodMethod(info);
}

void V8TestObject::ByteStringMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_byteStringMethod");

  test_object_v8_internal::ByteStringMethodMethod(info);
}

void V8TestObject::UsvStringMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_usvStringMethod");

  test_object_v8_internal::UsvStringMethodMethod(info);
}

void V8TestObject::ReadonlyDOMTimeStampMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_readonlyDOMTimeStampMethod");

  test_object_v8_internal::ReadonlyDOMTimeStampMethodMethod(info);
}

void V8TestObject::BooleanMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_booleanMethod");

  test_object_v8_internal::BooleanMethodMethod(info);
}

void V8TestObject::ByteMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_byteMethod");

  test_object_v8_internal::ByteMethodMethod(info);
}

void V8TestObject::DoubleMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_doubleMethod");

  test_object_v8_internal::DoubleMethodMethod(info);
}

void V8TestObject::FloatMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_floatMethod");

  test_object_v8_internal::FloatMethodMethod(info);
}

void V8TestObject::LongMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_longMethod");

  test_object_v8_internal::LongMethodMethod(info);
}

void V8TestObject::LongLongMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_longLongMethod");

  test_object_v8_internal::LongLongMethodMethod(info);
}

void V8TestObject::OctetMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_octetMethod");

  test_object_v8_internal::OctetMethodMethod(info);
}

void V8TestObject::ShortMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_shortMethod");

  test_object_v8_internal::ShortMethodMethod(info);
}

void V8TestObject::UnsignedLongMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_unsignedLongMethod");

  test_object_v8_internal::UnsignedLongMethodMethod(info);
}

void V8TestObject::UnsignedLongLongMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_unsignedLongLongMethod");

  test_object_v8_internal::UnsignedLongLongMethodMethod(info);
}

void V8TestObject::UnsignedShortMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_unsignedShortMethod");

  test_object_v8_internal::UnsignedShortMethodMethod(info);
}

void V8TestObject::VoidMethodDateArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodDateArg");

  test_object_v8_internal::VoidMethodDateArgMethod(info);
}

void V8TestObject::VoidMethodStringArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodStringArg");

  test_object_v8_internal::VoidMethodStringArgMethod(info);
}

void V8TestObject::VoidMethodByteStringArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodByteStringArg");

  test_object_v8_internal::VoidMethodByteStringArgMethod(info);
}

void V8TestObject::VoidMethodUSVStringArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodUSVStringArg");

  test_object_v8_internal::VoidMethodUSVStringArgMethod(info);
}

void V8TestObject::VoidMethodDOMTimeStampArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodDOMTimeStampArg");

  test_object_v8_internal::VoidMethodDOMTimeStampArgMethod(info);
}

void V8TestObject::VoidMethodBooleanArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodBooleanArg");

  test_object_v8_internal::VoidMethodBooleanArgMethod(info);
}

void V8TestObject::VoidMethodByteArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodByteArg");

  test_object_v8_internal::VoidMethodByteArgMethod(info);
}

void V8TestObject::VoidMethodDoubleArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodDoubleArg");

  test_object_v8_internal::VoidMethodDoubleArgMethod(info);
}

void V8TestObject::VoidMethodFloatArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodFloatArg");

  test_object_v8_internal::VoidMethodFloatArgMethod(info);
}

void V8TestObject::VoidMethodLongArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodLongArg");

  test_object_v8_internal::VoidMethodLongArgMethod(info);
}

void V8TestObject::VoidMethodLongLongArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodLongLongArg");

  test_object_v8_internal::VoidMethodLongLongArgMethod(info);
}

void V8TestObject::VoidMethodOctetArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodOctetArg");

  test_object_v8_internal::VoidMethodOctetArgMethod(info);
}

void V8TestObject::VoidMethodShortArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodShortArg");

  test_object_v8_internal::VoidMethodShortArgMethod(info);
}

void V8TestObject::VoidMethodUnsignedLongArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodUnsignedLongArg");

  test_object_v8_internal::VoidMethodUnsignedLongArgMethod(info);
}

void V8TestObject::VoidMethodUnsignedLongLongArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodUnsignedLongLongArg");

  test_object_v8_internal::VoidMethodUnsignedLongLongArgMethod(info);
}

void V8TestObject::VoidMethodUnsignedShortArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodUnsignedShortArg");

  test_object_v8_internal::VoidMethodUnsignedShortArgMethod(info);
}

void V8TestObject::TestInterfaceEmptyMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_testInterfaceEmptyMethod");

  test_object_v8_internal::TestInterfaceEmptyMethodMethod(info);
}

void V8TestObject::VoidMethodTestInterfaceEmptyArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodTestInterfaceEmptyArg");

  test_object_v8_internal::VoidMethodTestInterfaceEmptyArgMethod(info);
}

void V8TestObject::VoidMethodLongArgTestInterfaceEmptyArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodLongArgTestInterfaceEmptyArg");

  test_object_v8_internal::VoidMethodLongArgTestInterfaceEmptyArgMethod(info);
}

void V8TestObject::AnyMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_anyMethod");

  test_object_v8_internal::AnyMethodMethod(info);
}

void V8TestObject::VoidMethodEventTargetArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodEventTargetArg");

  test_object_v8_internal::VoidMethodEventTargetArgMethod(info);
}

void V8TestObject::VoidMethodAnyArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodAnyArg");

  test_object_v8_internal::VoidMethodAnyArgMethod(info);
}

void V8TestObject::VoidMethodAttrArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodAttrArg");

  test_object_v8_internal::VoidMethodAttrArgMethod(info);
}

void V8TestObject::VoidMethodDocumentArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodDocumentArg");

  test_object_v8_internal::VoidMethodDocumentArgMethod(info);
}

void V8TestObject::VoidMethodDocumentTypeArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodDocumentTypeArg");

  test_object_v8_internal::VoidMethodDocumentTypeArgMethod(info);
}

void V8TestObject::VoidMethodElementArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodElementArg");

  test_object_v8_internal::VoidMethodElementArgMethod(info);
}

void V8TestObject::VoidMethodNodeArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodNodeArg");

  test_object_v8_internal::VoidMethodNodeArgMethod(info);
}

void V8TestObject::ArrayBufferMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_arrayBufferMethod");

  test_object_v8_internal::ArrayBufferMethodMethod(info);
}

void V8TestObject::ArrayBufferViewMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_arrayBufferViewMethod");

  test_object_v8_internal::ArrayBufferViewMethodMethod(info);
}

void V8TestObject::ArrayBufferViewMethodRaisesExceptionMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_arrayBufferViewMethodRaisesException");

  test_object_v8_internal::ArrayBufferViewMethodRaisesExceptionMethod(info);
}

void V8TestObject::Float32ArrayMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_float32ArrayMethod");

  test_object_v8_internal::Float32ArrayMethodMethod(info);
}

void V8TestObject::Int32ArrayMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_int32ArrayMethod");

  test_object_v8_internal::Int32ArrayMethodMethod(info);
}

void V8TestObject::Uint8ArrayMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_uint8ArrayMethod");

  test_object_v8_internal::Uint8ArrayMethodMethod(info);
}

void V8TestObject::VoidMethodArrayBufferArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodArrayBufferArg");

  test_object_v8_internal::VoidMethodArrayBufferArgMethod(info);
}

void V8TestObject::VoidMethodArrayBufferOrNullArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodArrayBufferOrNullArg");

  test_object_v8_internal::VoidMethodArrayBufferOrNullArgMethod(info);
}

void V8TestObject::VoidMethodArrayBufferViewArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodArrayBufferViewArg");

  test_object_v8_internal::VoidMethodArrayBufferViewArgMethod(info);
}

void V8TestObject::VoidMethodFlexibleArrayBufferViewArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodFlexibleArrayBufferViewArg");

  test_object_v8_internal::VoidMethodFlexibleArrayBufferViewArgMethod(info);
}

void V8TestObject::VoidMethodFlexibleArrayBufferViewTypedArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodFlexibleArrayBufferViewTypedArg");

  test_object_v8_internal::VoidMethodFlexibleArrayBufferViewTypedArgMethod(info);
}

void V8TestObject::VoidMethodFloat32ArrayArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodFloat32ArrayArg");

  test_object_v8_internal::VoidMethodFloat32ArrayArgMethod(info);
}

void V8TestObject::VoidMethodInt32ArrayArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodInt32ArrayArg");

  test_object_v8_internal::VoidMethodInt32ArrayArgMethod(info);
}

void V8TestObject::VoidMethodUint8ArrayArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodUint8ArrayArg");

  test_object_v8_internal::VoidMethodUint8ArrayArgMethod(info);
}

void V8TestObject::VoidMethodAllowSharedArrayBufferViewArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodAllowSharedArrayBufferViewArg");

  test_object_v8_internal::VoidMethodAllowSharedArrayBufferViewArgMethod(info);
}

void V8TestObject::VoidMethodAllowSharedUint8ArrayArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodAllowSharedUint8ArrayArg");

  test_object_v8_internal::VoidMethodAllowSharedUint8ArrayArgMethod(info);
}

void V8TestObject::LongSequenceMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_longSequenceMethod");

  test_object_v8_internal::LongSequenceMethodMethod(info);
}

void V8TestObject::StringSequenceMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_stringSequenceMethod");

  test_object_v8_internal::StringSequenceMethodMethod(info);
}

void V8TestObject::TestInterfaceEmptySequenceMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_testInterfaceEmptySequenceMethod");

  test_object_v8_internal::TestInterfaceEmptySequenceMethodMethod(info);
}

void V8TestObject::VoidMethodSequenceLongArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodSequenceLongArg");

  test_object_v8_internal::VoidMethodSequenceLongArgMethod(info);
}

void V8TestObject::VoidMethodSequenceStringArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodSequenceStringArg");

  test_object_v8_internal::VoidMethodSequenceStringArgMethod(info);
}

void V8TestObject::VoidMethodSequenceTestInterfaceEmptyArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodSequenceTestInterfaceEmptyArg");

  test_object_v8_internal::VoidMethodSequenceTestInterfaceEmptyArgMethod(info);
}

void V8TestObject::VoidMethodSequenceSequenceDOMStringArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodSequenceSequenceDOMStringArg");

  test_object_v8_internal::VoidMethodSequenceSequenceDOMStringArgMethod(info);
}

void V8TestObject::VoidMethodNullableSequenceLongArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodNullableSequenceLongArg");

  test_object_v8_internal::VoidMethodNullableSequenceLongArgMethod(info);
}

void V8TestObject::LongFrozenArrayMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_longFrozenArrayMethod");

  test_object_v8_internal::LongFrozenArrayMethodMethod(info);
}

void V8TestObject::VoidMethodStringFrozenArrayMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodStringFrozenArrayMethod");

  test_object_v8_internal::VoidMethodStringFrozenArrayMethodMethod(info);
}

void V8TestObject::VoidMethodTestInterfaceEmptyFrozenArrayMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodTestInterfaceEmptyFrozenArrayMethod");

  test_object_v8_internal::VoidMethodTestInterfaceEmptyFrozenArrayMethodMethod(info);
}

void V8TestObject::NullableLongMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_nullableLongMethod");

  test_object_v8_internal::NullableLongMethodMethod(info);
}

void V8TestObject::NullableStringMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_nullableStringMethod");

  test_object_v8_internal::NullableStringMethodMethod(info);
}

void V8TestObject::NullableTestInterfaceMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_nullableTestInterfaceMethod");

  test_object_v8_internal::NullableTestInterfaceMethodMethod(info);
}

void V8TestObject::NullableLongSequenceMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_nullableLongSequenceMethod");

  test_object_v8_internal::NullableLongSequenceMethodMethod(info);
}

void V8TestObject::BooleanOrDOMStringOrUnrestrictedDoubleMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_booleanOrDOMStringOrUnrestrictedDoubleMethod");

  test_object_v8_internal::BooleanOrDOMStringOrUnrestrictedDoubleMethodMethod(info);
}

void V8TestObject::TestInterfaceOrLongMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_testInterfaceOrLongMethod");

  test_object_v8_internal::TestInterfaceOrLongMethodMethod(info);
}

void V8TestObject::VoidMethodDoubleOrDOMStringArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodDoubleOrDOMStringArg");

  test_object_v8_internal::VoidMethodDoubleOrDOMStringArgMethod(info);
}

void V8TestObject::VoidMethodDoubleOrDOMStringOrNullArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodDoubleOrDOMStringOrNullArg");

  test_object_v8_internal::VoidMethodDoubleOrDOMStringOrNullArgMethod(info);
}

void V8TestObject::VoidMethodDoubleOrNullOrDOMStringArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodDoubleOrNullOrDOMStringArg");

  test_object_v8_internal::VoidMethodDoubleOrNullOrDOMStringArgMethod(info);
}

void V8TestObject::VoidMethodDOMStringOrArrayBufferOrArrayBufferViewArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodDOMStringOrArrayBufferOrArrayBufferViewArg");

  test_object_v8_internal::VoidMethodDOMStringOrArrayBufferOrArrayBufferViewArgMethod(info);
}

void V8TestObject::VoidMethodArrayBufferOrArrayBufferViewOrDictionaryArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodArrayBufferOrArrayBufferViewOrDictionaryArg");

  test_object_v8_internal::VoidMethodArrayBufferOrArrayBufferViewOrDictionaryArgMethod(info);
}

void V8TestObject::VoidMethodBooleanOrElementSequenceArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodBooleanOrElementSequenceArg");

  test_object_v8_internal::VoidMethodBooleanOrElementSequenceArgMethod(info);
}

void V8TestObject::VoidMethodDoubleOrLongOrBooleanSequenceArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodDoubleOrLongOrBooleanSequenceArg");

  test_object_v8_internal::VoidMethodDoubleOrLongOrBooleanSequenceArgMethod(info);
}

void V8TestObject::VoidMethodElementSequenceOrByteStringDoubleOrStringRecordMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodElementSequenceOrByteStringDoubleOrStringRecord");

  test_object_v8_internal::VoidMethodElementSequenceOrByteStringDoubleOrStringRecordMethod(info);
}

void V8TestObject::VoidMethodArrayOfDoubleOrDOMStringArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodArrayOfDoubleOrDOMStringArg");

  test_object_v8_internal::VoidMethodArrayOfDoubleOrDOMStringArgMethod(info);
}

void V8TestObject::VoidMethodTestInterfaceEmptyOrNullArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodTestInterfaceEmptyOrNullArg");

  test_object_v8_internal::VoidMethodTestInterfaceEmptyOrNullArgMethod(info);
}

void V8TestObject::VoidMethodTestCallbackInterfaceArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodTestCallbackInterfaceArg");

  test_object_v8_internal::VoidMethodTestCallbackInterfaceArgMethod(info);
}

void V8TestObject::VoidMethodOptionalTestCallbackInterfaceArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodOptionalTestCallbackInterfaceArg");

  test_object_v8_internal::VoidMethodOptionalTestCallbackInterfaceArgMethod(info);
}

void V8TestObject::VoidMethodTestCallbackInterfaceOrNullArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodTestCallbackInterfaceOrNullArg");

  test_object_v8_internal::VoidMethodTestCallbackInterfaceOrNullArgMethod(info);
}

void V8TestObject::TestEnumMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_testEnumMethod");

  test_object_v8_internal::TestEnumMethodMethod(info);
}

void V8TestObject::VoidMethodTestEnumArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodTestEnumArg");

  test_object_v8_internal::VoidMethodTestEnumArgMethod(info);
}

void V8TestObject::VoidMethodTestMultipleEnumArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodTestMultipleEnumArg");

  test_object_v8_internal::VoidMethodTestMultipleEnumArgMethod(info);
}

void V8TestObject::DictionaryMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_dictionaryMethod");

  test_object_v8_internal::DictionaryMethodMethod(info);
}

void V8TestObject::TestDictionaryMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_testDictionaryMethod");

  test_object_v8_internal::TestDictionaryMethodMethod(info);
}

void V8TestObject::NullableTestDictionaryMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_nullableTestDictionaryMethod");

  test_object_v8_internal::NullableTestDictionaryMethodMethod(info);
}

void V8TestObject::StaticTestDictionaryMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_staticTestDictionaryMethod");

  test_object_v8_internal::StaticTestDictionaryMethodMethod(info);
}

void V8TestObject::StaticNullableTestDictionaryMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_staticNullableTestDictionaryMethod");

  test_object_v8_internal::StaticNullableTestDictionaryMethodMethod(info);
}

void V8TestObject::PassPermissiveDictionaryMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_passPermissiveDictionaryMethod");

  test_object_v8_internal::PassPermissiveDictionaryMethodMethod(info);
}

void V8TestObject::NodeFilterMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_nodeFilterMethod");

  test_object_v8_internal::NodeFilterMethodMethod(info);
}

void V8TestObject::PromiseMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_promiseMethod");

  test_object_v8_internal::PromiseMethodMethod(info);
}

void V8TestObject::PromiseMethodWithoutExceptionStateMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_promiseMethodWithoutExceptionState");

  test_object_v8_internal::PromiseMethodWithoutExceptionStateMethod(info);
}

void V8TestObject::SerializedScriptValueMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_serializedScriptValueMethod");

  test_object_v8_internal::SerializedScriptValueMethodMethod(info);
}

void V8TestObject::XPathNSResolverMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_xPathNSResolverMethod");

  test_object_v8_internal::XPathNSResolverMethodMethod(info);
}

void V8TestObject::VoidMethodDictionaryArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodDictionaryArg");

  test_object_v8_internal::VoidMethodDictionaryArgMethod(info);
}

void V8TestObject::VoidMethodNodeFilterArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodNodeFilterArg");

  test_object_v8_internal::VoidMethodNodeFilterArgMethod(info);
}

void V8TestObject::VoidMethodPromiseArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodPromiseArg");

  test_object_v8_internal::VoidMethodPromiseArgMethod(info);
}

void V8TestObject::VoidMethodSerializedScriptValueArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodSerializedScriptValueArg");

  test_object_v8_internal::VoidMethodSerializedScriptValueArgMethod(info);
}

void V8TestObject::VoidMethodXPathNSResolverArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodXPathNSResolverArg");

  test_object_v8_internal::VoidMethodXPathNSResolverArgMethod(info);
}

void V8TestObject::VoidMethodDictionarySequenceArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodDictionarySequenceArg");

  test_object_v8_internal::VoidMethodDictionarySequenceArgMethod(info);
}

void V8TestObject::VoidMethodStringArgLongArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodStringArgLongArg");

  test_object_v8_internal::VoidMethodStringArgLongArgMethod(info);
}

void V8TestObject::VoidMethodByteStringOrNullOptionalUSVStringArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodByteStringOrNullOptionalUSVStringArg");

  test_object_v8_internal::VoidMethodByteStringOrNullOptionalUSVStringArgMethod(info);
}

void V8TestObject::VoidMethodOptionalStringArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodOptionalStringArg");

  test_object_v8_internal::VoidMethodOptionalStringArgMethod(info);
}

void V8TestObject::VoidMethodOptionalTestInterfaceEmptyArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodOptionalTestInterfaceEmptyArg");

  test_object_v8_internal::VoidMethodOptionalTestInterfaceEmptyArgMethod(info);
}

void V8TestObject::VoidMethodOptionalLongArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodOptionalLongArg");

  test_object_v8_internal::VoidMethodOptionalLongArgMethod(info);
}

void V8TestObject::StringMethodOptionalLongArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_stringMethodOptionalLongArg");

  test_object_v8_internal::StringMethodOptionalLongArgMethod(info);
}

void V8TestObject::TestInterfaceEmptyMethodOptionalLongArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_testInterfaceEmptyMethodOptionalLongArg");

  test_object_v8_internal::TestInterfaceEmptyMethodOptionalLongArgMethod(info);
}

void V8TestObject::LongMethodOptionalLongArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_longMethodOptionalLongArg");

  test_object_v8_internal::LongMethodOptionalLongArgMethod(info);
}

void V8TestObject::VoidMethodLongArgOptionalLongArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodLongArgOptionalLongArg");

  test_object_v8_internal::VoidMethodLongArgOptionalLongArgMethod(info);
}

void V8TestObject::VoidMethodLongArgOptionalLongArgOptionalLongArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodLongArgOptionalLongArgOptionalLongArg");

  test_object_v8_internal::VoidMethodLongArgOptionalLongArgOptionalLongArgMethod(info);
}

void V8TestObject::VoidMethodLongArgOptionalTestInterfaceEmptyArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodLongArgOptionalTestInterfaceEmptyArg");

  test_object_v8_internal::VoidMethodLongArgOptionalTestInterfaceEmptyArgMethod(info);
}

void V8TestObject::VoidMethodTestInterfaceEmptyArgOptionalLongArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodTestInterfaceEmptyArgOptionalLongArg");

  test_object_v8_internal::VoidMethodTestInterfaceEmptyArgOptionalLongArgMethod(info);
}

void V8TestObject::VoidMethodOptionalDictionaryArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodOptionalDictionaryArg");

  test_object_v8_internal::VoidMethodOptionalDictionaryArgMethod(info);
}

void V8TestObject::VoidMethodDefaultByteStringArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodDefaultByteStringArg");

  test_object_v8_internal::VoidMethodDefaultByteStringArgMethod(info);
}

void V8TestObject::VoidMethodDefaultStringArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodDefaultStringArg");

  test_object_v8_internal::VoidMethodDefaultStringArgMethod(info);
}

void V8TestObject::VoidMethodDefaultIntegerArgsMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodDefaultIntegerArgs");

  test_object_v8_internal::VoidMethodDefaultIntegerArgsMethod(info);
}

void V8TestObject::VoidMethodDefaultDoubleArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodDefaultDoubleArg");

  test_object_v8_internal::VoidMethodDefaultDoubleArgMethod(info);
}

void V8TestObject::VoidMethodDefaultTrueBooleanArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodDefaultTrueBooleanArg");

  test_object_v8_internal::VoidMethodDefaultTrueBooleanArgMethod(info);
}

void V8TestObject::VoidMethodDefaultFalseBooleanArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodDefaultFalseBooleanArg");

  test_object_v8_internal::VoidMethodDefaultFalseBooleanArgMethod(info);
}

void V8TestObject::VoidMethodDefaultNullableByteStringArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodDefaultNullableByteStringArg");

  test_object_v8_internal::VoidMethodDefaultNullableByteStringArgMethod(info);
}

void V8TestObject::VoidMethodDefaultNullableStringArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodDefaultNullableStringArg");

  test_object_v8_internal::VoidMethodDefaultNullableStringArgMethod(info);
}

void V8TestObject::VoidMethodDefaultNullableTestInterfaceArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodDefaultNullableTestInterfaceArg");

  test_object_v8_internal::VoidMethodDefaultNullableTestInterfaceArgMethod(info);
}

void V8TestObject::VoidMethodDefaultDoubleOrStringArgsMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodDefaultDoubleOrStringArgs");

  test_object_v8_internal::VoidMethodDefaultDoubleOrStringArgsMethod(info);
}

void V8TestObject::VoidMethodDefaultStringSequenceArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodDefaultStringSequenceArg");

  test_object_v8_internal::VoidMethodDefaultStringSequenceArgMethod(info);
}

void V8TestObject::VoidMethodVariadicStringArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodVariadicStringArg");

  test_object_v8_internal::VoidMethodVariadicStringArgMethod(info);
}

void V8TestObject::VoidMethodStringArgVariadicStringArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodStringArgVariadicStringArg");

  test_object_v8_internal::VoidMethodStringArgVariadicStringArgMethod(info);
}

void V8TestObject::VoidMethodVariadicTestInterfaceEmptyArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodVariadicTestInterfaceEmptyArg");

  test_object_v8_internal::VoidMethodVariadicTestInterfaceEmptyArgMethod(info);
}

void V8TestObject::VoidMethodTestInterfaceEmptyArgVariadicTestInterfaceEmptyArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodTestInterfaceEmptyArgVariadicTestInterfaceEmptyArg");

  test_object_v8_internal::VoidMethodTestInterfaceEmptyArgVariadicTestInterfaceEmptyArgMethod(info);
}

void V8TestObject::OverloadedMethodAMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_overloadedMethodA");

  test_object_v8_internal::OverloadedMethodAMethod(info);
}

void V8TestObject::OverloadedMethodBMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_overloadedMethodB");

  test_object_v8_internal::OverloadedMethodBMethod(info);
}

void V8TestObject::OverloadedMethodCMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_overloadedMethodC");

  test_object_v8_internal::OverloadedMethodCMethod(info);
}

void V8TestObject::OverloadedMethodDMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_overloadedMethodD");

  test_object_v8_internal::OverloadedMethodDMethod(info);
}

void V8TestObject::OverloadedMethodEMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_overloadedMethodE");

  test_object_v8_internal::OverloadedMethodEMethod(info);
}

void V8TestObject::OverloadedMethodFMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_overloadedMethodF");

  test_object_v8_internal::OverloadedMethodFMethod(info);
}

void V8TestObject::OverloadedMethodGMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_overloadedMethodG");

  test_object_v8_internal::OverloadedMethodGMethod(info);
}

void V8TestObject::OverloadedMethodHMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_overloadedMethodH");

  test_object_v8_internal::OverloadedMethodHMethod(info);
}

void V8TestObject::OverloadedMethodIMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_overloadedMethodI");

  test_object_v8_internal::OverloadedMethodIMethod(info);
}

void V8TestObject::OverloadedMethodJMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_overloadedMethodJ");

  test_object_v8_internal::OverloadedMethodJMethod(info);
}

void V8TestObject::OverloadedMethodKMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_overloadedMethodK");

  test_object_v8_internal::OverloadedMethodKMethod(info);
}

void V8TestObject::OverloadedMethodLMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_overloadedMethodL");

  test_object_v8_internal::OverloadedMethodLMethod(info);
}

void V8TestObject::OverloadedMethodNMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_overloadedMethodN");

  test_object_v8_internal::OverloadedMethodNMethod(info);
}

void V8TestObject::PromiseOverloadMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_promiseOverloadMethod");

  test_object_v8_internal::PromiseOverloadMethodMethod(info);
}

void V8TestObject::OverloadedPerWorldBindingsMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_overloadedPerWorldBindingsMethod");

  test_object_v8_internal::OverloadedPerWorldBindingsMethodMethod(info);
}

void V8TestObject::OverloadedPerWorldBindingsMethodMethodCallbackForMainWorld(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_overloadedPerWorldBindingsMethod");

  test_object_v8_internal::OverloadedPerWorldBindingsMethodMethodForMainWorld(info);
}

void V8TestObject::OverloadedStaticMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_overloadedStaticMethod");

  test_object_v8_internal::OverloadedStaticMethodMethod(info);
}

void V8TestObject::ItemMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_item");

  test_object_v8_internal::ItemMethod(info);
}

void V8TestObject::SetItemMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_setItem");

  test_object_v8_internal::SetItemMethod(info);
}

void V8TestObject::VoidMethodClampUnsignedShortArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodClampUnsignedShortArg");

  test_object_v8_internal::VoidMethodClampUnsignedShortArgMethod(info);
}

void V8TestObject::VoidMethodClampUnsignedLongArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodClampUnsignedLongArg");

  test_object_v8_internal::VoidMethodClampUnsignedLongArgMethod(info);
}

void V8TestObject::VoidMethodDefaultUndefinedTestInterfaceEmptyArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodDefaultUndefinedTestInterfaceEmptyArg");

  test_object_v8_internal::VoidMethodDefaultUndefinedTestInterfaceEmptyArgMethod(info);
}

void V8TestObject::VoidMethodDefaultUndefinedLongArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodDefaultUndefinedLongArg");

  test_object_v8_internal::VoidMethodDefaultUndefinedLongArgMethod(info);
}

void V8TestObject::VoidMethodDefaultUndefinedStringArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodDefaultUndefinedStringArg");

  test_object_v8_internal::VoidMethodDefaultUndefinedStringArgMethod(info);
}

void V8TestObject::VoidMethodEnforceRangeLongArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodEnforceRangeLongArg");

  test_object_v8_internal::VoidMethodEnforceRangeLongArgMethod(info);
}

void V8TestObject::VoidMethodTreatNullAsEmptyStringStringArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_voidMethodTreatNullAsEmptyStringStringArg");

  test_object_v8_internal::VoidMethodTreatNullAsEmptyStringStringArgMethod(info);
}

void V8TestObject::ActivityLoggingAccessForAllWorldsMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_activityLoggingAccessForAllWorldsMethod");

  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);
  V8PerContextData* contextData = scriptState->PerContextData();
  if (contextData && contextData->ActivityLogger()) {
    contextData->ActivityLogger()->LogMethod("TestObject.activityLoggingAccessForAllWorldsMethod", info);
  }
  test_object_v8_internal::ActivityLoggingAccessForAllWorldsMethodMethod(info);
}

void V8TestObject::CallWithExecutionContextVoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_callWithExecutionContextVoidMethod");

  test_object_v8_internal::CallWithExecutionContextVoidMethodMethod(info);
}

void V8TestObject::CallWithScriptStateVoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_callWithScriptStateVoidMethod");

  test_object_v8_internal::CallWithScriptStateVoidMethodMethod(info);
}

void V8TestObject::CallWithScriptStateLongMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_callWithScriptStateLongMethod");

  test_object_v8_internal::CallWithScriptStateLongMethodMethod(info);
}

void V8TestObject::CallWithScriptStateExecutionContextVoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_callWithScriptStateExecutionContextVoidMethod");

  test_object_v8_internal::CallWithScriptStateExecutionContextVoidMethodMethod(info);
}

void V8TestObject::CallWithScriptStateScriptArgumentsVoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_callWithScriptStateScriptArgumentsVoidMethod");

  test_object_v8_internal::CallWithScriptStateScriptArgumentsVoidMethodMethod(info);
}

void V8TestObject::CallWithScriptStateScriptArgumentsVoidMethodOptionalBooleanArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_callWithScriptStateScriptArgumentsVoidMethodOptionalBooleanArg");

  test_object_v8_internal::CallWithScriptStateScriptArgumentsVoidMethodOptionalBooleanArgMethod(info);
}

void V8TestObject::CallWithCurrentWindowMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_callWithCurrentWindow");

  test_object_v8_internal::CallWithCurrentWindowMethod(info);
}

void V8TestObject::CallWithCurrentWindowScriptWindowMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_callWithCurrentWindowScriptWindow");

  test_object_v8_internal::CallWithCurrentWindowScriptWindowMethod(info);
}

void V8TestObject::CallWithThisValueMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_callWithThisValue");

  test_object_v8_internal::CallWithThisValueMethod(info);
}

void V8TestObject::CheckSecurityForNodeVoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_checkSecurityForNodeVoidMethod");

  test_object_v8_internal::CheckSecurityForNodeVoidMethodMethod(info);
}

void V8TestObject::CustomVoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_customVoidMethod");

  V8TestObject::CustomVoidMethodMethodCustom(info);
}

void V8TestObject::CustomCallPrologueVoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_customCallPrologueVoidMethod");

  test_object_v8_internal::CustomCallPrologueVoidMethodMethod(info);
}

void V8TestObject::CustomCallEpilogueVoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_customCallEpilogueVoidMethod");

  test_object_v8_internal::CustomCallEpilogueVoidMethodMethod(info);
}

void V8TestObject::DeprecatedVoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_deprecatedVoidMethod");

  Deprecation::CountDeprecation(CurrentExecutionContext(info.GetIsolate()), WebFeature::kvoidMethod);
  test_object_v8_internal::DeprecatedVoidMethodMethod(info);
}

void V8TestObject::ImplementedAsVoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_implementedAsVoidMethod");

  test_object_v8_internal::ImplementedAsVoidMethodMethod(info);
}

void V8TestObject::MeasureAsVoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_measureAsVoidMethod");

  UseCounter::Count(CurrentExecutionContext(info.GetIsolate()), WebFeature::kTestFeature);
  test_object_v8_internal::MeasureAsVoidMethodMethod(info);
}

void V8TestObject::MeasureMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_measureMethod");

  UseCounter::Count(CurrentExecutionContext(info.GetIsolate()), WebFeature::kV8TestObject_MeasureMethod_Method);
  test_object_v8_internal::MeasureMethodMethod(info);
}

void V8TestObject::MeasureOverloadedMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_measureOverloadedMethod");

  test_object_v8_internal::MeasureOverloadedMethodMethod(info);
}

void V8TestObject::DeprecateAsOverloadedMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_DeprecateAsOverloadedMethod");

  test_object_v8_internal::DeprecateAsOverloadedMethodMethod(info);
}

void V8TestObject::DeprecateAsSameValueOverloadedMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_DeprecateAsSameValueOverloadedMethod");

  test_object_v8_internal::DeprecateAsSameValueOverloadedMethodMethod(info);
}

void V8TestObject::MeasureAsOverloadedMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_measureAsOverloadedMethod");

  test_object_v8_internal::MeasureAsOverloadedMethodMethod(info);
}

void V8TestObject::MeasureAsSameValueOverloadedMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_measureAsSameValueOverloadedMethod");

  test_object_v8_internal::MeasureAsSameValueOverloadedMethodMethod(info);
}

void V8TestObject::CeReactionsNotOverloadedMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_ceReactionsNotOverloadedMethod");

  test_object_v8_internal::CeReactionsNotOverloadedMethodMethod(info);
}

void V8TestObject::CeReactionsOverloadedMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_ceReactionsOverloadedMethod");

  test_object_v8_internal::CeReactionsOverloadedMethodMethod(info);
}

void V8TestObject::DeprecateAsMeasureAsSameValueOverloadedMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_deprecateAsMeasureAsSameValueOverloadedMethod");

  test_object_v8_internal::DeprecateAsMeasureAsSameValueOverloadedMethodMethod(info);
}

void V8TestObject::DeprecateAsSameValueMeasureAsOverloadedMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_deprecateAsSameValueMeasureAsOverloadedMethod");

  test_object_v8_internal::DeprecateAsSameValueMeasureAsOverloadedMethodMethod(info);
}

void V8TestObject::DeprecateAsSameValueMeasureAsSameValueOverloadedMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_deprecateAsSameValueMeasureAsSameValueOverloadedMethod");

  test_object_v8_internal::DeprecateAsSameValueMeasureAsSameValueOverloadedMethodMethod(info);
}

void V8TestObject::NotEnumerableVoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_notEnumerableVoidMethod");

  test_object_v8_internal::NotEnumerableVoidMethodMethod(info);
}

void V8TestObject::OriginTrialEnabledVoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_originTrialEnabledVoidMethod");

  test_object_v8_internal::OriginTrialEnabledVoidMethodMethod(info);
}

void V8TestObject::PerWorldBindingsOriginTrialEnabledVoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_perWorldBindingsOriginTrialEnabledVoidMethod");

  test_object_v8_internal::PerWorldBindingsOriginTrialEnabledVoidMethodMethod(info);
}

void V8TestObject::PerWorldBindingsOriginTrialEnabledVoidMethodMethodCallbackForMainWorld(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_perWorldBindingsOriginTrialEnabledVoidMethod");

  test_object_v8_internal::PerWorldBindingsOriginTrialEnabledVoidMethodMethodForMainWorld(info);
}

void V8TestObject::PerWorldBindingsVoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_perWorldBindingsVoidMethod");

  test_object_v8_internal::PerWorldBindingsVoidMethodMethod(info);
}

void V8TestObject::PerWorldBindingsVoidMethodMethodCallbackForMainWorld(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_perWorldBindingsVoidMethod");

  test_object_v8_internal::PerWorldBindingsVoidMethodMethodForMainWorld(info);
}

void V8TestObject::PerWorldBindingsVoidMethodTestInterfaceEmptyArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_perWorldBindingsVoidMethodTestInterfaceEmptyArg");

  test_object_v8_internal::PerWorldBindingsVoidMethodTestInterfaceEmptyArgMethod(info);
}

void V8TestObject::PerWorldBindingsVoidMethodTestInterfaceEmptyArgMethodCallbackForMainWorld(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_perWorldBindingsVoidMethodTestInterfaceEmptyArg");

  test_object_v8_internal::PerWorldBindingsVoidMethodTestInterfaceEmptyArgMethodForMainWorld(info);
}

void V8TestObject::ActivityLoggingForAllWorldsPerWorldBindingsVoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_activityLoggingForAllWorldsPerWorldBindingsVoidMethod");

  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);
  V8PerContextData* contextData = scriptState->PerContextData();
  if (contextData && contextData->ActivityLogger()) {
    contextData->ActivityLogger()->LogMethod("TestObject.activityLoggingForAllWorldsPerWorldBindingsVoidMethod", info);
  }
  test_object_v8_internal::ActivityLoggingForAllWorldsPerWorldBindingsVoidMethodMethod(info);
}

void V8TestObject::ActivityLoggingForAllWorldsPerWorldBindingsVoidMethodMethodCallbackForMainWorld(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_activityLoggingForAllWorldsPerWorldBindingsVoidMethod");

  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);
  V8PerContextData* contextData = scriptState->PerContextData();
  if (contextData && contextData->ActivityLogger()) {
    contextData->ActivityLogger()->LogMethod("TestObject.activityLoggingForAllWorldsPerWorldBindingsVoidMethod", info);
  }
  test_object_v8_internal::ActivityLoggingForAllWorldsPerWorldBindingsVoidMethodMethodForMainWorld(info);
}

void V8TestObject::ActivityLoggingForIsolatedWorldsPerWorldBindingsVoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_activityLoggingForIsolatedWorldsPerWorldBindingsVoidMethod");

  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);
  V8PerContextData* contextData = scriptState->PerContextData();
  if (contextData && contextData->ActivityLogger()) {
    contextData->ActivityLogger()->LogMethod("TestObject.activityLoggingForIsolatedWorldsPerWorldBindingsVoidMethod", info);
  }
  test_object_v8_internal::ActivityLoggingForIsolatedWorldsPerWorldBindingsVoidMethodMethod(info);
}

void V8TestObject::ActivityLoggingForIsolatedWorldsPerWorldBindingsVoidMethodMethodCallbackForMainWorld(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_activityLoggingForIsolatedWorldsPerWorldBindingsVoidMethod");

  test_object_v8_internal::ActivityLoggingForIsolatedWorldsPerWorldBindingsVoidMethodMethodForMainWorld(info);
}

void V8TestObject::RaisesExceptionVoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_raisesExceptionVoidMethod");

  test_object_v8_internal::RaisesExceptionVoidMethodMethod(info);
}

void V8TestObject::RaisesExceptionStringMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_raisesExceptionStringMethod");

  test_object_v8_internal::RaisesExceptionStringMethodMethod(info);
}

void V8TestObject::RaisesExceptionVoidMethodOptionalLongArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_raisesExceptionVoidMethodOptionalLongArg");

  test_object_v8_internal::RaisesExceptionVoidMethodOptionalLongArgMethod(info);
}

void V8TestObject::RaisesExceptionVoidMethodTestCallbackInterfaceArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_raisesExceptionVoidMethodTestCallbackInterfaceArg");

  test_object_v8_internal::RaisesExceptionVoidMethodTestCallbackInterfaceArgMethod(info);
}

void V8TestObject::RaisesExceptionVoidMethodOptionalTestCallbackInterfaceArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_raisesExceptionVoidMethodOptionalTestCallbackInterfaceArg");

  test_object_v8_internal::RaisesExceptionVoidMethodOptionalTestCallbackInterfaceArgMethod(info);
}

void V8TestObject::RaisesExceptionTestInterfaceEmptyVoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_raisesExceptionTestInterfaceEmptyVoidMethod");

  test_object_v8_internal::RaisesExceptionTestInterfaceEmptyVoidMethodMethod(info);
}

void V8TestObject::RaisesExceptionXPathNSResolverVoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_raisesExceptionXPathNSResolverVoidMethod");

  test_object_v8_internal::RaisesExceptionXPathNSResolverVoidMethodMethod(info);
}

void V8TestObject::CallWithExecutionContextRaisesExceptionVoidMethodLongArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_callWithExecutionContextRaisesExceptionVoidMethodLongArg");

  test_object_v8_internal::CallWithExecutionContextRaisesExceptionVoidMethodLongArgMethod(info);
}

void V8TestObject::RuntimeEnabledVoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_runtimeEnabledVoidMethod");

  test_object_v8_internal::RuntimeEnabledVoidMethodMethod(info);
}

void V8TestObject::PerWorldBindingsRuntimeEnabledVoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_perWorldBindingsRuntimeEnabledVoidMethod");

  test_object_v8_internal::PerWorldBindingsRuntimeEnabledVoidMethodMethod(info);
}

void V8TestObject::PerWorldBindingsRuntimeEnabledVoidMethodMethodCallbackForMainWorld(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_perWorldBindingsRuntimeEnabledVoidMethod");

  test_object_v8_internal::PerWorldBindingsRuntimeEnabledVoidMethodMethodForMainWorld(info);
}

void V8TestObject::RuntimeEnabledOverloadedVoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_runtimeEnabledOverloadedVoidMethod");

  test_object_v8_internal::RuntimeEnabledOverloadedVoidMethodMethod(info);
}

void V8TestObject::PartiallyRuntimeEnabledOverloadedVoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_partiallyRuntimeEnabledOverloadedVoidMethod");

  test_object_v8_internal::PartiallyRuntimeEnabledOverloadedVoidMethodMethod(info);
}

void V8TestObject::LegacyInterfaceTypeCheckingVoidMethodTestInterfaceEmptyArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_legacyInterfaceTypeCheckingVoidMethodTestInterfaceEmptyArg");

  test_object_v8_internal::LegacyInterfaceTypeCheckingVoidMethodTestInterfaceEmptyArgMethod(info);
}

void V8TestObject::LegacyInterfaceTypeCheckingVoidMethodTestInterfaceEmptyVariadicArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_legacyInterfaceTypeCheckingVoidMethodTestInterfaceEmptyVariadicArg");

  test_object_v8_internal::LegacyInterfaceTypeCheckingVoidMethodTestInterfaceEmptyVariadicArgMethod(info);
}

void V8TestObject::UseToImpl4ArgumentsCheckingIfPossibleWithOptionalArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_useToImpl4ArgumentsCheckingIfPossibleWithOptionalArg");

  test_object_v8_internal::UseToImpl4ArgumentsCheckingIfPossibleWithOptionalArgMethod(info);
}

void V8TestObject::UseToImpl4ArgumentsCheckingIfPossibleWithNullableArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_useToImpl4ArgumentsCheckingIfPossibleWithNullableArg");

  test_object_v8_internal::UseToImpl4ArgumentsCheckingIfPossibleWithNullableArgMethod(info);
}

void V8TestObject::UseToImpl4ArgumentsCheckingIfPossibleWithUndefinedArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_useToImpl4ArgumentsCheckingIfPossibleWithUndefinedArg");

  test_object_v8_internal::UseToImpl4ArgumentsCheckingIfPossibleWithUndefinedArgMethod(info);
}

void V8TestObject::UnforgeableVoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_unforgeableVoidMethod");

  test_object_v8_internal::UnforgeableVoidMethodMethod(info);
}

void V8TestObject::NewObjectTestInterfaceMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_newObjectTestInterfaceMethod");

  test_object_v8_internal::NewObjectTestInterfaceMethodMethod(info);
}

void V8TestObject::NewObjectTestInterfacePromiseMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_newObjectTestInterfacePromiseMethod");

  test_object_v8_internal::NewObjectTestInterfacePromiseMethodMethod(info);
}

void V8TestObject::RuntimeCallStatsCounterMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE(info.GetIsolate(), RuntimeCallStats::CounterId::kRuntimeCallStatsCounterMethod);
  test_object_v8_internal::RuntimeCallStatsCounterMethodMethod(info);
}

void V8TestObject::ClearMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_clear");

  test_object_v8_internal::ClearMethod(info);
}

void V8TestObject::KeysMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_keys");

  test_object_v8_internal::KeysMethod(info);
}

void V8TestObject::ValuesMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_values");

  test_object_v8_internal::ValuesMethod(info);
}

void V8TestObject::ForEachMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_forEach");

  test_object_v8_internal::ForEachMethod(info);
}

void V8TestObject::HasMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_has");

  test_object_v8_internal::HasMethod(info);
}

void V8TestObject::GetMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_get");

  test_object_v8_internal::GetMethod(info);
}

void V8TestObject::DeleteMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_delete");

  test_object_v8_internal::DeleteMethod(info);
}

void V8TestObject::SetMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_set");

  test_object_v8_internal::SetMethod(info);
}

void V8TestObject::ToJSONMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_toJSON");

  test_object_v8_internal::ToJSONMethod(info);
}

void V8TestObject::ToStringMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_toString");

  test_object_v8_internal::ToStringMethod(info);
}

void V8TestObject::IteratorMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_iterator");

  test_object_v8_internal::IteratorMethod(info);
}

void V8TestObject::NamedPropertyGetterCallback(v8::Local<v8::Name> name, const v8::PropertyCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_NamedPropertyGetter");

  if (!name->IsString())
    return;
  const AtomicString& propertyName = ToCoreAtomicString(name.As<v8::String>());

  test_object_v8_internal::NamedPropertyGetter(propertyName, info);
}

void V8TestObject::NamedPropertySetterCallback(v8::Local<v8::Name> name, v8::Local<v8::Value> v8Value, const v8::PropertyCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_NamedPropertySetter");

  if (!name->IsString())
    return;
  const AtomicString& propertyName = ToCoreAtomicString(name.As<v8::String>());

  test_object_v8_internal::NamedPropertySetter(propertyName, v8Value, info);
}

void V8TestObject::NamedPropertyDeleterCallback(v8::Local<v8::Name> name, const v8::PropertyCallbackInfo<v8::Boolean>& info) {
  if (!name->IsString())
    return;
  const AtomicString& propertyName = ToCoreAtomicString(name.As<v8::String>());

  test_object_v8_internal::NamedPropertyDeleter(propertyName, info);
}

void V8TestObject::NamedPropertyQueryCallback(v8::Local<v8::Name> name, const v8::PropertyCallbackInfo<v8::Integer>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_NamedPropertyQuery");

  if (!name->IsString())
    return;
  const AtomicString& propertyName = ToCoreAtomicString(name.As<v8::String>());

  test_object_v8_internal::NamedPropertyQuery(propertyName, info);
}

void V8TestObject::NamedPropertyEnumeratorCallback(const v8::PropertyCallbackInfo<v8::Array>& info) {
  test_object_v8_internal::NamedPropertyEnumerator(info);
}

void V8TestObject::IndexedPropertyGetterCallback(uint32_t index, const v8::PropertyCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestObject_IndexedPropertyGetter");

  test_object_v8_internal::IndexedPropertyGetter(index, info);
}

void V8TestObject::IndexedPropertyDescriptorCallback(uint32_t index, const v8::PropertyCallbackInfo<v8::Value>& info) {
  test_object_v8_internal::IndexedPropertyDescriptor(index, info);
}

void V8TestObject::IndexedPropertySetterCallback(uint32_t index, v8::Local<v8::Value> v8Value, const v8::PropertyCallbackInfo<v8::Value>& info) {
  test_object_v8_internal::IndexedPropertySetter(index, v8Value, info);
}

void V8TestObject::IndexedPropertyDeleterCallback(uint32_t index, const v8::PropertyCallbackInfo<v8::Boolean>& info) {
  test_object_v8_internal::IndexedPropertyDeleter(index, info);
}

void V8TestObject::IndexedPropertyDefinerCallback(
    uint32_t index,
    const v8::PropertyDescriptor& desc,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  // https://heycam.github.io/webidl/#legacy-platform-object-defineownproperty
  // 3.9.3. [[DefineOwnProperty]]
  // step 1.1. If the result of calling IsDataDescriptor(Desc) is false, then
  //   return false.
  if (desc.has_get() || desc.has_set()) {
    V8SetReturnValue(info, v8::Null(info.GetIsolate()));
    if (info.ShouldThrowOnError()) {
      ExceptionState exceptionState(info.GetIsolate(),
                                    ExceptionState::kIndexedSetterContext,
                                    "TestObject");
      exceptionState.ThrowTypeError("Accessor properties are not allowed.");
    }
    return;
  }

  // Return nothing and fall back to indexedPropertySetterCallback.
}

// Suppress warning: global constructors, because AttributeConfiguration is trivial
// and does not depend on another global objects.
#if defined(COMPONENT_BUILD) && defined(WIN32) && defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#endif
static const V8DOMConfiguration::AttributeConfiguration V8TestObjectAttributes[] = {
    { "testInterfaceEmptyConstructorAttribute", V8TestObject::TestInterfaceEmptyConstructorAttributeConstructorGetterCallback, nullptr, static_cast<v8::PropertyAttribute>(v8::DontEnum), V8DOMConfiguration::kOnInstance, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kReplaceWithDataProperty, V8DOMConfiguration::kAllWorlds },
    { "testInterfaceEmptyConstructorAttribute", V8TestObject::TestInterfaceEmptyConstructorAttributeConstructorGetterCallback, nullptr, static_cast<v8::PropertyAttribute>(v8::DontEnum), V8DOMConfiguration::kOnInstance, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "measureAsFeatureNameTestInterfaceEmptyConstructorAttribute", V8TestObject::MeasureAsFeatureNameTestInterfaceEmptyConstructorAttributeConstructorGetterCallback, nullptr, static_cast<v8::PropertyAttribute>(v8::DontEnum), V8DOMConfiguration::kOnInstance, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
};
#if defined(COMPONENT_BUILD) && defined(WIN32) && defined(__clang__)
#pragma clang diagnostic pop
#endif

static const V8DOMConfiguration::AccessorConfiguration V8TestObjectAccessors[] = {
    { "stringifierAttribute", V8TestObject::StringifierAttributeAttributeGetterCallback, V8TestObject::StringifierAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "readonlyStringAttribute", V8TestObject::ReadonlyStringAttributeAttributeGetterCallback, nullptr, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::ReadOnly), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "readonlyTestInterfaceEmptyAttribute", V8TestObject::ReadonlyTestInterfaceEmptyAttributeAttributeGetterCallback, nullptr, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::ReadOnly), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "readonlyLongAttribute", V8TestObject::ReadonlyLongAttributeAttributeGetterCallback, nullptr, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::ReadOnly), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "dateAttribute", V8TestObject::DateAttributeAttributeGetterCallback, V8TestObject::DateAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "stringAttribute", V8TestObject::StringAttributeAttributeGetterCallback, V8TestObject::StringAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "byteStringAttribute", V8TestObject::ByteStringAttributeAttributeGetterCallback, V8TestObject::ByteStringAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "usvStringAttribute", V8TestObject::UsvStringAttributeAttributeGetterCallback, V8TestObject::UsvStringAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "domTimeStampAttribute", V8TestObject::DOMTimeStampAttributeAttributeGetterCallback, V8TestObject::DOMTimeStampAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "booleanAttribute", V8TestObject::BooleanAttributeAttributeGetterCallback, V8TestObject::BooleanAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "byteAttribute", V8TestObject::ByteAttributeAttributeGetterCallback, V8TestObject::ByteAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "doubleAttribute", V8TestObject::DoubleAttributeAttributeGetterCallback, V8TestObject::DoubleAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "floatAttribute", V8TestObject::FloatAttributeAttributeGetterCallback, V8TestObject::FloatAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "longAttribute", V8TestObject::LongAttributeAttributeGetterCallback, V8TestObject::LongAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "longLongAttribute", V8TestObject::LongLongAttributeAttributeGetterCallback, V8TestObject::LongLongAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "octetAttribute", V8TestObject::OctetAttributeAttributeGetterCallback, V8TestObject::OctetAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "shortAttribute", V8TestObject::ShortAttributeAttributeGetterCallback, V8TestObject::ShortAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "unrestrictedDoubleAttribute", V8TestObject::UnrestrictedDoubleAttributeAttributeGetterCallback, V8TestObject::UnrestrictedDoubleAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "unrestrictedFloatAttribute", V8TestObject::UnrestrictedFloatAttributeAttributeGetterCallback, V8TestObject::UnrestrictedFloatAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "unsignedLongAttribute", V8TestObject::UnsignedLongAttributeAttributeGetterCallback, V8TestObject::UnsignedLongAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "unsignedLongLongAttribute", V8TestObject::UnsignedLongLongAttributeAttributeGetterCallback, V8TestObject::UnsignedLongLongAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "unsignedShortAttribute", V8TestObject::UnsignedShortAttributeAttributeGetterCallback, V8TestObject::UnsignedShortAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "testInterfaceEmptyAttribute", V8TestObject::TestInterfaceEmptyAttributeAttributeGetterCallback, V8TestObject::TestInterfaceEmptyAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "testObjectAttribute", V8TestObject::TestObjectAttributeAttributeGetterCallback, V8TestObject::TestObjectAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "cssAttribute", V8TestObject::CSSAttributeAttributeGetterCallback, V8TestObject::CSSAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "imeAttribute", V8TestObject::ImeAttributeAttributeGetterCallback, V8TestObject::ImeAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "svgAttribute", V8TestObject::SVGAttributeAttributeGetterCallback, V8TestObject::SVGAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "xmlAttribute", V8TestObject::XmlAttributeAttributeGetterCallback, V8TestObject::XmlAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "nodeFilterAttribute", V8TestObject::NodeFilterAttributeAttributeGetterCallback, nullptr, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::ReadOnly), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "serializedScriptValueAttribute", V8TestObject::SerializedScriptValueAttributeAttributeGetterCallback, V8TestObject::SerializedScriptValueAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "anyAttribute", V8TestObject::AnyAttributeAttributeGetterCallback, V8TestObject::AnyAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "promiseAttribute", V8TestObject::PromiseAttributeAttributeGetterCallback, V8TestObject::PromiseAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kDoNotCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "windowAttribute", V8TestObject::WindowAttributeAttributeGetterCallback, V8TestObject::WindowAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "documentAttribute", V8TestObject::DocumentAttributeAttributeGetterCallback, V8TestObject::DocumentAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "documentFragmentAttribute", V8TestObject::DocumentFragmentAttributeAttributeGetterCallback, V8TestObject::DocumentFragmentAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "documentTypeAttribute", V8TestObject::DocumentTypeAttributeAttributeGetterCallback, V8TestObject::DocumentTypeAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "elementAttribute", V8TestObject::ElementAttributeAttributeGetterCallback, V8TestObject::ElementAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "nodeAttribute", V8TestObject::NodeAttributeAttributeGetterCallback, V8TestObject::NodeAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "shadowRootAttribute", V8TestObject::ShadowRootAttributeAttributeGetterCallback, V8TestObject::ShadowRootAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "arrayBufferAttribute", V8TestObject::ArrayBufferAttributeAttributeGetterCallback, V8TestObject::ArrayBufferAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "float32ArrayAttribute", V8TestObject::Float32ArrayAttributeAttributeGetterCallback, V8TestObject::Float32ArrayAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "uint8ArrayAttribute", V8TestObject::Uint8ArrayAttributeAttributeGetterCallback, V8TestObject::Uint8ArrayAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "self", V8TestObject::SelfAttributeGetterCallback, nullptr, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::ReadOnly), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "readonlyEventTargetAttribute", V8TestObject::ReadonlyEventTargetAttributeAttributeGetterCallback, nullptr, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::ReadOnly), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "readonlyEventTargetOrNullAttribute", V8TestObject::ReadonlyEventTargetOrNullAttributeAttributeGetterCallback, nullptr, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::ReadOnly), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "readonlyWindowAttribute", V8TestObject::ReadonlyWindowAttributeAttributeGetterCallback, nullptr, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::ReadOnly), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "htmlCollectionAttribute", V8TestObject::HTMLCollectionAttributeAttributeGetterCallback, nullptr, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::ReadOnly), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "htmlElementAttribute", V8TestObject::HTMLElementAttributeAttributeGetterCallback, nullptr, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::ReadOnly), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "stringFrozenArrayAttribute", V8TestObject::StringFrozenArrayAttributeAttributeGetterCallback, V8TestObject::StringFrozenArrayAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "testInterfaceEmptyFrozenArrayAttribute", V8TestObject::TestInterfaceEmptyFrozenArrayAttributeAttributeGetterCallback, V8TestObject::TestInterfaceEmptyFrozenArrayAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "booleanOrNullAttribute", V8TestObject::BooleanOrNullAttributeAttributeGetterCallback, V8TestObject::BooleanOrNullAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "stringOrNullAttribute", V8TestObject::StringOrNullAttributeAttributeGetterCallback, V8TestObject::StringOrNullAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "longOrNullAttribute", V8TestObject::LongOrNullAttributeAttributeGetterCallback, V8TestObject::LongOrNullAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "testInterfaceOrNullAttribute", V8TestObject::TestInterfaceOrNullAttributeAttributeGetterCallback, V8TestObject::TestInterfaceOrNullAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "testEnumAttribute", V8TestObject::TestEnumAttributeAttributeGetterCallback, V8TestObject::TestEnumAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "testEnumOrNullAttribute", V8TestObject::TestEnumOrNullAttributeAttributeGetterCallback, V8TestObject::TestEnumOrNullAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "staticStringAttribute", V8TestObject::StaticStringAttributeAttributeGetterCallback, V8TestObject::StaticStringAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnInterface, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "staticLongAttribute", V8TestObject::StaticLongAttributeAttributeGetterCallback, V8TestObject::StaticLongAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnInterface, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "eventHandlerAttribute", V8TestObject::EventHandlerAttributeAttributeGetterCallback, V8TestObject::EventHandlerAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "doubleOrStringAttribute", V8TestObject::DoubleOrStringAttributeAttributeGetterCallback, V8TestObject::DoubleOrStringAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "doubleOrStringOrNullAttribute", V8TestObject::DoubleOrStringOrNullAttributeAttributeGetterCallback, V8TestObject::DoubleOrStringOrNullAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "doubleOrNullStringAttribute", V8TestObject::DoubleOrNullStringAttributeAttributeGetterCallback, V8TestObject::DoubleOrNullStringAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "stringOrStringSequenceAttribute", V8TestObject::StringOrStringSequenceAttributeAttributeGetterCallback, V8TestObject::StringOrStringSequenceAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "testEnumOrDoubleAttribute", V8TestObject::TestEnumOrDoubleAttributeAttributeGetterCallback, V8TestObject::TestEnumOrDoubleAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "unrestrictedDoubleOrStringAttribute", V8TestObject::UnrestrictedDoubleOrStringAttributeAttributeGetterCallback, V8TestObject::UnrestrictedDoubleOrStringAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "nestedUnionAtribute", V8TestObject::NestedUnionAtributeAttributeGetterCallback, V8TestObject::NestedUnionAtributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "activityLoggingAccessForAllWorldsLongAttribute", V8TestObject::ActivityLoggingAccessForAllWorldsLongAttributeAttributeGetterCallback, V8TestObject::ActivityLoggingAccessForAllWorldsLongAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "activityLoggingGetterForAllWorldsLongAttribute", V8TestObject::ActivityLoggingGetterForAllWorldsLongAttributeAttributeGetterCallback, V8TestObject::ActivityLoggingGetterForAllWorldsLongAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "activityLoggingSetterForAllWorldsLongAttribute", V8TestObject::ActivityLoggingSetterForAllWorldsLongAttributeAttributeGetterCallback, V8TestObject::ActivityLoggingSetterForAllWorldsLongAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "cachedAttributeAnyAttribute", V8TestObject::CachedAttributeAnyAttributeAttributeGetterCallback, V8TestObject::CachedAttributeAnyAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "cachedArrayAttribute", V8TestObject::CachedArrayAttributeAttributeGetterCallback, V8TestObject::CachedArrayAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "cachedStringOrNoneAttribute", V8TestObject::CachedStringOrNoneAttributeAttributeGetterCallback, V8TestObject::CachedStringOrNoneAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "callWithExecutionContextAnyAttribute", V8TestObject::CallWithExecutionContextAnyAttributeAttributeGetterCallback, V8TestObject::CallWithExecutionContextAnyAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "callWithScriptStateAnyAttribute", V8TestObject::CallWithScriptStateAnyAttributeAttributeGetterCallback, V8TestObject::CallWithScriptStateAnyAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "callWithExecutionContextAndScriptStateAnyAttribute", V8TestObject::CallWithExecutionContextAndScriptStateAnyAttributeAttributeGetterCallback, V8TestObject::CallWithExecutionContextAndScriptStateAnyAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "checkSecurityForNodeReadonlyDocumentAttribute", V8TestObject::CheckSecurityForNodeReadonlyDocumentAttributeAttributeGetterCallback, nullptr, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::ReadOnly), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "customObjectAttribute", V8TestObject::CustomObjectAttributeAttributeGetterCallback, V8TestObject::CustomObjectAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "customGetterLongAttribute", V8TestObject::CustomGetterLongAttributeAttributeGetterCallback, V8TestObject::CustomGetterLongAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "customGetterReadonlyObjectAttribute", V8TestObject::CustomGetterReadonlyObjectAttributeAttributeGetterCallback, nullptr, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::ReadOnly), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "customSetterLongAttribute", V8TestObject::CustomSetterLongAttributeAttributeGetterCallback, V8TestObject::CustomSetterLongAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "deprecatedLongAttribute", V8TestObject::DeprecatedLongAttributeAttributeGetterCallback, V8TestObject::DeprecatedLongAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "enforceRangeLongAttribute", V8TestObject::EnforceRangeLongAttributeAttributeGetterCallback, V8TestObject::EnforceRangeLongAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "implementedAsLongAttribute", V8TestObject::ImplementedAsLongAttributeAttributeGetterCallback, V8TestObject::ImplementedAsLongAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "customImplementedAsLongAttribute", V8TestObject::CustomImplementedAsLongAttributeAttributeGetterCallback, V8TestObject::CustomImplementedAsLongAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "customGetterImplementedAsLongAttribute", V8TestObject::CustomGetterImplementedAsLongAttributeAttributeGetterCallback, V8TestObject::CustomGetterImplementedAsLongAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "customSetterImplementedAsLongAttribute", V8TestObject::CustomSetterImplementedAsLongAttributeAttributeGetterCallback, V8TestObject::CustomSetterImplementedAsLongAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "measureAsLongAttribute", V8TestObject::MeasureAsLongAttributeAttributeGetterCallback, V8TestObject::MeasureAsLongAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "notEnumerableLongAttribute", V8TestObject::NotEnumerableLongAttributeAttributeGetterCallback, V8TestObject::NotEnumerableLongAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::DontEnum), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "perWorldBindingsReadonlyTestInterfaceEmptyAttribute", V8TestObject::PerWorldBindingsReadonlyTestInterfaceEmptyAttributeAttributeGetterCallbackForMainWorld, nullptr, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::ReadOnly), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kMainWorld },
    { "perWorldBindingsReadonlyTestInterfaceEmptyAttribute", V8TestObject::PerWorldBindingsReadonlyTestInterfaceEmptyAttributeAttributeGetterCallback, nullptr, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::ReadOnly), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kNonMainWorlds },
    { "activityLoggingAccessPerWorldBindingsLongAttribute", V8TestObject::ActivityLoggingAccessPerWorldBindingsLongAttributeAttributeGetterCallbackForMainWorld, V8TestObject::ActivityLoggingAccessPerWorldBindingsLongAttributeAttributeSetterCallbackForMainWorld, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kMainWorld },
    { "activityLoggingAccessPerWorldBindingsLongAttribute", V8TestObject::ActivityLoggingAccessPerWorldBindingsLongAttributeAttributeGetterCallback, V8TestObject::ActivityLoggingAccessPerWorldBindingsLongAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kNonMainWorlds },
    { "activityLoggingAccessForIsolatedWorldsPerWorldBindingsLongAttribute", V8TestObject::ActivityLoggingAccessForIsolatedWorldsPerWorldBindingsLongAttributeAttributeGetterCallbackForMainWorld, V8TestObject::ActivityLoggingAccessForIsolatedWorldsPerWorldBindingsLongAttributeAttributeSetterCallbackForMainWorld, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kMainWorld },
    { "activityLoggingAccessForIsolatedWorldsPerWorldBindingsLongAttribute", V8TestObject::ActivityLoggingAccessForIsolatedWorldsPerWorldBindingsLongAttributeAttributeGetterCallback, V8TestObject::ActivityLoggingAccessForIsolatedWorldsPerWorldBindingsLongAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kNonMainWorlds },
    { "activityLoggingGetterPerWorldBindingsLongAttribute", V8TestObject::ActivityLoggingGetterPerWorldBindingsLongAttributeAttributeGetterCallbackForMainWorld, V8TestObject::ActivityLoggingGetterPerWorldBindingsLongAttributeAttributeSetterCallbackForMainWorld, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kMainWorld },
    { "activityLoggingGetterPerWorldBindingsLongAttribute", V8TestObject::ActivityLoggingGetterPerWorldBindingsLongAttributeAttributeGetterCallback, V8TestObject::ActivityLoggingGetterPerWorldBindingsLongAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kNonMainWorlds },
    { "activityLoggingGetterForIsolatedWorldsPerWorldBindingsLongAttribute", V8TestObject::ActivityLoggingGetterForIsolatedWorldsPerWorldBindingsLongAttributeAttributeGetterCallbackForMainWorld, V8TestObject::ActivityLoggingGetterForIsolatedWorldsPerWorldBindingsLongAttributeAttributeSetterCallbackForMainWorld, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kMainWorld },
    { "activityLoggingGetterForIsolatedWorldsPerWorldBindingsLongAttribute", V8TestObject::ActivityLoggingGetterForIsolatedWorldsPerWorldBindingsLongAttributeAttributeGetterCallback, V8TestObject::ActivityLoggingGetterForIsolatedWorldsPerWorldBindingsLongAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kNonMainWorlds },
    { "location", V8TestObject::LocationAttributeGetterCallback, V8TestObject::LocationAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "locationWithException", V8TestObject::LocationWithExceptionAttributeGetterCallback, V8TestObject::LocationWithExceptionAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "locationWithCallWith", V8TestObject::LocationWithCallWithAttributeGetterCallback, V8TestObject::LocationWithCallWithAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "locationByteString", V8TestObject::LocationByteStringAttributeGetterCallback, V8TestObject::LocationByteStringAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "locationWithPerWorldBindings", V8TestObject::LocationWithPerWorldBindingsAttributeGetterCallbackForMainWorld, V8TestObject::LocationWithPerWorldBindingsAttributeSetterCallbackForMainWorld, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kMainWorld },
    { "locationWithPerWorldBindings", V8TestObject::LocationWithPerWorldBindingsAttributeGetterCallback, V8TestObject::LocationWithPerWorldBindingsAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kNonMainWorlds },
    { "locationLegacyInterfaceTypeChecking", V8TestObject::LocationLegacyInterfaceTypeCheckingAttributeGetterCallback, V8TestObject::LocationLegacyInterfaceTypeCheckingAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "raisesExceptionLongAttribute", V8TestObject::RaisesExceptionLongAttributeAttributeGetterCallback, V8TestObject::RaisesExceptionLongAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "raisesExceptionGetterLongAttribute", V8TestObject::RaisesExceptionGetterLongAttributeAttributeGetterCallback, V8TestObject::RaisesExceptionGetterLongAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "setterRaisesExceptionLongAttribute", V8TestObject::SetterRaisesExceptionLongAttributeAttributeGetterCallback, V8TestObject::SetterRaisesExceptionLongAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "raisesExceptionTestInterfaceEmptyAttribute", V8TestObject::RaisesExceptionTestInterfaceEmptyAttributeAttributeGetterCallback, V8TestObject::RaisesExceptionTestInterfaceEmptyAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "cachedAttributeRaisesExceptionGetterAnyAttribute", V8TestObject::CachedAttributeRaisesExceptionGetterAnyAttributeAttributeGetterCallback, V8TestObject::CachedAttributeRaisesExceptionGetterAnyAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "reflectTestInterfaceAttribute", V8TestObject::ReflectTestInterfaceAttributeAttributeGetterCallback, V8TestObject::ReflectTestInterfaceAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "reflectReflectedNameAttributeTestAttribute", V8TestObject::ReflectReflectedNameAttributeTestAttributeAttributeGetterCallback, V8TestObject::ReflectReflectedNameAttributeTestAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "reflectBooleanAttribute", V8TestObject::ReflectBooleanAttributeAttributeGetterCallback, V8TestObject::ReflectBooleanAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "reflectLongAttribute", V8TestObject::ReflectLongAttributeAttributeGetterCallback, V8TestObject::ReflectLongAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "reflectUnsignedShortAttribute", V8TestObject::ReflectUnsignedShortAttributeAttributeGetterCallback, V8TestObject::ReflectUnsignedShortAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "reflectUnsignedLongAttribute", V8TestObject::ReflectUnsignedLongAttributeAttributeGetterCallback, V8TestObject::ReflectUnsignedLongAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "id", V8TestObject::IdAttributeGetterCallback, V8TestObject::IdAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "name", V8TestObject::NameAttributeGetterCallback, V8TestObject::NameAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "class", V8TestObject::ClassAttributeGetterCallback, V8TestObject::ClassAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "reflectedId", V8TestObject::ReflectedIdAttributeGetterCallback, V8TestObject::ReflectedIdAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "reflectedName", V8TestObject::ReflectedNameAttributeGetterCallback, V8TestObject::ReflectedNameAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "reflectedClass", V8TestObject::ReflectedClassAttributeGetterCallback, V8TestObject::ReflectedClassAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "limitedToOnlyOneAttribute", V8TestObject::LimitedToOnlyOneAttributeAttributeGetterCallback, V8TestObject::LimitedToOnlyOneAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "limitedToOnlyAttribute", V8TestObject::LimitedToOnlyAttributeAttributeGetterCallback, V8TestObject::LimitedToOnlyAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "limitedToOnlyOtherAttribute", V8TestObject::LimitedToOnlyOtherAttributeAttributeGetterCallback, V8TestObject::LimitedToOnlyOtherAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "limitedWithMissingDefaultAttribute", V8TestObject::LimitedWithMissingDefaultAttributeAttributeGetterCallback, V8TestObject::LimitedWithMissingDefaultAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "limitedWithInvalidMissingDefaultAttribute", V8TestObject::LimitedWithInvalidMissingDefaultAttributeAttributeGetterCallback, V8TestObject::LimitedWithInvalidMissingDefaultAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "corsSettingAttribute", V8TestObject::CorsSettingAttributeAttributeGetterCallback, nullptr, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::ReadOnly), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "limitedWithEmptyMissingInvalidAttribute", V8TestObject::LimitedWithEmptyMissingInvalidAttributeAttributeGetterCallback, nullptr, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::ReadOnly), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "RuntimeCallStatsCounterAttribute", V8TestObject::RuntimeCallStatsCounterAttributeAttributeGetterCallback, V8TestObject::RuntimeCallStatsCounterAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "RuntimeCallStatsCounterReadOnlyAttribute", V8TestObject::RuntimeCallStatsCounterReadOnlyAttributeAttributeGetterCallback, nullptr, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::ReadOnly), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "replaceableReadonlyLongAttribute", V8TestObject::ReplaceableReadonlyLongAttributeAttributeGetterCallback, V8TestObject::ReplaceableReadonlyLongAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "locationPutForwards", V8TestObject::LocationPutForwardsAttributeGetterCallback, V8TestObject::LocationPutForwardsAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "setterCallWithCurrentWindowAndEnteredWindowStringAttribute", V8TestObject::SetterCallWithCurrentWindowAndEnteredWindowStringAttributeAttributeGetterCallback, V8TestObject::SetterCallWithCurrentWindowAndEnteredWindowStringAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "setterCallWithExecutionContextStringAttribute", V8TestObject::SetterCallWithExecutionContextStringAttributeAttributeGetterCallback, V8TestObject::SetterCallWithExecutionContextStringAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "treatNullAsEmptyStringStringAttribute", V8TestObject::TreatNullAsEmptyStringStringAttributeAttributeGetterCallback, V8TestObject::TreatNullAsEmptyStringStringAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "legacyInterfaceTypeCheckingFloatAttribute", V8TestObject::LegacyInterfaceTypeCheckingFloatAttributeAttributeGetterCallback, V8TestObject::LegacyInterfaceTypeCheckingFloatAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "legacyInterfaceTypeCheckingTestInterfaceAttribute", V8TestObject::LegacyInterfaceTypeCheckingTestInterfaceAttributeAttributeGetterCallback, V8TestObject::LegacyInterfaceTypeCheckingTestInterfaceAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "legacyInterfaceTypeCheckingTestInterfaceOrNullAttribute", V8TestObject::LegacyInterfaceTypeCheckingTestInterfaceOrNullAttributeAttributeGetterCallback, V8TestObject::LegacyInterfaceTypeCheckingTestInterfaceOrNullAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "urlStringAttribute", V8TestObject::UrlStringAttributeAttributeGetterCallback, V8TestObject::UrlStringAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "urlStringAttribute", V8TestObject::UrlStringAttributeAttributeGetterCallback, V8TestObject::UrlStringAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "unforgeableLongAttribute", V8TestObject::UnforgeableLongAttributeAttributeGetterCallback, V8TestObject::UnforgeableLongAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::DontDelete), V8DOMConfiguration::kOnInstance, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "measuredLongAttribute", V8TestObject::MeasuredLongAttributeAttributeGetterCallback, V8TestObject::MeasuredLongAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "sameObjectAttribute", V8TestObject::SameObjectAttributeAttributeGetterCallback, nullptr, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::ReadOnly), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "saveSameObjectAttribute", V8TestObject::SaveSameObjectAttributeAttributeGetterCallback, nullptr, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::ReadOnly), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "staticSaveSameObjectAttribute", V8TestObject::StaticSaveSameObjectAttributeAttributeGetterCallback, nullptr, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::ReadOnly), V8DOMConfiguration::kOnInterface, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "unscopableLongAttribute", V8TestObject::UnscopableLongAttributeAttributeGetterCallback, V8TestObject::UnscopableLongAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "testInterfaceAttribute", V8TestObject::TestInterfaceAttributeAttributeGetterCallback, V8TestObject::TestInterfaceAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "size", V8TestObject::SizeAttributeGetterCallback, nullptr, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::DontEnum | v8::ReadOnly), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
};

static const V8DOMConfiguration::MethodConfiguration V8TestObjectMethods[] = {
    {"unscopableVoidMethod", V8TestObject::UnscopableVoidMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethod", V8TestObject::VoidMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"staticVoidMethod", V8TestObject::StaticVoidMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnInterface, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"dateMethod", V8TestObject::DateMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"stringMethod", V8TestObject::StringMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"byteStringMethod", V8TestObject::ByteStringMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"usvStringMethod", V8TestObject::UsvStringMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"readonlyDOMTimeStampMethod", V8TestObject::ReadonlyDOMTimeStampMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"booleanMethod", V8TestObject::BooleanMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"byteMethod", V8TestObject::ByteMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"doubleMethod", V8TestObject::DoubleMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"floatMethod", V8TestObject::FloatMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"longMethod", V8TestObject::LongMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"longLongMethod", V8TestObject::LongLongMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"octetMethod", V8TestObject::OctetMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"shortMethod", V8TestObject::ShortMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"unsignedLongMethod", V8TestObject::UnsignedLongMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"unsignedLongLongMethod", V8TestObject::UnsignedLongLongMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"unsignedShortMethod", V8TestObject::UnsignedShortMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodDateArg", V8TestObject::VoidMethodDateArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodStringArg", V8TestObject::VoidMethodStringArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodByteStringArg", V8TestObject::VoidMethodByteStringArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodUSVStringArg", V8TestObject::VoidMethodUSVStringArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodDOMTimeStampArg", V8TestObject::VoidMethodDOMTimeStampArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodBooleanArg", V8TestObject::VoidMethodBooleanArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodByteArg", V8TestObject::VoidMethodByteArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodDoubleArg", V8TestObject::VoidMethodDoubleArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodFloatArg", V8TestObject::VoidMethodFloatArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodLongArg", V8TestObject::VoidMethodLongArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodLongLongArg", V8TestObject::VoidMethodLongLongArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodOctetArg", V8TestObject::VoidMethodOctetArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodShortArg", V8TestObject::VoidMethodShortArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodUnsignedLongArg", V8TestObject::VoidMethodUnsignedLongArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodUnsignedLongLongArg", V8TestObject::VoidMethodUnsignedLongLongArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodUnsignedShortArg", V8TestObject::VoidMethodUnsignedShortArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"testInterfaceEmptyMethod", V8TestObject::TestInterfaceEmptyMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodTestInterfaceEmptyArg", V8TestObject::VoidMethodTestInterfaceEmptyArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodLongArgTestInterfaceEmptyArg", V8TestObject::VoidMethodLongArgTestInterfaceEmptyArgMethodCallback, 2, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"anyMethod", V8TestObject::AnyMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodEventTargetArg", V8TestObject::VoidMethodEventTargetArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodAnyArg", V8TestObject::VoidMethodAnyArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodAttrArg", V8TestObject::VoidMethodAttrArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodDocumentArg", V8TestObject::VoidMethodDocumentArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodDocumentTypeArg", V8TestObject::VoidMethodDocumentTypeArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodElementArg", V8TestObject::VoidMethodElementArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodNodeArg", V8TestObject::VoidMethodNodeArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"arrayBufferMethod", V8TestObject::ArrayBufferMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"arrayBufferViewMethod", V8TestObject::ArrayBufferViewMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"arrayBufferViewMethodRaisesException", V8TestObject::ArrayBufferViewMethodRaisesExceptionMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"float32ArrayMethod", V8TestObject::Float32ArrayMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"int32ArrayMethod", V8TestObject::Int32ArrayMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"uint8ArrayMethod", V8TestObject::Uint8ArrayMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodArrayBufferArg", V8TestObject::VoidMethodArrayBufferArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodArrayBufferOrNullArg", V8TestObject::VoidMethodArrayBufferOrNullArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodArrayBufferViewArg", V8TestObject::VoidMethodArrayBufferViewArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodFlexibleArrayBufferViewArg", V8TestObject::VoidMethodFlexibleArrayBufferViewArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodFlexibleArrayBufferViewTypedArg", V8TestObject::VoidMethodFlexibleArrayBufferViewTypedArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodFloat32ArrayArg", V8TestObject::VoidMethodFloat32ArrayArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodInt32ArrayArg", V8TestObject::VoidMethodInt32ArrayArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodUint8ArrayArg", V8TestObject::VoidMethodUint8ArrayArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodAllowSharedArrayBufferViewArg", V8TestObject::VoidMethodAllowSharedArrayBufferViewArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodAllowSharedUint8ArrayArg", V8TestObject::VoidMethodAllowSharedUint8ArrayArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"longSequenceMethod", V8TestObject::LongSequenceMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"stringSequenceMethod", V8TestObject::StringSequenceMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"testInterfaceEmptySequenceMethod", V8TestObject::TestInterfaceEmptySequenceMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodSequenceLongArg", V8TestObject::VoidMethodSequenceLongArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodSequenceStringArg", V8TestObject::VoidMethodSequenceStringArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodSequenceTestInterfaceEmptyArg", V8TestObject::VoidMethodSequenceTestInterfaceEmptyArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodSequenceSequenceDOMStringArg", V8TestObject::VoidMethodSequenceSequenceDOMStringArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodNullableSequenceLongArg", V8TestObject::VoidMethodNullableSequenceLongArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"longFrozenArrayMethod", V8TestObject::LongFrozenArrayMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodStringFrozenArrayMethod", V8TestObject::VoidMethodStringFrozenArrayMethodMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodTestInterfaceEmptyFrozenArrayMethod", V8TestObject::VoidMethodTestInterfaceEmptyFrozenArrayMethodMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"nullableLongMethod", V8TestObject::NullableLongMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"nullableStringMethod", V8TestObject::NullableStringMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"nullableTestInterfaceMethod", V8TestObject::NullableTestInterfaceMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"nullableLongSequenceMethod", V8TestObject::NullableLongSequenceMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"booleanOrDOMStringOrUnrestrictedDoubleMethod", V8TestObject::BooleanOrDOMStringOrUnrestrictedDoubleMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"testInterfaceOrLongMethod", V8TestObject::TestInterfaceOrLongMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodDoubleOrDOMStringArg", V8TestObject::VoidMethodDoubleOrDOMStringArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodDoubleOrDOMStringOrNullArg", V8TestObject::VoidMethodDoubleOrDOMStringOrNullArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodDoubleOrNullOrDOMStringArg", V8TestObject::VoidMethodDoubleOrNullOrDOMStringArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodDOMStringOrArrayBufferOrArrayBufferViewArg", V8TestObject::VoidMethodDOMStringOrArrayBufferOrArrayBufferViewArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodArrayBufferOrArrayBufferViewOrDictionaryArg", V8TestObject::VoidMethodArrayBufferOrArrayBufferViewOrDictionaryArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodBooleanOrElementSequenceArg", V8TestObject::VoidMethodBooleanOrElementSequenceArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodDoubleOrLongOrBooleanSequenceArg", V8TestObject::VoidMethodDoubleOrLongOrBooleanSequenceArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodElementSequenceOrByteStringDoubleOrStringRecord", V8TestObject::VoidMethodElementSequenceOrByteStringDoubleOrStringRecordMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodArrayOfDoubleOrDOMStringArg", V8TestObject::VoidMethodArrayOfDoubleOrDOMStringArgMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodTestInterfaceEmptyOrNullArg", V8TestObject::VoidMethodTestInterfaceEmptyOrNullArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodTestCallbackInterfaceArg", V8TestObject::VoidMethodTestCallbackInterfaceArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodOptionalTestCallbackInterfaceArg", V8TestObject::VoidMethodOptionalTestCallbackInterfaceArgMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodTestCallbackInterfaceOrNullArg", V8TestObject::VoidMethodTestCallbackInterfaceOrNullArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"testEnumMethod", V8TestObject::TestEnumMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodTestEnumArg", V8TestObject::VoidMethodTestEnumArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodTestMultipleEnumArg", V8TestObject::VoidMethodTestMultipleEnumArgMethodCallback, 2, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"dictionaryMethod", V8TestObject::DictionaryMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"testDictionaryMethod", V8TestObject::TestDictionaryMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"nullableTestDictionaryMethod", V8TestObject::NullableTestDictionaryMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"staticTestDictionaryMethod", V8TestObject::StaticTestDictionaryMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnInterface, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"staticNullableTestDictionaryMethod", V8TestObject::StaticNullableTestDictionaryMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnInterface, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"passPermissiveDictionaryMethod", V8TestObject::PassPermissiveDictionaryMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"nodeFilterMethod", V8TestObject::NodeFilterMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"promiseMethod", V8TestObject::PromiseMethodMethodCallback, 3, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kDoNotCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"promiseMethodWithoutExceptionState", V8TestObject::PromiseMethodWithoutExceptionStateMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kDoNotCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"serializedScriptValueMethod", V8TestObject::SerializedScriptValueMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"xPathNSResolverMethod", V8TestObject::XPathNSResolverMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodDictionaryArg", V8TestObject::VoidMethodDictionaryArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodNodeFilterArg", V8TestObject::VoidMethodNodeFilterArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodPromiseArg", V8TestObject::VoidMethodPromiseArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodSerializedScriptValueArg", V8TestObject::VoidMethodSerializedScriptValueArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodXPathNSResolverArg", V8TestObject::VoidMethodXPathNSResolverArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodDictionarySequenceArg", V8TestObject::VoidMethodDictionarySequenceArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodStringArgLongArg", V8TestObject::VoidMethodStringArgLongArgMethodCallback, 2, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodByteStringOrNullOptionalUSVStringArg", V8TestObject::VoidMethodByteStringOrNullOptionalUSVStringArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodOptionalStringArg", V8TestObject::VoidMethodOptionalStringArgMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodOptionalTestInterfaceEmptyArg", V8TestObject::VoidMethodOptionalTestInterfaceEmptyArgMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodOptionalLongArg", V8TestObject::VoidMethodOptionalLongArgMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"stringMethodOptionalLongArg", V8TestObject::StringMethodOptionalLongArgMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"testInterfaceEmptyMethodOptionalLongArg", V8TestObject::TestInterfaceEmptyMethodOptionalLongArgMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"longMethodOptionalLongArg", V8TestObject::LongMethodOptionalLongArgMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodLongArgOptionalLongArg", V8TestObject::VoidMethodLongArgOptionalLongArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodLongArgOptionalLongArgOptionalLongArg", V8TestObject::VoidMethodLongArgOptionalLongArgOptionalLongArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodLongArgOptionalTestInterfaceEmptyArg", V8TestObject::VoidMethodLongArgOptionalTestInterfaceEmptyArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodTestInterfaceEmptyArgOptionalLongArg", V8TestObject::VoidMethodTestInterfaceEmptyArgOptionalLongArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodOptionalDictionaryArg", V8TestObject::VoidMethodOptionalDictionaryArgMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodDefaultByteStringArg", V8TestObject::VoidMethodDefaultByteStringArgMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodDefaultStringArg", V8TestObject::VoidMethodDefaultStringArgMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodDefaultIntegerArgs", V8TestObject::VoidMethodDefaultIntegerArgsMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodDefaultDoubleArg", V8TestObject::VoidMethodDefaultDoubleArgMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodDefaultTrueBooleanArg", V8TestObject::VoidMethodDefaultTrueBooleanArgMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodDefaultFalseBooleanArg", V8TestObject::VoidMethodDefaultFalseBooleanArgMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodDefaultNullableByteStringArg", V8TestObject::VoidMethodDefaultNullableByteStringArgMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodDefaultNullableStringArg", V8TestObject::VoidMethodDefaultNullableStringArgMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodDefaultNullableTestInterfaceArg", V8TestObject::VoidMethodDefaultNullableTestInterfaceArgMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodDefaultDoubleOrStringArgs", V8TestObject::VoidMethodDefaultDoubleOrStringArgsMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodDefaultStringSequenceArg", V8TestObject::VoidMethodDefaultStringSequenceArgMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodVariadicStringArg", V8TestObject::VoidMethodVariadicStringArgMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodStringArgVariadicStringArg", V8TestObject::VoidMethodStringArgVariadicStringArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodVariadicTestInterfaceEmptyArg", V8TestObject::VoidMethodVariadicTestInterfaceEmptyArgMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodTestInterfaceEmptyArgVariadicTestInterfaceEmptyArg", V8TestObject::VoidMethodTestInterfaceEmptyArgVariadicTestInterfaceEmptyArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"overloadedMethodA", V8TestObject::OverloadedMethodAMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"overloadedMethodB", V8TestObject::OverloadedMethodBMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"overloadedMethodC", V8TestObject::OverloadedMethodCMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"overloadedMethodD", V8TestObject::OverloadedMethodDMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"overloadedMethodE", V8TestObject::OverloadedMethodEMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"overloadedMethodF", V8TestObject::OverloadedMethodFMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"overloadedMethodG", V8TestObject::OverloadedMethodGMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"overloadedMethodH", V8TestObject::OverloadedMethodHMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"overloadedMethodI", V8TestObject::OverloadedMethodIMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"overloadedMethodJ", V8TestObject::OverloadedMethodJMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"overloadedMethodK", V8TestObject::OverloadedMethodKMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"overloadedMethodL", V8TestObject::OverloadedMethodLMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"overloadedMethodN", V8TestObject::OverloadedMethodNMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"promiseOverloadMethod", V8TestObject::PromiseOverloadMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kDoNotCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"overloadedPerWorldBindingsMethod", V8TestObject::OverloadedPerWorldBindingsMethodMethodCallbackForMainWorld, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kMainWorld},
    {"overloadedPerWorldBindingsMethod", V8TestObject::OverloadedPerWorldBindingsMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kNonMainWorlds},
    {"overloadedStaticMethod", V8TestObject::OverloadedStaticMethodMethodCallback, 1, v8::None, V8DOMConfiguration::kOnInterface, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"item", V8TestObject::ItemMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"setItem", V8TestObject::SetItemMethodCallback, 2, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodClampUnsignedShortArg", V8TestObject::VoidMethodClampUnsignedShortArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodClampUnsignedLongArg", V8TestObject::VoidMethodClampUnsignedLongArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodDefaultUndefinedTestInterfaceEmptyArg", V8TestObject::VoidMethodDefaultUndefinedTestInterfaceEmptyArgMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodDefaultUndefinedLongArg", V8TestObject::VoidMethodDefaultUndefinedLongArgMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodDefaultUndefinedStringArg", V8TestObject::VoidMethodDefaultUndefinedStringArgMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodEnforceRangeLongArg", V8TestObject::VoidMethodEnforceRangeLongArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodTreatNullAsEmptyStringStringArg", V8TestObject::VoidMethodTreatNullAsEmptyStringStringArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"activityLoggingAccessForAllWorldsMethod", V8TestObject::ActivityLoggingAccessForAllWorldsMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"callWithExecutionContextVoidMethod", V8TestObject::CallWithExecutionContextVoidMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"callWithScriptStateVoidMethod", V8TestObject::CallWithScriptStateVoidMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"callWithScriptStateLongMethod", V8TestObject::CallWithScriptStateLongMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"callWithScriptStateExecutionContextVoidMethod", V8TestObject::CallWithScriptStateExecutionContextVoidMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"callWithScriptStateScriptArgumentsVoidMethod", V8TestObject::CallWithScriptStateScriptArgumentsVoidMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"callWithScriptStateScriptArgumentsVoidMethodOptionalBooleanArg", V8TestObject::CallWithScriptStateScriptArgumentsVoidMethodOptionalBooleanArgMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"callWithCurrentWindow", V8TestObject::CallWithCurrentWindowMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"callWithCurrentWindowScriptWindow", V8TestObject::CallWithCurrentWindowScriptWindowMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"callWithThisValue", V8TestObject::CallWithThisValueMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"checkSecurityForNodeVoidMethod", V8TestObject::CheckSecurityForNodeVoidMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"customVoidMethod", V8TestObject::CustomVoidMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"customCallPrologueVoidMethod", V8TestObject::CustomCallPrologueVoidMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"customCallEpilogueVoidMethod", V8TestObject::CustomCallEpilogueVoidMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"deprecatedVoidMethod", V8TestObject::DeprecatedVoidMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"implementedAsVoidMethod", V8TestObject::ImplementedAsVoidMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"measureAsVoidMethod", V8TestObject::MeasureAsVoidMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"measureMethod", V8TestObject::MeasureMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"measureOverloadedMethod", V8TestObject::MeasureOverloadedMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"DeprecateAsOverloadedMethod", V8TestObject::DeprecateAsOverloadedMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"DeprecateAsSameValueOverloadedMethod", V8TestObject::DeprecateAsSameValueOverloadedMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"measureAsOverloadedMethod", V8TestObject::MeasureAsOverloadedMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"measureAsSameValueOverloadedMethod", V8TestObject::MeasureAsSameValueOverloadedMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"ceReactionsNotOverloadedMethod", V8TestObject::CeReactionsNotOverloadedMethodMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"ceReactionsOverloadedMethod", V8TestObject::CeReactionsOverloadedMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"deprecateAsMeasureAsSameValueOverloadedMethod", V8TestObject::DeprecateAsMeasureAsSameValueOverloadedMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"deprecateAsSameValueMeasureAsOverloadedMethod", V8TestObject::DeprecateAsSameValueMeasureAsOverloadedMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"deprecateAsSameValueMeasureAsSameValueOverloadedMethod", V8TestObject::DeprecateAsSameValueMeasureAsSameValueOverloadedMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"notEnumerableVoidMethod", V8TestObject::NotEnumerableVoidMethodMethodCallback, 0, static_cast<v8::PropertyAttribute>(v8::DontEnum), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"perWorldBindingsVoidMethod", V8TestObject::PerWorldBindingsVoidMethodMethodCallbackForMainWorld, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kMainWorld},
    {"perWorldBindingsVoidMethod", V8TestObject::PerWorldBindingsVoidMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kNonMainWorlds},
    {"perWorldBindingsVoidMethodTestInterfaceEmptyArg", V8TestObject::PerWorldBindingsVoidMethodTestInterfaceEmptyArgMethodCallbackForMainWorld, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kMainWorld},
    {"perWorldBindingsVoidMethodTestInterfaceEmptyArg", V8TestObject::PerWorldBindingsVoidMethodTestInterfaceEmptyArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kNonMainWorlds},
    {"activityLoggingForAllWorldsPerWorldBindingsVoidMethod", V8TestObject::ActivityLoggingForAllWorldsPerWorldBindingsVoidMethodMethodCallbackForMainWorld, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kMainWorld},
    {"activityLoggingForAllWorldsPerWorldBindingsVoidMethod", V8TestObject::ActivityLoggingForAllWorldsPerWorldBindingsVoidMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kNonMainWorlds},
    {"activityLoggingForIsolatedWorldsPerWorldBindingsVoidMethod", V8TestObject::ActivityLoggingForIsolatedWorldsPerWorldBindingsVoidMethodMethodCallbackForMainWorld, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kMainWorld},
    {"activityLoggingForIsolatedWorldsPerWorldBindingsVoidMethod", V8TestObject::ActivityLoggingForIsolatedWorldsPerWorldBindingsVoidMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kNonMainWorlds},
    {"raisesExceptionVoidMethod", V8TestObject::RaisesExceptionVoidMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"raisesExceptionStringMethod", V8TestObject::RaisesExceptionStringMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"raisesExceptionVoidMethodOptionalLongArg", V8TestObject::RaisesExceptionVoidMethodOptionalLongArgMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"raisesExceptionVoidMethodTestCallbackInterfaceArg", V8TestObject::RaisesExceptionVoidMethodTestCallbackInterfaceArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"raisesExceptionVoidMethodOptionalTestCallbackInterfaceArg", V8TestObject::RaisesExceptionVoidMethodOptionalTestCallbackInterfaceArgMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"raisesExceptionTestInterfaceEmptyVoidMethod", V8TestObject::RaisesExceptionTestInterfaceEmptyVoidMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"raisesExceptionXPathNSResolverVoidMethod", V8TestObject::RaisesExceptionXPathNSResolverVoidMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"callWithExecutionContextRaisesExceptionVoidMethodLongArg", V8TestObject::CallWithExecutionContextRaisesExceptionVoidMethodLongArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"legacyInterfaceTypeCheckingVoidMethodTestInterfaceEmptyArg", V8TestObject::LegacyInterfaceTypeCheckingVoidMethodTestInterfaceEmptyArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"legacyInterfaceTypeCheckingVoidMethodTestInterfaceEmptyVariadicArg", V8TestObject::LegacyInterfaceTypeCheckingVoidMethodTestInterfaceEmptyVariadicArgMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"useToImpl4ArgumentsCheckingIfPossibleWithOptionalArg", V8TestObject::UseToImpl4ArgumentsCheckingIfPossibleWithOptionalArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"useToImpl4ArgumentsCheckingIfPossibleWithNullableArg", V8TestObject::UseToImpl4ArgumentsCheckingIfPossibleWithNullableArgMethodCallback, 2, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"useToImpl4ArgumentsCheckingIfPossibleWithUndefinedArg", V8TestObject::UseToImpl4ArgumentsCheckingIfPossibleWithUndefinedArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"unforgeableVoidMethod", V8TestObject::UnforgeableVoidMethodMethodCallback, 0, static_cast<v8::PropertyAttribute>(v8::ReadOnly | v8::DontDelete), V8DOMConfiguration::kOnInstance, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"newObjectTestInterfaceMethod", V8TestObject::NewObjectTestInterfaceMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"newObjectTestInterfacePromiseMethod", V8TestObject::NewObjectTestInterfacePromiseMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kDoNotCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"RuntimeCallStatsCounterMethod", V8TestObject::RuntimeCallStatsCounterMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"keys", V8TestObject::KeysMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"values", V8TestObject::ValuesMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"forEach", V8TestObject::ForEachMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"has", V8TestObject::HasMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"get", V8TestObject::GetMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"delete", V8TestObject::DeleteMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"set", V8TestObject::SetMethodCallback, 2, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"toJSON", V8TestObject::ToJSONMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"toString", V8TestObject::ToStringMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
};

static void InstallV8TestObjectTemplate(
    v8::Isolate* isolate,
    const DOMWrapperWorld& world,
    v8::Local<v8::FunctionTemplate> interfaceTemplate) {
  // Initialize the interface object's template.
  V8DOMConfiguration::InitializeDOMInterfaceTemplate(isolate, interfaceTemplate, V8TestObject::wrapperTypeInfo.interface_name, v8::Local<v8::FunctionTemplate>(), V8TestObject::internalFieldCount);

  v8::Local<v8::Signature> signature = v8::Signature::New(isolate, interfaceTemplate);
  ALLOW_UNUSED_LOCAL(signature);
  v8::Local<v8::ObjectTemplate> instanceTemplate = interfaceTemplate->InstanceTemplate();
  ALLOW_UNUSED_LOCAL(instanceTemplate);
  v8::Local<v8::ObjectTemplate> prototypeTemplate = interfaceTemplate->PrototypeTemplate();
  ALLOW_UNUSED_LOCAL(prototypeTemplate);

  // Register IDL constants, attributes and operations.
  V8DOMConfiguration::InstallAttributes(
      isolate, world, instanceTemplate, prototypeTemplate,
      V8TestObjectAttributes, base::size(V8TestObjectAttributes));
  V8DOMConfiguration::InstallAccessors(
      isolate, world, instanceTemplate, prototypeTemplate, interfaceTemplate,
      signature, V8TestObjectAccessors, base::size(V8TestObjectAccessors));
  V8DOMConfiguration::InstallMethods(
      isolate, world, instanceTemplate, prototypeTemplate, interfaceTemplate,
      signature, V8TestObjectMethods, base::size(V8TestObjectMethods));

  // Indexed properties
  v8::IndexedPropertyHandlerConfiguration indexedPropertyHandlerConfig(
      V8TestObject::IndexedPropertyGetterCallback,
      V8TestObject::IndexedPropertySetterCallback,
      V8TestObject::IndexedPropertyDescriptorCallback,
      V8TestObject::IndexedPropertyDeleterCallback,
      IndexedPropertyEnumerator<TestObject>,
      V8TestObject::IndexedPropertyDefinerCallback,
      v8::Local<v8::Value>(),
      v8::PropertyHandlerFlags::kNone);
  instanceTemplate->SetHandler(indexedPropertyHandlerConfig);
  // Named properties
  v8::NamedPropertyHandlerConfiguration namedPropertyHandlerConfig(V8TestObject::NamedPropertyGetterCallback, V8TestObject::NamedPropertySetterCallback, V8TestObject::NamedPropertyQueryCallback, V8TestObject::NamedPropertyDeleterCallback, V8TestObject::NamedPropertyEnumeratorCallback, v8::Local<v8::Value>(), static_cast<v8::PropertyHandlerFlags>(int(v8::PropertyHandlerFlags::kOnlyInterceptStrings) | int(v8::PropertyHandlerFlags::kNonMasking)));
  instanceTemplate->SetHandler(namedPropertyHandlerConfig);

  // Iterator (@@iterator)
  static const V8DOMConfiguration::SymbolKeyedMethodConfiguration
  symbolKeyedIteratorConfiguration = {
      v8::Symbol::GetIterator,
      "entries",
      V8TestObject::IteratorMethodCallback,
      0,
      v8::DontEnum,
      V8DOMConfiguration::kOnPrototype,
      V8DOMConfiguration::kCheckHolder,
      V8DOMConfiguration::kDoNotCheckAccess,
      V8DOMConfiguration::kHasSideEffect
  };
  V8DOMConfiguration::InstallMethod(isolate, world, prototypeTemplate, signature, symbolKeyedIteratorConfiguration);

  // Custom signature
  const V8DOMConfiguration::MethodConfiguration partiallyRuntimeEnabledOverloadedVoidMethodMethodConfiguration[] = {
    {"partiallyRuntimeEnabledOverloadedVoidMethod", V8TestObject::PartiallyRuntimeEnabledOverloadedVoidMethodMethodCallback, test_object_v8_internal::PartiallyRuntimeEnabledOverloadedVoidMethodMethodLength(), v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
  };
  for (const auto& methodConfig : partiallyRuntimeEnabledOverloadedVoidMethodMethodConfiguration)
    V8DOMConfiguration::InstallMethod(isolate, world, instanceTemplate, prototypeTemplate, interfaceTemplate, signature, methodConfig);

  V8TestObject::InstallRuntimeEnabledFeaturesOnTemplate(
      isolate, world, interfaceTemplate);
}

void V8TestObject::InstallRuntimeEnabledFeaturesOnTemplate(
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
        { "runtimeEnabledLongAttribute", V8TestObject::RuntimeEnabledLongAttributeAttributeGetterCallback, V8TestObject::RuntimeEnabledLongAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
        { "unscopableRuntimeEnabledLongAttribute", V8TestObject::UnscopableRuntimeEnabledLongAttributeAttributeGetterCallback, V8TestObject::UnscopableRuntimeEnabledLongAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    };
    V8DOMConfiguration::InstallAccessors(
        isolate, world, instance_template, prototype_template, interface_template,
        signature, accessor_configurations,
        base::size(accessor_configurations));
  }

  // Custom signature
  if (RuntimeEnabledFeatures::FeatureNameEnabled()) {
    const V8DOMConfiguration::MethodConfiguration unscopableRuntimeEnabledVoidMethodMethodConfiguration[] = {
      {"unscopableRuntimeEnabledVoidMethod", V8TestObject::UnscopableRuntimeEnabledVoidMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
    };
    for (const auto& methodConfig : unscopableRuntimeEnabledVoidMethodMethodConfiguration)
      V8DOMConfiguration::InstallMethod(isolate, world, instance_template, prototype_template, interface_template, signature, methodConfig);
  }
  if (RuntimeEnabledFeatures::FeatureNameEnabled()) {
    const V8DOMConfiguration::MethodConfiguration runtimeEnabledVoidMethodMethodConfiguration[] = {
      {"runtimeEnabledVoidMethod", V8TestObject::RuntimeEnabledVoidMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
    };
    for (const auto& methodConfig : runtimeEnabledVoidMethodMethodConfiguration)
      V8DOMConfiguration::InstallMethod(isolate, world, instance_template, prototype_template, interface_template, signature, methodConfig);
  }
  if (RuntimeEnabledFeatures::FeatureNameEnabled()) {
    const V8DOMConfiguration::MethodConfiguration perWorldBindingsRuntimeEnabledVoidMethodMethodConfiguration[] = {
      {"perWorldBindingsRuntimeEnabledVoidMethod", V8TestObject::PerWorldBindingsRuntimeEnabledVoidMethodMethodCallbackForMainWorld, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kMainWorld},
      {"perWorldBindingsRuntimeEnabledVoidMethod", V8TestObject::PerWorldBindingsRuntimeEnabledVoidMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kNonMainWorlds}
    };
    for (const auto& methodConfig : perWorldBindingsRuntimeEnabledVoidMethodMethodConfiguration)
      V8DOMConfiguration::InstallMethod(isolate, world, instance_template, prototype_template, interface_template, signature, methodConfig);
  }
  if (RuntimeEnabledFeatures::FeatureNameEnabled()) {
    const V8DOMConfiguration::MethodConfiguration runtimeEnabledOverloadedVoidMethodMethodConfiguration[] = {
      {"runtimeEnabledOverloadedVoidMethod", V8TestObject::RuntimeEnabledOverloadedVoidMethodMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
    };
    for (const auto& methodConfig : runtimeEnabledOverloadedVoidMethodMethodConfiguration)
      V8DOMConfiguration::InstallMethod(isolate, world, instance_template, prototype_template, interface_template, signature, methodConfig);
  }
  if (RuntimeEnabledFeatures::FeatureNameEnabled()) {
    const V8DOMConfiguration::MethodConfiguration clearMethodConfiguration[] = {
      {"clear", V8TestObject::ClearMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
    };
    for (const auto& methodConfig : clearMethodConfiguration)
      V8DOMConfiguration::InstallMethod(isolate, world, instance_template, prototype_template, interface_template, signature, methodConfig);
  }
}

void V8TestObject::InstallFeatureName(v8::Isolate* isolate, const DOMWrapperWorld& world, v8::Local<v8::Object> instance, v8::Local<v8::Object> prototype, v8::Local<v8::Function> interface) {
  v8::Local<v8::FunctionTemplate> interfaceTemplate = V8TestObject::wrapperTypeInfo.DomTemplate(isolate, world);
  v8::Local<v8::Signature> signature = v8::Signature::New(isolate, interfaceTemplate);
  ALLOW_UNUSED_LOCAL(signature);
  static const V8DOMConfiguration::AccessorConfiguration accessororiginTrialEnabledLongAttributeConfiguration[] = {
    { "originTrialEnabledLongAttribute", V8TestObject::OriginTrialEnabledLongAttributeAttributeGetterCallback, V8TestObject::OriginTrialEnabledLongAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds }
  };
  for (const auto& accessorConfig : accessororiginTrialEnabledLongAttributeConfiguration)
    V8DOMConfiguration::InstallAccessor(isolate, world, instance, prototype, interface, signature, accessorConfig);
  static const V8DOMConfiguration::AccessorConfiguration accessorunscopableOriginTrialEnabledLongAttributeConfiguration[] = {
    { "unscopableOriginTrialEnabledLongAttribute", V8TestObject::UnscopableOriginTrialEnabledLongAttributeAttributeGetterCallback, V8TestObject::UnscopableOriginTrialEnabledLongAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds }
  };
  for (const auto& accessorConfig : accessorunscopableOriginTrialEnabledLongAttributeConfiguration)
    V8DOMConfiguration::InstallAccessor(isolate, world, instance, prototype, interface, signature, accessorConfig);
  static const V8DOMConfiguration::MethodConfiguration methodOrigintrialenabledvoidmethodConfiguration[] = {
    {"originTrialEnabledVoidMethod", V8TestObject::OriginTrialEnabledVoidMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
  };
  for (const auto& methodConfig : methodOrigintrialenabledvoidmethodConfiguration)
    V8DOMConfiguration::InstallMethod(isolate, world, instance, prototype, interface, signature, methodConfig);
  static const V8DOMConfiguration::MethodConfiguration methodPerworldbindingsorigintrialenabledvoidmethodConfiguration[] = {
    {"perWorldBindingsOriginTrialEnabledVoidMethod", V8TestObject::PerWorldBindingsOriginTrialEnabledVoidMethodMethodCallbackForMainWorld, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kMainWorld},
  {"perWorldBindingsOriginTrialEnabledVoidMethod", V8TestObject::PerWorldBindingsOriginTrialEnabledVoidMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kNonMainWorlds}
  };
  for (const auto& methodConfig : methodPerworldbindingsorigintrialenabledvoidmethodConfiguration)
    V8DOMConfiguration::InstallMethod(isolate, world, instance, prototype, interface, signature, methodConfig);
}

void V8TestObject::InstallFeatureName(ScriptState* scriptState, v8::Local<v8::Object> instance) {
  V8PerContextData* perContextData = scriptState->PerContextData();
  v8::Local<v8::Object> prototype = perContextData->PrototypeForType(&V8TestObject::wrapperTypeInfo);
  v8::Local<v8::Function> interface = perContextData->ConstructorForType(&V8TestObject::wrapperTypeInfo);
  ALLOW_UNUSED_LOCAL(interface);
  InstallFeatureName(scriptState->GetIsolate(), scriptState->World(), instance, prototype, interface);
}

void V8TestObject::InstallFeatureName(ScriptState* scriptState) {
  InstallFeatureName(scriptState, v8::Local<v8::Object>());
}

v8::Local<v8::FunctionTemplate> V8TestObject::DomTemplate(v8::Isolate* isolate, const DOMWrapperWorld& world) {
  return V8DOMConfiguration::DomClassTemplate(isolate, world, const_cast<WrapperTypeInfo*>(&wrapperTypeInfo), InstallV8TestObjectTemplate);
}

bool V8TestObject::HasInstance(v8::Local<v8::Value> v8Value, v8::Isolate* isolate) {
  return V8PerIsolateData::From(isolate)->HasInstance(&wrapperTypeInfo, v8Value);
}

v8::Local<v8::Object> V8TestObject::FindInstanceInPrototypeChain(v8::Local<v8::Value> v8Value, v8::Isolate* isolate) {
  return V8PerIsolateData::From(isolate)->FindInstanceInPrototypeChain(&wrapperTypeInfo, v8Value);
}

TestObject* V8TestObject::ToImplWithTypeCheck(v8::Isolate* isolate, v8::Local<v8::Value> value) {
  return HasInstance(value, isolate) ? ToImpl(v8::Local<v8::Object>::Cast(value)) : nullptr;
}

TestObject* NativeValueTraits<TestObject>::NativeValue(v8::Isolate* isolate, v8::Local<v8::Value> value, ExceptionState& exceptionState) {
  TestObject* nativeValue = V8TestObject::ToImplWithTypeCheck(isolate, value);
  if (!nativeValue) {
    exceptionState.ThrowTypeError(ExceptionMessages::FailedToConvertJSValue(
        "TestObject"));
  }
  return nativeValue;
}

void V8TestObject::InstallConditionalFeatures(
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
        context, V8AtomicString(isolate, "unscopableLongAttribute"), v8::True(isolate))
        .FromJust();
    unscopables->CreateDataProperty(
        context, V8AtomicString(isolate, "unscopableOriginTrialEnabledLongAttribute"), v8::True(isolate))
        .FromJust();
    if (RuntimeEnabledFeatures::FeatureNameEnabled()) {
      unscopables->CreateDataProperty(
          context, V8AtomicString(isolate, "unscopableRuntimeEnabledLongAttribute"), v8::True(isolate))
          .FromJust();
    }
    if (RuntimeEnabledFeatures::FeatureNameEnabled()) {
      unscopables->CreateDataProperty(
          context, V8AtomicString(isolate, "unscopableRuntimeEnabledVoidMethod"), v8::True(isolate))
          .FromJust();
    }
    unscopables->CreateDataProperty(
        context, V8AtomicString(isolate, "unscopableVoidMethod"), v8::True(isolate))
        .FromJust();
    prototypeObject->CreateDataProperty(context, unscopablesSymbol, unscopables).FromJust();
  }
}

}  // namespace blink
