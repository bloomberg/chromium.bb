// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/union_container.cc.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "third_party/blink/renderer/bindings/tests/results/core/boolean_or_string_or_unrestricted_double.h"

#include "base/stl_util.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_for_core.h"

namespace blink {

BooleanOrStringOrUnrestrictedDouble::BooleanOrStringOrUnrestrictedDouble() : type_(SpecificType::kNone) {}

bool BooleanOrStringOrUnrestrictedDouble::GetAsBoolean() const {
  DCHECK(IsBoolean());
  return boolean_;
}

void BooleanOrStringOrUnrestrictedDouble::SetBoolean(bool value) {
  DCHECK(IsNull());
  boolean_ = value;
  type_ = SpecificType::kBoolean;
}

BooleanOrStringOrUnrestrictedDouble BooleanOrStringOrUnrestrictedDouble::FromBoolean(bool value) {
  BooleanOrStringOrUnrestrictedDouble container;
  container.SetBoolean(value);
  return container;
}

const String& BooleanOrStringOrUnrestrictedDouble::GetAsString() const {
  DCHECK(IsString());
  return string_;
}

void BooleanOrStringOrUnrestrictedDouble::SetString(const String& value) {
  DCHECK(IsNull());
  string_ = value;
  type_ = SpecificType::kString;
}

BooleanOrStringOrUnrestrictedDouble BooleanOrStringOrUnrestrictedDouble::FromString(const String& value) {
  BooleanOrStringOrUnrestrictedDouble container;
  container.SetString(value);
  return container;
}

double BooleanOrStringOrUnrestrictedDouble::GetAsUnrestrictedDouble() const {
  DCHECK(IsUnrestrictedDouble());
  return unrestricted_double_;
}

void BooleanOrStringOrUnrestrictedDouble::SetUnrestrictedDouble(double value) {
  DCHECK(IsNull());
  unrestricted_double_ = value;
  type_ = SpecificType::kUnrestrictedDouble;
}

BooleanOrStringOrUnrestrictedDouble BooleanOrStringOrUnrestrictedDouble::FromUnrestrictedDouble(double value) {
  BooleanOrStringOrUnrestrictedDouble container;
  container.SetUnrestrictedDouble(value);
  return container;
}

BooleanOrStringOrUnrestrictedDouble::BooleanOrStringOrUnrestrictedDouble(const BooleanOrStringOrUnrestrictedDouble&) = default;
BooleanOrStringOrUnrestrictedDouble::~BooleanOrStringOrUnrestrictedDouble() = default;
BooleanOrStringOrUnrestrictedDouble& BooleanOrStringOrUnrestrictedDouble::operator=(const BooleanOrStringOrUnrestrictedDouble&) = default;

void BooleanOrStringOrUnrestrictedDouble::Trace(Visitor* visitor) {
}

void V8BooleanOrStringOrUnrestrictedDouble::ToImpl(
    v8::Isolate* isolate,
    v8::Local<v8::Value> v8_value,
    BooleanOrStringOrUnrestrictedDouble& impl,
    UnionTypeConversionMode conversion_mode,
    ExceptionState& exception_state) {
  if (v8_value.IsEmpty())
    return;

  if (conversion_mode == UnionTypeConversionMode::kNullable && IsUndefinedOrNull(v8_value))
    return;

  if (v8_value->IsBoolean()) {
    impl.SetBoolean(v8_value.As<v8::Boolean>()->Value());
    return;
  }

  if (v8_value->IsNumber()) {
    double cpp_value = NativeValueTraits<IDLUnrestrictedDouble>::NativeValue(isolate, v8_value, exception_state);
    if (exception_state.HadException())
      return;
    impl.SetUnrestrictedDouble(cpp_value);
    return;
  }

  {
    V8StringResource<> cpp_value = v8_value;
    if (!cpp_value.Prepare(exception_state))
      return;
    impl.SetString(cpp_value);
    return;
  }
}

v8::Local<v8::Value> ToV8(const BooleanOrStringOrUnrestrictedDouble& impl, v8::Local<v8::Object> creationContext, v8::Isolate* isolate) {
  switch (impl.type_) {
    case BooleanOrStringOrUnrestrictedDouble::SpecificType::kNone:
      return v8::Null(isolate);
    case BooleanOrStringOrUnrestrictedDouble::SpecificType::kBoolean:
      return v8::Boolean::New(isolate, impl.GetAsBoolean());
    case BooleanOrStringOrUnrestrictedDouble::SpecificType::kString:
      return V8String(isolate, impl.GetAsString());
    case BooleanOrStringOrUnrestrictedDouble::SpecificType::kUnrestrictedDouble:
      return v8::Number::New(isolate, impl.GetAsUnrestrictedDouble());
    default:
      NOTREACHED();
  }
  return v8::Local<v8::Value>();
}

BooleanOrStringOrUnrestrictedDouble NativeValueTraits<BooleanOrStringOrUnrestrictedDouble>::NativeValue(
    v8::Isolate* isolate, v8::Local<v8::Value> value, ExceptionState& exception_state) {
  BooleanOrStringOrUnrestrictedDouble impl;
  V8BooleanOrStringOrUnrestrictedDouble::ToImpl(isolate, value, impl, UnionTypeConversionMode::kNotNullable, exception_state);
  return impl;
}

}  // namespace blink

