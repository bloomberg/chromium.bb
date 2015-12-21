// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/CSSImageListInterpolationType.h"

#include "core/animation/CSSImageInterpolationType.h"
#include "core/animation/ImageListPropertyFunctions.h"
#include "core/animation/ListInterpolationFunctions.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSValueList.h"
#include "core/css/resolver/StyleResolverState.h"

namespace blink {

class UnderlyingImageListChecker : public InterpolationType::ConversionChecker {
public:
    ~UnderlyingImageListChecker() final {}

    static PassOwnPtr<UnderlyingImageListChecker> create(const InterpolationType& type, PassOwnPtr<InterpolationValue> underlyingValue)
    {
        return adoptPtr(new UnderlyingImageListChecker(type, underlyingValue));
    }

private:
    UnderlyingImageListChecker(const InterpolationType& type, PassOwnPtr<InterpolationValue> underlyingValue)
        : ConversionChecker(type)
        , m_underlyingValue(underlyingValue)
    { }

    bool isValid(const InterpolationEnvironment&, const UnderlyingValue& underlyingValue) const final
    {
        if (!underlyingValue && !m_underlyingValue)
            return true;
        if (!underlyingValue || !m_underlyingValue)
            return false;
        return ListInterpolationFunctions::equalValues(m_underlyingValue->component(), underlyingValue->component(), CSSImageInterpolationType::equalNonInterpolableValues);
    }

    OwnPtr<InterpolationValue> m_underlyingValue;
};

PassOwnPtr<InterpolationValue> CSSImageListInterpolationType::maybeConvertNeutral(const UnderlyingValue& underlyingValue, ConversionCheckers& conversionCheckers) const
{
    if (!underlyingValue) {
        conversionCheckers.append(UnderlyingImageListChecker::create(*this, nullptr));
        return nullptr;
    }
    conversionCheckers.append(UnderlyingImageListChecker::create(*this, underlyingValue->clone()));
    return underlyingValue->clone();
}

PassOwnPtr<InterpolationValue> CSSImageListInterpolationType::maybeConvertInitial() const
{
    StyleImageList initialImageList;
    ImageListPropertyFunctions::getInitialImageList(cssProperty(), initialImageList);
    return maybeConvertStyleImageList(initialImageList);
}

PassOwnPtr<InterpolationValue> CSSImageListInterpolationType::maybeConvertStyleImageList(const StyleImageList& imageList) const
{
    if (imageList.size() == 0)
        return nullptr;

    InterpolationComponent listComponent = ListInterpolationFunctions::createList(imageList.size(), [&imageList](size_t index) {
        return CSSImageInterpolationType::maybeConvertStyleImage(*imageList[index], false);
    });
    if (!listComponent)
        return nullptr;
    return InterpolationValue::create(*this, listComponent);
}

class ParentImageListChecker : public InterpolationType::ConversionChecker {
public:
    ~ParentImageListChecker() final {}

    static PassOwnPtr<ParentImageListChecker> create(const InterpolationType& type, CSSPropertyID property, const StyleImageList& inheritedImageList)
    {
        return adoptPtr(new ParentImageListChecker(type, property, inheritedImageList));
    }

private:
    ParentImageListChecker(const InterpolationType& type, CSSPropertyID property, const StyleImageList& inheritedImageList)
        : ConversionChecker(type)
        , m_property(property)
        , m_inheritedImageList(inheritedImageList)
    { }

    bool isValid(const InterpolationEnvironment& environment, const UnderlyingValue&) const final
    {
        StyleImageList inheritedImageList;
        ImageListPropertyFunctions::getImageList(m_property, *environment.state().parentStyle(), inheritedImageList);
        return m_inheritedImageList == inheritedImageList;
    }

    CSSPropertyID m_property;
    StyleImageList m_inheritedImageList;
};

PassOwnPtr<InterpolationValue> CSSImageListInterpolationType::maybeConvertInherit(const StyleResolverState& state, ConversionCheckers& conversionCheckers) const
{
    if (!state.parentStyle())
        return nullptr;

    StyleImageList inheritedImageList;
    ImageListPropertyFunctions::getImageList(cssProperty(), *state.parentStyle(), inheritedImageList);
    conversionCheckers.append(ParentImageListChecker::create(*this, cssProperty(), inheritedImageList));
    return maybeConvertStyleImageList(inheritedImageList);
}

PassOwnPtr<InterpolationValue> CSSImageListInterpolationType::maybeConvertValue(const CSSValue& value, const StyleResolverState&, ConversionCheckers&) const
{
    if (value.isPrimitiveValue() && toCSSPrimitiveValue(value).getValueID() == CSSValueNone)
        return nullptr;

    RefPtrWillBeRawPtr<CSSValueList> tempList = nullptr;
    if (!value.isBaseValueList()) {
        tempList = CSSValueList::createCommaSeparated();
        tempList->append(const_cast<CSSValue*>(&value)); // Take ref.
    }
    const CSSValueList& valueList = tempList ? *tempList : toCSSValueList(value);

    const size_t length = valueList.length();
    OwnPtr<InterpolableList> interpolableList = InterpolableList::create(length);
    Vector<RefPtr<NonInterpolableValue>> nonInterpolableValues(length);
    for (size_t i = 0; i < length; i++) {
        InterpolationComponent component = CSSImageInterpolationType::maybeConvertCSSValue(*valueList.item(i), false);
        if (!component)
            return nullptr;
        interpolableList->set(i, component.interpolableValue.release());
        nonInterpolableValues[i] = component.nonInterpolableValue.release();
    }
    return InterpolationValue::create(*this, interpolableList.release(), NonInterpolableList::create(nonInterpolableValues));
}

PassOwnPtr<PairwisePrimitiveInterpolation> CSSImageListInterpolationType::mergeSingleConversions(InterpolationValue& startValue, InterpolationValue& endValue) const
{
    PairwiseInterpolationComponent component = ListInterpolationFunctions::mergeSingleConversions(
        startValue.mutableComponent(), endValue.mutableComponent(),
        CSSImageInterpolationType::mergeSingleConversionComponents);
    if (!component)
        return nullptr;
    return PairwisePrimitiveInterpolation::create(*this, component);
}

PassOwnPtr<InterpolationValue> CSSImageListInterpolationType::maybeConvertUnderlyingValue(const InterpolationEnvironment& environment) const
{
    StyleImageList underlyingImageList;
    ImageListPropertyFunctions::getImageList(cssProperty(), *environment.state().style(), underlyingImageList);
    return maybeConvertStyleImageList(underlyingImageList);
}

void CSSImageListInterpolationType::composite(UnderlyingValue& underlyingValue, double underlyingFraction, const InterpolationValue& value) const
{
    underlyingValue.set(&value);
}

void CSSImageListInterpolationType::apply(const InterpolableValue& interpolableValue, const NonInterpolableValue* nonInterpolableValue, InterpolationEnvironment& environment) const
{
    const InterpolableList& interpolableList = toInterpolableList(interpolableValue);
    const size_t length = interpolableList.length();
    ASSERT(length > 0);
    const NonInterpolableList& nonInterpolableList = toNonInterpolableList(*nonInterpolableValue);
    ASSERT(nonInterpolableList.length() == length);
    StyleImageList imageList(length);
    for (size_t i = 0; i < length; i++)
        imageList[i] = CSSImageInterpolationType::resolveStyleImage(cssProperty(), *interpolableList.get(i), nonInterpolableList.get(i), environment.state());
    ImageListPropertyFunctions::setImageList(cssProperty(), *environment.state().style(), imageList);
}

} // namespace blink
