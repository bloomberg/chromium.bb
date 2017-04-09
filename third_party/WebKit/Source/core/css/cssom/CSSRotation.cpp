// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/CSSRotation.h"

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
  if (!primitive_value.IsAngle())
    return nullptr;
  return CSSRotation::Create(CSSAngleValue::FromCSSValue(primitive_value));
}

CSSRotation* FromCSSRotate3d(const CSSFunctionValue& value) {
  DCHECK_EQ(value.length(), 4UL);
  DCHECK(IsNumberValue(value.Item(0)));
  DCHECK(IsNumberValue(value.Item(1)));
  DCHECK(IsNumberValue(value.Item(2)));
  const CSSPrimitiveValue& angle = ToCSSPrimitiveValue(value.Item(3));
  if (!angle.IsAngle())
    return nullptr;

  double x = ToCSSPrimitiveValue(value.Item(0)).GetDoubleValue();
  double y = ToCSSPrimitiveValue(value.Item(1)).GetDoubleValue();
  double z = ToCSSPrimitiveValue(value.Item(2)).GetDoubleValue();

  return CSSRotation::Create(x, y, z, CSSAngleValue::FromCSSValue(angle));
}

CSSRotation* FromCSSRotateXYZ(const CSSFunctionValue& value) {
  DCHECK_EQ(value.length(), 1UL);

  CSSAngleValue* angle =
      CSSAngleValue::FromCSSValue(ToCSSPrimitiveValue(value.Item(0)));
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

CSSFunctionValue* CSSRotation::ToCSSValue() const {
  CSSFunctionValue* result =
      CSSFunctionValue::Create(is2d_ ? CSSValueRotate : CSSValueRotate3d);
  if (!is2d_) {
    result->Append(
        *CSSPrimitiveValue::Create(x_, CSSPrimitiveValue::UnitType::kNumber));
    result->Append(
        *CSSPrimitiveValue::Create(y_, CSSPrimitiveValue::UnitType::kNumber));
    result->Append(
        *CSSPrimitiveValue::Create(z_, CSSPrimitiveValue::UnitType::kNumber));
  }
  result->Append(*CSSPrimitiveValue::Create(angle_->Value(), angle_->Unit()));
  return result;
}

}  // namespace blink
