// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSMatrixComponent_h
#define CSSMatrixComponent_h

#include <memory>
#include "core/css/cssom/CSSTransformComponent.h"
#include "core/geometry/DOMMatrix.h"
#include "platform/transforms/TransformationMatrix.h"

namespace blink {

class DOMMatrixReadOnly;

class CORE_EXPORT CSSMatrixComponent final : public CSSTransformComponent {
  WTF_MAKE_NONCOPYABLE(CSSMatrixComponent);
  DEFINE_WRAPPERTYPEINFO();

 public:
  static CSSMatrixComponent* Create(DOMMatrixReadOnly*);

  static CSSMatrixComponent* FromCSSValue(const CSSFunctionValue& value) {
    return nullptr;
  }

  DOMMatrix* matrix() const { return matrix_; }

  void setMatrix(DOMMatrix* matrix) { matrix_ = matrix; }

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

  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->Trace(matrix_);
    CSSTransformComponent::Trace(visitor);
  }

 private:
  CSSMatrixComponent(DOMMatrixReadOnly*);
  CSSMatrixComponent(DOMMatrixReadOnly*, TransformComponentType);

  Member<DOMMatrix> matrix_;
  bool is2d_;
};

}  // namespace blink

#endif
