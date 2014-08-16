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
    return m_matrix.isIdentity();
}

} // namespace blink
