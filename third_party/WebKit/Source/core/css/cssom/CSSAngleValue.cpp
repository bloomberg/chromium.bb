// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/CSSAngleValue.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/css/CSSPrimitiveValue.h"
#include "wtf/MathExtras.h"

namespace blink {

CSSAngleValue* CSSAngleValue::Create(double value,
                                     CSSPrimitiveValue::UnitType unit) {
  DCHECK(CSSPrimitiveValue::IsAngle(unit));
  return new CSSAngleValue(value, unit);
}

CSSAngleValue* CSSAngleValue::Create(double value, const String& unit) {
  CSSPrimitiveValue::UnitType primitive_unit =
      CSSPrimitiveValue::StringToUnitType(unit);
  return Create(value, primitive_unit);
}

CSSAngleValue* CSSAngleValue::FromCSSValue(const CSSPrimitiveValue& value) {
  DCHECK(value.IsAngle());
  if (value.IsCalculated())
    return nullptr;
  return new CSSAngleValue(value.GetDoubleValue(),
                           value.TypeWithCalcResolved());
}

double CSSAngleValue::degrees() const {
  switch (unit_) {
    case CSSPrimitiveValue::UnitType::kDegrees:
      return value_;
    case CSSPrimitiveValue::UnitType::kRadians:
      return rad2deg(value_);
    case CSSPrimitiveValue::UnitType::kGradians:
      return grad2deg(value_);
    case CSSPrimitiveValue::UnitType::kTurns:
      return turn2deg(value_);
    default:
      NOTREACHED();
      return 0;
  }
}

double CSSAngleValue::radians() const {
  return deg2rad(degrees());
}

double CSSAngleValue::gradians() const {
  return deg2grad(degrees());
}

double CSSAngleValue::turns() const {
  return deg2turn(degrees());
}

CSSValue* CSSAngleValue::ToCSSValue() const {
  return CSSPrimitiveValue::Create(value_, unit_);
}

}  // namespace blink
