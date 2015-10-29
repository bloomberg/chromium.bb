// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSImageListInterpolationType_h
#define CSSImageListInterpolationType_h

#include "core/animation/CSSInterpolationType.h"
#include "core/animation/ImageListPropertyFunctions.h"

namespace blink {

class CSSImageListInterpolationType : public CSSInterpolationType {
public:
    CSSImageListInterpolationType(CSSPropertyID property)
        : CSSInterpolationType(property)
    { }

    PassOwnPtr<InterpolationValue> maybeConvertUnderlyingValue(const InterpolationEnvironment&) const final;
    void composite(UnderlyingValue&, double underlyingFraction, const InterpolationValue&) const final;
    void apply(const InterpolableValue&, const NonInterpolableValue*, InterpolationEnvironment&) const final;

private:
    PassOwnPtr<InterpolationValue> maybeConvertStyleImageList(const StyleImageList&) const;

    PassOwnPtr<InterpolationValue> maybeConvertNeutral(const UnderlyingValue&, ConversionCheckers&) const final;
    PassOwnPtr<InterpolationValue> maybeConvertInitial() const final;
    PassOwnPtr<InterpolationValue> maybeConvertInherit(const StyleResolverState&, ConversionCheckers&) const final;
    PassOwnPtr<InterpolationValue> maybeConvertValue(const CSSValue&, const StyleResolverState&, ConversionCheckers&) const final;

    PassOwnPtr<PairwisePrimitiveInterpolation> mergeSingleConversions(InterpolationValue& startValue, InterpolationValue& endValue) const final;
};

} // namespace blink

#endif // CSSImageListInterpolationType_h
