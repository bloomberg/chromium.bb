// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/CSSPositionValue.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/css/CSSValuePair.h"
#include "core/css/cssom/CSSNumericValue.h"

namespace blink {

CSSPositionValue* CSSPositionValue::Create(CSSNumericValue* x,
                                           CSSNumericValue* y,
                                           ExceptionState& exception_state) {
  if (x->GetType() != CSSStyleValue::StyleValueType::kLengthType &&
      x->GetType() != CSSStyleValue::StyleValueType::kPercentType) {
    exception_state.ThrowTypeError(
        "Must pass length or percentage to x in CSSPositionValue");
    return nullptr;
  }
  if (y->GetType() != CSSStyleValue::StyleValueType::kLengthType &&
      y->GetType() != CSSStyleValue::StyleValueType::kPercentType) {
    exception_state.ThrowTypeError(
        "Must pass length or percentage to y in CSSPositionValue");
    return nullptr;
  }
  return new CSSPositionValue(x, y);
}

void CSSPositionValue::setX(CSSNumericValue* x,
                            ExceptionState& exception_state) {
  if (x->GetType() != CSSStyleValue::StyleValueType::kLengthType &&
      x->GetType() != CSSStyleValue::StyleValueType::kPercentType) {
    exception_state.ThrowTypeError(
        "Must pass length or percentage to x in CSSPositionValue");
    return;
  }
  x_ = x;
}

void CSSPositionValue::setY(CSSNumericValue* y,
                            ExceptionState& exception_state) {
  if (y->GetType() != CSSStyleValue::StyleValueType::kLengthType &&
      y->GetType() != CSSStyleValue::StyleValueType::kPercentType) {
    exception_state.ThrowTypeError(
        "Must pass length or percentage to y in CSSPositionValue");
    return;
  }
  y_ = y;
}

const CSSValue* CSSPositionValue::ToCSSValue() const {
  return CSSValuePair::Create(x_->ToCSSValue(), y_->ToCSSValue(),
                              CSSValuePair::kKeepIdenticalValues);
}

}  // namespace blink
