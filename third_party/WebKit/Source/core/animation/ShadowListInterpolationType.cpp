// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/animation/ShadowListInterpolationType.h"

#include "core/animation/ListInterpolationFunctions.h"
#include "core/animation/ShadowInterpolationFunctions.h"
#include "core/animation/ShadowListPropertyFunctions.h"
#include "core/css/CSSValueList.h"
#include "core/css/resolver/StyleBuilder.h"
#include "core/css/resolver/StyleResolverState.h"
#include "core/style/ShadowList.h"

namespace blink {

PassOwnPtr<InterpolationValue> ShadowListInterpolationType::convertShadowList(const ShadowList* shadowList, double zoom) const
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

PassOwnPtr<InterpolationValue> ShadowListInterpolationType::createNeutralValue() const
{
    InterpolationComponent emptyListComponent = ListInterpolationFunctions::createEmptyList();
    return InterpolationValue::create(*this, emptyListComponent);
}

PassOwnPtr<InterpolationValue> ShadowListInterpolationType::maybeConvertNeutral(const UnderlyingValue&, ConversionCheckers&) const
{
    return createNeutralValue();
}

PassOwnPtr<InterpolationValue> ShadowListInterpolationType::maybeConvertInitial() const
{
    return convertShadowList(ShadowListPropertyFunctions::getInitialShadowList(m_property), 1);
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

    bool isValid(const StyleResolverState& state, const UnderlyingValue&) const final
    {
        const ShadowList* parentShadowList = ShadowListPropertyFunctions::getShadowList(m_property, *state.parentStyle());
        if (!parentShadowList && !m_shadowList)
            return true;
        if (!parentShadowList || !m_shadowList)
            return false;
        return *parentShadowList == *m_shadowList;
    }

    const CSSPropertyID m_property;
    RefPtr<ShadowList> m_shadowList;
};

PassOwnPtr<InterpolationValue> ShadowListInterpolationType::maybeConvertInherit(const StyleResolverState* state, ConversionCheckers& conversionCheckers) const
{
    if (!state || !state->parentStyle())
        return nullptr;
    const ShadowList* parentShadowList = ShadowListPropertyFunctions::getShadowList(m_property, *state->parentStyle());
    conversionCheckers.append(ParentShadowListChecker::create(*this, m_property, const_cast<ShadowList*>(parentShadowList))); // Take ref.
    return convertShadowList(parentShadowList, state->parentStyle()->effectiveZoom());
}

PassOwnPtr<InterpolationValue> ShadowListInterpolationType::maybeConvertValue(const CSSValue& value, const StyleResolverState*, ConversionCheckers&) const
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

PassOwnPtr<PairwisePrimitiveInterpolation> ShadowListInterpolationType::mergeSingleConversions(InterpolationValue& startValue, InterpolationValue& endValue) const
{
    PairwiseInterpolationComponent component = ListInterpolationFunctions::mergeSingleConversions(
        startValue.mutableComponent(), endValue.mutableComponent(),
        ShadowInterpolationFunctions::mergeSingleConversions);
    if (!component)
        return nullptr;
    return PairwisePrimitiveInterpolation::create(*this, component);
}

PassOwnPtr<InterpolationValue> ShadowListInterpolationType::maybeConvertUnderlyingValue(const StyleResolverState& state) const
{
    if (!state.style())
        return nullptr;
    return convertShadowList(ShadowListPropertyFunctions::getShadowList(m_property, *state.style()), state.style()->effectiveZoom());
}

void ShadowListInterpolationType::composite(UnderlyingValue& underlyingValue, double underlyingFraction, const InterpolationValue& value) const
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

void ShadowListInterpolationType::apply(const InterpolableValue& interpolableValue, const NonInterpolableValue* nonInterpolableValue, StyleResolverState& state) const
{
    ShadowListPropertyFunctions::setShadowList(m_property, *state.style(), createShadowList(interpolableValue, nonInterpolableValue, state));
}

} // namespace blink
