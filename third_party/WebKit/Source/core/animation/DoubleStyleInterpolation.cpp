// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/animation/DoubleStyleInterpolation.h"

#include "core/css/CSSCalculationValue.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/resolver/StyleBuilder.h"

namespace blink {

PassOwnPtrWillBeRawPtr<InterpolableValue> DoubleStyleInterpolation::doubleToInterpolableValue(const CSSValue& value)
{
    ASSERT(canCreateFrom(value));
    const CSSPrimitiveValue& primitive = toCSSPrimitiveValue(value);
    return InterpolableNumber::create(primitive.getDoubleValue());
}

PassRefPtrWillBeRawPtr<CSSValue> DoubleStyleInterpolation::interpolableValueToDouble(InterpolableValue* value, ClampRange clamp)
{
    ASSERT(value->isNumber());
    double doubleValue = toInterpolableNumber(value)->value();
    if (clamp == ClampOpacity) {
        doubleValue = clampTo<float>(doubleValue, 0, nextafterf(1, 0));
    }
    return CSSPrimitiveValue::create(doubleValue, CSSPrimitiveValue::CSS_NUMBER);
}

bool DoubleStyleInterpolation::canCreateFrom(const CSSValue& value)
{
    return value.isPrimitiveValue() && toCSSPrimitiveValue(value).isNumber();
}

void DoubleStyleInterpolation::apply(StyleResolverState& state) const
{
    StyleBuilder::applyProperty(m_id, state, interpolableValueToDouble(m_cachedValue.get(), m_clamp).get());
}

void DoubleStyleInterpolation::trace(Visitor* visitor)
{
    StyleInterpolation::trace(visitor);
}
}
