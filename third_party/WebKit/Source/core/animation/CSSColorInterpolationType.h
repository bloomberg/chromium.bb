// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSColorInterpolationType_h
#define CSSColorInterpolationType_h

#include "core/CSSValueKeywords.h"
#include "core/animation/CSSInterpolationType.h"
#include "platform/graphics/Color.h"

namespace blink {

class StyleColor;

class CSSColorInterpolationType : public CSSInterpolationType {
public:
    CSSColorInterpolationType(CSSPropertyID property)
        : CSSInterpolationType(property)
    { }

    InterpolationValue maybeConvertUnderlyingValue(const InterpolationEnvironment&) const final;
    void apply(const InterpolableValue&, const NonInterpolableValue*, InterpolationEnvironment&) const final;

    static PassOwnPtr<InterpolableValue> createInterpolableColor(const Color&);
    static PassOwnPtr<InterpolableValue> createInterpolableColor(CSSValueID);
    static PassOwnPtr<InterpolableValue> createInterpolableColor(const StyleColor&);
    static PassOwnPtr<InterpolableValue> maybeCreateInterpolableColor(const CSSValue&);
    static Color resolveInterpolableColor(const InterpolableValue& interpolableColor, const StyleResolverState&, bool isVisited = false, bool isTextDecoration = false);

private:
    InterpolationValue maybeConvertNeutral(const InterpolationValue& underlying, ConversionCheckers&) const final;
    InterpolationValue maybeConvertInitial() const final;
    InterpolationValue maybeConvertInherit(const StyleResolverState&, ConversionCheckers&) const final;
    InterpolationValue maybeConvertValue(const CSSValue&, const StyleResolverState&, ConversionCheckers&) const final;
    InterpolationValue convertStyleColorPair(const StyleColor&, const StyleColor&) const;
};

} // namespace blink

#endif // CSSColorInterpolationType_h
