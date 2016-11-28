// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSSkew_h
#define CSSSkew_h

#include "core/css/cssom/CSSAngleValue.h"
#include "core/css/cssom/CSSMatrixTransformComponent.h"
#include "core/css/cssom/CSSTransformComponent.h"

namespace blink {

class CORE_EXPORT CSSSkew final : public CSSTransformComponent {
  WTF_MAKE_NONCOPYABLE(CSSSkew);
  DEFINE_WRAPPERTYPEINFO();

 public:
  static CSSSkew* create(const CSSAngleValue* ax, const CSSAngleValue* ay) {
    return new CSSSkew(ax, ay);
  }

  static CSSSkew* fromCSSValue(const CSSFunctionValue& value) {
    return nullptr;
  }

  // Bindings requires returning non-const pointers. This is safe because
  // CSSAngleValues are immutable.
  CSSAngleValue* ax() const { return const_cast<CSSAngleValue*>(m_ax.get()); }
  CSSAngleValue* ay() const { return const_cast<CSSAngleValue*>(m_ay.get()); }

  TransformComponentType type() const override { return SkewType; }

  CSSMatrixTransformComponent* asMatrix() const override {
    return CSSMatrixTransformComponent::skew(m_ax->degrees(), m_ay->degrees());
  }

  CSSFunctionValue* toCSSValue() const override;

  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->trace(m_ax);
    visitor->trace(m_ay);
    CSSTransformComponent::trace(visitor);
  }

 private:
  CSSSkew(const CSSAngleValue* ax, const CSSAngleValue* ay)
      : CSSTransformComponent(), m_ax(ax), m_ay(ay) {}

  Member<const CSSAngleValue> m_ax;
  Member<const CSSAngleValue> m_ay;
};

}  // namespace blink

#endif
