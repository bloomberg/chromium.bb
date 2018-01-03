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

bool IsValidTranslateXY(const CSSNumericValue* value) {
  return value && value->Type().MatchesBaseTypePercentage(
                      CSSNumericValueType::BaseType::kLength);
}

bool IsValidTranslateZ(const CSSNumericValue* value) {
  return value &&
         value->Type().MatchesBaseType(CSSNumericValueType::BaseType::kLength);
}

CSSTranslation* FromCSSTranslate(const CSSFunctionValue& value) {
  DCHECK_GT(value.length(), 0UL);

  CSSNumericValue* x =
      CSSNumericValue::FromCSSValue(ToCSSPrimitiveValue(value.Item(0)));

  if (value.length() == 1) {
    return CSSTranslation::Create(
        x, CSSUnitValue::Create(0, CSSPrimitiveValue::UnitType::kPixels));
  }

  DCHECK_EQ(value.length(), 2UL);

  CSSNumericValue* y =
      CSSNumericValue::FromCSSValue(ToCSSPrimitiveValue(value.Item(1)));

  return CSSTranslation::Create(x, y);
}

CSSTranslation* FromCSSTranslateXYZ(const CSSFunctionValue& value) {
  DCHECK_EQ(value.length(), 1UL);

  CSSNumericValue* length =
      CSSNumericValue::FromCSSValue(ToCSSPrimitiveValue(value.Item(0)));

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

  CSSNumericValue* x =
      CSSNumericValue::FromCSSValue(ToCSSPrimitiveValue(value.Item(0)));
  CSSNumericValue* y =
      CSSNumericValue::FromCSSValue(ToCSSPrimitiveValue(value.Item(1)));
  CSSNumericValue* z =
      CSSNumericValue::FromCSSValue(ToCSSPrimitiveValue(value.Item(2)));

  return CSSTranslation::Create(x, y, z);
}

}  // namespace

CSSTranslation* CSSTranslation::Create(CSSNumericValue* x,
                                       CSSNumericValue* y,
                                       ExceptionState& exception_state) {
  if (!IsValidTranslateXY(x) || !IsValidTranslateXY(y)) {
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
  if (!IsValidTranslateXY(x) || !IsValidTranslateXY(y) ||
      !IsValidTranslateZ(z)) {
    exception_state.ThrowTypeError(
        "Must pass length or percentage to X, Y and Z of CSSTranslation");
    return nullptr;
  }
  return new CSSTranslation(x, y, z, false /* is2D */);
}

CSSTranslation* CSSTranslation::Create(CSSNumericValue* x, CSSNumericValue* y) {
  return new CSSTranslation(
      x, y, CSSUnitValue::Create(0, CSSPrimitiveValue::UnitType::kPixels),
      true /* is2D */);
}

CSSTranslation* CSSTranslation::Create(CSSNumericValue* x,
                                       CSSNumericValue* y,
                                       CSSNumericValue* z) {
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
  if (!IsValidTranslateXY(x)) {
    exception_state.ThrowTypeError(
        "Must pass length or percentage to X of CSSTranslation");
    return;
  }
  x_ = x;
}

void CSSTranslation::setY(CSSNumericValue* y, ExceptionState& exception_state) {
  if (!IsValidTranslateXY(y)) {
    exception_state.ThrowTypeError(
        "Must pass length or percent to Y of CSSTranslation");
    return;
  }
  y_ = y;
}

void CSSTranslation::setZ(CSSNumericValue* z, ExceptionState& exception_state) {
  if (!IsValidTranslateZ(z)) {
    exception_state.ThrowTypeError("Must pass length to Z of CSSTranslation");
    return;
  }
  z_ = z;
}

const DOMMatrix* CSSTranslation::AsMatrix(
    ExceptionState& exception_state) const {
  CSSUnitValue* x = x_->to(CSSPrimitiveValue::UnitType::kPixels);
  CSSUnitValue* y = y_->to(CSSPrimitiveValue::UnitType::kPixels);
  CSSUnitValue* z = z_->to(CSSPrimitiveValue::UnitType::kPixels);

  if (!x || !y || !z) {
    exception_state.ThrowTypeError(
        "Cannot create matrix if units are not compatible with px");
    return nullptr;
  }

  DOMMatrix* matrix = DOMMatrix::Create();
  if (is2D())
    matrix->translateSelf(x->value(), y->value());
  else
    matrix->translateSelf(x->value(), y->value(), z->value());

  return matrix;
}

const CSSFunctionValue* CSSTranslation::ToCSSValue() const {
  const CSSValue* x = x_->ToCSSValue();
  const CSSValue* y = y_->ToCSSValue();

  CSSFunctionValue* result = CSSFunctionValue::Create(
      is2D() ? CSSValueTranslate : CSSValueTranslate3d);
  result->Append(*x);
  result->Append(*y);
  if (!is2D()) {
    const CSSValue* z = z_->ToCSSValue();
    result->Append(*z);
  }
  return result;
}

CSSTranslation::CSSTranslation(CSSNumericValue* x,
                               CSSNumericValue* y,
                               CSSNumericValue* z,
                               bool is2D)
    : CSSTransformComponent(is2D), x_(x), y_(y), z_(z) {
  DCHECK(IsValidTranslateXY(x));
  DCHECK(IsValidTranslateXY(y));
  DCHECK(IsValidTranslateZ(z));
}

}  // namespace blink
