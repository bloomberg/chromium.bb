// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/animation/SVGInterpolationType.h"

#include "core/animation/InterpolationEnvironment.h"
#include "core/animation/StringKeyframe.h"
#include "core/svg/SVGAnimateElement.h"

namespace blink {

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
    SVGElement& targetElement = environment.svgElement();
    SVGElement::InstanceUpdateBlocker blocker(&targetElement);
    RefPtrWillBeRawPtr<SVGPropertyBase> appliedValue = appliedSVGValue(interpolableValue, nonInterpolableValue);
    for (SVGElement* instance : SVGAnimateElement::findElementInstances(&targetElement)) {
        RefPtrWillBeRawPtr<SVGAnimatedPropertyBase> animatedProperty = instance->propertyFromAttribute(attribute());
        if (animatedProperty) {
            animatedProperty->setAnimatedValue(appliedValue);
            instance->invalidateSVGAttributes();
            instance->svgAttributeChanged(attribute());
        }
    }
}

} // namespace blink
