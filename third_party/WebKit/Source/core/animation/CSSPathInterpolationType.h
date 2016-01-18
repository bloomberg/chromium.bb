// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSPathInterpolationType_h
#define CSSPathInterpolationType_h

#include "core/animation/CSSInterpolationType.h"

namespace blink {

class CSSPathInterpolationType : public CSSInterpolationType {
public:
    CSSPathInterpolationType(CSSPropertyID property)
        : CSSInterpolationType(property)
    { }

    void apply(const InterpolableValue&, const NonInterpolableValue*, InterpolationEnvironment&) const final;
    void composite(UnderlyingValue&, double underlyingFraction, const InterpolationValue&) const final;

protected:
    PassOwnPtr<InterpolationValue> maybeConvertNeutral(const UnderlyingValue&, ConversionCheckers&) const final;
    PassOwnPtr<InterpolationValue> maybeConvertInitial() const final;
    PassOwnPtr<InterpolationValue> maybeConvertInherit(const StyleResolverState&, ConversionCheckers&) const final;
    PassOwnPtr<InterpolationValue> maybeConvertValue(const CSSValue&, const StyleResolverState&, ConversionCheckers&) const final;
    PassOwnPtr<InterpolationValue> maybeConvertUnderlyingValue(const InterpolationEnvironment&) const final;
    PassOwnPtr<PairwisePrimitiveInterpolation> mergeSingleConversions(InterpolationValue& startValue, InterpolationValue& endValue) const final;
};

} // namespace blink

#endif // CSSPathInterpolationType_h
