// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/dom/DOMMatrix.h"

namespace blink {

bool DOMMatrixReadOnly::is2D() const
{
    return m_is2D;
}

bool DOMMatrixReadOnly::isIdentity() const
{
    return m_matrix.isIdentity();
}

DOMMatrix* DOMMatrixReadOnly::multiply(DOMMatrix* other)
{
    return DOMMatrix::create(this)->multiplySelf(other);
}

DOMMatrix* DOMMatrixReadOnly::translate(double tx, double ty, double tz)
{
    return DOMMatrix::create(this)->translateSelf(tx, ty, tz);
}

DOMMatrix* DOMMatrixReadOnly::scale(double scale, double ox, double oy)
{
    return DOMMatrix::create(this)->scaleSelf(scale, ox, oy);
}

DOMMatrix* DOMMatrixReadOnly::scale3d(double scale, double ox, double oy, double oz)
{
    return DOMMatrix::create(this)->scale3dSelf(scale, ox, oy, oz);
}

DOMMatrix* DOMMatrixReadOnly::scaleNonUniform(double sx, double sy, double sz,
    double ox, double oy, double oz)
{
    return DOMMatrix::create(this)->scaleNonUniformSelf(sx, sy, sz, ox, oy, oz);
}

PassRefPtr<Float32Array> DOMMatrixReadOnly::toFloat32Array() const
{
    float array[] = {
        m_matrix.m11(), m_matrix.m12(), m_matrix.m13(), m_matrix.m14(),
        m_matrix.m21(), m_matrix.m22(), m_matrix.m23(), m_matrix.m24(),
        m_matrix.m31(), m_matrix.m32(), m_matrix.m33(), m_matrix.m34(),
        m_matrix.m41(), m_matrix.m42(), m_matrix.m43(), m_matrix.m44()
    };

    return Float32Array::create(array, 16);
}

PassRefPtr<Float64Array> DOMMatrixReadOnly::toFloat64Array() const
{
    double array[] = {
        m_matrix.m11(), m_matrix.m12(), m_matrix.m13(), m_matrix.m14(),
        m_matrix.m21(), m_matrix.m22(), m_matrix.m23(), m_matrix.m24(),
        m_matrix.m31(), m_matrix.m32(), m_matrix.m33(), m_matrix.m34(),
        m_matrix.m41(), m_matrix.m42(), m_matrix.m43(), m_matrix.m44()
    };

    return Float64Array::create(array, 16);
}

} // namespace blink
