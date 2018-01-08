// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSVisibilityInterpolationType_h
#define CSSVisibilityInterpolationType_h

#include "core/animation/CSSInterpolationType.h"
#include "core/style/ComputedStyleConstants.h"

namespace blink {

class CSSVisibilityInterpolationType : public CSSInterpolationType {
 public:
  CSSVisibilityInterpolationType(PropertyHandle property)
      : CSSInterpolationType(property) {
    DCHECK_EQ(CssProperty().PropertyID(), CSSPropertyVisibility);
  }

  InterpolationValue MaybeConvertStandardPropertyUnderlyingValue(
      const ComputedStyle&) const final;
  PairwiseInterpolationValue MaybeMergeSingles(
      InterpolationValue&& start,
      InterpolationValue&& end) const final;
  void Composite(UnderlyingValueOwner&,
                 double underlying_fraction,
                 const InterpolationValue&,
                 double interpolation_fraction) const final;
  void ApplyStandardPropertyValue(const InterpolableValue&,
                                  const NonInterpolableValue*,
                                  StyleResolverState&) const final;

 private:
  InterpolationValue CreateVisibilityValue(EVisibility) const;
  InterpolationValue MaybeConvertNeutral(const InterpolationValue& underlying,
                                         ConversionCheckers&) const final;
  InterpolationValue MaybeConvertInitial(const StyleResolverState&,
                                         ConversionCheckers&) const final;
  InterpolationValue MaybeConvertInherit(const StyleResolverState&,
                                         ConversionCheckers&) const final;
  InterpolationValue MaybeConvertValue(const CSSValue&,
                                       const StyleResolverState*,
                                       ConversionCheckers&) const final;
};

}  // namespace blink

#endif  // CSSVisibilityInterpolationType_h
