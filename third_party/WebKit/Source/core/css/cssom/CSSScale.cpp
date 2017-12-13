// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/CSSScale.h"

#include "core/css/CSSPrimitiveValue.h"
#include "core/css/cssom/CSSNumericValue.h"

namespace blink {

namespace {

CSSScale* FromScale(const CSSFunctionValue& value) {
  DCHECK(value.length() == 1U || value.length() == 2U);
  CSSNumericValue* x =
      CSSNumericValue::FromCSSValue(ToCSSPrimitiveValue(value.Item(0)));
  if (value.length() == 1U) {
    return CSSScale::Create(x, x);
  }

  CSSNumericValue* y =
      CSSNumericValue::FromCSSValue(ToCSSPrimitiveValue(value.Item(1)));
  return CSSScale::Create(x, y);
}

CSSScale* FromScaleXYZ(const CSSFunctionValue& value) {
  DCHECK_EQ(value.length(), 1U);

  CSSNumericValue* numeric_value =
      CSSNumericValue::FromCSSValue(ToCSSPrimitiveValue(value.Item(0)));
  CSSUnitValue* default_value = CSSUnitValue::Create(1);
  switch (value.FunctionType()) {
    case CSSValueScaleX:
      return CSSScale::Create(numeric_value, default_value);
    case CSSValueScaleY:
      return CSSScale::Create(default_value, numeric_value);
    case CSSValueScaleZ:
      return CSSScale::Create(default_value, default_value, numeric_value);
    default:
      NOTREACHED();
      return nullptr;
  }
}

CSSScale* FromScale3d(const CSSFunctionValue& value) {
  DCHECK_EQ(value.length(), 3U);

  CSSNumericValue* x =
      CSSNumericValue::FromCSSValue(ToCSSPrimitiveValue(value.Item(0)));
  CSSNumericValue* y =
      CSSNumericValue::FromCSSValue(ToCSSPrimitiveValue(value.Item(1)));
  CSSNumericValue* z =
      CSSNumericValue::FromCSSValue(ToCSSPrimitiveValue(value.Item(2)));

  return CSSScale::Create(x, y, z);
}

}  // namespace

CSSScale* CSSScale::Create(const CSSNumberish& x,
                           const CSSNumberish& y,
                           ExceptionState& exception_state) {
  CSSNumericValue* x_value = CSSNumericValue::FromNumberish(x);
  CSSNumericValue* y_value = CSSNumericValue::FromNumberish(y);

  if (!IsValidCoordinate(x_value) || !IsValidCoordinate(y_value)) {
    exception_state.ThrowTypeError("Must specify an number unit");
    return nullptr;
  }

  return CSSScale::Create(x_value, y_value);
}

CSSScale* CSSScale::Create(const CSSNumberish& x,
                           const CSSNumberish& y,
                           const CSSNumberish& z,
                           ExceptionState& exception_state) {
  CSSNumericValue* x_value = CSSNumericValue::FromNumberish(x);
  CSSNumericValue* y_value = CSSNumericValue::FromNumberish(y);
  CSSNumericValue* z_value = CSSNumericValue::FromNumberish(z);

  if (!IsValidCoordinate(x_value) || !IsValidCoordinate(y_value) ||
      !IsValidCoordinate(z_value)) {
    exception_state.ThrowTypeError("Must specify a number for X, Y and Z");
    return nullptr;
  }

  return CSSScale::Create(x_value, y_value, z_value);
}

CSSScale* CSSScale::FromCSSValue(const CSSFunctionValue& value) {
  switch (value.FunctionType()) {
    case CSSValueScale:
      return FromScale(value);
    case CSSValueScaleX:
    case CSSValueScaleY:
    case CSSValueScaleZ:
      return FromScaleXYZ(value);
    case CSSValueScale3d:
      return FromScale3d(value);
    default:
      NOTREACHED();
      return nullptr;
  }
}

void CSSScale::setX(const CSSNumberish& x, ExceptionState& exception_state) {
  CSSNumericValue* value = CSSNumericValue::FromNumberish(x);

  if (!IsValidCoordinate(value)) {
    exception_state.ThrowTypeError("Must specify a number unit");
    return;
  }

  x_ = value;
}

void CSSScale::setY(const CSSNumberish& y, ExceptionState& exception_state) {
  CSSNumericValue* value = CSSNumericValue::FromNumberish(y);

  if (!IsValidCoordinate(value)) {
    exception_state.ThrowTypeError("Must specify a number unit");
    return;
  }

  y_ = value;
}

void CSSScale::setZ(const CSSNumberish& z, ExceptionState& exception_state) {
  CSSNumericValue* value = CSSNumericValue::FromNumberish(z);

  if (!IsValidCoordinate(value)) {
    exception_state.ThrowTypeError("Must specify a number unit");
    return;
  }

  z_ = value;
}

const DOMMatrix* CSSScale::AsMatrix(ExceptionState& exception_state) const {
  CSSUnitValue* x = x_->to(CSSPrimitiveValue::UnitType::kNumber);
  CSSUnitValue* y = y_->to(CSSPrimitiveValue::UnitType::kNumber);
  CSSUnitValue* z = z_->to(CSSPrimitiveValue::UnitType::kNumber);

  if (!x || !y || !z) {
    exception_state.ThrowTypeError(
        "Cannot create matrix if values are not numbers");
    return nullptr;
  }

  DOMMatrix* result = DOMMatrix::Create();
  return result->scaleSelf(x->value(), y->value(), z->value());
}

const CSSFunctionValue* CSSScale::ToCSSValue(
    SecureContextMode secure_context_mode) const {
  CSSFunctionValue* result =
      CSSFunctionValue::Create(is2D() ? CSSValueScale : CSSValueScale3d);
  result->Append(*x_->ToCSSValue(secure_context_mode));
  result->Append(*y_->ToCSSValue(secure_context_mode));
  if (!is2D()) {
    result->Append(*z_->ToCSSValue(secure_context_mode));
  }
  return result;
}

CSSScale::CSSScale(CSSNumericValue* x,
                   CSSNumericValue* y,
                   CSSNumericValue* z,
                   bool is2D)
    : CSSTransformComponent(is2D), x_(x), y_(y), z_(z) {
  DCHECK(IsValidCoordinate(x));
  DCHECK(IsValidCoordinate(y));
  DCHECK(IsValidCoordinate(z));
}

// static
bool CSSScale::IsValidCoordinate(CSSNumericValue* coord) {
  return coord && coord->Type().MatchesNumber();
}

}  // namespace blink
