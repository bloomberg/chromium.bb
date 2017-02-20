// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/CSSTimeInterpolationType.h"

#include "core/css/CSSPrimitiveValue.h"

namespace blink {

InterpolationValue CSSTimeInterpolationType::maybeConvertNeutral(
    const InterpolationValue&,
    ConversionCheckers&) const {
  return InterpolationValue(InterpolableNumber::create(0));
}

InterpolationValue CSSTimeInterpolationType::maybeConvertValue(
    const CSSValue& value,
    const StyleResolverState*,
    ConversionCheckers&) const {
  if (!value.isPrimitiveValue() || !toCSSPrimitiveValue(value).isTime())
    return nullptr;
  return InterpolationValue(
      InterpolableNumber::create(toCSSPrimitiveValue(value).computeSeconds()));
}

const CSSValue* CSSTimeInterpolationType::createCSSValue(
    const InterpolableValue& value,
    const NonInterpolableValue*,
    const StyleResolverState&) const {
  return CSSPrimitiveValue::create(toInterpolableNumber(value).value(),
                                   CSSPrimitiveValue::UnitType::Seconds);
}

}  // namespace blink
