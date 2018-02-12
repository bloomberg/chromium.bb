// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/CSSSkewX.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/css/CSSFunctionValue.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/cssom/CSSNumericValue.h"
#include "core/css/cssom/CSSStyleValue.h"
#include "core/css/cssom/CSSUnitValue.h"
#include "core/geometry/DOMMatrix.h"

namespace blink {

namespace {

bool IsValidSkewXAngle(CSSNumericValue* value) {
  return value &&
         value->Type().MatchesBaseType(CSSNumericValueType::BaseType::kAngle);
}

}  // namespace

CSSSkewX* CSSSkewX::Create(CSSNumericValue* ax,
                           ExceptionState& exception_state) {
  if (!IsValidSkewXAngle(ax)) {
    exception_state.ThrowTypeError("CSSSkewX does not support non-angles");
    return nullptr;
  }
  return new CSSSkewX(ax);
}

void CSSSkewX::setAx(CSSNumericValue* value, ExceptionState& exception_state) {
  if (!IsValidSkewXAngle(value)) {
    exception_state.ThrowTypeError("Must specify an angle unit");
    return;
  }
  ax_ = value;
}

CSSSkewX* CSSSkewX::FromCSSValue(const CSSFunctionValue& value) {
  DCHECK_GT(value.length(), 0U);
  DCHECK_EQ(value.FunctionType(), CSSValueSkewX);
  if (value.length() == 1U) {
    return CSSSkewX::Create(
        CSSNumericValue::FromCSSValue(ToCSSPrimitiveValue(value.Item(0))));
  }
  NOTREACHED();
  return nullptr;
}

DOMMatrix* CSSSkewX::toMatrix(ExceptionState&) const {
  CSSUnitValue* ax = ax_->to(CSSPrimitiveValue::UnitType::kRadians);
  DCHECK(ax);
  DOMMatrix* result = DOMMatrix::Create();
  result->skewXSelf(ax->value());
  return result;
}

const CSSFunctionValue* CSSSkewX::ToCSSValue() const {
  const CSSValue* ax = ax_->ToCSSValue();
  if (!ax)
    return nullptr;

  CSSFunctionValue* result = CSSFunctionValue::Create(CSSValueSkewX);
  result->Append(*ax);
  return result;
}

CSSSkewX::CSSSkewX(CSSNumericValue* ax)
    : CSSTransformComponent(true /* is2D */), ax_(ax) {
  DCHECK(ax);
}

}  // namespace blink
