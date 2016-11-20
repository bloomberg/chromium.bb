// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/CSSScale.h"

#include "core/css/CSSPrimitiveValue.h"

namespace blink {

namespace {

CSSScale* fromScale(const CSSFunctionValue& value) {
  DCHECK(value.length() == 1U || value.length() == 2U);
  double x = toCSSPrimitiveValue(value.item(0)).getDoubleValue();
  if (value.length() == 1U)
    return CSSScale::create(x, 1);

  double y = toCSSPrimitiveValue(value.item(1)).getDoubleValue();
  return CSSScale::create(x, y);
}

CSSScale* fromScaleXYZ(const CSSFunctionValue& value) {
  DCHECK_EQ(value.length(), 1U);
  double doubleValue = toCSSPrimitiveValue(value.item(0)).getDoubleValue();
  switch (value.functionType()) {
    case CSSValueScaleX:
      return CSSScale::create(doubleValue, 1);
    case CSSValueScaleY:
      return CSSScale::create(1, doubleValue);
    case CSSValueScaleZ:
      return CSSScale::create(1, 1, doubleValue);
    default:
      NOTREACHED();
      return nullptr;
  }
}

CSSScale* fromScale3d(const CSSFunctionValue& value) {
  DCHECK_EQ(value.length(), 3U);
  double x = toCSSPrimitiveValue(value.item(0)).getDoubleValue();
  double y = toCSSPrimitiveValue(value.item(1)).getDoubleValue();
  double z = toCSSPrimitiveValue(value.item(2)).getDoubleValue();
  return CSSScale::create(x, y, z);
}

}  // namespace

CSSScale* CSSScale::fromCSSValue(const CSSFunctionValue& value) {
  switch (value.functionType()) {
    case CSSValueScale:
      return fromScale(value);
    case CSSValueScaleX:
    case CSSValueScaleY:
    case CSSValueScaleZ:
      return fromScaleXYZ(value);
    case CSSValueScale3d:
      return fromScale3d(value);
    default:
      NOTREACHED();
      return nullptr;
  }
}

CSSFunctionValue* CSSScale::toCSSValue() const {
  CSSFunctionValue* result =
      CSSFunctionValue::create(m_is2D ? CSSValueScale : CSSValueScale3d);

  result->append(
      *CSSPrimitiveValue::create(m_x, CSSPrimitiveValue::UnitType::Number));
  result->append(
      *CSSPrimitiveValue::create(m_y, CSSPrimitiveValue::UnitType::Number));
  if (!m_is2D)
    result->append(
        *CSSPrimitiveValue::create(m_z, CSSPrimitiveValue::UnitType::Number));

  return result;
}

}  // namespace blink
