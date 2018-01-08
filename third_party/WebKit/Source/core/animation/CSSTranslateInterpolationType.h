// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSTranslateInterpolationType_h
#define CSSTranslateInterpolationType_h

#include "core/animation/CSSInterpolationType.h"

namespace blink {

class CSSTranslateInterpolationType : public CSSInterpolationType {
 public:
  CSSTranslateInterpolationType(PropertyHandle property)
      : CSSInterpolationType(property) {
    DCHECK_EQ(CssProperty().PropertyID(), CSSPropertyTranslate);
  }

  InterpolationValue MaybeConvertStandardPropertyUnderlyingValue(
      const ComputedStyle&) const final;
  void Composite(UnderlyingValueOwner&,
                 double underlying_fraction,
                 const InterpolationValue&,
                 double interpolation_fraction) const final;
  void ApplyStandardPropertyValue(const InterpolableValue&,
                                  const NonInterpolableValue*,
                                  StyleResolverState&) const final;

 private:
  InterpolationValue MaybeConvertNeutral(const InterpolationValue& underlying,
                                         ConversionCheckers&) const final;
  InterpolationValue MaybeConvertInitial(const StyleResolverState&,
                                         ConversionCheckers&) const final;
  InterpolationValue MaybeConvertInherit(const StyleResolverState&,
                                         ConversionCheckers&) const final;
  InterpolationValue MaybeConvertValue(const CSSValue&,
                                       const StyleResolverState*,
                                       ConversionCheckers&) const final;

  PairwiseInterpolationValue MaybeMergeSingles(
      InterpolationValue&&,
      InterpolationValue&&) const final;
};

}  // namespace blink

#endif  // CSSTranslateInterpolationType_h
