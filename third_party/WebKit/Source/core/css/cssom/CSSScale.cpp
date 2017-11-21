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
  double x = ToCSSPrimitiveValue(value.Item(0)).GetDoubleValue();
  if (value.length() == 1U) {
    return CSSScale::Create(CSSUnitValue::Create(x), CSSUnitValue::Create(x));
  }

  double y = ToCSSPrimitiveValue(value.Item(1)).GetDoubleValue();
  return CSSScale::Create(CSSUnitValue::Create(x), CSSUnitValue::Create(y));
}

CSSScale* FromScaleXYZ(const CSSFunctionValue& value) {
  DCHECK_EQ(value.length(), 1U);

  CSSNumericValue* numeric_value =
      CSSUnitValue::Create(ToCSSPrimitiveValue(value.Item(0)).GetDoubleValue());
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
  double x = ToCSSPrimitiveValue(value.Item(0)).GetDoubleValue();
  double y = ToCSSPrimitiveValue(value.Item(1)).GetDoubleValue();
  double z = ToCSSPrimitiveValue(value.Item(2)).GetDoubleValue();

  return CSSScale::Create(CSSUnitValue::Create(x), CSSUnitValue::Create(y),
                          CSSUnitValue::Create(z));
}

}  // namespace

CSSScale* CSSScale::Create(const CSSNumberish& x,
                           const CSSNumberish& y,
                           ExceptionState& exception_state) {
  CSSNumericValue* x_value = CSSNumericValue::FromNumberish(x);
  CSSNumericValue* y_value = CSSNumericValue::FromNumberish(y);

  if (!isCoordValid(x_value) || !isCoordValid(y_value)) {
    exception_state.ThrowTypeError("Must specify an number unit");
    return nullptr;
  }

  return new CSSScale(x_value, y_value);
}

CSSScale* CSSScale::Create(const CSSNumberish& x,
                           const CSSNumberish& y,
                           const CSSNumberish& z,
                           ExceptionState& exception_state) {
  CSSNumericValue* x_value = CSSNumericValue::FromNumberish(x);
  CSSNumericValue* y_value = CSSNumericValue::FromNumberish(y);
  CSSNumericValue* z_value = CSSNumericValue::FromNumberish(z);

  if (!isCoordValid(x_value) || !isCoordValid(y_value) ||
      !isCoordValid(z_value)) {
    exception_state.ThrowTypeError("Must specify a number unit");
    return nullptr;
  }

  return new CSSScale(x_value, y_value, z_value);
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

  if (!isCoordValid(value)) {
    exception_state.ThrowTypeError("Must specify a number unit");
    return;
  }

  x_ = value;
}
void CSSScale::setY(const CSSNumberish& y, ExceptionState& exception_state) {
  CSSNumericValue* value = CSSNumericValue::FromNumberish(y);

  if (!isCoordValid(value)) {
    exception_state.ThrowTypeError("Must specify a number unit");
    return;
  }

  y_ = value;
}
void CSSScale::setZ(const CSSNumberish& z, ExceptionState& exception_state) {
  CSSNumericValue* value = CSSNumericValue::FromNumberish(z);

  if (!isCoordValid(value)) {
    exception_state.ThrowTypeError("Must specify a number unit");
    return;
  }

  z_ = value;
}

bool CSSScale::isCoordValid(CSSNumericValue* value) {
  // TODO: CSSMathValue are not supported yet
  if (!value->IsUnitValue()) {
    return false;
  }

  if (value->GetType() != CSSStyleValue::StyleValueType::kNumberType) {
    return false;
  }

  return true;
}

const DOMMatrix* CSSScale::AsMatrix(ExceptionState& exception_state) const {
  CSSUnitValue* x = x_->to(CSSPrimitiveValue::UnitType::kNumber);
  CSSUnitValue* y = y_->to(CSSPrimitiveValue::UnitType::kNumber);
  CSSUnitValue* z = z_->to(CSSPrimitiveValue::UnitType::kNumber);

  if (!x || !y || !z) {
    exception_state.ThrowTypeError(
        "Cannot create matrix if valuse are not convert to CSSUnitValue");
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

}  // namespace blink
