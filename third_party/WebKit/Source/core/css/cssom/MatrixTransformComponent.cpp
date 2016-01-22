// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/MatrixTransformComponent.h"

#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSValuePool.h"

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

} // namespace blink
