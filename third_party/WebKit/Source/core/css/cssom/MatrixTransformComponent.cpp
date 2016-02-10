// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/MatrixTransformComponent.h"

#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSValuePool.h"
#include "wtf/MathExtras.h"
#include <cmath>

namespace blink {

PassRefPtrWillBeRawPtr<CSSFunctionValue> MatrixTransformComponent::toCSSValue() const
{
    RefPtrWillBeRawPtr<CSSFunctionValue> result = CSSFunctionValue::create(m_is2D ? CSSValueMatrix : CSSValueMatrix3d);

    if (m_is2D) {
        double values[6] = {a(), b(), c(), d(), e(), f()};
        for (double value : values) {
            result->append(cssValuePool().createValue(value, CSSPrimitiveValue::UnitType::Number));
        }
    } else {
        double values[16] = {m11(), m12(), m13(), m14(), m21(), m22(), m23(), m24(),
            m31(), m32(), m33(), m34(), m41(), m42(), m43(), m44()};
        for (double value : values) {
            result->append(cssValuePool().createValue(value, CSSPrimitiveValue::UnitType::Number));
        }
    }

    return result.release();
}

MatrixTransformComponent* MatrixTransformComponent::perspective(double length)
{
    OwnPtr<TransformationMatrix> matrix = TransformationMatrix::create();
    if (length != 0)
        matrix->setM34(-1 / length);
    return new MatrixTransformComponent(matrix.release(), PerspectiveType);
}

MatrixTransformComponent* MatrixTransformComponent::rotate(double angle)
{
    OwnPtr<TransformationMatrix> matrix = TransformationMatrix::create();
    matrix->rotate(angle);
    return new MatrixTransformComponent(matrix.release(), RotationType);
}

MatrixTransformComponent* MatrixTransformComponent::rotate3d(double angle, double x, double y, double z)
{
    OwnPtr<TransformationMatrix> matrix = TransformationMatrix::create();
    matrix->rotate3d(x, y, z, angle);
    return new MatrixTransformComponent(matrix.release(), Rotation3DType);
}

MatrixTransformComponent* MatrixTransformComponent::scale(double x, double y)
{
    OwnPtr<TransformationMatrix> matrix = TransformationMatrix::create();
    matrix->setM11(x);
    matrix->setM22(y);
    return new MatrixTransformComponent(matrix.release(), ScaleType);
}

MatrixTransformComponent* MatrixTransformComponent::scale3d(double x, double y, double z)
{
    OwnPtr<TransformationMatrix> matrix = TransformationMatrix::create();
    matrix->setM11(x);
    matrix->setM22(y);
    matrix->setM33(z);
    return new MatrixTransformComponent(matrix.release(), Scale3DType);
}

MatrixTransformComponent* MatrixTransformComponent::skew(double ax, double ay)
{
    double tanAx = std::tan(deg2rad(ax));
    double tanAy = std::tan(deg2rad(ay));

    OwnPtr<TransformationMatrix> matrix = TransformationMatrix::create();
    matrix->setM12(tanAy);
    matrix->setM21(tanAx);
    return new MatrixTransformComponent(matrix.release(), SkewType);
}

MatrixTransformComponent* MatrixTransformComponent::translate(double x, double y)
{
    OwnPtr<TransformationMatrix> matrix = TransformationMatrix::create();
    matrix->setM41(x);
    matrix->setM42(y);
    return new MatrixTransformComponent(matrix.release(), TranslateType);
}

MatrixTransformComponent* MatrixTransformComponent::translate3d(double x, double y, double z)
{
    OwnPtr<TransformationMatrix> matrix = TransformationMatrix::create();
    matrix->setM41(x);
    matrix->setM42(y);
    matrix->setM43(z);
    return new MatrixTransformComponent(matrix.release(), Translate3DType);
}

} // namespace blink
