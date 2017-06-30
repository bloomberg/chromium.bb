// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSDefaultInterpolationType_h
#define CSSDefaultInterpolationType_h

#include "core/animation/InterpolationType.h"

namespace blink {

// Never supports pairwise conversion while always supporting single conversion.
// A catch all default for CSSValue interpolation.
class CSSDefaultInterpolationType : public InterpolationType {
 public:
  CSSDefaultInterpolationType(PropertyHandle property)
      : InterpolationType(property) {
    DCHECK(property.IsCSSProperty());
  }

  InterpolationValue MaybeConvertSingle(const PropertySpecificKeyframe&,
                                        const InterpolationEnvironment&,
                                        const InterpolationValue& underlying,
                                        ConversionCheckers&) const;

  PairwiseInterpolationValue MaybeConvertPairwise(
      const PropertySpecificKeyframe& start_keyframe,
      const PropertySpecificKeyframe& end_keyframe,
      const InterpolationEnvironment&,
      const InterpolationValue& underlying,
      ConversionCheckers&) const final {
    return nullptr;
  }

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

  void Apply(const InterpolableValue&,
             const NonInterpolableValue*,
             InterpolationEnvironment&) const final;
};

}  // namespace blink

#endif  // CSSDefaultInterpolationType_h
