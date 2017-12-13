// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/CSSRotation.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/css/CSSFunctionValue.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/cssom/CSSUnitValue.h"
#include "core/geometry/DOMMatrix.h"

namespace blink {

namespace {

bool IsValidCoordinate(const CSSNumericValue* value) {
  return value && value->Type().MatchesNumber();
}

bool IsValidAngle(const CSSNumericValue* value) {
  return value &&
         value->Type().MatchesBaseType(CSSNumericValueType::BaseType::kAngle);
}

CSSRotation* FromCSSRotate(const CSSFunctionValue& value) {
  DCHECK_EQ(value.length(), 1UL);
  CSSNumericValue* angle =
      CSSNumericValue::FromCSSValue(ToCSSPrimitiveValue(value.Item(0)));
  return CSSRotation::Create(angle);
}

CSSRotation* FromCSSRotate3d(const CSSFunctionValue& value) {
  DCHECK_EQ(value.length(), 4UL);

  CSSNumericValue* x =
      CSSNumericValue::FromCSSValue(ToCSSPrimitiveValue(value.Item(0)));
  CSSNumericValue* y =
      CSSNumericValue::FromCSSValue(ToCSSPrimitiveValue(value.Item(1)));
  CSSNumericValue* z =
      CSSNumericValue::FromCSSValue(ToCSSPrimitiveValue(value.Item(2)));
  CSSNumericValue* angle =
      CSSNumericValue::FromCSSValue(ToCSSPrimitiveValue(value.Item(3)));

  return CSSRotation::Create(x, y, z, angle);
}

CSSRotation* FromCSSRotateXYZ(const CSSFunctionValue& value) {
  DCHECK_EQ(value.length(), 1UL);

  CSSNumericValue* angle =
      CSSNumericValue::FromCSSValue(ToCSSPrimitiveValue(value.Item(0)));

  switch (value.FunctionType()) {
    case CSSValueRotateX:
      return CSSRotation::Create(CSSUnitValue::Create(1),
                                 CSSUnitValue::Create(0),
                                 CSSUnitValue::Create(0), angle);
    case CSSValueRotateY:
      return CSSRotation::Create(CSSUnitValue::Create(0),
                                 CSSUnitValue::Create(1),
                                 CSSUnitValue::Create(0), angle);
    case CSSValueRotateZ:
      return CSSRotation::Create(CSSUnitValue::Create(0),
                                 CSSUnitValue::Create(0),
                                 CSSUnitValue::Create(1), angle);
    default:
      NOTREACHED();
      return nullptr;
  }
}

}  // namespace

CSSRotation* CSSRotation::Create(CSSNumericValue* angle,
                                 ExceptionState& exception_state) {
  if (!IsValidAngle(angle)) {
    exception_state.ThrowTypeError("Must pass an angle to CSSRotation");
    return nullptr;
  }
  return new CSSRotation(CSSUnitValue::Create(0), CSSUnitValue::Create(0),
                         CSSUnitValue::Create(1), angle, true /* is2D */);
}

CSSRotation* CSSRotation::Create(const CSSNumberish& x,
                                 const CSSNumberish& y,
                                 const CSSNumberish& z,
                                 CSSNumericValue* angle,
                                 ExceptionState& exception_state) {
  CSSNumericValue* x_value = CSSNumericValue::FromNumberish(x);
  CSSNumericValue* y_value = CSSNumericValue::FromNumberish(y);
  CSSNumericValue* z_value = CSSNumericValue::FromNumberish(z);

  if (!IsValidCoordinate(x_value) || !IsValidCoordinate(y_value) ||
      !IsValidCoordinate(z_value)) {
    exception_state.ThrowTypeError("Must specify an number unit");
    return nullptr;
  }
  if (!IsValidAngle(angle)) {
    exception_state.ThrowTypeError("Must pass an angle to CSSRotation");
    return nullptr;
  }
  return new CSSRotation(x_value, y_value, z_value, angle, false /* is2D */);
}

CSSRotation* CSSRotation::Create(CSSNumericValue* angle) {
  return new CSSRotation(CSSUnitValue::Create(0), CSSUnitValue::Create(0),
                         CSSUnitValue::Create(1), angle, true /* is2D */);
}

CSSRotation* CSSRotation::Create(CSSNumericValue* x,
                                 CSSNumericValue* y,
                                 CSSNumericValue* z,
                                 CSSNumericValue* angle) {
  return new CSSRotation(x, y, z, angle, false /* is2D */);
}

CSSRotation* CSSRotation::FromCSSValue(const CSSFunctionValue& value) {
  switch (value.FunctionType()) {
    case CSSValueRotate:
      return FromCSSRotate(value);
    case CSSValueRotate3d:
      return FromCSSRotate3d(value);
    case CSSValueRotateX:
    case CSSValueRotateY:
    case CSSValueRotateZ:
      return FromCSSRotateXYZ(value);
    default:
      NOTREACHED();
      return nullptr;
  }
}

void CSSRotation::setAngle(CSSNumericValue* angle,
                           ExceptionState& exception_state) {
  if (!IsValidAngle(angle)) {
    exception_state.ThrowTypeError("Must pass an angle to CSSRotation");
    return;
  }
  angle_ = angle;
}

const DOMMatrix* CSSRotation::AsMatrix(ExceptionState& exception_state) const {
  CSSUnitValue* x = x_->to(CSSPrimitiveValue::UnitType::kNumber);
  CSSUnitValue* y = y_->to(CSSPrimitiveValue::UnitType::kNumber);
  CSSUnitValue* z = z_->to(CSSPrimitiveValue::UnitType::kNumber);
  if (!x || !y || !z) {
    exception_state.ThrowTypeError(
        "Cannot create matrix if units cannot be converted to CSSUnitValue");
    return nullptr;
  }

  DOMMatrix* matrix = DOMMatrix::Create();
  CSSUnitValue* angle = angle_->to(CSSPrimitiveValue::UnitType::kDegrees);
  if (is2D()) {
    matrix->rotateAxisAngleSelf(0, 0, 1, angle->value());
  } else {
    matrix->rotateAxisAngleSelf(x->value(), y->value(), z->value(),
                                angle->value());
  }
  return matrix;
}

const CSSFunctionValue* CSSRotation::ToCSSValue(
    SecureContextMode secure_context_mode) const {
  CSSUnitValue* x = x_->to(CSSPrimitiveValue::UnitType::kNumber);
  CSSUnitValue* y = y_->to(CSSPrimitiveValue::UnitType::kNumber);
  CSSUnitValue* z = z_->to(CSSPrimitiveValue::UnitType::kNumber);
  if (!x || !y || !z) {
    return nullptr;
  }
  CSSUnitValue* angle = ToCSSUnitValue(angle_);
  CSSFunctionValue* result =
      CSSFunctionValue::Create(is2D() ? CSSValueRotate : CSSValueRotate3d);
  if (!is2D()) {
    result->Append(*x->ToCSSValue(secure_context_mode));
    result->Append(*y->ToCSSValue(secure_context_mode));
    result->Append(*z->ToCSSValue(secure_context_mode));
  }
  result->Append(
      *CSSPrimitiveValue::Create(angle->value(), angle->GetInternalUnit()));
  return result;
}

void CSSRotation::setX(const CSSNumberish& x, ExceptionState& exception_state) {
  CSSNumericValue* value = CSSNumericValue::FromNumberish(x);
  if (!IsValidCoordinate(value)) {
    exception_state.ThrowTypeError("Must specify a number unit");
    return;
  }
  x_ = value;
}

void CSSRotation::setY(const CSSNumberish& y, ExceptionState& exception_state) {
  CSSNumericValue* value = CSSNumericValue::FromNumberish(y);
  if (!IsValidCoordinate(value)) {
    exception_state.ThrowTypeError("Must specify a number unit");
    return;
  }
  y_ = value;
}

void CSSRotation::setZ(const CSSNumberish& z, ExceptionState& exception_state) {
  CSSNumericValue* value = CSSNumericValue::FromNumberish(z);
  if (!IsValidCoordinate(value)) {
    exception_state.ThrowTypeError("Must specify a number unit");
    return;
  }
  z_ = value;
}

CSSRotation::CSSRotation(CSSNumericValue* x,
                         CSSNumericValue* y,
                         CSSNumericValue* z,
                         CSSNumericValue* angle,
                         bool is2D)
    : CSSTransformComponent(is2D), angle_(angle), x_(x), y_(y), z_(z) {
  DCHECK(IsValidCoordinate(x));
  DCHECK(IsValidCoordinate(y));
  DCHECK(IsValidCoordinate(z));
  DCHECK(IsValidAngle(angle));
}

}  // namespace blink
