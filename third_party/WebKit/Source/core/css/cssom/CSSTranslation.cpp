// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/CSSTranslation.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/cssom/CSSNumericValue.h"
#include "core/css/cssom/CSSStyleValue.h"
#include "core/geometry/DOMMatrix.h"

namespace blink {

namespace {

bool IsLengthOrPercent(const CSSNumericValue* value) {
  return (value->GetType() == CSSStyleValue::StyleValueType::kLengthType ||
          value->GetType() == CSSStyleValue::StyleValueType::kPercentType);
}

bool IsLengthValue(const CSSValue& value) {
  return value.IsPrimitiveValue() && ToCSSPrimitiveValue(value).IsLength();
}

CSSTranslation* FromCSSTranslate(const CSSFunctionValue& value) {
  DCHECK_GT(value.length(), 0UL);
  DCHECK(IsLengthValue(value.Item(0)));

  CSSNumericValue* x =
      CSSNumericValue::FromCSSValue(ToCSSPrimitiveValue(value.Item(0)));
  if (!x)
    return nullptr;

  if (value.length() == 1) {
    return CSSTranslation::Create(
        x, CSSUnitValue::Create(0, CSSPrimitiveValue::UnitType::kPixels));
  }

  DCHECK_EQ(value.length(), 2UL);
  DCHECK(IsLengthValue(value.Item(1)));

  CSSNumericValue* y =
      CSSNumericValue::FromCSSValue(ToCSSPrimitiveValue(value.Item(1)));
  if (!y)
    return nullptr;
  return CSSTranslation::Create(x, y);
}

CSSTranslation* FromCSSTranslateXYZ(const CSSFunctionValue& value) {
  DCHECK_EQ(value.length(), 1UL);
  DCHECK(IsLengthValue(value.Item(0)));

  CSSNumericValue* length =
      CSSNumericValue::FromCSSValue(ToCSSPrimitiveValue(value.Item(0)));
  if (!length)
    return nullptr;

  switch (value.FunctionType()) {
    case CSSValueTranslateX:
      return CSSTranslation::Create(
          length,
          CSSUnitValue::Create(0, CSSPrimitiveValue::UnitType::kPixels));
    case CSSValueTranslateY:
      return CSSTranslation::Create(
          CSSUnitValue::Create(0, CSSPrimitiveValue::UnitType::kPixels),
          length);
    case CSSValueTranslateZ:
      return CSSTranslation::Create(
          CSSUnitValue::Create(0, CSSPrimitiveValue::UnitType::kPixels),
          CSSUnitValue::Create(0, CSSPrimitiveValue::UnitType::kPixels),
          length);
    default:
      NOTREACHED();
      return nullptr;
  }
}

CSSTranslation* FromCSSTranslate3D(const CSSFunctionValue& value) {
  DCHECK_EQ(value.length(), 3UL);
  DCHECK(IsLengthValue(value.Item(0)));
  DCHECK(IsLengthValue(value.Item(1)));
  DCHECK(IsLengthValue(value.Item(2)));

  CSSNumericValue* x =
      CSSNumericValue::FromCSSValue(ToCSSPrimitiveValue(value.Item(0)));
  CSSNumericValue* y =
      CSSNumericValue::FromCSSValue(ToCSSPrimitiveValue(value.Item(1)));
  CSSNumericValue* z =
      CSSNumericValue::FromCSSValue(ToCSSPrimitiveValue(value.Item(2)));
  if (!x || !y || !z)
    return nullptr;

  return CSSTranslation::Create(x, y, z);
}

}  // namespace

CSSTranslation* CSSTranslation::Create(CSSNumericValue* x,
                                       CSSNumericValue* y,
                                       ExceptionState& exception_state) {
  if (!IsLengthOrPercent(x) || !IsLengthOrPercent(y)) {
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
  if (!IsLengthOrPercent(x) || !IsLengthOrPercent(y)) {
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

CSSTranslation* CSSTranslation::Create(CSSNumericValue* x, CSSNumericValue* y) {
  DCHECK(IsLengthOrPercent(x));
  DCHECK(IsLengthOrPercent(y));
  return new CSSTranslation(
      x, y, CSSUnitValue::Create(0, CSSPrimitiveValue::UnitType::kPixels),
      true /* is2D */);
}

CSSTranslation* CSSTranslation::Create(CSSNumericValue* x,
                                       CSSNumericValue* y,
                                       CSSNumericValue* z) {
  DCHECK(IsLengthOrPercent(x));
  DCHECK(IsLengthOrPercent(y));
  DCHECK_EQ(z->GetType(), CSSStyleValue::StyleValueType::kLengthType);
  DCHECK(!z->ContainsPercent());
  return new CSSTranslation(x, y, z, false /* is2D */);
}

CSSTranslation* CSSTranslation::FromCSSValue(const CSSFunctionValue& value) {
  switch (value.FunctionType()) {
    case CSSValueTranslateX:
    case CSSValueTranslateY:
    case CSSValueTranslateZ:
      return FromCSSTranslateXYZ(value);
    case CSSValueTranslate:
      return FromCSSTranslate(value);
    case CSSValueTranslate3d:
      return FromCSSTranslate3D(value);
    default:
      NOTREACHED();
      return nullptr;
  }
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
  if (z->GetType() != CSSStyleValue::StyleValueType::kLengthType) {
    exception_state.ThrowTypeError("Must pass length to Z of CSSTranslation");
    return;
  }
  if (z->ContainsPercent()) {
    exception_state.ThrowTypeError(
        "CSSTranslation does not support z CSSNumericValue with percent units");
    return;
  }
  z_ = z;
}

const DOMMatrix* CSSTranslation::AsMatrix() const {
  CSSUnitValue* x = x_->to(CSSPrimitiveValue::UnitType::kPixels);
  CSSUnitValue* y = y_->to(CSSPrimitiveValue::UnitType::kPixels);
  CSSUnitValue* z = z_->to(CSSPrimitiveValue::UnitType::kPixels);
  // TODO(meade): What should happen when the translation contains relative
  // lengths here?
  // https://github.com/w3c/css-houdini-drafts/issues/421
  if (!x || !y || !z)
    return nullptr;

  DOMMatrix* matrix = DOMMatrix::Create();
  return matrix->translate(x->value(), y->value(), z->value());
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
