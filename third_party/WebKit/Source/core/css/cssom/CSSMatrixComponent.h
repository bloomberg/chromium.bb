// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSMatrixComponent_h
#define CSSMatrixComponent_h

#include <memory>
#include "core/css/cssom/CSSTransformComponent.h"
#include "platform/transforms/TransformationMatrix.h"

namespace blink {

class CORE_EXPORT CSSMatrixComponent final : public CSSTransformComponent {
  WTF_MAKE_NONCOPYABLE(CSSMatrixComponent);
  DEFINE_WRAPPERTYPEINFO();

 public:
  static CSSMatrixComponent* Create(double a,
                                    double b,
                                    double c,
                                    double d,
                                    double e,
                                    double f) {
    return new CSSMatrixComponent(a, b, c, d, e, f);
  }

  static CSSMatrixComponent* Create(double m11,
                                    double m12,
                                    double m13,
                                    double m14,
                                    double m21,
                                    double m22,
                                    double m23,
                                    double m24,
                                    double m31,
                                    double m32,
                                    double m33,
                                    double m34,
                                    double m41,
                                    double m42,
                                    double m43,
                                    double m44) {
    return new CSSMatrixComponent(m11, m12, m13, m14, m21, m22, m23, m24, m31,
                                  m32, m33, m34, m41, m42, m43, m44);
  }

  static CSSMatrixComponent* FromCSSValue(const CSSFunctionValue& value) {
    return nullptr;
  }

  // 2D matrix attributes
  double a() const { return matrix_->A(); }
  double b() const { return matrix_->B(); }
  double c() const { return matrix_->C(); }
  double d() const { return matrix_->D(); }
  double e() const { return matrix_->E(); }
  double f() const { return matrix_->F(); }

  // 3D matrix attributes
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

  TransformComponentType GetType() const override {
    return is2d_ ? kMatrixType : kMatrix3DType;
  }

  // Bindings require a non const return value.
  CSSMatrixComponent* asMatrix() const override {
    return const_cast<CSSMatrixComponent*>(this);
  }

  CSSFunctionValue* ToCSSValue() const override;

  static CSSMatrixComponent* Perspective(double length);

  static CSSMatrixComponent* Rotate(double angle);
  static CSSMatrixComponent* Rotate3d(double angle,
                                      double x,
                                      double y,
                                      double z);

  static CSSMatrixComponent* Scale(double x, double y);
  static CSSMatrixComponent* Scale3d(double x, double y, double z);

  static CSSMatrixComponent* Skew(double x, double y);

  static CSSMatrixComponent* Translate(double x, double y);
  static CSSMatrixComponent* Translate3d(double x, double y, double z);

 private:
  CSSMatrixComponent(double a, double b, double c, double d, double e, double f)
      : CSSTransformComponent(),
        matrix_(TransformationMatrix::Create(a, b, c, d, e, f)),
        is2d_(true) {}

  CSSMatrixComponent(double m11,
                     double m12,
                     double m13,
                     double m14,
                     double m21,
                     double m22,
                     double m23,
                     double m24,
                     double m31,
                     double m32,
                     double m33,
                     double m34,
                     double m41,
                     double m42,
                     double m43,
                     double m44)
      : CSSTransformComponent(),
        matrix_(TransformationMatrix::Create(m11,
                                             m12,
                                             m13,
                                             m14,
                                             m21,
                                             m22,
                                             m23,
                                             m24,
                                             m31,
                                             m32,
                                             m33,
                                             m34,
                                             m41,
                                             m42,
                                             m43,
                                             m44)),
        is2d_(false) {}

  CSSMatrixComponent(std::unique_ptr<const TransformationMatrix> matrix,
                     TransformComponentType from_type)
      : CSSTransformComponent(),
        matrix_(std::move(matrix)),
        is2d_(Is2DComponentType(from_type)) {}

  // TransformationMatrix needs to be 16-byte aligned. PartitionAlloc
  // supports 16-byte alignment but Oilpan doesn't. So we use an std::unique_ptr
  // to allocate TransformationMatrix on PartitionAlloc.
  // TODO(oilpan): Oilpan should support 16-byte aligned allocations.
  std::unique_ptr<const TransformationMatrix> matrix_;
  bool is2d_;
};

}  // namespace blink

#endif
