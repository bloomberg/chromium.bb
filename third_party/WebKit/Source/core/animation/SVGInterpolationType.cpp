// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/SVGInterpolationType.h"

#include "core/animation/SVGInterpolationEnvironment.h"
#include "core/animation/StringKeyframe.h"
#include "core/svg/SVGElement.h"
#include "core/svg/properties/SVGProperty.h"

namespace blink {

InterpolationValue SVGInterpolationType::MaybeConvertSingle(
    const PropertySpecificKeyframe& keyframe,
    const InterpolationEnvironment& environment,
    const InterpolationValue& underlying,
    ConversionCheckers& conversion_checkers) const {
  if (keyframe.IsNeutral())
    return MaybeConvertNeutral(underlying, conversion_checkers);

  SVGPropertyBase* svg_value =
      ToSVGInterpolationEnvironment(environment)
          .SvgBaseValue()
          .CloneForAnimation(ToSVGPropertySpecificKeyframe(keyframe).Value());
  return MaybeConvertSVGValue(*svg_value);
}

InterpolationValue SVGInterpolationType::MaybeConvertUnderlyingValue(
    const InterpolationEnvironment& environment) const {
  return MaybeConvertSVGValue(
      ToSVGInterpolationEnvironment(environment).SvgBaseValue());
}

void SVGInterpolationType::Apply(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue* non_interpolable_value,
    InterpolationEnvironment& environment) const {
  ToSVGInterpolationEnvironment(environment)
      .SvgElement()
      .SetWebAnimatedAttribute(
          Attribute(),
          AppliedSVGValue(interpolable_value, non_interpolable_value));
}

}  // namespace blink
