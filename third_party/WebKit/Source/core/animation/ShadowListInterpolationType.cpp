// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/animation/ShadowListInterpolationType.h"

#include "core/animation/ShadowInterpolationFunctions.h"
#include "core/animation/ShadowListPropertyFunctions.h"
#include "core/css/CSSValueList.h"
#include "core/css/resolver/StyleBuilder.h"
#include "core/css/resolver/StyleResolverState.h"
#include "core/style/ShadowList.h"
#include "wtf/MathExtras.h"

namespace blink {

class NonInterpolableList : public NonInterpolableValue {
public:
    ~NonInterpolableList() override { }
    static PassRefPtr<NonInterpolableList> create(Vector<RefPtr<NonInterpolableValue>>& list)
    {
        return adoptRef(new NonInterpolableList(list));
    }

    size_t length() const { return m_list.size(); }
    const NonInterpolableValue* get(size_t index) const { return m_list[index].get(); }
    NonInterpolableValue* get(size_t index) { return m_list[index].get(); }
    RefPtr<NonInterpolableValue>& getMutable(size_t index) { return m_list[index]; }

    DECLARE_NON_INTERPOLABLE_VALUE_TYPE();

private:
    NonInterpolableList(Vector<RefPtr<NonInterpolableValue>>& list)
    {
        m_list.swap(list);
    }

    Vector<RefPtr<NonInterpolableValue>> m_list;
};

DEFINE_NON_INTERPOLABLE_VALUE_TYPE(NonInterpolableList);
DEFINE_NON_INTERPOLABLE_VALUE_TYPE_CASTS(NonInterpolableList);

PassOwnPtr<InterpolationValue> ShadowListInterpolationType::convertShadowList(const ShadowList* shadowList, double zoom) const
{
    if (!shadowList)
        return createNeutralValue();

    size_t length = shadowList->shadows().size();
    ASSERT(length > 0);
    OwnPtr<InterpolableList> interpolableList = InterpolableList::create(length);
    Vector<RefPtr<NonInterpolableValue>> nonInterpolableValues(length);
    for (size_t i = 0; i < length; i++) {
        const ShadowData& shadowData = shadowList->shadows()[i];
        InterpolationComponentValue component = ShadowInterpolationFunctions::convertShadowData(shadowData, zoom);
        interpolableList->set(i, component.interpolableValue.release());
        nonInterpolableValues[i] = component.nonInterpolableValue.release();
    }
    return InterpolationValue::create(*this, interpolableList.release(), NonInterpolableList::create(nonInterpolableValues));
}

PassOwnPtr<InterpolationValue> ShadowListInterpolationType::createNeutralValue() const
{
    return InterpolationValue::create(*this, InterpolableList::create(0));
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
    size_t length = valueList.length();
    if (length == 0)
        return createNeutralValue();

    OwnPtr<InterpolableList> interpolableList = InterpolableList::create(length);
    Vector<RefPtr<NonInterpolableValue>> nonInterpolableValues(length);
    for (size_t i = 0; i < length; i++) {
        const CSSValue& innerValue = *valueList.item(i);
        InterpolationComponentValue component = ShadowInterpolationFunctions::maybeConvertCSSValue(innerValue);
        if (!component)
            return nullptr;
        interpolableList->set(i, component.interpolableValue.release());
        nonInterpolableValues[i] = component.nonInterpolableValue.release();
    }
    return InterpolationValue::create(*this, interpolableList.release(), NonInterpolableList::create(nonInterpolableValues));
}

static void repeatToLength(InterpolationComponentValue& value, size_t length)
{
    InterpolableList& interpolableList = toInterpolableList(*value.interpolableValue);
    NonInterpolableList& nonInterpolableList = toNonInterpolableList(*value.nonInterpolableValue);
    size_t currentLength = interpolableList.length();
    ASSERT(currentLength > 0);
    if (currentLength == length)
        return;
    ASSERT(currentLength < length);
    OwnPtr<InterpolableList> newInterpolableList = InterpolableList::create(length);
    Vector<RefPtr<NonInterpolableValue>> newNonInterpolableValues(length);
    for (size_t i = length; i-- > 0;) {
        newInterpolableList->set(i, i < currentLength ? interpolableList.getMutable(i).release() : interpolableList.get(i % currentLength)->clone());
        newNonInterpolableValues[i] = nonInterpolableList.get(i % currentLength);
    }
    value.interpolableValue = newInterpolableList.release();
    value.nonInterpolableValue = NonInterpolableList::create(newNonInterpolableValues);
}

static bool nonInterpolableListsAreCompatible(const NonInterpolableList& a, const NonInterpolableList& b, size_t length)
{
    for (size_t i = 0; i < length; i++) {
        if (!ShadowInterpolationFunctions::nonInterpolableValuesAreCompatible(a.get(i % a.length()), b.get(i % b.length())))
            return false;
    }
    return true;
}

PassOwnPtr<PairwisePrimitiveInterpolation> ShadowListInterpolationType::mergeSingleConversions(InterpolationValue& startValue, InterpolationValue& endValue) const
{
    size_t startLength = toInterpolableList(startValue.interpolableValue()).length();
    size_t endLength = toInterpolableList(endValue.interpolableValue()).length();

    if (startLength == 0 && endLength == 0) {
        return PairwisePrimitiveInterpolation::create(*this,
            startValue.mutableComponent().interpolableValue.release(),
            endValue.mutableComponent().interpolableValue.release(),
            nullptr);
    }

    if (startLength == 0) {
        OwnPtr<InterpolableValue> startInterpolableValue = endValue.interpolableValue().cloneAndZero();
        return PairwisePrimitiveInterpolation::create(*this,
            startInterpolableValue.release(),
            endValue.mutableComponent().interpolableValue.release(),
            endValue.mutableComponent().nonInterpolableValue.release());
    }

    if (endLength == 0) {
        OwnPtr<InterpolableValue> endInterpolableValue = startValue.interpolableValue().cloneAndZero();
        return PairwisePrimitiveInterpolation::create(*this,
            startValue.mutableComponent().interpolableValue.release(),
            endInterpolableValue.release(),
            startValue.mutableComponent().nonInterpolableValue.release());
    }

    size_t finalLength = lowestCommonMultiple(startLength, endLength);
    if (!nonInterpolableListsAreCompatible(toNonInterpolableList(*startValue.nonInterpolableValue()), toNonInterpolableList(*endValue.nonInterpolableValue()), finalLength))
        return nullptr;

    repeatToLength(startValue.mutableComponent(), finalLength);
    repeatToLength(endValue.mutableComponent(), finalLength);
    NonInterpolableList& startNonInterpolableList = toNonInterpolableList(*startValue.mutableComponent().nonInterpolableValue);
    NonInterpolableList& endNonInterpolableList = toNonInterpolableList(*endValue.mutableComponent().nonInterpolableValue);
    Vector<RefPtr<NonInterpolableValue>> newNonInterpolableValues(finalLength);
    for (size_t i = 0; i < finalLength; i++)
        newNonInterpolableValues[i] = ShadowInterpolationFunctions::mergeNonInterpolableValues(startNonInterpolableList.get(i), endNonInterpolableList.get(i));
    return PairwisePrimitiveInterpolation::create(*this,
        startValue.mutableComponent().interpolableValue.release(),
        endValue.mutableComponent().interpolableValue.release(),
        NonInterpolableList::create(newNonInterpolableValues));
}

PassOwnPtr<InterpolationValue> ShadowListInterpolationType::maybeConvertUnderlyingValue(const StyleResolverState& state) const
{
    if (!state.style())
        return nullptr;
    return convertShadowList(ShadowListPropertyFunctions::getShadowList(m_property, *state.style()), state.style()->effectiveZoom());
}

void ShadowListInterpolationType::composite(UnderlyingValue& underlyingValue, double underlyingFraction, const InterpolationValue& value) const
{
    size_t underlyingLength = toInterpolableList(underlyingValue->interpolableValue()).length();
    if (underlyingLength == 0) {
        ASSERT(!underlyingValue->nonInterpolableValue());
        underlyingValue.set(&value);
        return;
    }

    const InterpolableList& interpolableList = toInterpolableList(value.interpolableValue());
    size_t valueLength = interpolableList.length();
    if (valueLength == 0) {
        ASSERT(!value.nonInterpolableValue());
        underlyingValue.mutableComponent().interpolableValue->scale(underlyingFraction);
        return;
    }

    const NonInterpolableList& nonInterpolableList = toNonInterpolableList(*value.nonInterpolableValue());
    size_t newLength = lowestCommonMultiple(underlyingLength, valueLength);
    if (!nonInterpolableListsAreCompatible(toNonInterpolableList(*underlyingValue->nonInterpolableValue()), nonInterpolableList, newLength)) {
        underlyingValue.set(&value);
        return;
    }

    InterpolationComponentValue& underlyingComponent = underlyingValue.mutableComponent();
    if (underlyingLength < newLength)
        repeatToLength(underlyingComponent, newLength);

    InterpolableList& underlyingInterpolableList = toInterpolableList(*underlyingComponent.interpolableValue);
    NonInterpolableList& underlyingNonInterpolableList = toNonInterpolableList(*underlyingComponent.nonInterpolableValue);
    for (size_t i = 0; i < newLength; i++) {
        ShadowInterpolationFunctions::composite(
            underlyingInterpolableList.getMutable(i),
            underlyingNonInterpolableList.getMutable(i),
            underlyingFraction,
            *interpolableList.get(i % valueLength),
            nonInterpolableList.get(i % valueLength));
    }
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
