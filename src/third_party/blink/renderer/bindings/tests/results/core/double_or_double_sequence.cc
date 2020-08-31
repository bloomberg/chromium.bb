// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/union_container.cc.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "third_party/blink/renderer/bindings/tests/results/core/double_or_double_sequence.h"

#include "base/stl_util.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/script_iterator.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_for_core.h"

namespace blink {

DoubleOrDoubleSequence::DoubleOrDoubleSequence() : type_(SpecificType::kNone) {}

double DoubleOrDoubleSequence::GetAsDouble() const {
  DCHECK(IsDouble());
  return double_;
}

void DoubleOrDoubleSequence::SetDouble(double value) {
  DCHECK(IsNull());
  double_ = value;
  type_ = SpecificType::kDouble;
}

DoubleOrDoubleSequence DoubleOrDoubleSequence::FromDouble(double value) {
  DoubleOrDoubleSequence container;
  container.SetDouble(value);
  return container;
}

const Vector<double>& DoubleOrDoubleSequence::GetAsDoubleSequence() const {
  DCHECK(IsDoubleSequence());
  return double_sequence_;
}

void DoubleOrDoubleSequence::SetDoubleSequence(const Vector<double>& value) {
  DCHECK(IsNull());
  double_sequence_ = value;
  type_ = SpecificType::kDoubleSequence;
}

DoubleOrDoubleSequence DoubleOrDoubleSequence::FromDoubleSequence(const Vector<double>& value) {
  DoubleOrDoubleSequence container;
  container.SetDoubleSequence(value);
  return container;
}

DoubleOrDoubleSequence::DoubleOrDoubleSequence(const DoubleOrDoubleSequence&) = default;
DoubleOrDoubleSequence::~DoubleOrDoubleSequence() = default;
DoubleOrDoubleSequence& DoubleOrDoubleSequence::operator=(const DoubleOrDoubleSequence&) = default;

void DoubleOrDoubleSequence::Trace(Visitor* visitor) {
}

void V8DoubleOrDoubleSequence::ToImpl(
    v8::Isolate* isolate,
    v8::Local<v8::Value> v8_value,
    DoubleOrDoubleSequence& impl,
    UnionTypeConversionMode conversion_mode,
    ExceptionState& exception_state) {
  if (v8_value.IsEmpty())
    return;

  if (conversion_mode == UnionTypeConversionMode::kNullable && IsUndefinedOrNull(v8_value))
    return;

  if (v8_value->IsObject()) {
    ScriptIterator script_iterator = ScriptIterator::FromIterable(
        isolate, v8_value.As<v8::Object>(), exception_state);
    if (exception_state.HadException())
      return;
    if (!script_iterator.IsNull()) {
      Vector<double> cpp_value = NativeValueTraits<IDLSequence<IDLDouble>>::NativeValue(isolate, std::move(script_iterator), exception_state);
      if (exception_state.HadException())
        return;
      impl.SetDoubleSequence(cpp_value);
      return;
    }
  }

  if (v8_value->IsNumber()) {
    double cpp_value = NativeValueTraits<IDLDouble>::NativeValue(isolate, v8_value, exception_state);
    if (exception_state.HadException())
      return;
    impl.SetDouble(cpp_value);
    return;
  }

  {
    double cpp_value = NativeValueTraits<IDLDouble>::NativeValue(isolate, v8_value, exception_state);
    if (exception_state.HadException())
      return;
    impl.SetDouble(cpp_value);
    return;
  }
}

v8::Local<v8::Value> ToV8(const DoubleOrDoubleSequence& impl, v8::Local<v8::Object> creationContext, v8::Isolate* isolate) {
  switch (impl.type_) {
    case DoubleOrDoubleSequence::SpecificType::kNone:
      return v8::Null(isolate);
    case DoubleOrDoubleSequence::SpecificType::kDouble:
      return v8::Number::New(isolate, impl.GetAsDouble());
    case DoubleOrDoubleSequence::SpecificType::kDoubleSequence:
      return ToV8(impl.GetAsDoubleSequence(), creationContext, isolate);
    default:
      NOTREACHED();
  }
  return v8::Local<v8::Value>();
}

DoubleOrDoubleSequence NativeValueTraits<DoubleOrDoubleSequence>::NativeValue(
    v8::Isolate* isolate, v8::Local<v8::Value> value, ExceptionState& exception_state) {
  DoubleOrDoubleSequence impl;
  V8DoubleOrDoubleSequence::ToImpl(isolate, value, impl, UnionTypeConversionMode::kNotNullable, exception_state);
  return impl;
}

}  // namespace blink

