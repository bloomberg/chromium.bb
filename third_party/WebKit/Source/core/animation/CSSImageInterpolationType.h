// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSImageInterpolationType_h
#define CSSImageInterpolationType_h

#include "core/animation/CSSInterpolationType.h"

namespace blink {

class StyleImage;

class CSSImageInterpolationType : public CSSInterpolationType {
public:
    CSSImageInterpolationType(CSSPropertyID property)
        : CSSInterpolationType(property)
    { }

    PassOwnPtr<InterpolationValue> maybeConvertUnderlyingValue(const InterpolationEnvironment&) const final;
    void composite(UnderlyingValue&, double underlyingFraction, const InterpolationValue&) const final;
    void apply(const InterpolableValue&, const NonInterpolableValue*, InterpolationEnvironment&) const final;

    static InterpolationComponent maybeConvertCSSValue(const CSSValue&, bool acceptGradients);
    static InterpolationComponent maybeConvertStyleImage(const StyleImage&, bool acceptGradients);
    static InterpolationComponent maybeConvertStyleImage(const StyleImage* image, bool acceptGradients) { return image ? maybeConvertStyleImage(*image, acceptGradients) : nullptr; }
    static PairwiseInterpolationComponent mergeSingleConversionComponents(InterpolationComponent& startValue, InterpolationComponent& endValue);
    static PassRefPtrWillBeRawPtr<CSSValue> createCSSValue(const InterpolableValue&, const NonInterpolableValue*);
    static PassRefPtrWillBeRawPtr<StyleImage> resolveStyleImage(CSSPropertyID, const InterpolableValue&, const NonInterpolableValue*, StyleResolverState&);
    static bool equalNonInterpolableValues(const NonInterpolableValue*, const NonInterpolableValue*);

private:
    PassOwnPtr<InterpolationValue> maybeConvertNeutral(const UnderlyingValue&, ConversionCheckers&) const final;
    PassOwnPtr<InterpolationValue> maybeConvertInitial() const final;
    PassOwnPtr<InterpolationValue> maybeConvertInherit(const StyleResolverState&, ConversionCheckers&) const final;
    PassOwnPtr<InterpolationValue> maybeConvertValue(const CSSValue&, const StyleResolverState&, ConversionCheckers&) const final;

    PassOwnPtr<PairwisePrimitiveInterpolation> mergeSingleConversions(InterpolationValue& startValue, InterpolationValue& endValue) const final;
};

} // namespace blink

#endif // CSSImageInterpolationType_h
