// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/SVGPointListInterpolationType.h"

#include "core/animation/InterpolationEnvironment.h"
#include "core/animation/StringKeyframe.h"
#include "core/animation/UnderlyingLengthChecker.h"
#include "core/svg/SVGPointList.h"

namespace blink {

PassOwnPtr<InterpolationValue> SVGPointListInterpolationType::maybeConvertNeutral(const UnderlyingValue& underlyingValue, ConversionCheckers& conversionCheckers) const
{
    size_t underlyingLength = UnderlyingLengthChecker::getUnderlyingLength(underlyingValue);
    conversionCheckers.append(UnderlyingLengthChecker::create(*this, underlyingLength));

    if (underlyingLength == 0)
        return nullptr;

    OwnPtr<InterpolableList> result = InterpolableList::create(underlyingLength);
    for (size_t i = 0; i < underlyingLength; i++)
        result->set(i, InterpolableNumber::create(0));
    return InterpolationValue::create(*this, result.release());
}

PassOwnPtr<InterpolationValue> SVGPointListInterpolationType::maybeConvertSVGValue(const SVGPropertyBase& svgValue) const
{
    if (svgValue.type() != AnimatedPoints)
        return nullptr;

    const SVGPointList& pointList = toSVGPointList(svgValue);
    OwnPtr<InterpolableList> result = InterpolableList::create(pointList.length() * 2);
    for (size_t i = 0; i < pointList.length(); i++) {
        const SVGPoint& point = *pointList.at(i);
        result->set(2 * i, InterpolableNumber::create(point.x()));
        result->set(2 * i + 1, InterpolableNumber::create(point.y()));
    }

    return InterpolationValue::create(*this, result.release());
}

PassOwnPtr<PairwisePrimitiveInterpolation> SVGPointListInterpolationType::mergeSingleConversions(InterpolationValue& startValue, InterpolationValue& endValue) const
{
    size_t startLength = toInterpolableList(startValue.interpolableValue()).length();
    size_t endLength = toInterpolableList(endValue.interpolableValue()).length();
    if (startLength != endLength)
        return nullptr;

    return InterpolationType::mergeSingleConversions(startValue, endValue);
}

void SVGPointListInterpolationType::composite(UnderlyingValue& underlyingValue, double underlyingFraction, const InterpolationValue& value) const
{
    size_t startLength = toInterpolableList(underlyingValue->interpolableValue()).length();
    size_t endLength = toInterpolableList(value.interpolableValue()).length();
    if (startLength == endLength)
        InterpolationType::composite(underlyingValue, underlyingFraction, value);
    else
        underlyingValue.set(&value);
}

PassRefPtrWillBeRawPtr<SVGPropertyBase> SVGPointListInterpolationType::appliedSVGValue(const InterpolableValue& interpolableValue, const NonInterpolableValue*) const
{
    RefPtrWillBeRawPtr<SVGPointList> result = SVGPointList::create();

    const InterpolableList& list = toInterpolableList(interpolableValue);
    ASSERT(list.length() % 2 == 0);
    for (size_t i = 0; i < list.length(); i += 2) {
        FloatPoint point = FloatPoint(
            toInterpolableNumber(list.get(i))->value(),
            toInterpolableNumber(list.get(i + 1))->value());
        result->append(SVGPoint::create(point));
    }

    return result.release();
}

} // namespace blink
