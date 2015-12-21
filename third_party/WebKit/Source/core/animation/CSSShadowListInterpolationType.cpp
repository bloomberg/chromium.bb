// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/CSSShadowListInterpolationType.h"

#include "core/animation/ListInterpolationFunctions.h"
#include "core/animation/ShadowInterpolationFunctions.h"
#include "core/animation/ShadowListPropertyFunctions.h"
#include "core/css/CSSValueList.h"
#include "core/css/resolver/StyleBuilder.h"
#include "core/css/resolver/StyleResolverState.h"
#include "core/style/ShadowList.h"

namespace blink {

PassOwnPtr<InterpolationValue> CSSShadowListInterpolationType::convertShadowList(const ShadowList* shadowList, double zoom) const
{
    if (!shadowList)
        return createNeutralValue();
    const ShadowDataVector& shadows = shadowList->shadows();
    InterpolationComponent listComponent = ListInterpolationFunctions::createList(shadows.size(), [&shadows, zoom](size_t index) {
        return ShadowInterpolationFunctions::convertShadowData(shadows[index], zoom);
    });
    ASSERT(listComponent);
    return InterpolationValue::create(*this, listComponent);
}

PassOwnPtr<InterpolationValue> CSSShadowListInterpolationType::createNeutralValue() const
{
    InterpolationComponent emptyListComponent = ListInterpolationFunctions::createEmptyList();
    return InterpolationValue::create(*this, emptyListComponent);
}

PassOwnPtr<InterpolationValue> CSSShadowListInterpolationType::maybeConvertNeutral(const UnderlyingValue&, ConversionCheckers&) const
{
    return createNeutralValue();
}

PassOwnPtr<InterpolationValue> CSSShadowListInterpolationType::maybeConvertInitial() const
{
    return convertShadowList(ShadowListPropertyFunctions::getInitialShadowList(cssProperty()), 1);
}

class ParentShadowListChecker : public InterpolationType::ConversionChecker {
public:
    static PassOwnPtr<ParentShadowListChecker> create(const InterpolationType& type, CSSPropertyID property, PassRefPtr<ShadowList> shadowList)
    {
        return adoptPtr(new ParentShadowListChecker(type, property, shadowList));
    }

private:
    ParentShadowListChecker(const InterpolationType& type, CSSPropertyID property, PassRefPtr<ShadowList> shadowList)
        : ConversionChecker(type)
        , m_property(property)
        , m_shadowList(shadowList)
    { }

    bool isValid(const InterpolationEnvironment& environment, const UnderlyingValue&) const final
    {
        const ShadowList* parentShadowList = ShadowListPropertyFunctions::getShadowList(m_property, *environment.state().parentStyle());
        if (!parentShadowList && !m_shadowList)
            return true;
        if (!parentShadowList || !m_shadowList)
            return false;
        return *parentShadowList == *m_shadowList;
    }

    const CSSPropertyID m_property;
    RefPtr<ShadowList> m_shadowList;
};

PassOwnPtr<InterpolationValue> CSSShadowListInterpolationType::maybeConvertInherit(const StyleResolverState& state, ConversionCheckers& conversionCheckers) const
{
    if (!state.parentStyle())
        return nullptr;
    const ShadowList* parentShadowList = ShadowListPropertyFunctions::getShadowList(cssProperty(), *state.parentStyle());
    conversionCheckers.append(ParentShadowListChecker::create(*this, cssProperty(), const_cast<ShadowList*>(parentShadowList))); // Take ref.
    return convertShadowList(parentShadowList, state.parentStyle()->effectiveZoom());
}

PassOwnPtr<InterpolationValue> CSSShadowListInterpolationType::maybeConvertValue(const CSSValue& value, const StyleResolverState&, ConversionCheckers&) const
{
    if (value.isPrimitiveValue() && toCSSPrimitiveValue(value).getValueID() == CSSValueNone)
        return createNeutralValue();

    if (!value.isBaseValueList())
        return nullptr;

    const CSSValueList& valueList = toCSSValueList(value);
    InterpolationComponent listComponent = ListInterpolationFunctions::createList(valueList.length(), [&valueList](size_t index) {
        return ShadowInterpolationFunctions::maybeConvertCSSValue(*valueList.item(index));
    });
    if (!listComponent)
        return nullptr;
    return InterpolationValue::create(*this, listComponent);
}

PassOwnPtr<PairwisePrimitiveInterpolation> CSSShadowListInterpolationType::mergeSingleConversions(InterpolationValue& startValue, InterpolationValue& endValue) const
{
    PairwiseInterpolationComponent component = ListInterpolationFunctions::mergeSingleConversions(
        startValue.mutableComponent(), endValue.mutableComponent(),
        ShadowInterpolationFunctions::mergeSingleConversions);
    if (!component)
        return nullptr;
    return PairwisePrimitiveInterpolation::create(*this, component);
}

PassOwnPtr<InterpolationValue> CSSShadowListInterpolationType::maybeConvertUnderlyingValue(const InterpolationEnvironment& environment) const
{
    if (!environment.state().style())
        return nullptr;
    return convertShadowList(ShadowListPropertyFunctions::getShadowList(cssProperty(), *environment.state().style()), environment.state().style()->effectiveZoom());
}

void CSSShadowListInterpolationType::composite(UnderlyingValue& underlyingValue, double underlyingFraction, const InterpolationValue& value) const
{
    ListInterpolationFunctions::composite(underlyingValue, underlyingFraction, value,
        ShadowInterpolationFunctions::nonInterpolableValuesAreCompatible,
        ShadowInterpolationFunctions::composite);
}

static PassRefPtr<ShadowList> createShadowList(const InterpolableValue& interpolableValue, const NonInterpolableValue* nonInterpolableValue, const StyleResolverState& state)
{
    const InterpolableList& interpolableList = toInterpolableList(interpolableValue);
    size_t length = interpolableList.length();
    if (length == 0)
        return nullptr;
    const NonInterpolableList& nonInterpolableList = toNonInterpolableList(*nonInterpolableValue);
    ShadowDataVector shadows;
    for (size_t i = 0; i < length; i++)
        shadows.append(ShadowInterpolationFunctions::createShadowData(*interpolableList.get(i), nonInterpolableList.get(i), state));
    return ShadowList::adopt(shadows);
}

void CSSShadowListInterpolationType::apply(const InterpolableValue& interpolableValue, const NonInterpolableValue* nonInterpolableValue, InterpolationEnvironment& environment) const
{
    ShadowListPropertyFunctions::setShadowList(cssProperty(), *environment.state().style(), createShadowList(interpolableValue, nonInterpolableValue, environment.state()));
}

} // namespace blink
