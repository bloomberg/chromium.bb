// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/SVGInterpolationType.h"

#include "core/animation/InterpolationEnvironment.h"
#include "core/animation/StringKeyframe.h"
#include "core/svg/SVGElement.h"
#include "core/svg/properties/SVGProperty.h"

namespace blink {

PassOwnPtr<InterpolationValue> SVGInterpolationType::maybeConvertNeutral(const UnderlyingValue&, ConversionCheckers&) const
{
    ASSERT_NOT_REACHED();
    // This function must be overridden, unless maybeConvertSingle is overridden to no longer need it.
    return nullptr;
}

PassOwnPtr<InterpolationValue> SVGInterpolationType::maybeConvertSingle(const PropertySpecificKeyframe& keyframe, const InterpolationEnvironment& environment, const UnderlyingValue& underlyingValue, ConversionCheckers& conversionCheckers) const
{
    if (keyframe.isNeutral())
        return maybeConvertNeutral(underlyingValue, conversionCheckers);

    RefPtrWillBeRawPtr<SVGPropertyBase> svgValue = environment.svgBaseValue().cloneForAnimation(toSVGPropertySpecificKeyframe(keyframe).value());
    return maybeConvertSVGValue(*svgValue);
}

PassOwnPtr<InterpolationValue> SVGInterpolationType::maybeConvertUnderlyingValue(const InterpolationEnvironment& environment) const
{
    return maybeConvertSVGValue(environment.svgBaseValue());
}

void SVGInterpolationType::apply(const InterpolableValue& interpolableValue, const NonInterpolableValue* nonInterpolableValue, InterpolationEnvironment& environment) const
{
    environment.svgElement().setWebAnimatedAttribute(attribute(), appliedSVGValue(interpolableValue, nonInterpolableValue));
}

} // namespace blink
