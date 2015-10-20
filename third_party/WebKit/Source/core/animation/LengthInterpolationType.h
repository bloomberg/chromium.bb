// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LengthInterpolationType_h
#define LengthInterpolationType_h

#include "core/animation/InterpolationType.h"
#include "core/animation/LengthPropertyFunctions.h"

namespace blink {

class ComputedStyle;

class LengthInterpolationType : public InterpolationType {
public:
    LengthInterpolationType(CSSPropertyID);

    PassOwnPtr<InterpolationValue> maybeConvertUnderlyingValue(const StyleResolverState&) const final;
    void composite(UnderlyingValue&, double underlyingFraction, const InterpolationValue&) const final;
    void apply(const InterpolableValue&, const NonInterpolableValue*, StyleResolverState&) const final;

    static Length resolveInterpolableLength(const InterpolableValue&, const NonInterpolableValue*, const CSSToLengthConversionData&, ValueRange = ValueRangeAll);
    static PassOwnPtr<InterpolableValue> createInterpolablePixels(double pixels);
    static InterpolationComponentValue maybeConvertCSSValue(const CSSValue&);

private:
    float effectiveZoom(const ComputedStyle&) const;

    PassOwnPtr<InterpolationValue> maybeConvertLength(const Length&, float zoom) const;
    PassOwnPtr<InterpolationValue> maybeConvertNeutral() const final;
    PassOwnPtr<InterpolationValue> maybeConvertInitial() const final;
    PassOwnPtr<InterpolationValue> maybeConvertInherit(const StyleResolverState*, ConversionCheckers&) const final;
    PassOwnPtr<InterpolationValue> maybeConvertValue(const CSSValue&, const StyleResolverState*, ConversionCheckers&) const final;

    PassOwnPtr<PairwisePrimitiveInterpolation> mergeSingleConversions(InterpolationValue&, InterpolationValue&) const final;

    const ValueRange m_valueRange;
};

} // namespace blink

#endif // LengthInterpolationType_h
