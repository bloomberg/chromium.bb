// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/CSSMatrixComponent.h"

#include <cmath>
#include <memory>
#include "core/css/CSSPrimitiveValue.h"
#include "platform/wtf/MathExtras.h"

namespace blink {

CSSFunctionValue* CSSMatrixComponent::ToCSSValue() const {
  CSSFunctionValue* result =
      CSSFunctionValue::Create(is2d_ ? CSSValueMatrix : CSSValueMatrix3d);

  if (is2d_) {
    double values[6] = {a(), b(), c(), d(), e(), f()};
    for (double value : values) {
      result->Append(*CSSPrimitiveValue::Create(
          value, CSSPrimitiveValue::UnitType::kNumber));
    }
  } else {
    double values[16] = {m11(), m12(), m13(), m14(), m21(), m22(),
                         m23(), m24(), m31(), m32(), m33(), m34(),
                         m41(), m42(), m43(), m44()};
    for (double value : values) {
      result->Append(*CSSPrimitiveValue::Create(
          value, CSSPrimitiveValue::UnitType::kNumber));
    }
  }

  return result;
}

CSSMatrixComponent* CSSMatrixComponent::Perspective(double length) {
  std::unique_ptr<TransformationMatrix> matrix = TransformationMatrix::Create();
  if (length != 0)
    matrix->SetM34(-1 / length);
  return new CSSMatrixComponent(std::move(matrix), kPerspectiveType);
}

CSSMatrixComponent* CSSMatrixComponent::Rotate(double angle) {
  std::unique_ptr<TransformationMatrix> matrix = TransformationMatrix::Create();
  matrix->Rotate(angle);
  return new CSSMatrixComponent(std::move(matrix), kRotationType);
}

CSSMatrixComponent* CSSMatrixComponent::Rotate3d(double angle,
                                                 double x,
                                                 double y,
                                                 double z) {
  std::unique_ptr<TransformationMatrix> matrix = TransformationMatrix::Create();
  matrix->Rotate3d(x, y, z, angle);
  return new CSSMatrixComponent(std::move(matrix), kRotation3DType);
}

CSSMatrixComponent* CSSMatrixComponent::Scale(double x, double y) {
  std::unique_ptr<TransformationMatrix> matrix = TransformationMatrix::Create();
  matrix->SetM11(x);
  matrix->SetM22(y);
  return new CSSMatrixComponent(std::move(matrix), kScaleType);
}

CSSMatrixComponent* CSSMatrixComponent::Scale3d(double x, double y, double z) {
  std::unique_ptr<TransformationMatrix> matrix = TransformationMatrix::Create();
  matrix->SetM11(x);
  matrix->SetM22(y);
  matrix->SetM33(z);
  return new CSSMatrixComponent(std::move(matrix), kScale3DType);
}

CSSMatrixComponent* CSSMatrixComponent::Skew(double ax, double ay) {
  double tan_ax = std::tan(deg2rad(ax));
  double tan_ay = std::tan(deg2rad(ay));

  std::unique_ptr<TransformationMatrix> matrix = TransformationMatrix::Create();
  matrix->SetM12(tan_ay);
  matrix->SetM21(tan_ax);
  return new CSSMatrixComponent(std::move(matrix), kSkewType);
}

CSSMatrixComponent* CSSMatrixComponent::Translate(double x, double y) {
  std::unique_ptr<TransformationMatrix> matrix = TransformationMatrix::Create();
  matrix->SetM41(x);
  matrix->SetM42(y);
  return new CSSMatrixComponent(std::move(matrix), kTranslationType);
}

CSSMatrixComponent* CSSMatrixComponent::Translate3d(double x,
                                                    double y,
                                                    double z) {
  std::unique_ptr<TransformationMatrix> matrix = TransformationMatrix::Create();
  matrix->SetM41(x);
  matrix->SetM42(y);
  matrix->SetM43(z);
  return new CSSMatrixComponent(std::move(matrix), kTranslation3DType);
}

}  // namespace blink
