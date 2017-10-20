// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ShadowInterpolationFunctions_h
#define ShadowInterpolationFunctions_h

#include "core/animation/InterpolationValue.h"
#include "core/animation/PairwiseInterpolationValue.h"
#include <memory>

namespace blink {

class ShadowData;
class CSSValue;
class StyleResolverState;

class ShadowInterpolationFunctions {
 public:
  static InterpolationValue ConvertShadowData(const ShadowData&, double zoom);
  static InterpolationValue MaybeConvertCSSValue(const CSSValue&);
  static std::unique_ptr<InterpolableValue> CreateNeutralInterpolableValue();
  static bool NonInterpolableValuesAreCompatible(const NonInterpolableValue*,
                                                 const NonInterpolableValue*);
  static PairwiseInterpolationValue MaybeMergeSingles(
      InterpolationValue&& start,
      InterpolationValue&& end);
  static void Composite(std::unique_ptr<InterpolableValue>&,
                        scoped_refptr<NonInterpolableValue>&,
                        double underlying_fraction,
                        const InterpolableValue&,
                        const NonInterpolableValue*);
  static ShadowData CreateShadowData(const InterpolableValue&,
                                     const NonInterpolableValue*,
                                     const StyleResolverState&);
};

}  // namespace blink

#endif  // ShadowInterpolationFunctions_h
