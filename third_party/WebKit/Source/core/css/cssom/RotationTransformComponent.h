// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RotationTransformComponent_h
#define RotationTransformComponent_h

#include "core/css/cssom/TransformComponent.h"

namespace blink {

class CORE_EXPORT RotationTransformComponent final : public TransformComponent {
    WTF_MAKE_NONCOPYABLE(RotationTransformComponent);
    DEFINE_WRAPPERTYPEINFO();
public:
    static RotationTransformComponent* create(double angle)
    {
        return new RotationTransformComponent(angle);
    }

    static RotationTransformComponent* create(double angle, double x, double y, double z)
    {
        return new RotationTransformComponent(angle, x, y, z);
    }

    double angle() const { return m_angle; }
    double x() const { return m_x; }
    double y() const { return m_y; }
    double z() const { return m_z; }

    TransformComponentType type() const override { return m_is2D ? RotationType : Rotation3DType; }

    PassRefPtrWillBeRawPtr<CSSFunctionValue> toCSSValue() const override;

private:
    RotationTransformComponent(double angle)
        : m_angle(angle)
        , m_x(0)
        , m_y(0)
        , m_z(1)
        , m_is2D(true)
    { }

    RotationTransformComponent(double angle, double x, double y, double z)
        : m_angle(angle)
        , m_x(x)
        , m_y(y)
        , m_z(z)
        , m_is2D(false)
    { }

    double m_angle;
    double m_x;
    double m_y;
    double m_z;
    bool m_is2D;
};

} // namespace blink

#endif
