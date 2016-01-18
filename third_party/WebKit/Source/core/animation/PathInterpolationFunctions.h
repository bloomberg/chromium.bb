// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PathInterpolationFunctions_h
#define PathInterpolationFunctions_h

#include "core/animation/InterpolationType.h"
#include "core/svg/SVGPathByteStream.h"

namespace blink {

class PathInterpolationFunctions {
public:
    static PassRefPtr<SVGPathByteStream> appliedValue(const InterpolableValue&, const NonInterpolableValue*);

    static void composite(UnderlyingValue&, double underlyingFraction, const InterpolationValue&);

    static PassOwnPtr<InterpolationValue> convertValue(const InterpolationType&, const SVGPathByteStream&);

    static PassOwnPtr<InterpolationValue> maybeConvertNeutral(const InterpolationType&, const UnderlyingValue&, InterpolationType::ConversionCheckers&);

    static PassOwnPtr<PairwisePrimitiveInterpolation> mergeSingleConversions(const InterpolationType&, InterpolationValue& startValue, InterpolationValue& endValue);
};

} // namespace blink

#endif // PathInterpolationFunctions_h
