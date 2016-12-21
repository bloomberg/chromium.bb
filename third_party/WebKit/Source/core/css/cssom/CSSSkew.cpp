// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/CSSSkew.h"

#include "core/css/CSSFunctionValue.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/cssom/CSSAngleValue.h"

namespace blink {

CSSSkew* CSSSkew::fromCSSValue(const CSSFunctionValue& value) {
  const CSSPrimitiveValue& xValue = toCSSPrimitiveValue(value.item(0));
  if (xValue.isCalculated()) {
    // TODO(meade): Decide what we want to do with calc angles.
    return nullptr;
  }
  DCHECK(xValue.isAngle());
  switch (value.functionType()) {
    case CSSValueSkew:
      if (value.length() == 2U) {
        const CSSPrimitiveValue& yValue = toCSSPrimitiveValue(value.item(1));
        if (yValue.isCalculated()) {
          // TODO(meade): Decide what we want to do with calc angles.
          return nullptr;
        }
        DCHECK(yValue.isAngle());
        return CSSSkew::create(CSSAngleValue::fromCSSValue(xValue),
                               CSSAngleValue::fromCSSValue(yValue));
      }
    // Else fall through; skew(ax) == skewX(ax).
    case CSSValueSkewX:
      DCHECK_EQ(value.length(), 1U);
      return CSSSkew::create(
          CSSAngleValue::fromCSSValue(xValue),
          CSSAngleValue::create(0, CSSPrimitiveValue::UnitType::Degrees));
    case CSSValueSkewY:
      DCHECK_EQ(value.length(), 1U);
      return CSSSkew::create(
          CSSAngleValue::create(0, CSSPrimitiveValue::UnitType::Degrees),
          CSSAngleValue::fromCSSValue(xValue));
    default:
      NOTREACHED();
      return nullptr;
  }
}

CSSFunctionValue* CSSSkew::toCSSValue() const {
  CSSFunctionValue* result = CSSFunctionValue::create(CSSValueSkew);
  result->append(*CSSPrimitiveValue::create(m_ax->value(), m_ax->unit()));
  result->append(*CSSPrimitiveValue::create(m_ay->value(), m_ay->unit()));
  return result;
}

}  // namespace blink
