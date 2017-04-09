// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/CSSAngleInterpolationType.h"

#include "core/css/CSSPrimitiveValue.h"

namespace blink {

InterpolationValue CSSAngleInterpolationType::MaybeConvertNeutral(
    const InterpolationValue&,
    ConversionCheckers&) const {
  return InterpolationValue(InterpolableNumber::Create(0));
}

InterpolationValue CSSAngleInterpolationType::MaybeConvertValue(
    const CSSValue& value,
    const StyleResolverState*,
    ConversionCheckers&) const {
  if (!value.IsPrimitiveValue() || !ToCSSPrimitiveValue(value).IsAngle())
    return nullptr;
  return InterpolationValue(
      InterpolableNumber::Create(ToCSSPrimitiveValue(value).ComputeDegrees()));
}

const CSSValue* CSSAngleInterpolationType::CreateCSSValue(
    const InterpolableValue& value,
    const NonInterpolableValue*,
    const StyleResolverState&) const {
  return CSSPrimitiveValue::Create(ToInterpolableNumber(value).Value(),
                                   CSSPrimitiveValue::UnitType::kDegrees);
}

}  // namespace blink
