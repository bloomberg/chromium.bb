// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/ScaleTransformComponent.h"

#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSValuePool.h"

namespace blink {

CSSFunctionValue* ScaleTransformComponent::toCSSValue() const
{
    CSSFunctionValue* result = CSSFunctionValue::create(m_is2D ? CSSValueScale : CSSValueScale3d);

    result->append(cssValuePool().createValue(m_x, CSSPrimitiveValue::UnitType::Number));
    result->append(cssValuePool().createValue(m_y, CSSPrimitiveValue::UnitType::Number));
    if (!m_is2D)
        result->append(cssValuePool().createValue(m_z, CSSPrimitiveValue::UnitType::Number));

    return result;
}

} // namespace blink
