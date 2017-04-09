// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSTranslation_h
#define CSSTranslation_h

#include "core/css/cssom/CSSLengthValue.h"
#include "core/css/cssom/CSSTransformComponent.h"

namespace blink {

class ExceptionState;

class CORE_EXPORT CSSTranslation final : public CSSTransformComponent {
  WTF_MAKE_NONCOPYABLE(CSSTranslation);
  DEFINE_WRAPPERTYPEINFO();

 public:
  static CSSTranslation* Create(CSSLengthValue* x,
                                CSSLengthValue* y,
                                ExceptionState&) {
    return new CSSTranslation(x, y, nullptr);
  }
  static CSSTranslation* Create(CSSLengthValue* x,
                                CSSLengthValue* y,
                                CSSLengthValue* z,
                                ExceptionState&);

  static CSSTranslation* FromCSSValue(const CSSFunctionValue& value) {
    return nullptr;
  }

  CSSLengthValue* x() const { return x_; }
  CSSLengthValue* y() const { return y_; }
  CSSLengthValue* z() const { return z_; }

  TransformComponentType GetType() const override {
    return Is2D() ? kTranslationType : kTranslation3DType;
  }

  // TODO: Implement asMatrix for CSSTranslation.
  CSSMatrixComponent* asMatrix() const override { return nullptr; }

  CSSFunctionValue* ToCSSValue() const override;

  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->Trace(x_);
    visitor->Trace(y_);
    visitor->Trace(z_);
    CSSTransformComponent::Trace(visitor);
  }

 private:
  CSSTranslation(CSSLengthValue* x, CSSLengthValue* y, CSSLengthValue* z)
      : CSSTransformComponent(), x_(x), y_(y), z_(z) {}

  bool Is2D() const { return !z_; }

  Member<CSSLengthValue> x_;
  Member<CSSLengthValue> y_;
  Member<CSSLengthValue> z_;
};

}  // namespace blink

#endif
