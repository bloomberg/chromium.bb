// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/CSSSkew.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/css/CSSFunctionValue.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/cssom/CSSNumericValue.h"
#include "core/css/cssom/CSSStyleValue.h"
#include "core/css/cssom/CSSUnitValue.h"
#include "core/geometry/DOMMatrix.h"

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
        return CSSSkew::Create(CSSNumericValue::FromCSSValue(x_value),
                               CSSNumericValue::FromCSSValue(y_value));
      }
    // Else fall through; skew(ax) == skewX(ax).
    case CSSValueSkewX:
      DCHECK_EQ(value.length(), 1U);
      return CSSSkew::Create(
          CSSNumericValue::FromCSSValue(x_value),
          CSSUnitValue::Create(0, CSSPrimitiveValue::UnitType::kDegrees));
    case CSSValueSkewY:
      DCHECK_EQ(value.length(), 1U);
      return CSSSkew::Create(
          CSSUnitValue::Create(0, CSSPrimitiveValue::UnitType::kDegrees),
          CSSNumericValue::FromCSSValue(x_value));
    default:
      NOTREACHED();
      return nullptr;
  }
}

const DOMMatrix* CSSSkew::AsMatrix(ExceptionState&) const {
  CSSUnitValue* ax = ax_->to(CSSPrimitiveValue::UnitType::kRadians);
  CSSUnitValue* ay = ay_->to(CSSPrimitiveValue::UnitType::kRadians);
  DCHECK(ax);
  DCHECK(ay);
  DOMMatrix* result = DOMMatrix::Create();
  result->setM12(std::tan(ay->value()));
  result->setM21(std::tan(ax->value()));
  return result;
}

const CSSFunctionValue* CSSSkew::ToCSSValue() const {
  // TDOO(meade): Handle calc angles here.
  CSSUnitValue* ax = ToCSSUnitValue(ax_);
  CSSUnitValue* ay = ToCSSUnitValue(ay_);

  CSSFunctionValue* result = CSSFunctionValue::Create(CSSValueSkew);
  result->Append(
      *CSSPrimitiveValue::Create(ax->value(), ax->GetInternalUnit()));
  result->Append(
      *CSSPrimitiveValue::Create(ay->value(), ay->GetInternalUnit()));
  return result;
}

}  // namespace blink
