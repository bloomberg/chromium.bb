// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/CSSAngleInterpolationType.h"

#include "core/css/CSSPrimitiveValue.h"

namespace blink {

InterpolationValue CSSAngleInterpolationType::maybeConvertNeutral(
    const InterpolationValue&,
    ConversionCheckers&) const {
  return InterpolationValue(InterpolableNumber::create(0));
}

InterpolationValue CSSAngleInterpolationType::maybeConvertValue(
    const CSSValue& value,
    const StyleResolverState*,
    ConversionCheckers&) const {
  if (!value.isPrimitiveValue() || !toCSSPrimitiveValue(value).isAngle())
    return nullptr;
  return InterpolationValue(
      InterpolableNumber::create(toCSSPrimitiveValue(value).computeDegrees()));
}

const CSSValue* CSSAngleInterpolationType::createCSSValue(
    const InterpolableValue& value,
    const NonInterpolableValue*,
    const StyleResolverState&) const {
  return CSSPrimitiveValue::create(toInterpolableNumber(value).value(),
                                   CSSPrimitiveValue::UnitType::Degrees);
}

}  // namespace blink
