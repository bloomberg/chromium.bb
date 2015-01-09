// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/animation/ShadowStyleInterpolation.h"

#include "core/animation/ColorStyleInterpolation.h"
#include "core/animation/LengthStyleInterpolation.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSValuePool.h"
#include "core/css/resolver/StyleBuilder.h"

namespace blink {

bool ShadowStyleInterpolation::canCreateFrom(const CSSValue& value)
{
    return value.isShadowValue();
}
PassOwnPtrWillBeRawPtr<InterpolableValue> ShadowStyleInterpolation::shadowToInterpolableValue(const CSSValue& value)
{
    OwnPtrWillBeRawPtr<InterpolableList> result = InterpolableList::create(6);
    const CSSShadowValue* shadowValue = toCSSShadowValue(&value);
    ASSERT(shadowValue);

    result->set(0, LengthStyleInterpolation::lengthToInterpolableValue(*shadowValue->x));
    result->set(1, LengthStyleInterpolation::lengthToInterpolableValue(*shadowValue->y));
    result->set(2, LengthStyleInterpolation::lengthToInterpolableValue(*shadowValue->blur));
    result->set(3, LengthStyleInterpolation::lengthToInterpolableValue(*shadowValue->spread));
    result->set(4, ColorStyleInterpolation::colorToInterpolableValue(*shadowValue->color));

    return result.release();
}

PassRefPtrWillBeRawPtr<CSSValue> ShadowStyleInterpolation::interpolableValueToShadow(InterpolableValue* value, bool styleFlag)
{
    InterpolableList* shadow = toInterpolableList(value);
    RefPtrWillBeRawPtr<CSSPrimitiveValue> x = LengthStyleInterpolation::interpolableValueToLength(shadow->get(0), ValueRangeNonNegative);
    RefPtrWillBeRawPtr<CSSPrimitiveValue> y = LengthStyleInterpolation::interpolableValueToLength(shadow->get(1), ValueRangeNonNegative);
    RefPtrWillBeRawPtr<CSSPrimitiveValue> blur = LengthStyleInterpolation::interpolableValueToLength(shadow->get(2), ValueRangeNonNegative);
    RefPtrWillBeRawPtr<CSSPrimitiveValue> spread = LengthStyleInterpolation::interpolableValueToLength(shadow->get(3), ValueRangeNonNegative);
    RefPtrWillBeRawPtr<CSSPrimitiveValue> color = ColorStyleInterpolation::interpolableValueToColor(*(shadow->get(4)));

    RefPtrWillBeRawPtr<CSSPrimitiveValue> style = styleFlag ? CSSPrimitiveValue::createIdentifier(CSSValueInset) : CSSPrimitiveValue::createIdentifier(CSSValueNone);

    RefPtrWillBeRawPtr<CSSShadowValue> result = CSSShadowValue::create(x, y, blur, spread, style, color);
    return result.release();
}

PassRefPtrWillBeRawPtr<ShadowStyleInterpolation> ShadowStyleInterpolation::maybeCreateFromShadow(const CSSValue& start, const CSSValue& end, CSSPropertyID id)
{
    if (canCreateFrom(start) && canCreateFrom(end)) {
        if (toCSSShadowValue(start).style == toCSSShadowValue(end).style) {
            bool styleFlag = (toCSSShadowValue(start).style->getValueID() == CSSValueInset) ? true : false;
            return ShadowStyleInterpolation::create(start, end, id, styleFlag);
        }
    }
    return nullptr;
}


void ShadowStyleInterpolation::apply(StyleResolverState& state) const
{
    StyleBuilder::applyProperty(m_id, state, interpolableValueToShadow(m_cachedValue.get(), m_styleFlag).get());
}

void ShadowStyleInterpolation::trace(Visitor* visitor)
{
    StyleInterpolation::trace(visitor);
}

}
