// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSRotation_h
#define CSSRotation_h

#include "core/css/cssom/CSSAngleValue.h"
#include "core/css/cssom/CSSMatrixComponent.h"
#include "core/css/cssom/CSSTransformComponent.h"

namespace blink {

class CORE_EXPORT CSSRotation final : public CSSTransformComponent {
  WTF_MAKE_NONCOPYABLE(CSSRotation);
  DEFINE_WRAPPERTYPEINFO();

 public:
  static CSSRotation* create(const CSSAngleValue* angleValue) {
    return new CSSRotation(angleValue);
  }

  static CSSRotation* create(double x,
                             double y,
                             double z,
                             const CSSAngleValue* angleValue) {
    return new CSSRotation(x, y, z, angleValue);
  }

  static CSSRotation* fromCSSValue(const CSSFunctionValue&);

  // Bindings requires returning non-const pointers. This is safe because
  // CSSAngleValues are immutable.
  CSSAngleValue* angle() const {
    return const_cast<CSSAngleValue*>(m_angle.get());
  }
  double x() const { return m_x; }
  double y() const { return m_y; }
  double z() const { return m_z; }

  TransformComponentType type() const override {
    return m_is2D ? RotationType : Rotation3DType;
  }

  CSSMatrixComponent* asMatrix() const override {
    return m_is2D ? CSSMatrixComponent::rotate(m_angle->degrees())
                  : CSSMatrixComponent::rotate3d(m_angle->degrees(), m_x, m_y,
                                                 m_z);
  }

  CSSFunctionValue* toCSSValue() const override;

  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->trace(m_angle);
    CSSTransformComponent::trace(visitor);
  }

 private:
  CSSRotation(const CSSAngleValue* angle)
      : m_angle(angle), m_x(0), m_y(0), m_z(1), m_is2D(true) {}

  CSSRotation(double x, double y, double z, const CSSAngleValue* angle)
      : m_angle(angle), m_x(x), m_y(y), m_z(z), m_is2D(false) {}

  Member<const CSSAngleValue> m_angle;
  double m_x;
  double m_y;
  double m_z;
  bool m_is2D;
};

}  // namespace blink

#endif
