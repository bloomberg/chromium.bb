// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/SVGPathInterpolationType.h"

#include "core/animation/PathInterpolationFunctions.h"

#include "core/svg/SVGPath.h"

namespace blink {

PassOwnPtr<InterpolationValue> SVGPathInterpolationType::maybeConvertSVGValue(const SVGPropertyBase& svgValue) const
{
    if (svgValue.type() != AnimatedPath)
        return nullptr;

    return PathInterpolationFunctions::convertValue(*this, toSVGPath(svgValue).byteStream());
}

PassOwnPtr<InterpolationValue> SVGPathInterpolationType::maybeConvertNeutral(const UnderlyingValue& underlyingValue, ConversionCheckers& conversionCheckers) const
{
    return PathInterpolationFunctions::maybeConvertNeutral(*this, underlyingValue, conversionCheckers);
}

PassOwnPtr<PairwisePrimitiveInterpolation> SVGPathInterpolationType::mergeSingleConversions(InterpolationValue& startValue, InterpolationValue& endValue) const
{
    return PathInterpolationFunctions::mergeSingleConversions(*this, startValue, endValue);
}

void SVGPathInterpolationType::composite(UnderlyingValue& underlyingValue, double underlyingFraction, const InterpolationValue& value) const
{
    PathInterpolationFunctions::composite(underlyingValue, underlyingFraction, value);
}

PassRefPtrWillBeRawPtr<SVGPropertyBase> SVGPathInterpolationType::appliedSVGValue(const InterpolableValue& interpolableValue, const NonInterpolableValue* nonInterpolableValue) const
{
    return SVGPath::create(CSSPathValue::create(PathInterpolationFunctions::appliedValue(interpolableValue, nonInterpolableValue)));
}

} // namespace blink
