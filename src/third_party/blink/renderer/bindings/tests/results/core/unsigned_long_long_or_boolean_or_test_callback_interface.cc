// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/union_container.cc.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "third_party/blink/renderer/bindings/tests/results/core/unsigned_long_long_or_boolean_or_test_callback_interface.h"

#include "base/stl_util.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_test_callback_interface.h"

namespace blink {

UnsignedLongLongOrBooleanOrTestCallbackInterface::UnsignedLongLongOrBooleanOrTestCallbackInterface() : type_(SpecificType::kNone) {}

bool UnsignedLongLongOrBooleanOrTestCallbackInterface::GetAsBoolean() const {
  DCHECK(IsBoolean());
  return boolean_;
}

void UnsignedLongLongOrBooleanOrTestCallbackInterface::SetBoolean(bool value) {
  DCHECK(IsNull());
  boolean_ = value;
  type_ = SpecificType::kBoolean;
}

UnsignedLongLongOrBooleanOrTestCallbackInterface UnsignedLongLongOrBooleanOrTestCallbackInterface::FromBoolean(bool value) {
  UnsignedLongLongOrBooleanOrTestCallbackInterface container;
  container.SetBoolean(value);
  return container;
}

V8TestCallbackInterface* UnsignedLongLongOrBooleanOrTestCallbackInterface::GetAsTestCallbackInterface() const {
  DCHECK(IsTestCallbackInterface());
  return test_callback_interface_;
}

void UnsignedLongLongOrBooleanOrTestCallbackInterface::SetTestCallbackInterface(V8TestCallbackInterface* value) {
  DCHECK(IsNull());
  test_callback_interface_ = value;
  type_ = SpecificType::kTestCallbackInterface;
}

UnsignedLongLongOrBooleanOrTestCallbackInterface UnsignedLongLongOrBooleanOrTestCallbackInterface::FromTestCallbackInterface(V8TestCallbackInterface* value) {
  UnsignedLongLongOrBooleanOrTestCallbackInterface container;
  container.SetTestCallbackInterface(value);
  return container;
}

uint64_t UnsignedLongLongOrBooleanOrTestCallbackInterface::GetAsUnsignedLongLong() const {
  DCHECK(IsUnsignedLongLong());
  return unsigned_long_long_;
}

void UnsignedLongLongOrBooleanOrTestCallbackInterface::SetUnsignedLongLong(uint64_t value) {
  DCHECK(IsNull());
  unsigned_long_long_ = value;
  type_ = SpecificType::kUnsignedLongLong;
}

UnsignedLongLongOrBooleanOrTestCallbackInterface UnsignedLongLongOrBooleanOrTestCallbackInterface::FromUnsignedLongLong(uint64_t value) {
  UnsignedLongLongOrBooleanOrTestCallbackInterface container;
  container.SetUnsignedLongLong(value);
  return container;
}

UnsignedLongLongOrBooleanOrTestCallbackInterface::UnsignedLongLongOrBooleanOrTestCallbackInterface(const UnsignedLongLongOrBooleanOrTestCallbackInterface&) = default;
UnsignedLongLongOrBooleanOrTestCallbackInterface::~UnsignedLongLongOrBooleanOrTestCallbackInterface() = default;
UnsignedLongLongOrBooleanOrTestCallbackInterface& UnsignedLongLongOrBooleanOrTestCallbackInterface::operator=(const UnsignedLongLongOrBooleanOrTestCallbackInterface&) = default;

void UnsignedLongLongOrBooleanOrTestCallbackInterface::Trace(Visitor* visitor) {
  visitor->Trace(test_callback_interface_);
}

void V8UnsignedLongLongOrBooleanOrTestCallbackInterface::ToImpl(
    v8::Isolate* isolate,
    v8::Local<v8::Value> v8_value,
    UnsignedLongLongOrBooleanOrTestCallbackInterface& impl,
    UnionTypeConversionMode conversion_mode,
    ExceptionState& exception_state) {
  if (v8_value.IsEmpty())
    return;

  if (conversion_mode == UnionTypeConversionMode::kNullable && IsUndefinedOrNull(v8_value))
    return;

  if (V8TestCallbackInterface::HasInstance(v8_value, isolate)) {
    V8TestCallbackInterface* cpp_value = V8TestCallbackInterface::ToImpl(v8::Local<v8::Object>::Cast(v8_value));
    impl.SetTestCallbackInterface(cpp_value);
    return;
  }

  if (v8_value->IsBoolean()) {
    impl.SetBoolean(v8_value.As<v8::Boolean>()->Value());
    return;
  }

  if (v8_value->IsNumber()) {
    uint64_t cpp_value = NativeValueTraits<IDLUnsignedLongLong>::NativeValue(isolate, v8_value, exception_state);
    if (exception_state.HadException())
      return;
    impl.SetUnsignedLongLong(cpp_value);
    return;
  }

  {
    uint64_t cpp_value = NativeValueTraits<IDLUnsignedLongLong>::NativeValue(isolate, v8_value, exception_state);
    if (exception_state.HadException())
      return;
    impl.SetUnsignedLongLong(cpp_value);
    return;
  }
}

v8::Local<v8::Value> ToV8(const UnsignedLongLongOrBooleanOrTestCallbackInterface& impl, v8::Local<v8::Object> creationContext, v8::Isolate* isolate) {
  switch (impl.type_) {
    case UnsignedLongLongOrBooleanOrTestCallbackInterface::SpecificType::kNone:
      return v8::Null(isolate);
    case UnsignedLongLongOrBooleanOrTestCallbackInterface::SpecificType::kBoolean:
      return v8::Boolean::New(isolate, impl.GetAsBoolean());
    case UnsignedLongLongOrBooleanOrTestCallbackInterface::SpecificType::kTestCallbackInterface:
      return ToV8(impl.GetAsTestCallbackInterface(), creationContext, isolate);
    case UnsignedLongLongOrBooleanOrTestCallbackInterface::SpecificType::kUnsignedLongLong:
      return v8::Number::New(isolate, static_cast<double>(impl.GetAsUnsignedLongLong()));
    default:
      NOTREACHED();
  }
  return v8::Local<v8::Value>();
}

UnsignedLongLongOrBooleanOrTestCallbackInterface NativeValueTraits<UnsignedLongLongOrBooleanOrTestCallbackInterface>::NativeValue(
    v8::Isolate* isolate, v8::Local<v8::Value> value, ExceptionState& exception_state) {
  UnsignedLongLongOrBooleanOrTestCallbackInterface impl;
  V8UnsignedLongLongOrBooleanOrTestCallbackInterface::ToImpl(isolate, value, impl, UnionTypeConversionMode::kNotNullable, exception_state);
  return impl;
}

}  // namespace blink

