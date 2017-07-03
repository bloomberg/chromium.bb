// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/CSSTranslation.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/cssom/CSSNumericValue.h"
#include "core/css/cssom/CSSStyleValue.h"

namespace blink {

CSSTranslation* CSSTranslation::Create(CSSNumericValue* x,
                                       CSSNumericValue* y,
                                       ExceptionState& exception_state) {
  if ((x->GetType() != CSSStyleValue::StyleValueType::kLengthType &&
       x->GetType() != CSSStyleValue::StyleValueType::kPercentType) ||
      (y->GetType() != CSSStyleValue::StyleValueType::kLengthType &&
       y->GetType() != CSSStyleValue::StyleValueType::kPercentType)) {
    exception_state.ThrowTypeError(
        "Must pass length or percentage to X and Y of CSSTranslation");
    return nullptr;
  }
  return new CSSTranslation(
      x, y, CSSUnitValue::Create(0, CSSPrimitiveValue::UnitType::kPixels),
      true /* is2D */);
}

CSSTranslation* CSSTranslation::Create(CSSNumericValue* x,
                                       CSSNumericValue* y,
                                       CSSNumericValue* z,
                                       ExceptionState& exception_state) {
  if ((x->GetType() != CSSStyleValue::StyleValueType::kLengthType &&
       x->GetType() != CSSStyleValue::StyleValueType::kPercentType) ||
      (y->GetType() != CSSStyleValue::StyleValueType::kLengthType &&
       y->GetType() != CSSStyleValue::StyleValueType::kPercentType)) {
    exception_state.ThrowTypeError(
        "Must pass length or percentage to X and Y of CSSTranslation");
    return nullptr;
  }
  if (z && z->GetType() != CSSStyleValue::StyleValueType::kLengthType) {
    exception_state.ThrowTypeError("Must pass length to Z of CSSTranslation");
    return nullptr;
  }
  if (z && z->ContainsPercent()) {
    exception_state.ThrowTypeError(
        "CSSTranslation does not support z CSSNumericValue with percent units");
    return nullptr;
  }
  return new CSSTranslation(x, y, z, false /* is2D */);
}

void CSSTranslation::setX(CSSNumericValue* x, ExceptionState& exception_state) {
  if (x->GetType() != CSSStyleValue::StyleValueType::kLengthType &&
      x->GetType() != CSSStyleValue::StyleValueType::kPercentType) {
    exception_state.ThrowTypeError(
        "Must pass length or percentage to X of CSSTranslation");
    return;
  }
  x_ = x;
}

void CSSTranslation::setY(CSSNumericValue* y, ExceptionState& exception_state) {
  if (y->GetType() != CSSStyleValue::StyleValueType::kLengthType &&
      y->GetType() != CSSStyleValue::StyleValueType::kPercentType) {
    exception_state.ThrowTypeError(
        "Must pass length or percent to Y of CSSTranslation");
    return;
  }
  y_ = y;
}

void CSSTranslation::setZ(CSSNumericValue* z, ExceptionState& exception_state) {
  if (z && z->GetType() != CSSStyleValue::StyleValueType::kLengthType) {
    exception_state.ThrowTypeError("Must pass length to Z of CSSTranslation");
    return;
  }
  if (z && z->ContainsPercent()) {
    exception_state.ThrowTypeError(
        "CSSTranslation does not support z CSSNumericValue with percent units");
    return;
  }
  z_ = z;
}

CSSFunctionValue* CSSTranslation::ToCSSValue() const {
  CSSFunctionValue* result = CSSFunctionValue::Create(
      is2D() ? CSSValueTranslate : CSSValueTranslate3d);
  result->Append(*x_->ToCSSValue());
  result->Append(*y_->ToCSSValue());
  if (!is2D())
    result->Append(*z_->ToCSSValue());
  return result;
}

}  // namespace blink
