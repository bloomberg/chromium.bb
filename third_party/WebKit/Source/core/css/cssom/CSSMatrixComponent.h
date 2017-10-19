// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSMatrixComponent_h
#define CSSMatrixComponent_h

#include "core/css/cssom/CSSTransformComponent.h"
#include "core/geometry/DOMMatrix.h"
#include "core/geometry/DOMMatrixReadOnly.h"

namespace blink {

class CSSMatrixComponentOptions;

// Represents a matrix value in a CSSTransformValue used for properties like
// "transform".
// See CSSMatrixComponent.idl for more information about this class.
class CORE_EXPORT CSSMatrixComponent final : public CSSTransformComponent {
  WTF_MAKE_NONCOPYABLE(CSSMatrixComponent);
  DEFINE_WRAPPERTYPEINFO();

 public:
  // Constructors defined in the IDL.
  static CSSMatrixComponent* Create(DOMMatrixReadOnly*,
                                    const CSSMatrixComponentOptions&);

  // Blink-internal ways of creating CSSMatrixComponents.
  static CSSMatrixComponent* FromCSSValue(const CSSFunctionValue& value) {
    // TODO(meade): Implement.
    return nullptr;
  }

  // Getters and setters for attributes defined in the IDL.
  DOMMatrix* matrix() { return matrix_.Get(); }
  void setMatrix(DOMMatrix* matrix) { matrix_ = matrix; }

  // Internal methods - from CSSTransformComponent.
  TransformComponentType GetType() const final { return kMatrixType; }
  const DOMMatrix* AsMatrix(ExceptionState&) const final;
  const CSSFunctionValue* ToCSSValue() const final;

  virtual void Trace(blink::Visitor* visitor) {
    visitor->Trace(matrix_);
    CSSTransformComponent::Trace(visitor);
  }

 private:
  CSSMatrixComponent(DOMMatrixReadOnly* matrix, bool is2D)
      : CSSTransformComponent(is2D), matrix_(DOMMatrix::Create(matrix)) {}

  Member<DOMMatrix> matrix_;
};

}  // namespace blink

#endif
