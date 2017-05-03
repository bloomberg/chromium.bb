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

#ifndef CSSMatrix_h
#define CSSMatrix_h

#include <memory>
#include "platform/bindings/ScriptWrappable.h"
#include "platform/transforms/TransformationMatrix.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class ExceptionState;
class ExecutionContext;

class CSSMatrix final : public GarbageCollectedFinalized<CSSMatrix>,
                        public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static CSSMatrix* Create(const TransformationMatrix& m) {
    return new CSSMatrix(m);
  }
  static CSSMatrix* Create(ExecutionContext*, const String&, ExceptionState&);

  double a() const { return matrix_->A(); }
  double b() const { return matrix_->B(); }
  double c() const { return matrix_->C(); }
  double d() const { return matrix_->D(); }
  double e() const { return matrix_->E(); }
  double f() const { return matrix_->F(); }

  void setA(double f) { matrix_->SetA(f); }
  void setB(double f) { matrix_->SetB(f); }
  void setC(double f) { matrix_->SetC(f); }
  void setD(double f) { matrix_->SetD(f); }
  void setE(double f) { matrix_->SetE(f); }
  void setF(double f) { matrix_->SetF(f); }

  double m11() const { return matrix_->M11(); }
  double m12() const { return matrix_->M12(); }
  double m13() const { return matrix_->M13(); }
  double m14() const { return matrix_->M14(); }
  double m21() const { return matrix_->M21(); }
  double m22() const { return matrix_->M22(); }
  double m23() const { return matrix_->M23(); }
  double m24() const { return matrix_->M24(); }
  double m31() const { return matrix_->M31(); }
  double m32() const { return matrix_->M32(); }
  double m33() const { return matrix_->M33(); }
  double m34() const { return matrix_->M34(); }
  double m41() const { return matrix_->M41(); }
  double m42() const { return matrix_->M42(); }
  double m43() const { return matrix_->M43(); }
  double m44() const { return matrix_->M44(); }

  void setM11(double f) { matrix_->SetM11(f); }
  void setM12(double f) { matrix_->SetM12(f); }
  void setM13(double f) { matrix_->SetM13(f); }
  void setM14(double f) { matrix_->SetM14(f); }
  void setM21(double f) { matrix_->SetM21(f); }
  void setM22(double f) { matrix_->SetM22(f); }
  void setM23(double f) { matrix_->SetM23(f); }
  void setM24(double f) { matrix_->SetM24(f); }
  void setM31(double f) { matrix_->SetM31(f); }
  void setM32(double f) { matrix_->SetM32(f); }
  void setM33(double f) { matrix_->SetM33(f); }
  void setM34(double f) { matrix_->SetM34(f); }
  void setM41(double f) { matrix_->SetM41(f); }
  void setM42(double f) { matrix_->SetM42(f); }
  void setM43(double f) { matrix_->SetM43(f); }
  void setM44(double f) { matrix_->SetM44(f); }

  void setMatrixValue(const String&, ExceptionState&);

  // The following math function return a new matrix with the
  // specified operation applied. The this value is not modified.

  // Multiply this matrix by secondMatrix, on the right
  // (result = this * secondMatrix)
  CSSMatrix* multiply(CSSMatrix* second_matrix) const;

  // Return the inverse of this matrix. Throw an exception if the matrix is not
  // invertible.
  CSSMatrix* inverse(ExceptionState&) const;

  // Return this matrix translated by the passed values.
  // Passing a NaN will use a value of 0. This allows the 3D form to used for 2D
  // operations.
  // Operation is performed as though the this matrix is multiplied by a matrix
  // with the translation values on the left
  // (result = translation(x,y,z) * this)
  CSSMatrix* translate(double x, double y, double z) const;

  // Returns this matrix scaled by the passed values.
  // Passing scaleX or scaleZ as NaN uses a value of 1, but passing scaleY of
  // NaN makes it the same as scaleX. This allows the 3D form to used for 2D
  // operations Operation is performed as though the this matrix is multiplied
  // by a matrix with the scale values on the left
  // (result = scale(x,y,z) * this)
  CSSMatrix* scale(double scale_x, double scale_y, double scale_z) const;

  // Returns this matrix rotated by the passed values.
  // If rotY and rotZ are NaN, rotate about Z (rotX=0, rotateY=0, rotateZ=rotX).
  // Otherwise use a rotation value of 0 for any passed NaN.
  // Operation is performed as though the this matrix is multiplied by a matrix
  // with the rotation values on the left (result = rotation(x,y,z) * this)
  CSSMatrix* rotate(double rot_x, double rot_y, double rot_z) const;

  // Returns this matrix rotated about the passed axis by the passed angle.
  // Passing a NaN will use a value of 0. If the axis is (0,0,0) use a value
  // Operation is performed as though the this matrix is multiplied by a matrix
  // with the rotation values on the left
  // (result = rotation(x,y,z,angle) * this)
  CSSMatrix* rotateAxisAngle(double x, double y, double z, double angle) const;

  // Return this matrix skewed along the X axis by the passed values.
  // Passing a NaN will use a value of 0.
  // Operation is performed as though the this matrix is multiplied by a matrix
  // with the skew values on the left (result = skewX(angle) * this)
  CSSMatrix* skewX(double angle) const;

  // Return this matrix skewed along the Y axis by the passed values.
  // Passing a NaN will use a value of 0.
  // Operation is performed as though the this matrix is multiplied by a matrix
  // with the skew values on the left (result = skewY(angle) * this)
  CSSMatrix* skewY(double angle) const;

  const TransformationMatrix& Transform() const { return *matrix_; }

  String toString() const;

  DEFINE_INLINE_TRACE() {}

 protected:
  CSSMatrix(const TransformationMatrix&);
  CSSMatrix(const String&, ExceptionState&);

  // TransformationMatrix needs to be 16-byte aligned. PartitionAlloc
  // supports 16-byte alignment but Oilpan doesn't. So we use an std::unique_ptr
  // to allocate TransformationMatrix on PartitionAlloc.
  // TODO(oilpan): Oilpan should support 16-byte aligned allocations.
  std::unique_ptr<TransformationMatrix> matrix_;
};

}  // namespace blink

#endif  // CSSMatrix_h
