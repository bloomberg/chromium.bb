// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSFontWeightInterpolationType_h
#define CSSFontWeightInterpolationType_h

#include "core/animation/CSSInterpolationType.h"
#include "platform/fonts/FontTraits.h"

namespace blink {

class CSSFontWeightInterpolationType : public CSSInterpolationType {
public:
    CSSFontWeightInterpolationType(CSSPropertyID property)
        : CSSInterpolationType(property)
    {
        ASSERT(property == CSSPropertyFontWeight);
    }

    PassOwnPtr<InterpolationValue> maybeConvertUnderlyingValue(const InterpolationEnvironment&) const final;
    void apply(const InterpolableValue&, const NonInterpolableValue*, InterpolationEnvironment&) const final;

private:
    PassOwnPtr<InterpolationValue> createFontWeightValue(FontWeight) const;
    PassOwnPtr<InterpolationValue> maybeConvertNeutral(const UnderlyingValue&, ConversionCheckers&) const final;
    PassOwnPtr<InterpolationValue> maybeConvertInitial() const final;
    PassOwnPtr<InterpolationValue> maybeConvertInherit(const StyleResolverState&, ConversionCheckers&) const final;
    PassOwnPtr<InterpolationValue> maybeConvertValue(const CSSValue&, const StyleResolverState&, ConversionCheckers&) const final;
};

} // namespace blink

#endif // CSSFontWeightInterpolationType_h
