// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ShadowListInterpolationType_h
#define ShadowListInterpolationType_h

#include "core/animation/InterpolationType.h"

namespace blink {

class ShadowList;

class ShadowListInterpolationType : public InterpolationType {
public:
    ShadowListInterpolationType(CSSPropertyID property)
        : InterpolationType(property)
    { }

    PassOwnPtr<InterpolationValue> maybeConvertUnderlyingValue(const StyleResolverState&) const final;
    void composite(UnderlyingValue&, double underlyingFraction, const InterpolationValue&) const final;
    void apply(const InterpolableValue&, const NonInterpolableValue*, StyleResolverState&) const final;

private:
    PassOwnPtr<InterpolationValue> convertShadowList(const ShadowList*, double zoom) const;
    PassOwnPtr<InterpolationValue> createNeutralValue() const;

    PassOwnPtr<InterpolationValue> maybeConvertNeutral(const UnderlyingValue&, ConversionCheckers&) const final;
    PassOwnPtr<InterpolationValue> maybeConvertInitial() const final;
    PassOwnPtr<InterpolationValue> maybeConvertInherit(const StyleResolverState*, ConversionCheckers&) const final;
    PassOwnPtr<InterpolationValue> maybeConvertValue(const CSSValue&, const StyleResolverState*, ConversionCheckers&) const final;
    PassOwnPtr<PairwisePrimitiveInterpolation> mergeSingleConversions(InterpolationValue& startValue, InterpolationValue& endValue) const final;
};

} // namespace blink

#endif // ShadowListInterpolationType_h
