// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PathInterpolationFunctions_h
#define PathInterpolationFunctions_h

#include "core/animation/InterpolationType.h"
#include "core/svg/SVGPathByteStream.h"
#include <memory>

namespace blink {

class StylePath;

class PathInterpolationFunctions {
 public:
  enum CoordinateConversion { PreserveCoordinates, ForceAbsolute };

  static std::unique_ptr<SVGPathByteStream> AppliedValue(
      const InterpolableValue&,
      const NonInterpolableValue*);

  static void Composite(UnderlyingValueOwner&,
                        double underlying_fraction,
                        const InterpolationType&,
                        const InterpolationValue&);

  static InterpolationValue ConvertValue(const SVGPathByteStream&,
                                         CoordinateConversion);

  static InterpolationValue ConvertValue(const StylePath*,
                                         CoordinateConversion);

  static InterpolationValue MaybeConvertNeutral(
      const InterpolationValue& underlying,
      InterpolationType::ConversionCheckers&);

  static PairwiseInterpolationValue MaybeMergeSingles(
      InterpolationValue&& start,
      InterpolationValue&& end);
};

}  // namespace blink

#endif  // PathInterpolationFunctions_h
