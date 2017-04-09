// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SVGValueInterpolationType_h
#define SVGValueInterpolationType_h

#include "core/animation/SVGInterpolationType.h"

namespace blink {

// Never supports pairwise conversion while always supporting single conversion.
// A catch all default for SVGValues.
class SVGValueInterpolationType : public SVGInterpolationType {
 public:
  SVGValueInterpolationType(const QualifiedName& attribute)
      : SVGInterpolationType(attribute) {}

 private:
  PairwiseInterpolationValue MaybeConvertPairwise(
      const PropertySpecificKeyframe& start_keyframe,
      const PropertySpecificKeyframe& end_keyframe,
      const InterpolationEnvironment&,
      const InterpolationValue& underlying,
      ConversionCheckers&) const final {
    return nullptr;
  }

  InterpolationValue MaybeConvertNeutral(const InterpolationValue& underlying,
                                         ConversionCheckers&) const final {
    return nullptr;
  }

  InterpolationValue MaybeConvertSVGValue(const SVGPropertyBase&) const final;

  InterpolationValue MaybeConvertUnderlyingValue(
      const InterpolationEnvironment&) const final {
    return nullptr;
  }

  void Composite(UnderlyingValueOwner& underlying_value_owner,
                 double underlying_fraction,
                 const InterpolationValue& value,
                 double interpolation_fraction) const final {
    underlying_value_owner.Set(*this, value);
  }

  SVGPropertyBase* AppliedSVGValue(const InterpolableValue&,
                                   const NonInterpolableValue*) const final;
};

}  // namespace blink

#endif  // SVGValueInterpolationType_h
