// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/PositionValue.h"

#include "core/css/CSSValuePair.h"

namespace blink {

PassRefPtrWillBeRawPtr<CSSValue> PositionValue::toCSSValue() const
{
    return CSSValuePair::create(m_x->toCSSValue(), m_y->toCSSValue(), CSSValuePair::KeepIdenticalValues);
}

} // namespace blink
