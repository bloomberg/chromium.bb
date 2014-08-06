// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/dom/DOMMatrixReadOnly.h"

namespace blink {

bool DOMMatrixReadOnly::is2D() const
{
    return m_is2D;
}

bool DOMMatrixReadOnly::isIdentity() const
{
    return m11() == 1 && m21() == 0 && m31() == 0 && m41() == 0
        && m12() == 0 && m22() == 1 && m32() == 0 && m42() == 0
        && m13() == 0 && m23() == 0 && m33() == 1 && m43() == 0
        && m14() == 0 && m24() == 0 && m34() == 0 && m44() == 1;
}

} // namespace blink
