// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSLengthInterpolationType_h
#define CSSLengthInterpolationType_h

#include "core/animation/CSSInterpolationType.h"
#include "core/animation/LengthPropertyFunctions.h"

namespace blink {

class ComputedStyle;
class CSSToLengthConversionData;

class CSSLengthInterpolationType : public CSSInterpolationType {
public:
    CSSLengthInterpolationType(CSSPropertyID);

    InterpolationValue maybeConvertUnderlyingValue(const InterpolationEnvironment&) const final;
    void composite(UnderlyingValueOwner&, double underlyingFraction, const InterpolationValue&) const final;
    void apply(const InterpolableValue&, const NonInterpolableValue*, InterpolationEnvironment&) const final;

    static Length resolveInterpolableLength(const InterpolableValue&, const NonInterpolableValue*, const CSSToLengthConversionData&, ValueRange = ValueRangeAll);
    static PassOwnPtr<InterpolableValue> createInterpolablePixels(double pixels);
    static InterpolationValue maybeConvertCSSValue(const CSSValue&);
    static InterpolationValue maybeConvertLength(const Length&, float zoom);
    static PassOwnPtr<InterpolableList> createNeutralInterpolableValue();
    static PairwiseInterpolationValue staticMergeSingleConversions(InterpolationValue& start, InterpolationValue& end);
    static bool nonInterpolableValuesAreCompatible(const NonInterpolableValue*, const NonInterpolableValue*);
    static void composite(OwnPtr<InterpolableValue>&, RefPtr<NonInterpolableValue>&, double underlyingFraction, const InterpolableValue&, const NonInterpolableValue*);

private:
    float effectiveZoom(const ComputedStyle&) const;

    InterpolationValue maybeConvertNeutral(const InterpolationValue& underlying, ConversionCheckers&) const final;
    InterpolationValue maybeConvertInitial() const final;
    InterpolationValue maybeConvertInherit(const StyleResolverState&, ConversionCheckers&) const final;
    InterpolationValue maybeConvertValue(const CSSValue&, const StyleResolverState&, ConversionCheckers&) const final;

    PairwiseInterpolationValue mergeSingleConversions(InterpolationValue& start, InterpolationValue& end) const final
    {
        return staticMergeSingleConversions(start, end);
    }

    const ValueRange m_valueRange;
};

} // namespace blink

#endif // CSSLengthInterpolationType_h
