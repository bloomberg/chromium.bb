// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/CSSSimpleLength.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/cssom/CSSCalcLength.h"
#include "wtf/text/StringBuilder.h"

namespace blink {

CSSSimpleLength* CSSSimpleLength::Create(double value,
                                         const String& type,
                                         ExceptionState& exception_state) {
  CSSPrimitiveValue::UnitType unit = CSSLengthValue::UnitFromName(type);
  if (!CSSLengthValue::IsSupportedLengthUnit(unit)) {
    exception_state.ThrowTypeError("Invalid unit for CSSSimpleLength: " + type);
    return nullptr;
  }
  return new CSSSimpleLength(value, unit);
}

CSSSimpleLength* CSSSimpleLength::FromCSSValue(const CSSPrimitiveValue& value) {
  DCHECK(value.IsLength() || value.IsPercentage());
  if (value.IsPercentage())
    return new CSSSimpleLength(value.GetDoubleValue(),
                               CSSPrimitiveValue::UnitType::kPercentage);
  return new CSSSimpleLength(value.GetDoubleValue(),
                             value.TypeWithCalcResolved());
}

bool CSSSimpleLength::ContainsPercent() const {
  return LengthUnit() == CSSPrimitiveValue::UnitType::kPercentage;
}

String CSSSimpleLength::unit() const {
  if (LengthUnit() == CSSPrimitiveValue::UnitType::kPercentage)
    return "percent";
  return CSSPrimitiveValue::UnitTypeToString(unit_);
}

CSSLengthValue* CSSSimpleLength::AddInternal(const CSSLengthValue* other) {
  const CSSSimpleLength* o = ToCSSSimpleLength(other);
  DCHECK_EQ(unit_, o->unit_);
  return Create(value_ + o->value(), unit_);
}

CSSLengthValue* CSSSimpleLength::SubtractInternal(const CSSLengthValue* other) {
  const CSSSimpleLength* o = ToCSSSimpleLength(other);
  DCHECK_EQ(unit_, o->unit_);
  return Create(value_ - o->value(), unit_);
}

CSSLengthValue* CSSSimpleLength::MultiplyInternal(double x) {
  return Create(value_ * x, unit_);
}

CSSLengthValue* CSSSimpleLength::DivideInternal(double x) {
  DCHECK_NE(x, 0);
  return Create(value_ / x, unit_);
}

CSSValue* CSSSimpleLength::ToCSSValue() const {
  return CSSPrimitiveValue::Create(value_, unit_);
}

}  // namespace blink
