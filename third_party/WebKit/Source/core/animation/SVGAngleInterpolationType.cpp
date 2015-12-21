// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/SVGAngleInterpolationType.h"

#include "core/animation/InterpolationEnvironment.h"
#include "core/animation/StringKeyframe.h"
#include "core/svg/SVGAngle.h"

namespace blink {

PassOwnPtr<InterpolationValue> SVGAngleInterpolationType::maybeConvertNeutral(const UnderlyingValue&, ConversionCheckers&) const
{
    return InterpolationValue::create(*this, InterpolableNumber::create(0));
}

PassOwnPtr<InterpolationValue> SVGAngleInterpolationType::maybeConvertSVGValue(const SVGPropertyBase& svgValue) const
{
    if (toSVGAngle(svgValue).orientType()->enumValue() != SVGMarkerOrientAngle)
        return nullptr;
    return InterpolationValue::create(*this, InterpolableNumber::create(toSVGAngle(svgValue).value()));
}

PassRefPtrWillBeRawPtr<SVGPropertyBase> SVGAngleInterpolationType::appliedSVGValue(const InterpolableValue& interpolableValue, const NonInterpolableValue*) const
{
    double doubleValue = toInterpolableNumber(interpolableValue).value();
    RefPtrWillBeRawPtr<SVGAngle> result = SVGAngle::create();
    result->newValueSpecifiedUnits(SVGAngle::SVG_ANGLETYPE_DEG, doubleValue);
    return result.release();
}

} // namespace blink
