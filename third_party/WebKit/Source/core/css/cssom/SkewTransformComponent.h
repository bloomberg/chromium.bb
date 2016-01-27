// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SkewTransformComponent_h
#define SkewTransformComponent_h

#include "core/css/cssom/TransformComponent.h"

namespace blink {

class CORE_EXPORT SkewTransformComponent final : public TransformComponent {
    WTF_MAKE_NONCOPYABLE(SkewTransformComponent);
    DEFINE_WRAPPERTYPEINFO();
public:
    static SkewTransformComponent* create(double ax, double ay)
    {
        return new SkewTransformComponent(ax, ay);
    }

    double ax() const { return m_ax; }
    double ay() const { return m_ay; }

    TransformComponentType type() const override { return SkewType; }

    PassRefPtrWillBeRawPtr<CSSFunctionValue> toCSSValue() const override;

private:
    SkewTransformComponent(double ax, double ay) : TransformComponent(), m_ax(ax), m_ay(ay) { }

    double m_ax;
    double m_ay;
};

} // namespace blink

#endif
