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

    static PassOwnPtr<UnderlyingImageListChecker> create(const InterpolationValue& underlying)
    {
        return adoptPtr(new UnderlyingImageListChecker(underlying));
    }

private:
    UnderlyingImageListChecker(const InterpolationValue& underlying)
        : m_underlying(underlying.clone())
    { }

    bool isValid(const InterpolationEnvironment&, const InterpolationValue& underlying) const final
    {
        return ListInterpolationFunctions::equalValues(m_underlying, underlying, CSSImageInterpolationType::equalNonInterpolableValues);
    }

    const InterpolationValue m_underlying;
};

InterpolationValue CSSImageListInterpolationType::maybeConvertNeutral(const InterpolationValue& underlying, ConversionCheckers& conversionCheckers) const
{
    conversionCheckers.append(UnderlyingImageListChecker::create(underlying));
    return underlying.clone();
}

InterpolationValue CSSImageListInterpolationType::maybeConvertInitial() const
{
    StyleImageList initialImageList;
    ImageListPropertyFunctions::getInitialImageList(cssProperty(), initialImageList);
    return maybeConvertStyleImageList(initialImageList);
}

InterpolationValue CSSImageListInterpolationType::maybeConvertStyleImageList(const StyleImageList& imageList) const
{
    if (imageList.size() == 0)
        return nullptr;

    return ListInterpolationFunctions::createList(imageList.size(), [&imageList](size_t index) {
        return CSSImageInterpolationType::maybeConvertStyleImage(*imageList[index], false);
    });
}

class ParentImageListChecker : public InterpolationType::ConversionChecker {
public:
    ~ParentImageListChecker() final {}

    static PassOwnPtr<ParentImageListChecker> create(CSSPropertyID property, const StyleImageList& inheritedImageList)
    {
        return adoptPtr(new ParentImageListChecker(property, inheritedImageList));
    }

private:
    ParentImageListChecker(CSSPropertyID property, const StyleImageList& inheritedImageList)
        : m_property(property)
        , m_inheritedImageList(inheritedImageList)
    { }

    bool isValid(const InterpolationEnvironment& environment, const InterpolationValue& underlying) const final
    {
        StyleImageList inheritedImageList;
        ImageListPropertyFunctions::getImageList(m_property, *environment.state().parentStyle(), inheritedImageList);
        return m_inheritedImageList == inheritedImageList;
    }

    CSSPropertyID m_property;
    StyleImageList m_inheritedImageList;
};

InterpolationValue CSSImageListInterpolationType::maybeConvertInherit(const StyleResolverState& state, ConversionCheckers& conversionCheckers) const
{
    if (!state.parentStyle())
        return nullptr;

    StyleImageList inheritedImageList;
    ImageListPropertyFunctions::getImageList(cssProperty(), *state.parentStyle(), inheritedImageList);
    conversionCheckers.append(ParentImageListChecker::create(cssProperty(), inheritedImageList));
    return maybeConvertStyleImageList(inheritedImageList);
}

InterpolationValue CSSImageListInterpolationType::maybeConvertValue(const CSSValue& value, const StyleResolverState&, ConversionCheckers&) const
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
        InterpolationValue component = CSSImageInterpolationType::maybeConvertCSSValue(*valueList.item(i), false);
        if (!component)
            return nullptr;
        interpolableList->set(i, component.interpolableValue.release());
        nonInterpolableValues[i] = component.nonInterpolableValue.release();
    }
    return InterpolationValue(interpolableList.release(), NonInterpolableList::create(nonInterpolableValues));
}

PairwiseInterpolationValue CSSImageListInterpolationType::mergeSingleConversions(InterpolationValue& start, InterpolationValue& end) const
{
    return ListInterpolationFunctions::mergeSingleConversions(start, end, CSSImageInterpolationType::staticMergeSingleConversions);
}

InterpolationValue CSSImageListInterpolationType::maybeConvertUnderlyingValue(const InterpolationEnvironment& environment) const
{
    StyleImageList underlyingImageList;
    ImageListPropertyFunctions::getImageList(cssProperty(), *environment.state().style(), underlyingImageList);
    return maybeConvertStyleImageList(underlyingImageList);
}

void CSSImageListInterpolationType::composite(UnderlyingValueOwner& underlyingValueOwner, double underlyingFraction, const InterpolationValue& value) const
{
    underlyingValueOwner.set(*this, value);
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
