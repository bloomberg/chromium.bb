// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MatrixTransformComponent_h
#define MatrixTransformComponent_h

#include "core/css/cssom/TransformComponent.h"
#include "platform/transforms/TransformationMatrix.h"

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
    double a() const { return m_matrix->a(); }
    double b() const { return m_matrix->b(); }
    double c() const { return m_matrix->c(); }
    double d() const { return m_matrix->d(); }
    double e() const { return m_matrix->e(); }
    double f() const { return m_matrix->f(); }

    // 3D matrix attributes
    double m11() const { return m_matrix->m11(); }
    double m12() const { return m_matrix->m12(); }
    double m13() const { return m_matrix->m13(); }
    double m14() const { return m_matrix->m14(); }
    double m21() const { return m_matrix->m21(); }
    double m22() const { return m_matrix->m22(); }
    double m23() const { return m_matrix->m23(); }
    double m24() const { return m_matrix->m24(); }
    double m31() const { return m_matrix->m31(); }
    double m32() const { return m_matrix->m32(); }
    double m33() const { return m_matrix->m33(); }
    double m34() const { return m_matrix->m34(); }
    double m41() const { return m_matrix->m41(); }
    double m42() const { return m_matrix->m42(); }
    double m43() const { return m_matrix->m43(); }
    double m44() const { return m_matrix->m44(); }

    TransformComponentType type() const override { return m_is2D ? MatrixType : Matrix3DType; }

    PassRefPtrWillBeRawPtr<CSSFunctionValue> toCSSValue() const override;

private:
    MatrixTransformComponent(double a, double b, double c, double d, double e, double f)
        : TransformComponent()
        , m_matrix(TransformationMatrix::create(a, b, c, d, e, f))
        , m_is2D(true)
    { }

    MatrixTransformComponent(double m11, double m12, double m13, double m14,
        double m21, double m22, double m23, double m24,
        double m31, double m32, double m33, double m34,
        double m41, double m42, double m43, double m44)
        : TransformComponent()
        , m_matrix(TransformationMatrix::create(m11, m12, m13, m14, m21, m22, m23, m24, m31, m32, m33, m34, m41, m42, m43, m44))
        , m_is2D(false)
    { }

    // TransformationMatrix needs to be 16-byte aligned. PartitionAlloc
    // supports 16-byte alignment but Oilpan doesn't. So we use an OwnPtr
    // to allocate TransformationMatrix on PartitionAlloc.
    // TODO(oilpan): Oilpan should support 16-byte aligned allocations.
    OwnPtr<const TransformationMatrix> m_matrix;
    bool m_is2D;
};

} // namespace blink

#endif
