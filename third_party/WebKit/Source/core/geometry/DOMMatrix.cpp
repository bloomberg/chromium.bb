// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/geometry/DOMMatrix.h"

#include "core/dom/ExecutionContext.h"

namespace blink {

DOMMatrix* DOMMatrix::Create() {
  return new DOMMatrix(TransformationMatrix());
}

DOMMatrix* DOMMatrix::Create(ExecutionContext* execution_context,
                             ExceptionState& exception_state) {
  return new DOMMatrix(TransformationMatrix());
}

DOMMatrix* DOMMatrix::Create(ExecutionContext* execution_context,
                             StringOrUnrestrictedDoubleSequence& init,
                             ExceptionState& exception_state) {
  if (init.isString()) {
    if (!execution_context->IsDocument()) {
      exception_state.ThrowTypeError(
          "DOMMatrix can't be constructed with strings on workers.");
      return nullptr;
    }

    DOMMatrix* matrix = new DOMMatrix(TransformationMatrix());
    matrix->SetMatrixValueFromString(init.getAsString(), exception_state);
    return matrix;
  }

  if (init.isUnrestrictedDoubleSequence()) {
    const Vector<double>& sequence = init.getAsUnrestrictedDoubleSequence();
    if (sequence.size() != 6 && sequence.size() != 16) {
      exception_state.ThrowTypeError(
          "The sequence must contain 6 elements for a 2D matrix or 16 elements "
          "for a 3D matrix.");
      return nullptr;
    }
    return new DOMMatrix(sequence, sequence.size());
  }

  NOTREACHED();
  return nullptr;
}

DOMMatrix* DOMMatrix::Create(DOMMatrixReadOnly* other,
                             ExceptionState& exception_state) {
  return new DOMMatrix(other->Matrix(), other->is2D());
}

DOMMatrix* DOMMatrix::Create(const SkMatrix44& matrix,
                             ExceptionState& exception_state) {
  TransformationMatrix transformation_matrix(matrix);
  return new DOMMatrix(transformation_matrix, transformation_matrix.IsAffine());
}

DOMMatrix* DOMMatrix::CreateForSerialization(double sequence[], int size) {
  return new DOMMatrix(sequence, size);
}

DOMMatrix* DOMMatrix::fromFloat32Array(NotShared<DOMFloat32Array> float32_array,
                                       ExceptionState& exception_state) {
  if (float32_array.View()->length() != 6 &&
      float32_array.View()->length() != 16) {
    exception_state.ThrowTypeError(
        "The sequence must contain 6 elements for a 2D matrix or 16 elements "
        "for a 3D matrix.");
    return nullptr;
  }
  return new DOMMatrix(float32_array.View()->Data(),
                       float32_array.View()->length());
}

DOMMatrix* DOMMatrix::fromFloat64Array(NotShared<DOMFloat64Array> float64_array,
                                       ExceptionState& exception_state) {
  if (float64_array.View()->length() != 6 &&
      float64_array.View()->length() != 16) {
    exception_state.ThrowTypeError(
        "The sequence must contain 6 elements for a 2D matrix or 16 elements "
        "for a 3D matrix.");
    return nullptr;
  }
  return new DOMMatrix(float64_array.View()->Data(),
                       float64_array.View()->length());
}

template <typename T>
DOMMatrix::DOMMatrix(T sequence, int size)
    : DOMMatrixReadOnly(sequence, size) {}

DOMMatrix::DOMMatrix(const TransformationMatrix& matrix, bool is2d)
    : DOMMatrixReadOnly(matrix, is2d) {}

DOMMatrix* DOMMatrix::fromMatrix(DOMMatrixInit& other,
                                 ExceptionState& exception_state) {
  if (!ValidateAndFixup(other, exception_state)) {
    DCHECK(exception_state.HadException());
    return nullptr;
  }
  if (other.is2D()) {
    return new DOMMatrix({other.m11(), other.m12(), other.m21(), other.m22(),
                          other.m41(), other.m42()},
                         other.is2D());
  }

  return new DOMMatrix({other.m11(), other.m12(), other.m13(), other.m14(),
                        other.m21(), other.m22(), other.m23(), other.m24(),
                        other.m31(), other.m32(), other.m33(), other.m34(),
                        other.m41(), other.m42(), other.m43(), other.m44()},
                       other.is2D());
}

void DOMMatrix::SetIs2D(bool value) {
  if (is2d_)
    is2d_ = value;
}

void DOMMatrix::SetNAN() {
  matrix_->SetM11(NAN);
  matrix_->SetM12(NAN);
  matrix_->SetM13(NAN);
  matrix_->SetM14(NAN);
  matrix_->SetM21(NAN);
  matrix_->SetM22(NAN);
  matrix_->SetM23(NAN);
  matrix_->SetM24(NAN);
  matrix_->SetM31(NAN);
  matrix_->SetM32(NAN);
  matrix_->SetM33(NAN);
  matrix_->SetM34(NAN);
  matrix_->SetM41(NAN);
  matrix_->SetM42(NAN);
  matrix_->SetM43(NAN);
  matrix_->SetM44(NAN);
}

DOMMatrix* DOMMatrix::multiplySelf(DOMMatrixInit& other,
                                   ExceptionState& exception_state) {
  DOMMatrix* other_matrix = DOMMatrix::fromMatrix(other, exception_state);
  if (!other_matrix) {
    DCHECK(exception_state.HadException());
    return nullptr;
  }
  return multiplySelf(*other_matrix);
}

DOMMatrix* DOMMatrix::multiplySelf(const DOMMatrix& other_matrix) {
  if (!other_matrix.is2D())
    is2d_ = false;

  *matrix_ *= other_matrix.Matrix();

  return this;
}

DOMMatrix* DOMMatrix::preMultiplySelf(DOMMatrixInit& other,
                                      ExceptionState& exception_state) {
  DOMMatrix* other_matrix = DOMMatrix::fromMatrix(other, exception_state);
  if (!other_matrix) {
    DCHECK(exception_state.HadException());
    return nullptr;
  }
  if (!other_matrix->is2D())
    is2d_ = false;

  TransformationMatrix& matrix = *matrix_;
  *matrix_ = other_matrix->Matrix() * matrix;

  return this;
}

DOMMatrix* DOMMatrix::translateSelf(double tx, double ty, double tz) {
  if (!tx && !ty && !tz)
    return this;

  if (tz)
    is2d_ = false;

  if (is2d_)
    matrix_->Translate(tx, ty);
  else
    matrix_->Translate3d(tx, ty, tz);

  return this;
}

DOMMatrix* DOMMatrix::scaleSelf(double sx) {
  return scaleSelf(sx, sx);
}

DOMMatrix* DOMMatrix::scaleSelf(double sx,
                                double sy,
                                double sz,
                                double ox,
                                double oy,
                                double oz) {
  if (sz != 1 || oz)
    is2d_ = false;

  if (sx == 1 && sy == 1 && sz == 1)
    return this;

  bool has_translation = (ox || oy || oz);

  if (has_translation)
    translateSelf(ox, oy, oz);

  if (is2d_)
    matrix_->ScaleNonUniform(sx, sy);
  else
    matrix_->Scale3d(sx, sy, sz);

  if (has_translation)
    translateSelf(-ox, -oy, -oz);

  return this;
}

DOMMatrix* DOMMatrix::scale3dSelf(double scale,
                                  double ox,
                                  double oy,
                                  double oz) {
  return scaleSelf(scale, scale, scale, ox, oy, oz);
}

DOMMatrix* DOMMatrix::rotateSelf(double rot_x) {
  return rotateSelf(0, 0, rot_x);
}

DOMMatrix* DOMMatrix::rotateSelf(double rot_x, double rot_y) {
  return rotateSelf(rot_x, rot_y, 0);
}

DOMMatrix* DOMMatrix::rotateSelf(double rot_x, double rot_y, double rot_z) {
  if (rot_z)
    matrix_->Rotate3d(0, 0, 1, rot_z);

  if (rot_y) {
    matrix_->Rotate3d(0, 1, 0, rot_y);
    is2d_ = false;
  }

  if (rot_x) {
    matrix_->Rotate3d(1, 0, 0, rot_x);
    is2d_ = false;
  }

  return this;
}

DOMMatrix* DOMMatrix::rotateFromVectorSelf(double x, double y) {
  matrix_->Rotate(rad2deg(atan2(y, x)));
  return this;
}

DOMMatrix* DOMMatrix::rotateAxisAngleSelf(double x,
                                          double y,
                                          double z,
                                          double angle) {
  matrix_->Rotate3d(x, y, z, angle);

  if (x != 0 || y != 0)
    is2d_ = false;

  return this;
}

DOMMatrix* DOMMatrix::skewXSelf(double sx) {
  matrix_->SkewX(sx);
  return this;
}

DOMMatrix* DOMMatrix::skewYSelf(double sy) {
  matrix_->SkewY(sy);
  return this;
}

DOMMatrix* DOMMatrix::perspectiveSelf(double p) {
  matrix_->ApplyPerspective(p);
  return this;
}

DOMMatrix* DOMMatrix::invertSelf() {
  if (matrix_->IsInvertible()) {
    matrix_ = TransformationMatrix::Create(matrix_->Inverse());
  } else {
    SetNAN();
    SetIs2D(false);
  }
  return this;
}

DOMMatrix* DOMMatrix::setMatrixValue(const String& input_string,
                                     ExceptionState& exception_state) {
  SetMatrixValueFromString(input_string, exception_state);
  return this;
}

}  // namespace blink
