// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/SVGIntegerInterpolationType.h"

#include "core/animation/InterpolationEnvironment.h"
#include "core/svg/SVGInteger.h"

namespace blink {

InterpolationValue SVGIntegerInterpolationType::MaybeConvertNeutral(
    const InterpolationValue&,
    ConversionCheckers&) const {
  return InterpolationValue(InterpolableNumber::Create(0));
}

InterpolationValue SVGIntegerInterpolationType::MaybeConvertSVGValue(
    const SVGPropertyBase& svg_value) const {
  if (svg_value.GetType() != kAnimatedInteger)
    return nullptr;
  return InterpolationValue(
      InterpolableNumber::Create(ToSVGInteger(svg_value).Value()));
}

SVGPropertyBase* SVGIntegerInterpolationType::AppliedSVGValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue*) const {
  double value = ToInterpolableNumber(interpolable_value).Value();
  return SVGInteger::Create(round(value));
}

}  // namespace blink
