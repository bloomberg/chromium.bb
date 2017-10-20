// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SizeInterpolationFunctions_h
#define SizeInterpolationFunctions_h

#include "core/animation/InterpolationValue.h"
#include "core/animation/PairwiseInterpolationValue.h"
#include "core/style/FillLayer.h"
#include <memory>

namespace blink {

class CSSToLengthConversionData;
class CSSValue;

class SizeInterpolationFunctions {
  STATIC_ONLY(SizeInterpolationFunctions);

 public:
  static InterpolationValue ConvertFillSizeSide(const FillSize&,
                                                float zoom,
                                                bool convert_width);
  static InterpolationValue MaybeConvertCSSSizeSide(const CSSValue&,
                                                    bool convert_width);
  static PairwiseInterpolationValue MaybeMergeSingles(
      InterpolationValue&& start,
      InterpolationValue&& end);
  static InterpolationValue CreateNeutralValue(const NonInterpolableValue*);
  static bool NonInterpolableValuesAreCompatible(const NonInterpolableValue*,
                                                 const NonInterpolableValue*);
  static void Composite(std::unique_ptr<InterpolableValue>&,
                        scoped_refptr<NonInterpolableValue>&,
                        double underlying_fraction,
                        const InterpolableValue&,
                        const NonInterpolableValue*);
  static FillSize CreateFillSize(
      const InterpolableValue& interpolable_value_a,
      const NonInterpolableValue* non_interpolable_value_a,
      const InterpolableValue& interpolable_value_b,
      const NonInterpolableValue* non_interpolable_value_b,
      const CSSToLengthConversionData&);
};

}  // namespace blink

#endif  // SizeInterpolationFunctions_h
