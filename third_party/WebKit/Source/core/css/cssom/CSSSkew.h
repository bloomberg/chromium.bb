// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSSkew_h
#define CSSSkew_h

#include "core/css/cssom/CSSAngleValue.h"
#include "core/css/cssom/CSSMatrixComponent.h"
#include "core/css/cssom/CSSTransformComponent.h"

namespace blink {

class CORE_EXPORT CSSSkew final : public CSSTransformComponent {
  WTF_MAKE_NONCOPYABLE(CSSSkew);
  DEFINE_WRAPPERTYPEINFO();

 public:
  static CSSSkew* Create(const CSSAngleValue* ax, const CSSAngleValue* ay) {
    return new CSSSkew(ax, ay);
  }

  static CSSSkew* FromCSSValue(const CSSFunctionValue&);

  // Bindings requires returning non-const pointers. This is safe because
  // CSSAngleValues are immutable.
  CSSAngleValue* ax() const { return const_cast<CSSAngleValue*>(ax_.Get()); }
  CSSAngleValue* ay() const { return const_cast<CSSAngleValue*>(ay_.Get()); }

  TransformComponentType GetType() const override { return kSkewType; }

  CSSMatrixComponent* asMatrix() const override {
    return CSSMatrixComponent::Skew(ax_->degrees(), ay_->degrees());
  }

  CSSFunctionValue* ToCSSValue() const override;

  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->Trace(ax_);
    visitor->Trace(ay_);
    CSSTransformComponent::Trace(visitor);
  }

 private:
  CSSSkew(const CSSAngleValue* ax, const CSSAngleValue* ay)
      : CSSTransformComponent(), ax_(ax), ay_(ay) {}

  Member<const CSSAngleValue> ax_;
  Member<const CSSAngleValue> ay_;
};

}  // namespace blink

#endif
