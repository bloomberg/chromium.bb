// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSSkew_h
#define CSSSkew_h

#include "core/css/cssom/CSSMatrixComponent.h"
#include "core/css/cssom/CSSNumericValue.h"
#include "core/css/cssom/CSSTransformComponent.h"

namespace blink {

class CORE_EXPORT CSSSkew final : public CSSTransformComponent {
  WTF_MAKE_NONCOPYABLE(CSSSkew);
  DEFINE_WRAPPERTYPEINFO();

 public:
  static CSSSkew* Create(const CSSNumericValue* ax, const CSSNumericValue* ay) {
    return new CSSSkew(ax, ay);
  }

  static CSSSkew* FromCSSValue(const CSSFunctionValue&);

  // Bindings requires returning non-const pointers. This is safe because
  // CSSNumericValues are immutable.
  CSSNumericValue* ax() const {
    return const_cast<CSSNumericValue*>(ax_.Get());
  }
  CSSNumericValue* ay() const {
    return const_cast<CSSNumericValue*>(ay_.Get());
  }

  TransformComponentType GetType() const override { return kSkewType; }

  CSSMatrixComponent* asMatrix() const override {
    return nullptr;
    // TODO(meade): Reimplement this once the number/unit types
    // are re-implemented.
    // return CSSMatrixComponent::Skew(ax_->degrees(), ay_->degrees());
  }

  CSSFunctionValue* ToCSSValue() const override;

  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->Trace(ax_);
    visitor->Trace(ay_);
    CSSTransformComponent::Trace(visitor);
  }

 private:
  CSSSkew(const CSSNumericValue* ax, const CSSNumericValue* ay)
      : CSSTransformComponent(), ax_(ax), ay_(ay) {}

  Member<const CSSNumericValue> ax_;
  Member<const CSSNumericValue> ay_;
};

}  // namespace blink

#endif
