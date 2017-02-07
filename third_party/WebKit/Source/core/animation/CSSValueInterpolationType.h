// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSValueInterpolationType_h
#define CSSValueInterpolationType_h

#include "core/animation/CSSInterpolationType.h"

namespace blink {

// Never supports pairwise conversion while always supporting single conversion.
// A catch all for default for CSSValues.
class CSSValueInterpolationType : public CSSInterpolationType {
 public:
  CSSValueInterpolationType(PropertyHandle property)
      : CSSInterpolationType(property) {}

  PairwiseInterpolationValue maybeConvertPairwise(
      const PropertySpecificKeyframe& startKeyframe,
      const PropertySpecificKeyframe& endKeyframe,
      const InterpolationEnvironment&,
      const InterpolationValue& underlying,
      ConversionCheckers&) const final {
    return nullptr;
  }

  InterpolationValue maybeConvertStandardPropertyUnderlyingValue(
      const ComputedStyle&) const final {
    return nullptr;
  }

  InterpolationValue maybeConvertNeutral(const InterpolationValue& underlying,
                                         ConversionCheckers&) const final {
    // This type will never interpolate or composite with the underlying value.
    // Returning nullptr here means no value will be applied and the value in
    // ComputedStyle will remain unchanged.
    return nullptr;
  }
  InterpolationValue maybeConvertInitial(const StyleResolverState&,
                                         ConversionCheckers&) const final;
  InterpolationValue maybeConvertInherit(const StyleResolverState&,
                                         ConversionCheckers&) const final;
  InterpolationValue maybeConvertValue(const CSSValue& value,
                                       const StyleResolverState*,
                                       ConversionCheckers&) const final;

  void composite(UnderlyingValueOwner& underlyingValueOwner,
                 double underlyingFraction,
                 const InterpolationValue& value,
                 double interpolationFraction) const final {
    underlyingValueOwner.set(*this, value);
  }

  void applyStandardPropertyValue(const InterpolableValue&,
                                  const NonInterpolableValue*,
                                  StyleResolverState&) const final;

  const CSSValue* createCSSValue(const InterpolableValue&,
                                 const NonInterpolableValue*,
                                 const StyleResolverState&) const final;
};

}  // namespace blink

#endif  // CSSValueInterpolationType_h
