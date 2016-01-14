// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SVGLengthListInterpolationType_h
#define SVGLengthListInterpolationType_h

#include "core/SVGNames.h"
#include "core/animation/SVGInterpolationType.h"
#include "core/svg/SVGLength.h"

namespace blink {

enum class SVGLengthMode;

class SVGLengthListInterpolationType : public SVGInterpolationType {
public:
    SVGLengthListInterpolationType(const QualifiedName& attribute)
        : SVGInterpolationType(attribute)
        , m_unitMode(SVGLength::lengthModeForAnimatedLengthAttribute(attribute))
        , m_negativeValuesForbidden(SVGLength::negativeValuesForbiddenForAnimatedLengthAttribute(attribute))
    { }

private:
    PassOwnPtr<InterpolationValue> maybeConvertNeutral(const UnderlyingValue&, ConversionCheckers&) const final;
    PassOwnPtr<InterpolationValue> maybeConvertSVGValue(const SVGPropertyBase& svgValue) const final;
    PassOwnPtr<PairwisePrimitiveInterpolation> mergeSingleConversions(InterpolationValue& startValue, InterpolationValue& endValue) const final;
    void composite(UnderlyingValue&, double underlyingFraction, const InterpolationValue&) const final;
    PassRefPtrWillBeRawPtr<SVGPropertyBase> appliedSVGValue(const InterpolableValue&, const NonInterpolableValue*) const final;
    void apply(const InterpolableValue&, const NonInterpolableValue*, InterpolationEnvironment&) const final;

    const SVGLengthMode m_unitMode;
    const bool m_negativeValuesForbidden;
};

} // namespace blink

#endif // SVGLengthListInterpolationType_h
