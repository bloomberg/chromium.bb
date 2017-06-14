// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/CSSSkew.h"

#include "core/css/CSSFunctionValue.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/cssom/CSSNumericValue.h"
#include "core/css/cssom/CSSUnitValue.h"

namespace blink {

void CSSSkew::setAx(CSSNumericValue* value, ExceptionState& exception_state) {
  if (value->GetType() != CSSStyleValue::StyleValueType::kAngleType) {
    exception_state.ThrowTypeError("Must specify an angle unit");
    return;
  }
  if (value->IsCalculated()) {
    exception_state.ThrowTypeError("Calculated angles are not supported yet");
    return;
  }
  ax_ = value;
}

void CSSSkew::setAy(CSSNumericValue* value, ExceptionState& exception_state) {
  if (value->GetType() != CSSStyleValue::StyleValueType::kAngleType) {
    exception_state.ThrowTypeError("Must specify an angle unit");
    return;
  }
  if (value->IsCalculated()) {
    exception_state.ThrowTypeError("Calculated angles are not supported yet");
    return;
  }
  ay_ = value;
}

CSSSkew* CSSSkew::FromCSSValue(const CSSFunctionValue& value) {
  return nullptr;
  // TODO(meade): Re-enable this code once numbers and units types have been
  // re-written to the new spec.
  // const CSSPrimitiveValue& x_value = ToCSSPrimitiveValue(value.Item(0));
  // if (x_value.IsCalculated()) {
  //   // TODO(meade): Decide what we want to do with calc angles.
  //   return nullptr;
  // }
  // DCHECK(x_value.IsAngle());
  // switch (value.FunctionType()) {
  //   case CSSValueSkew:
  //     if (value.length() == 2U) {
  //       const CSSPrimitiveValue& y_value =
  //       ToCSSPrimitiveValue(value.Item(1)); if (y_value.IsCalculated()) {
  //         // TODO(meade): Decide what we want to do with calc angles.
  //         return nullptr;
  //       }
  //       DCHECK(y_value.IsAngle());
  //       return CSSSkew::Create(CSSNumericValue::FromCSSValue(x_value),
  //                              CSSNumericValue::FromCSSValue(y_value));
  //     }
  //   // Else fall through; skew(ax) == skewX(ax).
  //   case CSSValueSkewX:
  //     DCHECK_EQ(value.length(), 1U);
  //     return CSSSkew::Create(
  //         CSSNumericValue::FromCSSValue(x_value),
  //         CSSUnitValue::Create(0, CSSPrimitiveValue::UnitType::kDegrees));
  //   case CSSValueSkewY:
  //     DCHECK_EQ(value.length(), 1U);
  //     return CSSSkew::Create(
  //         CSSUnitValue::Create(0, CSSPrimitiveValue::UnitType::kDegrees),
  //         CSSNumericValue::FromCSSValue(x_value));
  //   default:
  //     NOTREACHED();
  //     return nullptr;
  // }
}

CSSFunctionValue* CSSSkew::ToCSSValue() const {
  return nullptr;
  // TODO(meade): Re-implement this when we finish rewriting number/length
  // types.
  // CSSFunctionValue* result = CSSFunctionValue::Create(CSSValueSkew);
  // result->Append(*CSSPrimitiveValue::Create(ax_->Value(), ax_->Unit()));
  // result->Append(*CSSPrimitiveValue::Create(ay_->Value(), ay_->Unit()));
  // return result;
}

}  // namespace blink
