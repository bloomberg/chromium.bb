// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SVGLengthInterpolationType_h
#define SVGLengthInterpolationType_h

#include "core/animation/SVGInterpolationType.h"

#include "core/svg/SVGLength.h"

namespace blink {

class SVGLengthContext;
enum class SVGLengthMode;

class SVGLengthInterpolationType : public SVGInterpolationType {
public:
    SVGLengthInterpolationType(const QualifiedName& attribute)
        : SVGInterpolationType(attribute)
        , m_unitMode(SVGLength::lengthModeForAnimatedLengthAttribute(attribute))
        , m_negativeValuesForbidden(SVGLength::negativeValuesForbiddenForAnimatedLengthAttribute(attribute))
    { }

private:
    PassOwnPtr<InterpolationValue> maybeConvertNeutral(const UnderlyingValue&, ConversionCheckers&) const final;
    PassOwnPtr<InterpolationValue> maybeConvertSVGValue(const SVGPropertyBase& svgValue) const final;
    PassRefPtrWillBeRawPtr<SVGPropertyBase> appliedSVGValue(const InterpolableValue&, const NonInterpolableValue*) const final;
    PassRefPtrWillBeRawPtr<SVGPropertyBase> appliedSVGValue(const InterpolableValue&, const NonInterpolableValue*, const SVGLengthContext&) const;
    void apply(const InterpolableValue&, const NonInterpolableValue*, InterpolationEnvironment&) const final;

    const SVGLengthMode m_unitMode;
    const bool m_negativeValuesForbidden;
};

} // namespace blink

#endif // SVGLengthInterpolationType_h
