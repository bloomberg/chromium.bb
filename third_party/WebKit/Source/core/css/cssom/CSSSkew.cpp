// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/CSSSkew.h"

#include "core/css/CSSFunctionValue.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/cssom/CSSAngleValue.h"

namespace blink {

CSSSkew* CSSSkew::FromCSSValue(const CSSFunctionValue& value) {
  const CSSPrimitiveValue& x_value = ToCSSPrimitiveValue(value.Item(0));
  if (x_value.IsCalculated()) {
    // TODO(meade): Decide what we want to do with calc angles.
    return nullptr;
  }
  DCHECK(x_value.IsAngle());
  switch (value.FunctionType()) {
    case CSSValueSkew:
      if (value.length() == 2U) {
        const CSSPrimitiveValue& y_value = ToCSSPrimitiveValue(value.Item(1));
        if (y_value.IsCalculated()) {
          // TODO(meade): Decide what we want to do with calc angles.
          return nullptr;
        }
        DCHECK(y_value.IsAngle());
        return CSSSkew::Create(CSSAngleValue::FromCSSValue(x_value),
                               CSSAngleValue::FromCSSValue(y_value));
      }
    // Else fall through; skew(ax) == skewX(ax).
    case CSSValueSkewX:
      DCHECK_EQ(value.length(), 1U);
      return CSSSkew::Create(
          CSSAngleValue::FromCSSValue(x_value),
          CSSAngleValue::Create(0, CSSPrimitiveValue::UnitType::kDegrees));
    case CSSValueSkewY:
      DCHECK_EQ(value.length(), 1U);
      return CSSSkew::Create(
          CSSAngleValue::Create(0, CSSPrimitiveValue::UnitType::kDegrees),
          CSSAngleValue::FromCSSValue(x_value));
    default:
      NOTREACHED();
      return nullptr;
  }
}

CSSFunctionValue* CSSSkew::ToCSSValue() const {
  CSSFunctionValue* result = CSSFunctionValue::Create(CSSValueSkew);
  result->Append(*CSSPrimitiveValue::Create(ax_->Value(), ax_->Unit()));
  result->Append(*CSSPrimitiveValue::Create(ay_->Value(), ay_->Unit()));
  return result;
}

}  // namespace blink
