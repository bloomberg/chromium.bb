// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ShadowInterpolationFunctions_h
#define ShadowInterpolationFunctions_h

#include "core/animation/InterpolationValue.h"

namespace blink {

class ShadowData;
class CSSValue;
class StyleResolverState;

class ShadowInterpolationFunctions {
public:
    static InterpolationComponent convertShadowData(const ShadowData&, double zoom);
    static InterpolationComponent maybeConvertCSSValue(const CSSValue&);
    static bool nonInterpolableValuesAreCompatible(const NonInterpolableValue*, const NonInterpolableValue*);
    static PairwiseInterpolationComponent mergeSingleConversions(InterpolationComponent& start, InterpolationComponent& end);
    static void composite(OwnPtr<InterpolableValue>&, RefPtr<NonInterpolableValue>&, double underlyingFraction, const InterpolableValue&, const NonInterpolableValue*);
    static ShadowData createShadowData(const InterpolableValue&, const NonInterpolableValue*, const StyleResolverState&);
};

} // namespace blink

#endif // ShadowInterpolationFunctions_h
