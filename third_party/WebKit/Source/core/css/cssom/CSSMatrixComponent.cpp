// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/CSSMatrixComponent.h"

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

CSSMatrixComponent::CSSMatrixComponent(DOMMatrixReadOnly* matrix)
    : CSSTransformComponent() {
  matrix_ = DOMMatrix::Create(matrix);
  is2d_ = matrix->is2D();
}

}  // namespace blink
