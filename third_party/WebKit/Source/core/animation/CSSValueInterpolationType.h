// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSValueInterpolationType_h
#define CSSValueInterpolationType_h

#include "core/animation/CSSInterpolationType.h"

namespace blink {

// Never supports pairwise conversion while always supporting single conversion.
// A catch all for default for CSSValues.
class CSSValueInterpolationType : public CSSInterpolationType {
public:
    CSSValueInterpolationType(CSSPropertyID property)
        : CSSInterpolationType(property)
    { }

    PassOwnPtr<PairwisePrimitiveInterpolation> maybeConvertPairwise(const PropertySpecificKeyframe& startKeyframe, const PropertySpecificKeyframe& endKeyframe, const InterpolationEnvironment&, const UnderlyingValue&, ConversionCheckers&) const final
    {
        return nullptr;
    }

    PassOwnPtr<InterpolationValue> maybeConvertSingle(const PropertySpecificKeyframe&, const InterpolationEnvironment&, const UnderlyingValue&, ConversionCheckers&) const final;

    PassOwnPtr<InterpolationValue> maybeConvertUnderlyingValue(const InterpolationEnvironment&) const final
    {
        return nullptr;
    }

    void composite(UnderlyingValue& underlyingValue, double underlyingFraction, const InterpolationValue& value) const final
    {
        underlyingValue.set(&value);
    }

    void apply(const InterpolableValue&, const NonInterpolableValue*, InterpolationEnvironment&) const final;
};

} // namespace blink

#endif // CSSValueInterpolationType_h
