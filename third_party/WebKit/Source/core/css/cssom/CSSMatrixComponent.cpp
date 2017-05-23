// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/CSSMatrixComponent.h"

#include <cmath>
#include <memory>
#include "core/css/CSSPrimitiveValue.h"
#include "platform/wtf/MathExtras.h"

namespace blink {

CSSMatrixComponent* CSSMatrixComponent::Create(DOMMatrixReadOnly* matrix) {
  return new CSSMatrixComponent(matrix);
}

CSSFunctionValue* CSSMatrixComponent::ToCSSValue() const {
  CSSFunctionValue* result =
      CSSFunctionValue::Create(is2d_ ? CSSValueMatrix : CSSValueMatrix3d);

  if (is2d_) {
    double values[6] = {matrix()->a(), matrix()->b(), matrix()->c(),
                        matrix()->d(), matrix()->e(), matrix()->f()};
    for (double value : values) {
      result->Append(*CSSPrimitiveValue::Create(
          value, CSSPrimitiveValue::UnitType::kNumber));
    }
  } else {
    double values[16] = {
        matrix()->m11(), matrix()->m12(), matrix()->m13(), matrix()->m14(),
        matrix()->m21(), matrix()->m22(), matrix()->m23(), matrix()->m24(),
        matrix()->m31(), matrix()->m32(), matrix()->m33(), matrix()->m34(),
        matrix()->m41(), matrix()->m42(), matrix()->m43(), matrix()->m44()};
    for (double value : values) {
      result->Append(*CSSPrimitiveValue::Create(
          value, CSSPrimitiveValue::UnitType::kNumber));
    }
  }

  return result;
}

CSSMatrixComponent* CSSMatrixComponent::Perspective(double length) {
  DOMMatrixInit init;
  DOMMatrix* matrix = DOMMatrix::fromMatrix(init, ASSERT_NO_EXCEPTION);

  if (length != 0)
    matrix->setM34(-1 / length);
  return new CSSMatrixComponent(matrix, kPerspectiveType);
}

CSSMatrixComponent* CSSMatrixComponent::Rotate(double angle) {
  DOMMatrixInit init;
  DOMMatrix* matrix = DOMMatrix::fromMatrix(init, ASSERT_NO_EXCEPTION);

  matrix->rotateSelf(angle);
  return new CSSMatrixComponent(matrix, kRotationType);
}

CSSMatrixComponent* CSSMatrixComponent::Rotate3d(double angle,
                                                 double x,
                                                 double y,
                                                 double z) {
  DOMMatrixInit init;
  DOMMatrix* matrix = DOMMatrix::fromMatrix(init, ASSERT_NO_EXCEPTION);

  matrix->rotateAxisAngleSelf(x, y, z, angle);
  return new CSSMatrixComponent(matrix, kRotation3DType);
}

CSSMatrixComponent* CSSMatrixComponent::Scale(double x, double y) {
  DOMMatrixInit init;
  DOMMatrix* matrix = DOMMatrix::fromMatrix(init, ASSERT_NO_EXCEPTION);

  matrix->setM11(x);
  matrix->setM22(y);
  return new CSSMatrixComponent(matrix, kScaleType);
}

CSSMatrixComponent* CSSMatrixComponent::Scale3d(double x, double y, double z) {
  DOMMatrixInit init;
  DOMMatrix* matrix = DOMMatrix::fromMatrix(init, ASSERT_NO_EXCEPTION);

  matrix->setM11(x);
  matrix->setM22(y);
  matrix->setM33(z);
  return new CSSMatrixComponent(matrix, kScale3DType);
}

CSSMatrixComponent* CSSMatrixComponent::Skew(double ax, double ay) {
  double tan_ax = std::tan(deg2rad(ax));
  double tan_ay = std::tan(deg2rad(ay));

  DOMMatrixInit init;
  DOMMatrix* matrix = DOMMatrix::fromMatrix(init, ASSERT_NO_EXCEPTION);

  matrix->setM12(tan_ay);
  matrix->setM21(tan_ax);
  return new CSSMatrixComponent(matrix, kSkewType);
}

CSSMatrixComponent* CSSMatrixComponent::Translate(double x, double y) {
  DOMMatrixInit init;
  DOMMatrix* matrix = DOMMatrix::fromMatrix(init, ASSERT_NO_EXCEPTION);

  matrix->setM41(x);
  matrix->setM42(y);
  return new CSSMatrixComponent(matrix, kTranslationType);
}

CSSMatrixComponent* CSSMatrixComponent::Translate3d(double x,
                                                    double y,
                                                    double z) {
  DOMMatrixInit init;
  DOMMatrix* matrix = DOMMatrix::fromMatrix(init, ASSERT_NO_EXCEPTION);

  matrix->setM41(x);
  matrix->setM42(y);
  matrix->setM43(z);
  return new CSSMatrixComponent(matrix, kTranslation3DType);
}

CSSMatrixComponent::CSSMatrixComponent(DOMMatrixReadOnly* matrix)
    : CSSTransformComponent() {
  matrix_ = DOMMatrix::Create(matrix);
  is2d_ = matrix->is2D();
}

CSSMatrixComponent::CSSMatrixComponent(DOMMatrixReadOnly* matrix,
                                       TransformComponentType from_type)
    : CSSTransformComponent() {
  matrix_ = DOMMatrix::Create(matrix);
  is2d_ = Is2DComponentType(from_type);
}

}  // namespace blink
