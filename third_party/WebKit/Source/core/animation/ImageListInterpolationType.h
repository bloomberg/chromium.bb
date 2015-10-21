// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ImageListInterpolationType_h
#define ImageListInterpolationType_h

#include "core/animation/ImageListPropertyFunctions.h"
#include "core/animation/InterpolationType.h"

namespace blink {

class ImageListInterpolationType : public InterpolationType {
public:
    ImageListInterpolationType(CSSPropertyID property)
        : InterpolationType(property)
    { }

    PassOwnPtr<InterpolationValue> maybeConvertUnderlyingValue(const StyleResolverState&) const final;
    void composite(UnderlyingValue&, double underlyingFraction, const InterpolationValue&) const final;
    void apply(const InterpolableValue&, const NonInterpolableValue*, StyleResolverState&) const final;

private:
    PassOwnPtr<InterpolationValue> maybeConvertStyleImageList(const StyleImageList&) const;

    PassOwnPtr<InterpolationValue> maybeConvertNeutral(const UnderlyingValue&, ConversionCheckers&) const final;
    PassOwnPtr<InterpolationValue> maybeConvertInitial() const final;
    PassOwnPtr<InterpolationValue> maybeConvertInherit(const StyleResolverState*, ConversionCheckers&) const final;
    PassOwnPtr<InterpolationValue> maybeConvertValue(const CSSValue&, const StyleResolverState*, ConversionCheckers&) const final;

    PassOwnPtr<PairwisePrimitiveInterpolation> mergeSingleConversions(InterpolationValue& startValue, InterpolationValue& endValue) const final;
};

} // namespace blink

#endif // ImageListInterpolationType_h
