// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ImageInterpolationType_h
#define ImageInterpolationType_h

#include "core/animation/InterpolationType.h"

namespace blink {

class StyleImage;

class ImageInterpolationType : public InterpolationType {
public:
    ImageInterpolationType(CSSPropertyID property)
        : InterpolationType(property)
    { }

    PassOwnPtr<InterpolationValue> maybeConvertUnderlyingValue(const StyleResolverState&) const final;
    void composite(UnderlyingValue&, double underlyingFraction, const InterpolationValue&) const final;
    void apply(const InterpolableValue&, const NonInterpolableValue*, StyleResolverState&) const final;

    static InterpolationComponent maybeConvertCSSValue(const CSSValue&);
    static InterpolationComponent maybeConvertStyleImage(const StyleImage&);
    static InterpolationComponent maybeConvertStyleImage(const StyleImage* image) { return image ? maybeConvertStyleImage(*image) : nullptr; }
    static PairwiseInterpolationComponent mergeSingleConversionComponents(InterpolationComponent& startValue, InterpolationComponent& endValue);
    static PassRefPtrWillBeRawPtr<CSSValue> createCSSValue(const InterpolableValue&, const NonInterpolableValue*);
    static PassRefPtrWillBeRawPtr<StyleImage> resolveStyleImage(CSSPropertyID, const InterpolableValue&, const NonInterpolableValue*, StyleResolverState&);

private:
    PassOwnPtr<InterpolationValue> maybeConvertNeutral(const UnderlyingValue&, ConversionCheckers&) const final;
    PassOwnPtr<InterpolationValue> maybeConvertInitial() const final;
    PassOwnPtr<InterpolationValue> maybeConvertInherit(const StyleResolverState*, ConversionCheckers&) const final;
    PassOwnPtr<InterpolationValue> maybeConvertValue(const CSSValue&, const StyleResolverState*, ConversionCheckers&) const final;

    PassOwnPtr<PairwisePrimitiveInterpolation> mergeSingleConversions(InterpolationValue& startValue, InterpolationValue& endValue) const final;
};

} // namespace blink

#endif // ImageInterpolationType_h
