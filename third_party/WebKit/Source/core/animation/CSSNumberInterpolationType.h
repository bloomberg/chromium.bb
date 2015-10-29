// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSNumberInterpolationType_h
#define CSSNumberInterpolationType_h

#include "core/animation/CSSInterpolationType.h"

namespace blink {

class CSSNumberInterpolationType : public CSSInterpolationType {
public:
    CSSNumberInterpolationType(CSSPropertyID property)
        : CSSInterpolationType(property)
    { }

    PassOwnPtr<InterpolationValue> maybeConvertUnderlyingValue(const InterpolationEnvironment&) const final;
    void apply(const InterpolableValue&, const NonInterpolableValue*, InterpolationEnvironment&) const final;

private:
    PassOwnPtr<InterpolationValue> createNumberValue(double number) const;
    PassOwnPtr<InterpolationValue> maybeConvertNeutral(const UnderlyingValue&, ConversionCheckers&) const final;
    PassOwnPtr<InterpolationValue> maybeConvertInitial() const final;
    PassOwnPtr<InterpolationValue> maybeConvertInherit(const StyleResolverState&, ConversionCheckers&) const final;
    PassOwnPtr<InterpolationValue> maybeConvertValue(const CSSValue&, const StyleResolverState&, ConversionCheckers&) const final;
};

} // namespace blink

#endif // CSSNumberInterpolationType_h
