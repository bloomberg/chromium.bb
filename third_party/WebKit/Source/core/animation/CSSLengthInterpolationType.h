// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSLengthInterpolationType_h
#define CSSLengthInterpolationType_h

#include "core/animation/CSSInterpolationType.h"
#include "core/animation/LengthPropertyFunctions.h"

namespace blink {

class ComputedStyle;

class CSSLengthInterpolationType : public CSSInterpolationType {
public:
    CSSLengthInterpolationType(CSSPropertyID);

    PassOwnPtr<InterpolationValue> maybeConvertUnderlyingValue(const InterpolationEnvironment&) const final;
    void composite(UnderlyingValue&, double underlyingFraction, const InterpolationValue&) const final;
    void apply(const InterpolableValue&, const NonInterpolableValue*, InterpolationEnvironment&) const final;

    static Length resolveInterpolableLength(const InterpolableValue&, const NonInterpolableValue*, const CSSToLengthConversionData&, ValueRange = ValueRangeAll);
    static PassOwnPtr<InterpolableValue> createInterpolablePixels(double pixels);
    static InterpolationComponent maybeConvertCSSValue(const CSSValue&);

private:
    float effectiveZoom(const ComputedStyle&) const;

    PassOwnPtr<InterpolationValue> maybeConvertLength(const Length&, float zoom) const;
    PassOwnPtr<InterpolationValue> maybeConvertNeutral(const UnderlyingValue&, ConversionCheckers&) const final;
    PassOwnPtr<InterpolationValue> maybeConvertInitial() const final;
    PassOwnPtr<InterpolationValue> maybeConvertInherit(const StyleResolverState&, ConversionCheckers&) const final;
    PassOwnPtr<InterpolationValue> maybeConvertValue(const CSSValue&, const StyleResolverState&, ConversionCheckers&) const final;

    PassOwnPtr<PairwisePrimitiveInterpolation> mergeSingleConversions(InterpolationValue&, InterpolationValue&) const final;

    const ValueRange m_valueRange;
};

} // namespace blink

#endif // CSSLengthInterpolationType_h
