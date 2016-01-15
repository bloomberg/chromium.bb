// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MatrixTransformComponent_h
#define MatrixTransformComponent_h

#include "core/css/cssom/TransformComponent.h"

namespace blink {

class CORE_EXPORT MatrixTransformComponent final : public TransformComponent {
    WTF_MAKE_NONCOPYABLE(MatrixTransformComponent);
    DEFINE_WRAPPERTYPEINFO();
public:
    static MatrixTransformComponent* create(double a, double b, double c, double d, double e, double f)
    {
        return new MatrixTransformComponent(a, b, c, d, e, f);
    }

    static MatrixTransformComponent* create(double m11, double m12, double m13, double m14,
        double m21, double m22, double m23, double m24,
        double m31, double m32, double m33, double m34,
        double m41, double m42, double m43, double m44)
    {
        return new MatrixTransformComponent(m11, m12, m13, m14, m21, m22, m23, m24, m31, m32, m33, m34, m41, m42, m43, m44);
    }

    // 2D matrix attributes
    double a() const { return m_m11; }
    double b() const { return m_m12; }
    double c() const { return m_m21; }
    double d() const { return m_m22; }
    double e() const { return m_m41; }
    double f() const { return m_m42; }

    // 3D matrix attributes
    double m11() const { return m_m11; }
    double m12() const { return m_m12; }
    double m13() const { return m_m13; }
    double m14() const { return m_m14; }
    double m21() const { return m_m21; }
    double m22() const { return m_m22; }
    double m23() const { return m_m23; }
    double m24() const { return m_m24; }
    double m31() const { return m_m31; }
    double m32() const { return m_m32; }
    double m33() const { return m_m33; }
    double m34() const { return m_m34; }
    double m41() const { return m_m41; }
    double m42() const { return m_m42; }
    double m43() const { return m_m43; }
    double m44() const { return m_m44; }

    TransformComponentType type() const override { return m_is2D ? MatrixType : Matrix3DType; }

    String cssString() const override;
    PassRefPtrWillBeRawPtr<CSSFunctionValue> toCSSValue() const override;

private:
    MatrixTransformComponent(double a, double b, double c, double d, double e, double f)
        : TransformComponent()
        , m_m11(a)
        , m_m12(b)
        , m_m13(0)
        , m_m14(0)
        , m_m21(c)
        , m_m22(d)
        , m_m23(0)
        , m_m24(0)
        , m_m31(0)
        , m_m32(0)
        , m_m33(1)
        , m_m34(0)
        , m_m41(e)
        , m_m42(f)
        , m_m43(0)
        , m_m44(1)
        , m_is2D(true)
    { }

    MatrixTransformComponent(double m11, double m12, double m13, double m14,
        double m21, double m22, double m23, double m24,
        double m31, double m32, double m33, double m34,
        double m41, double m42, double m43, double m44)
        : TransformComponent()
        , m_m11(m11)
        , m_m12(m12)
        , m_m13(m13)
        , m_m14(m14)
        , m_m21(m21)
        , m_m22(m22)
        , m_m23(m23)
        , m_m24(m24)
        , m_m31(m31)
        , m_m32(m32)
        , m_m33(m33)
        , m_m34(m34)
        , m_m41(m41)
        , m_m42(m42)
        , m_m43(m43)
        , m_m44(m44)
        , m_is2D(false)
    { }

    double m_m11;
    double m_m12;
    double m_m13;
    double m_m14;
    double m_m21;
    double m_m22;
    double m_m23;
    double m_m24;
    double m_m31;
    double m_m32;
    double m_m33;
    double m_m34;
    double m_m41;
    double m_m42;
    double m_m43;
    double m_m44;
    bool m_is2D;
};

} // namespace blink

#endif
