// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/CSSSkewY.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/css/CSSFunctionValue.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/cssom/CSSNumericValue.h"
#include "core/css/cssom/CSSStyleValue.h"
#include "core/css/cssom/CSSUnitValue.h"
#include "core/geometry/DOMMatrix.h"

namespace blink {

namespace {

bool IsValidSkewYAngle(CSSNumericValue* value) {
  return value &&
         value->Type().MatchesBaseType(CSSNumericValueType::BaseType::kAngle);
}

}  // namespace

CSSSkewY* CSSSkewY::Create(CSSNumericValue* ay,
                           ExceptionState& exception_state) {
  if (!IsValidSkewYAngle(ay)) {
    exception_state.ThrowTypeError("CSSSkewY does not support non-angles");
    return nullptr;
  }
  return new CSSSkewY(ay);
}

void CSSSkewY::setAy(CSSNumericValue* value, ExceptionState& exception_state) {
  if (!IsValidSkewYAngle(value)) {
    exception_state.ThrowTypeError("Must specify an angle unit");
    return;
  }
  ay_ = value;
}

CSSSkewY* CSSSkewY::FromCSSValue(const CSSFunctionValue& value) {
  DCHECK_GT(value.length(), 0U);
  DCHECK_EQ(value.FunctionType(), CSSValueSkewY);
  if (value.length(), 1U) {
    return CSSSkewY::Create(
        CSSNumericValue::FromCSSValue(ToCSSPrimitiveValue(value.Item(0))));
  }
  NOTREACHED();
  return nullptr;
}

DOMMatrix* CSSSkewY::toMatrix(ExceptionState&) const {
  CSSUnitValue* ay = ay_->to(CSSPrimitiveValue::UnitType::kRadians);
  DCHECK(ay);
  DOMMatrix* result = DOMMatrix::Create();
  result->skewYSelf(ay->value());
  return result;
}

const CSSFunctionValue* CSSSkewY::ToCSSValue() const {
  const CSSValue* ay = ay_->ToCSSValue();
  if (!ay)
    return nullptr;

  CSSFunctionValue* result = CSSFunctionValue::Create(CSSValueSkewY);
  result->Append(*ay);
  return result;
}

CSSSkewY::CSSSkewY(CSSNumericValue* ay)
    : CSSTransformComponent(true /* is2D */), ay_(ay) {
  DCHECK(ay);
}

}  // namespace blink
