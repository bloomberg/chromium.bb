// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/dom/DOMMatrix.h"

namespace blink {

DOMMatrix* DOMMatrix::create()
{
    return new DOMMatrix(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1);
}

DOMMatrix* DOMMatrix::create(DOMMatrixReadOnly* other)
{
    return new DOMMatrix(other->m11(), other->m12(), other->m13(), other->m14(),
        other->m21(), other->m22(), other->m23(), other->m24(),
        other->m31(), other->m32(), other->m33(), other->m34(),
        other->m41(), other->m42(), other->m43(), other->m44(),
        other->is2D());
}

DOMMatrix::DOMMatrix(double m11, double m12, double m13, double m14,
    double m21, double m22, double m23, double m24,
    double m31, double m32, double m33, double m34,
    double m41, double m42, double m43, double m44,
    bool is2D)
{
    m_matrix[0][0] = m11;
    m_matrix[0][1] = m12;
    m_matrix[0][2] = m13;
    m_matrix[0][3] = m14;
    m_matrix[1][0] = m21;
    m_matrix[1][1] = m22;
    m_matrix[1][2] = m23;
    m_matrix[1][3] = m24;
    m_matrix[2][0] = m31;
    m_matrix[2][1] = m32;
    m_matrix[2][2] = m33;
    m_matrix[2][3] = m34;
    m_matrix[3][0] = m41;
    m_matrix[3][1] = m42;
    m_matrix[3][2] = m43;
    m_matrix[3][3] = m44;
    m_is2D = is2D;
}

void DOMMatrix::setIs2D(bool value)
{
    if (m_is2D)
        m_is2D = value;
}

} // namespace blink
