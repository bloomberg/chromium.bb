// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSMatrixComponent_h
#define CSSMatrixComponent_h

#include "core/css/cssom/CSSTransformComponent.h"
#include "core/geometry/DOMMatrix.h"

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

  DOMMatrix* AsMatrix() const override { return matrix(); }

  CSSFunctionValue* ToCSSValue() const override;

  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->Trace(matrix_);
    CSSTransformComponent::Trace(visitor);
  }

 private:
  CSSMatrixComponent(DOMMatrixReadOnly*);

  Member<DOMMatrix> matrix_;
  bool is2d_;
};

}  // namespace blink

#endif
