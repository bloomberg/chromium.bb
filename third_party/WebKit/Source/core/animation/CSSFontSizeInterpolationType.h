// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSFontSizeInterpolationType_h
#define CSSFontSizeInterpolationType_h

#include "core/animation/CSSInterpolationType.h"

namespace blink {

class CSSFontSizeInterpolationType : public CSSInterpolationType {
 public:
  CSSFontSizeInterpolationType(PropertyHandle property)
      : CSSInterpolationType(property) {
    DCHECK_EQ(CssProperty().PropertyID(), CSSPropertyFontSize);
  }

  InterpolationValue MaybeConvertStandardPropertyUnderlyingValue(
      const ComputedStyle&) const final;
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
};

}  // namespace blink

#endif  // CSSFontSizeInterpolationType_h
