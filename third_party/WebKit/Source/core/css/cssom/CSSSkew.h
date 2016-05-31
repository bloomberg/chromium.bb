// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSSkew_h
#define CSSSkew_h

#include "core/css/cssom/MatrixTransformComponent.h"
#include "core/css/cssom/TransformComponent.h"

namespace blink {

class CORE_EXPORT CSSSkew final : public TransformComponent {
    WTF_MAKE_NONCOPYABLE(CSSSkew);
    DEFINE_WRAPPERTYPEINFO();
public:
    static CSSSkew* create(double ax, double ay)
    {
        return new CSSSkew(ax, ay);
    }

    double ax() const { return m_ax; }
    double ay() const { return m_ay; }

    TransformComponentType type() const override { return SkewType; }

    MatrixTransformComponent* asMatrix() const override
    {
        return MatrixTransformComponent::skew(m_ax, m_ay);
    }

    CSSFunctionValue* toCSSValue() const override;

private:
    CSSSkew(double ax, double ay) : TransformComponent(), m_ax(ax), m_ay(ay) { }

    double m_ax;
    double m_ay;
};

} // namespace blink

#endif
