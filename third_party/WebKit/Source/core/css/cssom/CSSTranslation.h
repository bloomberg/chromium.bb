// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSTranslation_h
#define CSSTranslation_h

#include "core/css/cssom/CSSTransformComponent.h"

namespace blink {

class CSSNumericValue;
class DOMMatrix;
class ExceptionState;

// Represents a translation value in a CSSTransformValue used for properties
// like "transform".
// See CSSTranslation.idl for more information about this class.
class CORE_EXPORT CSSTranslation final : public CSSTransformComponent {
  WTF_MAKE_NONCOPYABLE(CSSTranslation);
  DEFINE_WRAPPERTYPEINFO();

 public:
  // Constructors defined in the IDL.
  static CSSTranslation* Create(CSSNumericValue* x,
                                CSSNumericValue* y,
                                ExceptionState&) {
    return new CSSTranslation(x, y, nullptr);
  }
  static CSSTranslation* Create(CSSNumericValue* x,
                                CSSNumericValue* y,
                                CSSNumericValue* z,
                                ExceptionState&);

  // Blink-internal ways of creating CSSTranslations.
  static CSSTranslation* FromCSSValue(const CSSFunctionValue& value) {
    return nullptr;
  }

  // Getters and setters for attributes defined in the IDL.
  CSSNumericValue* x() const { return x_; }
  CSSNumericValue* y() const { return y_; }
  CSSNumericValue* z() const { return z_; }

  // Internal methods - from CSSTransformComponent.
  TransformComponentType GetType() const override {
    return Is2D() ? kTranslationType : kTranslation3DType;
  }
  // TODO: Implement AsMatrix for CSSTranslation.
  DOMMatrix* AsMatrix() const override { return nullptr; }
  CSSFunctionValue* ToCSSValue() const override;

  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->Trace(x_);
    visitor->Trace(y_);
    visitor->Trace(z_);
    CSSTransformComponent::Trace(visitor);
  }

 private:
  CSSTranslation(CSSNumericValue* x, CSSNumericValue* y, CSSNumericValue* z)
      : CSSTransformComponent(), x_(x), y_(y), z_(z) {}

  bool Is2D() const { return !z_; }

  Member<CSSNumericValue> x_;
  Member<CSSNumericValue> y_;
  Member<CSSNumericValue> z_;
};

}  // namespace blink

#endif
