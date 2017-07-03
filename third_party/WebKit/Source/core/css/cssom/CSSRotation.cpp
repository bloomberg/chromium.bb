// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/CSSRotation.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/css/CSSFunctionValue.h"
#include "core/css/CSSPrimitiveValue.h"

namespace blink {

namespace {

bool IsNumberValue(const CSSValue& value) {
  return value.IsPrimitiveValue() && ToCSSPrimitiveValue(value).IsNumber();
}

CSSRotation* FromCSSRotate(const CSSFunctionValue& value) {
  DCHECK_EQ(value.length(), 1UL);
  const CSSPrimitiveValue& primitive_value = ToCSSPrimitiveValue(value.Item(0));
  if (primitive_value.IsCalculated() || !primitive_value.IsAngle())
    return nullptr;
  return CSSRotation::Create(CSSNumericValue::FromCSSValue(primitive_value));
}

CSSRotation* FromCSSRotate3d(const CSSFunctionValue& value) {
  DCHECK_EQ(value.length(), 4UL);
  DCHECK(IsNumberValue(value.Item(0)));
  DCHECK(IsNumberValue(value.Item(1)));
  DCHECK(IsNumberValue(value.Item(2)));
  const CSSPrimitiveValue& angle = ToCSSPrimitiveValue(value.Item(3));
  if (angle.IsCalculated() || !angle.IsAngle())
    return nullptr;

  double x = ToCSSPrimitiveValue(value.Item(0)).GetDoubleValue();
  double y = ToCSSPrimitiveValue(value.Item(1)).GetDoubleValue();
  double z = ToCSSPrimitiveValue(value.Item(2)).GetDoubleValue();

  return CSSRotation::Create(x, y, z, CSSNumericValue::FromCSSValue(angle));
}

CSSRotation* FromCSSRotateXYZ(const CSSFunctionValue& value) {
  DCHECK_EQ(value.length(), 1UL);
  const CSSPrimitiveValue& primitive_value = ToCSSPrimitiveValue(value.Item(0));
  if (primitive_value.IsCalculated())
    return nullptr;
  CSSNumericValue* angle = CSSNumericValue::FromCSSValue(primitive_value);
  switch (value.FunctionType()) {
    case CSSValueRotateX:
      return CSSRotation::Create(1, 0, 0, angle);
    case CSSValueRotateY:
      return CSSRotation::Create(0, 1, 0, angle);
    case CSSValueRotateZ:
      return CSSRotation::Create(0, 0, 1, angle);
    default:
      NOTREACHED();
      return nullptr;
  }
}

}  // namespace

CSSRotation* CSSRotation::Create(CSSNumericValue* angle,
                                 ExceptionState& exception_state) {
  if (angle->GetType() != CSSStyleValue::StyleValueType::kAngleType) {
    exception_state.ThrowTypeError("Must pass an angle to CSSRotation");
    return nullptr;
  }
  return new CSSRotation(0, 0, 1, angle, true /* is2D */);
}

CSSRotation* CSSRotation::Create(double x,
                                 double y,
                                 double z,
                                 CSSNumericValue* angle,
                                 ExceptionState& exception_state) {
  if (angle->GetType() != CSSStyleValue::StyleValueType::kAngleType) {
    exception_state.ThrowTypeError("Must pass an angle to CSSRotation");
    return nullptr;
  }
  return new CSSRotation(x, y, z, angle, false /* is2D */);
}

CSSRotation* CSSRotation::Create(CSSNumericValue* angle) {
  DCHECK_EQ(angle->GetType(), CSSStyleValue::StyleValueType::kAngleType);
  return new CSSRotation(0, 0, 1, angle, true /* is2D */);
}

CSSRotation* CSSRotation::Create(double x,
                                 double y,
                                 double z,
                                 CSSNumericValue* angle) {
  DCHECK_EQ(angle->GetType(), CSSStyleValue::StyleValueType::kAngleType);
  return new CSSRotation(x, y, z, angle, false /* is2D */);
}

CSSRotation* CSSRotation::FromCSSValue(const CSSFunctionValue& value) {
  switch (value.FunctionType()) {
    case CSSValueRotate:
      return FromCSSRotate(value);
    case CSSValueRotate3d:
      return FromCSSRotate3d(value);
    case CSSValueRotateX:
    case CSSValueRotateY:
    case CSSValueRotateZ:
      return FromCSSRotateXYZ(value);
    default:
      NOTREACHED();
      return nullptr;
  }
}

void CSSRotation::setAngle(CSSNumericValue* angle,
                           ExceptionState& exception_state) {
  if (angle->GetType() != CSSStyleValue::StyleValueType::kAngleType) {
    exception_state.ThrowTypeError("Must pass an angle to CSSRotation");
    return;
  }
  if (angle->IsCalculated()) {
    exception_state.ThrowTypeError("Calculated angles are not supported yet");
    return;
  }
  angle_ = angle;
}

CSSFunctionValue* CSSRotation::ToCSSValue() const {
  return nullptr;
  // TODO(meade): Re-implement this when we finish rewriting number/length
  // types.
  // CSSFunctionValue* result =
  //     CSSFunctionValue::Create(is2D() ? CSSValueRotate : CSSValueRotate3d);
  // if (!is2D()) {
  //   result->Append(
  //      *CSSPrimitiveValue::Create(x_, CSSPrimitiveValue::UnitType::kNumber));
  //   result->Append(
  //      *CSSPrimitiveValue::Create(y_, CSSPrimitiveValue::UnitType::kNumber));
  //   result->Append(
  //      *CSSPrimitiveValue::Create(z_, CSSPrimitiveValue::UnitType::kNumber));
  // }
  // result->Append(*CSSPrimitiveValue::Create(angle_->Value(),
  //                                           angle_->Unit()));
  // return result;
}

}  // namespace blink
