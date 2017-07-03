// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/CSSScale.h"

#include "core/css/CSSPrimitiveValue.h"

namespace blink {

namespace {

CSSScale* FromScale(const CSSFunctionValue& value) {
  DCHECK(value.length() == 1U || value.length() == 2U);
  double x = ToCSSPrimitiveValue(value.Item(0)).GetDoubleValue();
  if (value.length() == 1U)
    return CSSScale::Create(x, 1);

  double y = ToCSSPrimitiveValue(value.Item(1)).GetDoubleValue();
  return CSSScale::Create(x, y);
}

CSSScale* FromScaleXYZ(const CSSFunctionValue& value) {
  DCHECK_EQ(value.length(), 1U);
  double double_value = ToCSSPrimitiveValue(value.Item(0)).GetDoubleValue();
  switch (value.FunctionType()) {
    case CSSValueScaleX:
      return CSSScale::Create(double_value, 1);
    case CSSValueScaleY:
      return CSSScale::Create(1, double_value);
    case CSSValueScaleZ:
      return CSSScale::Create(1, 1, double_value);
    default:
      NOTREACHED();
      return nullptr;
  }
}

CSSScale* FromScale3d(const CSSFunctionValue& value) {
  DCHECK_EQ(value.length(), 3U);
  double x = ToCSSPrimitiveValue(value.Item(0)).GetDoubleValue();
  double y = ToCSSPrimitiveValue(value.Item(1)).GetDoubleValue();
  double z = ToCSSPrimitiveValue(value.Item(2)).GetDoubleValue();
  return CSSScale::Create(x, y, z);
}

}  // namespace

CSSScale* CSSScale::FromCSSValue(const CSSFunctionValue& value) {
  switch (value.FunctionType()) {
    case CSSValueScale:
      return FromScale(value);
    case CSSValueScaleX:
    case CSSValueScaleY:
    case CSSValueScaleZ:
      return FromScaleXYZ(value);
    case CSSValueScale3d:
      return FromScale3d(value);
    default:
      NOTREACHED();
      return nullptr;
  }
}

CSSFunctionValue* CSSScale::ToCSSValue() const {
  CSSFunctionValue* result =
      CSSFunctionValue::Create(is2D() ? CSSValueScale : CSSValueScale3d);

  result->Append(
      *CSSPrimitiveValue::Create(x_, CSSPrimitiveValue::UnitType::kNumber));
  result->Append(
      *CSSPrimitiveValue::Create(y_, CSSPrimitiveValue::UnitType::kNumber));
  if (!is2D())
    result->Append(
        *CSSPrimitiveValue::Create(z_, CSSPrimitiveValue::UnitType::kNumber));

  return result;
}

}  // namespace blink
