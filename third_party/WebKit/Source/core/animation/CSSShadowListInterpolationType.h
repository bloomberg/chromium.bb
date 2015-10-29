// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSShadowListInterpolationType_h
#define CSSShadowListInterpolationType_h

#include "core/animation/CSSInterpolationType.h"

namespace blink {

class ShadowList;

class CSSShadowListInterpolationType : public CSSInterpolationType {
public:
    CSSShadowListInterpolationType(CSSPropertyID property)
        : CSSInterpolationType(property)
    { }

    PassOwnPtr<InterpolationValue> maybeConvertUnderlyingValue(const InterpolationEnvironment&) const final;
    void composite(UnderlyingValue&, double underlyingFraction, const InterpolationValue&) const final;
    void apply(const InterpolableValue&, const NonInterpolableValue*, InterpolationEnvironment&) const final;

private:
    PassOwnPtr<InterpolationValue> convertShadowList(const ShadowList*, double zoom) const;
    PassOwnPtr<InterpolationValue> createNeutralValue() const;

    PassOwnPtr<InterpolationValue> maybeConvertNeutral(const UnderlyingValue&, ConversionCheckers&) const final;
    PassOwnPtr<InterpolationValue> maybeConvertInitial() const final;
    PassOwnPtr<InterpolationValue> maybeConvertInherit(const StyleResolverState&, ConversionCheckers&) const final;
    PassOwnPtr<InterpolationValue> maybeConvertValue(const CSSValue&, const StyleResolverState&, ConversionCheckers&) const final;
    PassOwnPtr<PairwisePrimitiveInterpolation> mergeSingleConversions(InterpolationValue& startValue, InterpolationValue& endValue) const final;
};

} // namespace blink

#endif // CSSShadowListInterpolationType_h
