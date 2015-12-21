// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/SVGNumberInterpolationType.h"

#include "core/animation/InterpolationEnvironment.h"
#include "core/animation/StringKeyframe.h"
#include "core/svg/SVGNumber.h"
#include "core/svg/properties/SVGAnimatedProperty.h"

namespace blink {

PassOwnPtr<InterpolationValue> SVGNumberInterpolationType::maybeConvertNeutral(const UnderlyingValue&, ConversionCheckers&) const
{
    return InterpolationValue::create(*this, InterpolableNumber::create(0));
}

PassOwnPtr<InterpolationValue> SVGNumberInterpolationType::maybeConvertSVGValue(const SVGPropertyBase& svgValue) const
{
    if (svgValue.type() != AnimatedNumber)
        return nullptr;
    return InterpolationValue::create(*this, InterpolableNumber::create(toSVGNumber(svgValue).value()));
}

PassRefPtrWillBeRawPtr<SVGPropertyBase> SVGNumberInterpolationType::appliedSVGValue(const InterpolableValue& interpolableValue, const NonInterpolableValue*) const
{
    double value = toInterpolableNumber(interpolableValue).value();
    return SVGNumber::create(m_isNonNegative && value < 0 ? 0 : value);
}

} // namespace blink
