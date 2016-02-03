// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/CSSLengthListInterpolationType.h"

#include "core/animation/CSSLengthInterpolationType.h"
#include "core/animation/LengthListPropertyFunctions.h"
#include "core/animation/ListInterpolationFunctions.h"
#include "core/animation/UnderlyingLengthChecker.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSValueList.h"
#include "core/css/resolver/StyleResolverState.h"

namespace blink {

CSSLengthListInterpolationType::CSSLengthListInterpolationType(CSSPropertyID property)
    : CSSInterpolationType(property)
    , m_valueRange(LengthListPropertyFunctions::valueRange(property))
{
}

InterpolationValue CSSLengthListInterpolationType::maybeConvertNeutral(const InterpolationValue& underlying, ConversionCheckers& conversionCheckers) const
{
    size_t underlyingLength = UnderlyingLengthChecker::getUnderlyingLength(underlying);
    conversionCheckers.append(UnderlyingLengthChecker::create(*this, underlyingLength));

    if (underlyingLength == 0)
        return nullptr;

    return ListInterpolationFunctions::createList(underlyingLength, [](size_t) {
        return InterpolationValue(CSSLengthInterpolationType::createNeutralInterpolableValue());
    });
}

InterpolationValue CSSLengthListInterpolationType::maybeConvertInitial() const
{
    return maybeConvertLengthList(LengthListPropertyFunctions::getInitialLengthList(cssProperty()), 1);
}

InterpolationValue CSSLengthListInterpolationType::maybeConvertLengthList(const RefVector<Length>* lengthList, float zoom) const
{
    if (!lengthList || lengthList->size() == 0)
        return nullptr;

    return ListInterpolationFunctions::createList(lengthList->size(), [lengthList, zoom](size_t index) {
        return CSSLengthInterpolationType::maybeConvertLength(lengthList->at(index), zoom);
    });
}

class ParentLengthListChecker : public InterpolationType::ConversionChecker {
public:
    ~ParentLengthListChecker() final {}

    static PassOwnPtr<ParentLengthListChecker> create(const InterpolationType& type, CSSPropertyID property, PassRefPtr<RefVector<Length>> inheritedLengthList)
    {
        return adoptPtr(new ParentLengthListChecker(type, property, inheritedLengthList));
    }

private:
    ParentLengthListChecker(const InterpolationType& type, CSSPropertyID property, PassRefPtr<RefVector<Length>> inheritedLengthList)
        : ConversionChecker(type)
        , m_property(property)
        , m_inheritedLengthList(inheritedLengthList)
    { }

    bool isValid(const InterpolationEnvironment& environment, const InterpolationValue& underlying) const final
    {
        const RefVector<Length>* lengthList = LengthListPropertyFunctions::getLengthList(m_property, *environment.state().parentStyle());
        if (!lengthList && !m_inheritedLengthList)
            return true;
        if (!lengthList || !m_inheritedLengthList)
            return false;
        return *m_inheritedLengthList == *lengthList;
    }

    CSSPropertyID m_property;
    RefPtr<RefVector<Length>> m_inheritedLengthList;
};

InterpolationValue CSSLengthListInterpolationType::maybeConvertInherit(const StyleResolverState& state, ConversionCheckers& conversionCheckers) const
{
    if (!state.parentStyle())
        return nullptr;

    const RefVector<Length>* inheritedLengthList = LengthListPropertyFunctions::getLengthList(cssProperty(), *state.parentStyle());
    conversionCheckers.append(ParentLengthListChecker::create(*this, cssProperty(),
        const_cast<RefVector<Length>*>(inheritedLengthList))); // Take ref.
    return maybeConvertLengthList(inheritedLengthList, state.parentStyle()->effectiveZoom());
}

InterpolationValue CSSLengthListInterpolationType::maybeConvertValue(const CSSValue& value, const StyleResolverState&, ConversionCheckers&) const
{
    if (!value.isBaseValueList())
        return nullptr;

    const CSSValueList& list = toCSSValueList(value);
    return ListInterpolationFunctions::createList(list.length(), [&list](size_t index) {
        return CSSLengthInterpolationType::maybeConvertCSSValue(*list.item(index));
    });
}

PairwiseInterpolationValue CSSLengthListInterpolationType::mergeSingleConversions(InterpolationValue& start, InterpolationValue& end) const
{
    return ListInterpolationFunctions::mergeSingleConversions(start, end, CSSLengthInterpolationType::staticMergeSingleConversions);
}

InterpolationValue CSSLengthListInterpolationType::maybeConvertUnderlyingValue(const InterpolationEnvironment& environment) const
{
    const RefVector<Length>* underlyingLengthList = LengthListPropertyFunctions::getLengthList(cssProperty(), *environment.state().style());
    return maybeConvertLengthList(underlyingLengthList, environment.state().style()->effectiveZoom());
}

void CSSLengthListInterpolationType::composite(UnderlyingValueOwner& underlyingValueOwner, double underlyingFraction, const InterpolationValue& value) const
{
    ListInterpolationFunctions::composite(underlyingValueOwner, underlyingFraction, *this, value,
        CSSLengthInterpolationType::nonInterpolableValuesAreCompatible,
        CSSLengthInterpolationType::composite);
}

void CSSLengthListInterpolationType::apply(const InterpolableValue& interpolableValue, const NonInterpolableValue* nonInterpolableValue, InterpolationEnvironment& environment) const
{
    const InterpolableList& interpolableList = toInterpolableList(interpolableValue);
    const size_t length = interpolableList.length();
    ASSERT(length > 0);
    const NonInterpolableList& nonInterpolableList = toNonInterpolableList(*nonInterpolableValue);
    ASSERT(nonInterpolableList.length() == length);
    RefPtr<RefVector<Length>> result = RefVector<Length>::create();
    for (size_t i = 0; i < length; i++) {
        result->append(CSSLengthInterpolationType::resolveInterpolableLength(
            *interpolableList.get(i),
            nonInterpolableList.get(i),
            environment.state().cssToLengthConversionData(),
            m_valueRange));
    }
    LengthListPropertyFunctions::setLengthList(cssProperty(), *environment.state().style(), result.release());
}

} // namespace blink
