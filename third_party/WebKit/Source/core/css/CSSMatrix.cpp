/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/css/CSSMatrix.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/CSSPropertyNames.h"
#include "core/CSSValueKeywords.h"
#include "core/css/CSSIdentifierValue.h"
#include "core/css/CSSToLengthConversionData.h"
#include "core/css/StylePropertySet.h"
#include "core/css/parser/CSSParser.h"
#include "core/css/resolver/TransformBuilder.h"
#include "core/dom/ExceptionCode.h"
#include "core/frame/UseCounter.h"
#include "core/layout/api/LayoutViewItem.h"
#include "core/style/ComputedStyle.h"
#include "platform/wtf/MathExtras.h"

namespace blink {

CSSMatrix* CSSMatrix::Create(ExecutionContext* execution_context,
                             const String& s,
                             ExceptionState& exception_state) {
  return new CSSMatrix(s, exception_state);
}

CSSMatrix::CSSMatrix(const TransformationMatrix& m)
    : matrix_(TransformationMatrix::Create(m)) {}

CSSMatrix::CSSMatrix(const String& s, ExceptionState& exception_state)
    : matrix_(TransformationMatrix::Create()) {
  setMatrixValue(s, exception_state);
}

static inline PassRefPtr<ComputedStyle> CreateInitialStyle() {
  RefPtr<ComputedStyle> initial_style = ComputedStyle::Create();
  initial_style->GetFont().Update(nullptr);
  return initial_style;
}

void CSSMatrix::setMatrixValue(const String& string,
                               ExceptionState& exception_state) {
  if (string.IsEmpty())
    return;

  if (const CSSValue* value =
          CSSParser::ParseSingleValue(CSSPropertyTransform, string)) {
    // Check for a "none" transform. In these cases we can use the default
    // identity matrix.
    if (value->IsIdentifierValue() &&
        (ToCSSIdentifierValue(value))->GetValueID() == CSSValueNone)
      return;

    DEFINE_STATIC_REF(ComputedStyle, initial_style, CreateInitialStyle());
    TransformOperations operations =
        TransformBuilder::CreateTransformOperations(
            *value, CSSToLengthConversionData(initial_style, initial_style,
                                              LayoutViewItem(nullptr), 1.0f));

    // Convert transform operations to a TransformationMatrix. This can fail
    // if a param has a percentage ('%')
    if (operations.DependsOnBoxSize()) {
      exception_state.ThrowDOMException(kSyntaxError,
                                        "The transformation depends on the box "
                                        "size, which is not supported.");
    }
    matrix_ = TransformationMatrix::Create();
    operations.Apply(FloatSize(0, 0), *matrix_);
  } else {  // There is something there but parsing failed.
    exception_state.ThrowDOMException(kSyntaxError,
                                      "Failed to parse '" + string + "'.");
  }
}

// Perform a concatenation of the matrices (this * secondMatrix)
CSSMatrix* CSSMatrix::multiply(CSSMatrix* second_matrix) const {
  if (!second_matrix)
    return nullptr;

  return CSSMatrix::Create(
      TransformationMatrix(*matrix_).Multiply(*second_matrix->matrix_));
}

CSSMatrix* CSSMatrix::inverse(ExceptionState& exception_state) const {
  if (!matrix_->IsInvertible()) {
    exception_state.ThrowDOMException(kNotSupportedError,
                                      "The matrix is not invertable.");
    return nullptr;
  }

  return CSSMatrix::Create(matrix_->Inverse());
}

CSSMatrix* CSSMatrix::translate(double x, double y, double z) const {
  if (std::isnan(x))
    x = 0;
  if (std::isnan(y))
    y = 0;
  if (std::isnan(z))
    z = 0;
  return CSSMatrix::Create(TransformationMatrix(*matrix_).Translate3d(x, y, z));
}

CSSMatrix* CSSMatrix::scale(double scale_x,
                            double scale_y,
                            double scale_z) const {
  if (std::isnan(scale_x))
    scale_x = 1;
  if (std::isnan(scale_y))
    scale_y = scale_x;
  if (std::isnan(scale_z))
    scale_z = 1;
  return CSSMatrix::Create(
      TransformationMatrix(*matrix_).Scale3d(scale_x, scale_y, scale_z));
}

CSSMatrix* CSSMatrix::rotate(double rot_x, double rot_y, double rot_z) const {
  if (std::isnan(rot_x))
    rot_x = 0;

  if (std::isnan(rot_y) && std::isnan(rot_z)) {
    rot_z = rot_x;
    rot_x = 0;
    rot_y = 0;
  }

  if (std::isnan(rot_y))
    rot_y = 0;
  if (std::isnan(rot_z))
    rot_z = 0;
  return CSSMatrix::Create(
      TransformationMatrix(*matrix_).Rotate3d(rot_x, rot_y, rot_z));
}

CSSMatrix* CSSMatrix::rotateAxisAngle(double x,
                                      double y,
                                      double z,
                                      double angle) const {
  if (std::isnan(x))
    x = 0;
  if (std::isnan(y))
    y = 0;
  if (std::isnan(z))
    z = 0;
  if (std::isnan(angle))
    angle = 0;
  if (!x && !y && !z)
    z = 1;
  return CSSMatrix::Create(
      TransformationMatrix(*matrix_).Rotate3d(x, y, z, angle));
}

CSSMatrix* CSSMatrix::skewX(double angle) const {
  if (std::isnan(angle))
    angle = 0;
  return CSSMatrix::Create(TransformationMatrix(*matrix_).SkewX(angle));
}

CSSMatrix* CSSMatrix::skewY(double angle) const {
  if (std::isnan(angle))
    angle = 0;
  return CSSMatrix::Create(TransformationMatrix(*matrix_).SkewY(angle));
}

String CSSMatrix::toString() const {
  // FIXME - Need to ensure valid CSS floating point values
  // (https://bugs.webkit.org/show_bug.cgi?id=20674)
  if (matrix_->IsAffine()) {
    return String::Format("matrix(%g, %g, %g, %g, %g, %g)", matrix_->A(),
                          matrix_->B(), matrix_->C(), matrix_->D(),
                          matrix_->E(), matrix_->F());
  }
  return String::Format(
      "matrix3d(%g, %g, %g, %g, %g, %g, %g, %g, %g, %g, %g, %g, %g, %g, %g, "
      "%g)",
      matrix_->M11(), matrix_->M12(), matrix_->M13(), matrix_->M14(),
      matrix_->M21(), matrix_->M22(), matrix_->M23(), matrix_->M24(),
      matrix_->M31(), matrix_->M32(), matrix_->M33(), matrix_->M34(),
      matrix_->M41(), matrix_->M42(), matrix_->M43(), matrix_->M44());
}

}  // namespace blink
