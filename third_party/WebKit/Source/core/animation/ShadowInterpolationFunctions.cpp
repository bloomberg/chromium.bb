// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/animation/ShadowInterpolationFunctions.h"

#include "core/animation/ColorInterpolationType.h"
#include "core/animation/InterpolationValue.h"
#include "core/animation/LengthInterpolationType.h"
#include "core/animation/NonInterpolableValue.h"
#include "core/css/CSSShadowValue.h"
#include "core/css/resolver/StyleResolverState.h"
#include "core/style/ShadowData.h"
#include "platform/geometry/FloatPoint.h"

namespace blink {

enum ShadowComponentIndex {
    ShadowX,
    ShadowY,
    ShadowBlur,
    ShadowSpread,
    ShadowColor,
    ShadowComponentIndexCount,
};

class ShadowNonInterpolableValue : public NonInterpolableValue {
public:
    ~ShadowNonInterpolableValue() final { }

    static PassRefPtr<ShadowNonInterpolableValue> create(ShadowStyle shadowStyle)
    {
        return adoptRef(new ShadowNonInterpolableValue(shadowStyle));
    }

    ShadowStyle style() const { return m_style; }

    DECLARE_NON_INTERPOLABLE_VALUE_TYPE();

private:
    ShadowNonInterpolableValue(ShadowStyle shadowStyle)
        : m_style(shadowStyle)
    { }

    ShadowStyle m_style;
};

DEFINE_NON_INTERPOLABLE_VALUE_TYPE(ShadowNonInterpolableValue);
DEFINE_NON_INTERPOLABLE_VALUE_TYPE_CASTS(ShadowNonInterpolableValue);

bool ShadowInterpolationFunctions::nonInterpolableValuesAreCompatible(const NonInterpolableValue* a, const NonInterpolableValue* b)
{
    return toShadowNonInterpolableValue(*a).style() == toShadowNonInterpolableValue(*b).style();
}

PassRefPtr<NonInterpolableValue> ShadowInterpolationFunctions::mergeNonInterpolableValues(const NonInterpolableValue* a, const NonInterpolableValue* b)
{
    ASSERT(nonInterpolableValuesAreCompatible(a, b));
    return const_cast<NonInterpolableValue*>(b); // Take ref.
}

InterpolationComponentValue ShadowInterpolationFunctions::convertShadowData(const ShadowData& shadowData, double zoom)
{
    OwnPtr<InterpolableList> interpolableList = InterpolableList::create(ShadowComponentIndexCount);
    interpolableList->set(ShadowX, LengthInterpolationType::createInterpolablePixels(shadowData.x() / zoom));
    interpolableList->set(ShadowY, LengthInterpolationType::createInterpolablePixels(shadowData.y() / zoom));
    interpolableList->set(ShadowBlur, LengthInterpolationType::createInterpolablePixels(shadowData.blur() / zoom));
    interpolableList->set(ShadowSpread, LengthInterpolationType::createInterpolablePixels(shadowData.spread() / zoom));
    interpolableList->set(ShadowColor, ColorInterpolationType::createInterpolableColor(shadowData.color()));
    return InterpolationComponentValue(interpolableList.release(), ShadowNonInterpolableValue::create(shadowData.style()));
}

InterpolationComponentValue ShadowInterpolationFunctions::maybeConvertCSSValue(const CSSValue& value)
{
    if (!value.isShadowValue())
        return nullptr;
    const CSSShadowValue& shadow = toCSSShadowValue(value);

    ShadowStyle style = Normal;
    if (shadow.style) {
        if (shadow.style->getValueID() == CSSValueInset)
            style = Inset;
        else
            return nullptr;
    }

    OwnPtr<InterpolableList> interpolableList = InterpolableList::create(ShadowComponentIndexCount);
    static_assert(ShadowX == 0, "Enum ordering check.");
    static_assert(ShadowY == 1, "Enum ordering check.");
    static_assert(ShadowBlur == 2, "Enum ordering check.");
    static_assert(ShadowSpread == 3, "Enum ordering check.");
    const CSSPrimitiveValue* lengths[] = {
        shadow.x.get(),
        shadow.y.get(),
        shadow.blur.get(),
        shadow.spread.get(),
    };
    for (size_t i = 0; i < WTF_ARRAY_LENGTH(lengths); i++) {
        if (lengths[i]) {
            InterpolationComponentValue component = LengthInterpolationType::maybeConvertCSSValue(*lengths[i]);
            if (!component)
                return nullptr;
            ASSERT(!component.nonInterpolableValue);
            interpolableList->set(i, component.interpolableValue.release());
        } else {
            interpolableList->set(i, LengthInterpolationType::createInterpolablePixels(0));
        }
    }

    if (shadow.color) {
        OwnPtr<InterpolableValue> interpolableColor = ColorInterpolationType::maybeCreateInterpolableColor(*shadow.color);
        if (!interpolableColor)
            return nullptr;
        interpolableList->set(ShadowColor, interpolableColor.release());
    } else {
        interpolableList->set(ShadowColor, ColorInterpolationType::createInterpolableColor(StyleColor::currentColor()));
    }

    return InterpolationComponentValue(interpolableList.release(), ShadowNonInterpolableValue::create(style));
}

void ShadowInterpolationFunctions::composite(OwnPtr<InterpolableValue>& underlyingInterpolableValue, RefPtr<NonInterpolableValue>& underlyingNonInterpolableValue, double underlyingFraction, const InterpolableValue& interpolableValue, const NonInterpolableValue* nonInterpolableValue)
{
    ASSERT(nonInterpolableValuesAreCompatible(underlyingNonInterpolableValue.get(), nonInterpolableValue));
    InterpolableList& underlyingInterpolableList = toInterpolableList(*underlyingInterpolableValue);
    const InterpolableList& interpolableList = toInterpolableList(interpolableValue);
    underlyingInterpolableList.scaleAndAdd(underlyingFraction, interpolableList);
}

ShadowData ShadowInterpolationFunctions::createShadowData(const InterpolableValue& interpolableValue, const NonInterpolableValue* nonInterpolableValue, const StyleResolverState& state)
{
    const InterpolableList& interpolableList = toInterpolableList(interpolableValue);
    const ShadowNonInterpolableValue& shadowNonInterpolableValue = toShadowNonInterpolableValue(*nonInterpolableValue);
    const CSSToLengthConversionData& conversionData = state.cssToLengthConversionData();
    Length shadowX = LengthInterpolationType::resolveInterpolableLength(*interpolableList.get(ShadowX), nullptr, conversionData);
    Length shadowY = LengthInterpolationType::resolveInterpolableLength(*interpolableList.get(ShadowY), nullptr, conversionData);
    Length shadowBlur = LengthInterpolationType::resolveInterpolableLength(*interpolableList.get(ShadowBlur), nullptr, conversionData, ValueRangeNonNegative);
    Length shadowSpread = LengthInterpolationType::resolveInterpolableLength(*interpolableList.get(ShadowSpread), nullptr, conversionData);
    ASSERT(shadowX.isFixed() && shadowY.isFixed() && shadowBlur.isFixed() && shadowSpread.isFixed());
    return ShadowData(
        FloatPoint(shadowX.value(), shadowY.value()), shadowBlur.value(), shadowSpread.value(), shadowNonInterpolableValue.style(),
        ColorInterpolationType::resolveInterpolableColor(*interpolableList.get(ShadowColor), state));
}

} // namespace blink
