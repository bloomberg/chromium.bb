// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/SkewTransformComponent.h"

#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSValuePool.h"

namespace blink {

PassRefPtrWillBeRawPtr<CSSFunctionValue> SkewTransformComponent::toCSSValue() const
{
    RefPtrWillBeRawPtr<CSSFunctionValue> result = CSSFunctionValue::create(CSSValueSkew);
    result->append(cssValuePool().createValue(m_ax, CSSPrimitiveValue::UnitType::Number));
    result->append(cssValuePool().createValue(m_ay, CSSPrimitiveValue::UnitType::Number));
    return result.release();
}

} // namespace blink
