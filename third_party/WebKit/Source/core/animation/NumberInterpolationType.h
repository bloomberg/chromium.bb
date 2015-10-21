// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NumberInterpolationType_h
#define NumberInterpolationType_h

#include "core/animation/InterpolationType.h"

namespace blink {

class NumberInterpolationType : public InterpolationType {
public:
    NumberInterpolationType(CSSPropertyID property)
        : InterpolationType(property)
    { }

    PassOwnPtr<InterpolationValue> maybeConvertUnderlyingValue(const StyleResolverState&) const final;
    void apply(const InterpolableValue&, const NonInterpolableValue*, StyleResolverState&) const final;

private:
    PassOwnPtr<InterpolationValue> createNumberValue(double number) const;
    PassOwnPtr<InterpolationValue> maybeConvertNeutral(const UnderlyingValue&, ConversionCheckers&) const final;
    PassOwnPtr<InterpolationValue> maybeConvertInitial() const final;
    PassOwnPtr<InterpolationValue> maybeConvertInherit(const StyleResolverState*, ConversionCheckers&) const final;
    PassOwnPtr<InterpolationValue> maybeConvertValue(const CSSValue&, const StyleResolverState*, ConversionCheckers&) const final;
};

} // namespace blink

#endif // NumberInterpolationType_h
