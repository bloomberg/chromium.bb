// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScaleTransformComponent_h
#define ScaleTransformComponent_h

#include "core/css/cssom/TransformComponent.h"
#include "platform/transforms/ScaleTransformOperation.h"

namespace blink {

class CORE_EXPORT ScaleTransformComponent final : public TransformComponent {
    WTF_MAKE_NONCOPYABLE(ScaleTransformComponent);
    DEFINE_WRAPPERTYPEINFO();
public:
    static ScaleTransformComponent* create(double x, double y)
    {
        return new ScaleTransformComponent(x, y);
    }

    static ScaleTransformComponent* create(double x, double y, double z)
    {
        return new ScaleTransformComponent(x, y, z);
    }

    double x() const { return m_x; }
    double y() const { return m_y; }
    double z() const { return m_z; }

    TransformComponentType type() const override { return m_is2D ? ScaleType : Scale3DType; }

    String cssString() const override;
    PassRefPtrWillBeRawPtr<CSSFunctionValue> toCSSValue() const override;

private:
    ScaleTransformComponent(double x, double y) : TransformComponent(), m_x(x), m_y(y), m_z(1), m_is2D(true) { }
    ScaleTransformComponent(double x, double y, double z) : TransformComponent(), m_x(x), m_y(y), m_z(z), m_is2D(false) { }

    double m_x;
    double m_y;
    double m_z;
    bool m_is2D;
};

} // namespace blink

#endif
