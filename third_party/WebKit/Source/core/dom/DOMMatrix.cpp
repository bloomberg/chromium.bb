// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/dom/DOMMatrix.h"

namespace blink {

DOMMatrix* DOMMatrix::create()
{
    return new DOMMatrix(TransformationMatrix());
}

DOMMatrix* DOMMatrix::create(DOMMatrixReadOnly* other)
{
    return new DOMMatrix(other->matrix(), other->is2D());
}

DOMMatrix::DOMMatrix(const TransformationMatrix& matrix, bool is2D)
{
    m_matrix = matrix;
    m_is2D = is2D;
}

void DOMMatrix::setIs2D(bool value)
{
    if (m_is2D)
        m_is2D = value;
}

DOMMatrix* DOMMatrix::translateSelf(double tx, double ty, double tz)
{
    if (!tx && !ty && !tz)
        return this;

    if (tz)
        m_is2D = false;

    if (m_is2D)
        m_matrix.translate(tx, ty);
    else
        m_matrix.translate3d(tx, ty, tz);

    return this;
}

} // namespace blink
