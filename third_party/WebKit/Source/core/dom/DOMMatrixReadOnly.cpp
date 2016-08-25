// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/DOMMatrix.h"

namespace blink {

DOMMatrixReadOnly* DOMMatrixReadOnly::create(Vector<double> sequence, ExceptionState& exceptionState)
{
    if (sequence.size() != 6 && sequence.size() != 16) {
        exceptionState.throwTypeError("An invalid number sequence is specified. The sequence must contain 6 elements for 2D matrix and 16 elements for 3D matrix.");
        return nullptr;
    }
    return new DOMMatrixReadOnly(sequence);
}

DOMMatrixReadOnly::DOMMatrixReadOnly(Vector<double> sequence)
{
    if (sequence.size() == 6) {
        m_matrix = TransformationMatrix::create(
            sequence[0], sequence[1], sequence[2], sequence[3],
            sequence[4], sequence[5]);
        m_is2D = true;
    } else if (sequence.size() == 16) {
        m_matrix = TransformationMatrix::create(
            sequence[0], sequence[1], sequence[2], sequence[3],
            sequence[4], sequence[5], sequence[6], sequence[7],
            sequence[8], sequence[9], sequence[10], sequence[11],
            sequence[12], sequence[13], sequence[14], sequence[15]);
        m_is2D = false;
    } else {
        NOTREACHED();
    }
}

DOMMatrixReadOnly::~DOMMatrixReadOnly()
{
}

bool DOMMatrixReadOnly::is2D() const
{
    return m_is2D;
}

bool DOMMatrixReadOnly::isIdentity() const
{
    return m_matrix->isIdentity();
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

DOMMatrix* DOMMatrixReadOnly::skewX(double sx)
{
    return DOMMatrix::create(this)->skewXSelf(sx);
}

DOMMatrix* DOMMatrixReadOnly::skewY(double sy)
{
    return DOMMatrix::create(this)->skewYSelf(sy);
}

DOMMatrix* DOMMatrixReadOnly::flipX()
{
    DOMMatrix* flipX = DOMMatrix::create(this);
    flipX->setM11(-this->m11());
    flipX->setM12(-this->m12());
    flipX->setM13(-this->m13());
    flipX->setM14(-this->m14());
    return flipX;
}

DOMMatrix* DOMMatrixReadOnly::flipY()
{
    DOMMatrix* flipY = DOMMatrix::create(this);
    flipY->setM21(-this->m21());
    flipY->setM22(-this->m22());
    flipY->setM23(-this->m23());
    flipY->setM24(-this->m24());
    return flipY;
}

DOMFloat32Array* DOMMatrixReadOnly::toFloat32Array() const
{
    float array[] = {
        static_cast<float>(m_matrix->m11()), static_cast<float>(m_matrix->m12()), static_cast<float>(m_matrix->m13()), static_cast<float>(m_matrix->m14()),
        static_cast<float>(m_matrix->m21()), static_cast<float>(m_matrix->m22()), static_cast<float>(m_matrix->m23()), static_cast<float>(m_matrix->m24()),
        static_cast<float>(m_matrix->m31()), static_cast<float>(m_matrix->m32()), static_cast<float>(m_matrix->m33()), static_cast<float>(m_matrix->m34()),
        static_cast<float>(m_matrix->m41()), static_cast<float>(m_matrix->m42()), static_cast<float>(m_matrix->m43()), static_cast<float>(m_matrix->m44())
    };

    return DOMFloat32Array::create(array, 16);
}

DOMFloat64Array* DOMMatrixReadOnly::toFloat64Array() const
{
    double array[] = {
        m_matrix->m11(), m_matrix->m12(), m_matrix->m13(), m_matrix->m14(),
        m_matrix->m21(), m_matrix->m22(), m_matrix->m23(), m_matrix->m24(),
        m_matrix->m31(), m_matrix->m32(), m_matrix->m33(), m_matrix->m34(),
        m_matrix->m41(), m_matrix->m42(), m_matrix->m43(), m_matrix->m44()
    };

    return DOMFloat64Array::create(array, 16);
}

const String DOMMatrixReadOnly::toString() const
{
    std::stringstream stream;
    if (is2D()) {
        stream << "matrix("
        << a() << ", " << b() << ", " << c() << ", "
        << d() << ", " << e() << ", " << f();
    } else {
        stream << "matrix3d("
        << m11() << ", " << m12() << ", " << m13() << ", " << m14() << ", "
        << m21() << ", " << m22() << ", " << m23() << ", " << m24() << ", "
        << m31() << ", " << m32() << ", " << m33() << ", " << m34() << ", "
        << m41() << ", " << m42() << ", " << m43() << ", " << m44();
    }
    stream << ")";

    return String(stream.str().c_str());
}

} // namespace blink
