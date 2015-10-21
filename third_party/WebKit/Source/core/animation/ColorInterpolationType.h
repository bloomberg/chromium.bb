// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ColorInterpolationType_h
#define ColorInterpolationType_h

#include "core/CSSValueKeywords.h"
#include "core/animation/InterpolationType.h"
#include "platform/graphics/Color.h"

namespace blink {

class StyleColor;

class ColorInterpolationType : public InterpolationType {
public:
    ColorInterpolationType(CSSPropertyID property)
        : InterpolationType(property)
    { }

    PassOwnPtr<InterpolationValue> maybeConvertUnderlyingValue(const StyleResolverState&) const final;
    void apply(const InterpolableValue&, const NonInterpolableValue*, StyleResolverState&) const final;

    static PassOwnPtr<InterpolableValue> createInterpolableColor(const Color&);
    static PassOwnPtr<InterpolableValue> createInterpolableColor(CSSValueID);
    static PassOwnPtr<InterpolableValue> createInterpolableColor(const StyleColor&);
    static PassOwnPtr<InterpolableValue> maybeCreateInterpolableColor(const CSSValue&);
    static Color resolveInterpolableColor(const InterpolableValue& interpolableColor, const StyleResolverState&, bool isVisited = false, bool isTextDecoration = false);

private:
    PassOwnPtr<InterpolationValue> maybeConvertNeutral(const UnderlyingValue&, ConversionCheckers&) const final;
    PassOwnPtr<InterpolationValue> maybeConvertInitial() const final;
    PassOwnPtr<InterpolationValue> maybeConvertInherit(const StyleResolverState*, ConversionCheckers&) const final;
    PassOwnPtr<InterpolationValue> maybeConvertValue(const CSSValue&, const StyleResolverState*, ConversionCheckers&) const final;
    PassOwnPtr<InterpolationValue> convertStyleColorPair(const StyleColor&, const StyleColor&) const;
};

} // namespace blink

#endif // ColorInterpolationType_h
