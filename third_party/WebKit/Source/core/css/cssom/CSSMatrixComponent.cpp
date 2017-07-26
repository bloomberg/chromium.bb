// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/CSSMatrixComponent.h"

#include "core/css/CSSPrimitiveValue.h"
#include "core/css/cssom/CSSMatrixComponentOptions.h"
#include "core/geometry/DOMMatrix.h"
#include "platform/wtf/MathExtras.h"

namespace blink {

namespace {

DOMMatrix* To2DMatrix(DOMMatrixReadOnly* matrix) {
  DOMMatrix* twoDimensionalMatrix = DOMMatrix::Create();
  twoDimensionalMatrix->setA(matrix->m11());
  twoDimensionalMatrix->setB(matrix->m12());
  twoDimensionalMatrix->setC(matrix->m21());
  twoDimensionalMatrix->setD(matrix->m22());
  twoDimensionalMatrix->setE(matrix->m41());
  twoDimensionalMatrix->setF(matrix->m42());
  return twoDimensionalMatrix;
}

}  // namespace

CSSMatrixComponent* CSSMatrixComponent::Create(
    DOMMatrixReadOnly* matrix,
    const CSSMatrixComponentOptions& options) {
  return new CSSMatrixComponent(matrix, options.is2D() || matrix->is2D());
}

const DOMMatrix* CSSMatrixComponent::AsMatrix(ExceptionState&) const {
  if (is2D() && !matrix_->is2D())
    return To2DMatrix(matrix_);

  return matrix_.Get();
}

const CSSFunctionValue* CSSMatrixComponent::ToCSSValue() const {
  CSSFunctionValue* result =
      CSSFunctionValue::Create(is2D() ? CSSValueMatrix : CSSValueMatrix3d);

  if (is2D()) {
    double values[6] = {matrix_->a(), matrix_->b(), matrix_->c(),
                        matrix_->d(), matrix_->e(), matrix_->f()};
    for (double value : values) {
      result->Append(*CSSPrimitiveValue::Create(
          value, CSSPrimitiveValue::UnitType::kNumber));
    }
  } else {
    double values[16] = {
        matrix_->m11(), matrix_->m12(), matrix_->m13(), matrix_->m14(),
        matrix_->m21(), matrix_->m22(), matrix_->m23(), matrix_->m24(),
        matrix_->m31(), matrix_->m32(), matrix_->m33(), matrix_->m34(),
        matrix_->m41(), matrix_->m42(), matrix_->m43(), matrix_->m44()};
    for (double value : values) {
      result->Append(*CSSPrimitiveValue::Create(
          value, CSSPrimitiveValue::UnitType::kNumber));
    }
  }

  return result;
}

}  // namespace blink
