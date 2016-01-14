// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/SVGLengthListInterpolationType.h"

#include "core/animation/InterpolationEnvironment.h"
#include "core/animation/SVGLengthInterpolationType.h"
#include "core/animation/UnderlyingLengthChecker.h"
#include "core/svg/SVGLengthList.h"

namespace blink {

PassOwnPtr<InterpolationValue> SVGLengthListInterpolationType::maybeConvertNeutral(const UnderlyingValue& underlyingValue, ConversionCheckers& conversionCheckers) const
{
    size_t underlyingLength = UnderlyingLengthChecker::getUnderlyingLength(underlyingValue);
    conversionCheckers.append(UnderlyingLengthChecker::create(*this, underlyingLength));

    if (underlyingLength == 0)
        return nullptr;

    OwnPtr<InterpolableList> result = InterpolableList::create(underlyingLength);
    for (size_t i = 0; i < underlyingLength; i++)
        result->set(i, SVGLengthInterpolationType::neutralInterpolableValue());
    return InterpolationValue::create(*this, result.release());
}

PassOwnPtr<InterpolationValue> SVGLengthListInterpolationType::maybeConvertSVGValue(const SVGPropertyBase& svgValue) const
{
    if (svgValue.type() != AnimatedLengthList)
        return nullptr;

    const SVGLengthList& lengthList = toSVGLengthList(svgValue);
    OwnPtr<InterpolableList> result = InterpolableList::create(lengthList.length());
    for (size_t i = 0; i < lengthList.length(); i++) {
        InterpolationComponent component = SVGLengthInterpolationType::convertSVGLength(*lengthList.at(i));
        result->set(i, component.interpolableValue.release());
    }
    return InterpolationValue::create(*this, result.release());
}

PassOwnPtr<PairwisePrimitiveInterpolation> SVGLengthListInterpolationType::mergeSingleConversions(InterpolationValue& startValue, InterpolationValue& endValue) const
{
    size_t startLength = toInterpolableList(startValue.interpolableValue()).length();
    size_t endLength = toInterpolableList(endValue.interpolableValue()).length();
    if (startLength != endLength)
        return nullptr;
    return InterpolationType::mergeSingleConversions(startValue, endValue);
}

void SVGLengthListInterpolationType::composite(UnderlyingValue& underlyingValue, double underlyingFraction, const InterpolationValue& value) const
{
    size_t startLength = toInterpolableList(underlyingValue->interpolableValue()).length();
    size_t endLength = toInterpolableList(value.interpolableValue()).length();

    if (startLength == endLength)
        InterpolationType::composite(underlyingValue, underlyingFraction, value);
    else
        underlyingValue.set(&value);
}

PassRefPtrWillBeRawPtr<SVGPropertyBase> SVGLengthListInterpolationType::appliedSVGValue(const InterpolableValue& interpolableValue, const NonInterpolableValue*) const
{
    ASSERT_NOT_REACHED();
    // This function is no longer called, because apply has been overridden.
    return nullptr;
}

void SVGLengthListInterpolationType::apply(const InterpolableValue& interpolableValue, const NonInterpolableValue* nonInterpolableValue, InterpolationEnvironment& environment) const
{
    SVGElement& element = environment.svgElement();
    SVGLengthContext lengthContext(&element);

    RefPtrWillBeRawPtr<SVGLengthList> result = SVGLengthList::create(m_unitMode);
    const InterpolableList& list = toInterpolableList(interpolableValue);
    for (size_t i = 0; i < list.length(); i++) {
        result->append(SVGLengthInterpolationType::resolveInterpolableSVGLength(*list.get(i), lengthContext, m_unitMode, m_negativeValuesForbidden));
    }

    element.setWebAnimatedAttribute(attribute(), result.release());
}

} // namespace blink
