// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSSkew_h
#define CSSSkew_h

#include "core/css/cssom/CSSMatrixComponent.h"
#include "core/css/cssom/CSSNumericValue.h"
#include "core/css/cssom/CSSTransformComponent.h"

namespace blink {

// Represents a skew value in a CSSTransformValue used for properties like
// "transform".
// See CSSSkew.idl for more documentation about this class.
class CORE_EXPORT CSSSkew final : public CSSTransformComponent {
  WTF_MAKE_NONCOPYABLE(CSSSkew);
  DEFINE_WRAPPERTYPEINFO();

 public:
  // Constructor defined in the IDL.
  static CSSSkew* Create(CSSNumericValue* ax, CSSNumericValue* ay) {
    return new CSSSkew(ax, ay);
  }

  // Internal ways of creating CSSSkews.
  static CSSSkew* FromCSSValue(const CSSFunctionValue&);

  // Getters and setters for the ax and ay attributes defined in the IDL.
  CSSNumericValue* ax() const { return ax_.Get(); }
  CSSNumericValue* ay() const { return ay_.Get(); }
  void setAx(CSSNumericValue*, ExceptionState&);
  void setAy(CSSNumericValue*, ExceptionState&);

  CSSMatrixComponent* asMatrix() const override {
    return nullptr;
    // TODO(meade): Reimplement this once the number/unit types
    // are re-implemented.
    // return CSSMatrixComponent::Skew(ax_->degrees(), ay_->degrees());
  }

  // Internal methods - from CSSTransformComponent.
  TransformComponentType GetType() const override { return kSkewType; }
  CSSFunctionValue* ToCSSValue() const override;

  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->Trace(ax_);
    visitor->Trace(ay_);
    CSSTransformComponent::Trace(visitor);
  }

 private:
  CSSSkew(CSSNumericValue* ax, CSSNumericValue* ay)
      : CSSTransformComponent(), ax_(ax), ay_(ay) {}

  Member<CSSNumericValue> ax_;
  Member<CSSNumericValue> ay_;
};

}  // namespace blink

#endif
