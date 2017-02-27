// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/CSSMatrixComponent.h"

#include <cmath>
#include <memory>
#include "core/css/CSSPrimitiveValue.h"
#include "wtf/MathExtras.h"

namespace blink {

CSSFunctionValue* CSSMatrixComponent::toCSSValue() const {
  CSSFunctionValue* result =
      CSSFunctionValue::create(m_is2D ? CSSValueMatrix : CSSValueMatrix3d);

  if (m_is2D) {
    double values[6] = {a(), b(), c(), d(), e(), f()};
    for (double value : values) {
      result->append(*CSSPrimitiveValue::create(
          value, CSSPrimitiveValue::UnitType::Number));
    }
  } else {
    double values[16] = {m11(), m12(), m13(), m14(), m21(), m22(),
                         m23(), m24(), m31(), m32(), m33(), m34(),
                         m41(), m42(), m43(), m44()};
    for (double value : values) {
      result->append(*CSSPrimitiveValue::create(
          value, CSSPrimitiveValue::UnitType::Number));
    }
  }

  return result;
}

CSSMatrixComponent* CSSMatrixComponent::perspective(double length) {
  std::unique_ptr<TransformationMatrix> matrix = TransformationMatrix::create();
  if (length != 0)
    matrix->setM34(-1 / length);
  return new CSSMatrixComponent(std::move(matrix), PerspectiveType);
}

CSSMatrixComponent* CSSMatrixComponent::rotate(double angle) {
  std::unique_ptr<TransformationMatrix> matrix = TransformationMatrix::create();
  matrix->rotate(angle);
  return new CSSMatrixComponent(std::move(matrix), RotationType);
}

CSSMatrixComponent* CSSMatrixComponent::rotate3d(double angle,
                                                 double x,
                                                 double y,
                                                 double z) {
  std::unique_ptr<TransformationMatrix> matrix = TransformationMatrix::create();
  matrix->rotate3d(x, y, z, angle);
  return new CSSMatrixComponent(std::move(matrix), Rotation3DType);
}

CSSMatrixComponent* CSSMatrixComponent::scale(double x, double y) {
  std::unique_ptr<TransformationMatrix> matrix = TransformationMatrix::create();
  matrix->setM11(x);
  matrix->setM22(y);
  return new CSSMatrixComponent(std::move(matrix), ScaleType);
}

CSSMatrixComponent* CSSMatrixComponent::scale3d(double x, double y, double z) {
  std::unique_ptr<TransformationMatrix> matrix = TransformationMatrix::create();
  matrix->setM11(x);
  matrix->setM22(y);
  matrix->setM33(z);
  return new CSSMatrixComponent(std::move(matrix), Scale3DType);
}

CSSMatrixComponent* CSSMatrixComponent::skew(double ax, double ay) {
  double tanAx = std::tan(deg2rad(ax));
  double tanAy = std::tan(deg2rad(ay));

  std::unique_ptr<TransformationMatrix> matrix = TransformationMatrix::create();
  matrix->setM12(tanAy);
  matrix->setM21(tanAx);
  return new CSSMatrixComponent(std::move(matrix), SkewType);
}

CSSMatrixComponent* CSSMatrixComponent::translate(double x, double y) {
  std::unique_ptr<TransformationMatrix> matrix = TransformationMatrix::create();
  matrix->setM41(x);
  matrix->setM42(y);
  return new CSSMatrixComponent(std::move(matrix), TranslationType);
}

CSSMatrixComponent* CSSMatrixComponent::translate3d(double x,
                                                    double y,
                                                    double z) {
  std::unique_ptr<TransformationMatrix> matrix = TransformationMatrix::create();
  matrix->setM41(x);
  matrix->setM42(y);
  matrix->setM43(z);
  return new CSSMatrixComponent(std::move(matrix), Translation3DType);
}

}  // namespace blink
