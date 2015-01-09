// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/animation/ColorStyleInterpolation.h"

#include "core/css/CSSPrimitiveValue.h"
#include "core/css/resolver/StyleBuilder.h"
#include "wtf/MathExtras.h"

#include <algorithm>

namespace blink {

bool ColorStyleInterpolation::canCreateFrom(const CSSValue& value)
{
    return value.isPrimitiveValue() && toCSSPrimitiveValue(value).isRGBColor();
}

PassOwnPtrWillBeRawPtr<InterpolableValue> ColorStyleInterpolation::colorToInterpolableValue(const CSSValue& value)
{
    ASSERT(value.isPrimitiveValue());
    const CSSPrimitiveValue& primitive = toCSSPrimitiveValue(value);
    const RGBA32 color = primitive.getRGBA32Value();
    int alpha = alphaChannel(color);

    OwnPtrWillBeRawPtr<InterpolableList> list = InterpolableList::create(4);
    list->set(0, InterpolableNumber::create(redChannel(color) * alpha));
    list->set(1, InterpolableNumber::create(greenChannel(color) * alpha));
    list->set(2, InterpolableNumber::create(blueChannel(color) * alpha));
    list->set(3, InterpolableNumber::create(alpha));

    return list->clone();
}

PassRefPtrWillBeRawPtr<CSSPrimitiveValue> ColorStyleInterpolation::interpolableValueToColor(const InterpolableValue& value)
{
    ASSERT(value.isList());
    const InterpolableList* list = toInterpolableList(&value);

    double alpha = toInterpolableNumber(list->get(3))->value();
    if (!alpha)
        return CSSPrimitiveValue::createColor(Color::transparent);

    // Clamping is inside makeRGBA.
    unsigned rgba = makeRGBA(
        round(toInterpolableNumber(list->get(0))->value() / alpha),
        round(toInterpolableNumber(list->get(1))->value() / alpha),
        round(toInterpolableNumber(list->get(2))->value() / alpha),
        round(alpha));

    return CSSPrimitiveValue::createColor(rgba);
}

void ColorStyleInterpolation::apply(StyleResolverState& state) const
{
    StyleBuilder::applyProperty(m_id, state, interpolableValueToColor(*m_cachedValue).get());
}

void ColorStyleInterpolation::trace(Visitor* visitor)
{
    StyleInterpolation::trace(visitor);
}
}
