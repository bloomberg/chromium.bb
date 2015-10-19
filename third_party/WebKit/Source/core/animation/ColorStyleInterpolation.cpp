// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/animation/ColorStyleInterpolation.h"

#include "core/CSSValueKeywords.h"
#include "core/css/parser/CSSPropertyParser.h"
#include "core/css/resolver/StyleBuilder.h"
#include "core/layout/LayoutTheme.h"
#include "platform/graphics/Color.h"
#include "wtf/MathExtras.h"

#include <algorithm>

namespace blink {

bool ColorStyleInterpolation::canCreateFrom(const CSSValue& value)
{
    return value.isColorValue() || (value.isPrimitiveValue() && toCSSPrimitiveValue(value).isValueID());
}

PassOwnPtr<InterpolableValue> ColorStyleInterpolation::colorToInterpolableValue(const CSSValue& value)
{
    RGBA32 color;
    if (value.isColorValue()) {
        color = toCSSColorValue(value).value();
    } else {
        const CSSPrimitiveValue& primitive = toCSSPrimitiveValue(value);
        if (CSSPropertyParser::isSystemColor(primitive.getValueID())) {
            color = LayoutTheme::theme().systemColor(primitive.getValueID()).rgb();
        } else {
            Color colorFromID;
            colorFromID.setNamedColor(getValueName(primitive.getValueID()));
            color = colorFromID.rgb();
        }
    }

    int alpha = alphaChannel(color);

    OwnPtr<InterpolableList> list = InterpolableList::create(4);
    list->set(0, InterpolableNumber::create(redChannel(color) * alpha));
    list->set(1, InterpolableNumber::create(greenChannel(color) * alpha));
    list->set(2, InterpolableNumber::create(blueChannel(color) * alpha));
    list->set(3, InterpolableNumber::create(alpha));

    return list->clone();
}

PassRefPtrWillBeRawPtr<CSSColorValue> ColorStyleInterpolation::interpolableValueToColor(const InterpolableValue& value)
{
    ASSERT(value.isList());
    const InterpolableList* list = toInterpolableList(&value);

    double alpha = toInterpolableNumber(list->get(3))->value();
    if (!alpha)
        return CSSColorValue::create(Color::transparent);

    // Clamping is inside makeRGBA.
    unsigned rgba = makeRGBA(
        round(toInterpolableNumber(list->get(0))->value() / alpha),
        round(toInterpolableNumber(list->get(1))->value() / alpha),
        round(toInterpolableNumber(list->get(2))->value() / alpha),
        round(alpha));

    return CSSColorValue::create(rgba);
}

void ColorStyleInterpolation::apply(StyleResolverState& state) const
{
    StyleBuilder::applyProperty(m_id, state, interpolableValueToColor(*m_cachedValue).get());
}

PassRefPtr<ColorStyleInterpolation> ColorStyleInterpolation::maybeCreateFromColor(const CSSValue& start, const CSSValue& end, CSSPropertyID id)
{
    if (canCreateFrom(start) && !toCSSPrimitiveValue(start).colorIsDerivedFromElement() && canCreateFrom(end) && !toCSSPrimitiveValue(end).colorIsDerivedFromElement())
        return adoptRef(new ColorStyleInterpolation(colorToInterpolableValue(start), colorToInterpolableValue(end), id));
    return nullptr;
}

bool ColorStyleInterpolation::shouldUseLegacyStyleInterpolation(const CSSValue& start, const CSSValue& end)
{
    if (ColorStyleInterpolation::canCreateFrom(start) && ColorStyleInterpolation::canCreateFrom(end)) {
        if (toCSSPrimitiveValue(start).colorIsDerivedFromElement() || toCSSPrimitiveValue(end).colorIsDerivedFromElement())
            return true;
    }
    return false;
}

}
