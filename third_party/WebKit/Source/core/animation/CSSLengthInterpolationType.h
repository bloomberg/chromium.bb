// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSLengthInterpolationType_h
#define CSSLengthInterpolationType_h

#include <memory>
#include "core/CoreExport.h"
#include "core/animation/CSSInterpolationType.h"
#include "core/animation/LengthPropertyFunctions.h"

namespace blink {

class ComputedStyle;

class CORE_EXPORT CSSLengthInterpolationType : public CSSInterpolationType {
 public:
  CSSLengthInterpolationType(PropertyHandle,
                             const PropertyRegistration* = nullptr);

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
  float EffectiveZoom(const ComputedStyle&) const;

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
      InterpolationValue&& start,
      InterpolationValue&& end) const final;

  const CSSValue* CreateCSSValue(const InterpolableValue&,
                                 const NonInterpolableValue*,
                                 const StyleResolverState&) const final;

  const ValueRange value_range_;
};

}  // namespace blink

#endif  // CSSLengthInterpolationType_h
