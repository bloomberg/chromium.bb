// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/ListInterpolationFunctions.h"

#include "core/animation/UnderlyingValueOwner.h"
#include "core/css/CSSValueList.h"
#include "wtf/MathExtras.h"

namespace blink {

DEFINE_NON_INTERPOLABLE_VALUE_TYPE(NonInterpolableList);

bool ListInterpolationFunctions::equalValues(const InterpolationValue& a, const InterpolationValue& b, EqualNonInterpolableValuesCallback equalNonInterpolableValues)
{
    if (!a && !b)
        return true;

    if (!a || !b)
        return false;

    const InterpolableList& interpolableListA = toInterpolableList(*a.interpolableValue);
    const InterpolableList& interpolableListB = toInterpolableList(*b.interpolableValue);

    if (interpolableListA.length() != interpolableListB.length())
        return false;

    size_t length = interpolableListA.length();
    if (length == 0)
        return true;

    const NonInterpolableList& nonInterpolableListA = toNonInterpolableList(*a.nonInterpolableValue);
    const NonInterpolableList& nonInterpolableListB = toNonInterpolableList(*b.nonInterpolableValue);

    for (size_t i = 0; i < length; i++) {
        if (!equalNonInterpolableValues(nonInterpolableListA.get(i), nonInterpolableListB.get(i)))
            return false;
    }
    return true;
}

PairwiseInterpolationValue ListInterpolationFunctions::mergeSingleConversions(InterpolationValue& start, InterpolationValue& end, MergeSingleItemConversionsCallback mergeSingleItemConversions)
{
    size_t startLength = toInterpolableList(*start.interpolableValue).length();
    size_t endLength = toInterpolableList(*end.interpolableValue).length();

    if (startLength == 0 && endLength == 0) {
        return PairwiseInterpolationValue(
            start.interpolableValue.release(),
            end.interpolableValue.release(),
            nullptr);
    }

    if (startLength == 0) {
        OwnPtr<InterpolableValue> startInterpolableValue = end.interpolableValue->cloneAndZero();
        return PairwiseInterpolationValue(
            startInterpolableValue.release(),
            end.interpolableValue.release(),
            end.nonInterpolableValue.release());
    }

    if (endLength == 0) {
        OwnPtr<InterpolableValue> endInterpolableValue = start.interpolableValue->cloneAndZero();
        return PairwiseInterpolationValue(
            start.interpolableValue.release(),
            endInterpolableValue.release(),
            start.nonInterpolableValue.release());
    }

    size_t finalLength = lowestCommonMultiple(startLength, endLength);
    OwnPtr<InterpolableList> resultStartInterpolableList = InterpolableList::create(finalLength);
    OwnPtr<InterpolableList> resultEndInterpolableList = InterpolableList::create(finalLength);
    Vector<RefPtr<NonInterpolableValue>> resultNonInterpolableValues(finalLength);

    InterpolableList& startInterpolableList = toInterpolableList(*start.interpolableValue);
    InterpolableList& endInterpolableList = toInterpolableList(*end.interpolableValue);
    NonInterpolableList& startNonInterpolableList = toNonInterpolableList(*start.nonInterpolableValue);
    NonInterpolableList& endNonInterpolableList = toNonInterpolableList(*end.nonInterpolableValue);

    for (size_t i = 0; i < finalLength; i++) {
        InterpolationValue start(startInterpolableList.get(i % startLength)->clone(), startNonInterpolableList.get(i % startLength));
        InterpolationValue end(endInterpolableList.get(i % endLength)->clone(), endNonInterpolableList.get(i % endLength));
        PairwiseInterpolationValue result = mergeSingleItemConversions(start, end);
        if (!result)
            return nullptr;
        resultStartInterpolableList->set(i, result.startInterpolableValue.release());
        resultEndInterpolableList->set(i, result.endInterpolableValue.release());
        resultNonInterpolableValues[i] = result.nonInterpolableValue.release();
    }

    return PairwiseInterpolationValue(
        resultStartInterpolableList.release(),
        resultEndInterpolableList.release(),
        NonInterpolableList::create(resultNonInterpolableValues));
}

static void repeatToLength(InterpolationValue& value, size_t length)
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

static bool nonInterpolableListsAreCompatible(const NonInterpolableList& a, const NonInterpolableList& b, size_t length, ListInterpolationFunctions::NonInterpolableValuesAreCompatibleCallback nonInterpolableValuesAreCompatible)
{
    for (size_t i = 0; i < length; i++) {
        if (!nonInterpolableValuesAreCompatible(a.get(i % a.length()), b.get(i % b.length())))
            return false;
    }
    return true;
}

void ListInterpolationFunctions::composite(UnderlyingValueOwner& underlyingValueOwner, double underlyingFraction, const InterpolationType& type, const InterpolationValue& value, NonInterpolableValuesAreCompatibleCallback nonInterpolableValuesAreCompatible, CompositeItemCallback compositeItem)
{
    size_t underlyingLength = toInterpolableList(*underlyingValueOwner.value().interpolableValue).length();
    if (underlyingLength == 0) {
        ASSERT(!underlyingValueOwner.value().nonInterpolableValue);
        underlyingValueOwner.set(type, value);
        return;
    }

    const InterpolableList& interpolableList = toInterpolableList(*value.interpolableValue);
    size_t valueLength = interpolableList.length();
    if (valueLength == 0) {
        ASSERT(!value.nonInterpolableValue);
        underlyingValueOwner.mutableValue().interpolableValue->scale(underlyingFraction);
        return;
    }

    const NonInterpolableList& nonInterpolableList = toNonInterpolableList(*value.nonInterpolableValue);
    size_t newLength = lowestCommonMultiple(underlyingLength, valueLength);
    if (!nonInterpolableListsAreCompatible(toNonInterpolableList(*underlyingValueOwner.value().nonInterpolableValue), nonInterpolableList, newLength, nonInterpolableValuesAreCompatible)) {
        underlyingValueOwner.set(type, value);
        return;
    }

    InterpolationValue& underlyingValue = underlyingValueOwner.mutableValue();
    if (underlyingLength < newLength)
        repeatToLength(underlyingValue, newLength);

    InterpolableList& underlyingInterpolableList = toInterpolableList(*underlyingValue.interpolableValue);
    NonInterpolableList& underlyingNonInterpolableList = toNonInterpolableList(*underlyingValue.nonInterpolableValue);
    for (size_t i = 0; i < newLength; i++) {
        compositeItem(
            underlyingInterpolableList.getMutable(i),
            underlyingNonInterpolableList.getMutable(i),
            underlyingFraction,
            *interpolableList.get(i % valueLength),
            nonInterpolableList.get(i % valueLength));
    }
}

} // namespace blink
