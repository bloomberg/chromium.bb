// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/CSSFontWeightInterpolationType.h"

#include "core/css/CSSPrimitiveValueMappings.h"
#include "core/css/resolver/StyleResolverState.h"

namespace blink {

static double fontWeightToDouble(FontWeight fontWeight)
{
    switch (fontWeight) {
    case FontWeight100:
        return 100;
    case FontWeight200:
        return 200;
    case FontWeight300:
        return 300;
    case FontWeight400:
        return 400;
    case FontWeight500:
        return 500;
    case FontWeight600:
        return 600;
    case FontWeight700:
        return 700;
    case FontWeight800:
        return 800;
    case FontWeight900:
        return 900;
    default:
        ASSERT_NOT_REACHED();
        return 400;
    }
}

static FontWeight doubleToFontWeight(double value)
{
    static const FontWeight fontWeights[] = {
        FontWeight100,
        FontWeight200,
        FontWeight300,
        FontWeight400,
        FontWeight500,
        FontWeight600,
        FontWeight700,
        FontWeight800,
        FontWeight900,
    };

    int index = round(value / 100 - 1);
    int clampedIndex = clampTo<int>(index, 0, WTF_ARRAY_LENGTH(fontWeights) - 1);
    return fontWeights[clampedIndex];
}

class ParentFontWeightChecker : public InterpolationType::ConversionChecker {
public:
    static PassOwnPtr<ParentFontWeightChecker> create(const InterpolationType& type, FontWeight fontWeight)
    {
        return adoptPtr(new ParentFontWeightChecker(type, fontWeight));
    }

private:
    ParentFontWeightChecker(const InterpolationType& type, FontWeight fontWeight)
        : ConversionChecker(type)
        , m_fontWeight(fontWeight)
    { }

    bool isValid(const InterpolationEnvironment& environment, const InterpolationValue&) const final
    {
        return m_fontWeight == environment.state().parentStyle()->fontWeight();
    }

    DEFINE_INLINE_VIRTUAL_TRACE() { ConversionChecker::trace(visitor); }

    const double m_fontWeight;
};

InterpolationValue CSSFontWeightInterpolationType::createFontWeightValue(FontWeight fontWeight) const
{
    return InterpolationValue(InterpolableNumber::create(fontWeightToDouble(fontWeight)));
}

InterpolationValue CSSFontWeightInterpolationType::maybeConvertNeutral(const InterpolationValue&, ConversionCheckers&) const
{
    return InterpolationValue(InterpolableNumber::create(0));
}

InterpolationValue CSSFontWeightInterpolationType::maybeConvertInitial() const
{
    return createFontWeightValue(FontWeightNormal);
}

InterpolationValue CSSFontWeightInterpolationType::maybeConvertInherit(const StyleResolverState& state, ConversionCheckers& conversionCheckers) const
{
    if (!state.parentStyle())
        return nullptr;
    FontWeight inheritedFontWeight = state.parentStyle()->fontWeight();
    conversionCheckers.append(ParentFontWeightChecker::create(*this, inheritedFontWeight));
    return createFontWeightValue(inheritedFontWeight);
}

InterpolationValue CSSFontWeightInterpolationType::maybeConvertValue(const CSSValue& value, const StyleResolverState& state, ConversionCheckers& conversionCheckers) const
{
    if (!value.isPrimitiveValue())
        return nullptr;

    const CSSPrimitiveValue& primitiveValue = toCSSPrimitiveValue(value);
    CSSValueID keyword = primitiveValue.getValueID();

    switch (keyword) {
    case CSSValueInvalid:
        return nullptr;

    case CSSValueBolder:
    case CSSValueLighter: {
        FontWeight inheritedFontWeight = state.parentStyle()->fontWeight();
        conversionCheckers.append(ParentFontWeightChecker::create(*this, inheritedFontWeight));
        if (keyword == CSSValueBolder)
            return createFontWeightValue(FontDescription::bolderWeight(inheritedFontWeight));
        return createFontWeightValue(FontDescription::lighterWeight(inheritedFontWeight));
    }

    default:
        return createFontWeightValue(primitiveValue.convertTo<FontWeight>());
    }
}

InterpolationValue CSSFontWeightInterpolationType::maybeConvertUnderlyingValue(const InterpolationEnvironment& environment) const
{
    return createFontWeightValue(environment.state().style()->fontWeight());
}

void CSSFontWeightInterpolationType::apply(const InterpolableValue& interpolableValue, const NonInterpolableValue*, InterpolationEnvironment& environment) const
{
    environment.state().fontBuilder().setWeight(doubleToFontWeight(toInterpolableNumber(interpolableValue).value()));
}

} // namespace blink
