// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NumberSVGInterpolation_h
#define NumberSVGInterpolation_h

#include "core/animation/SVGInterpolation.h"
#include "core/svg/SVGNumber.h"
#include "core/svg/SVGNumberList.h"

namespace blink {

enum SVGNumberNegativeValuesMode {
    AllowNegativeNumbers,
    ForbidNegativeNumbers
};

class NumberSVGInterpolation : public SVGInterpolation {
public:
    typedef SVGNumberList ListType;
    typedef void NonInterpolableType;

    static PassRefPtr<NumberSVGInterpolation> create(SVGPropertyBase* start, SVGPropertyBase* end, PassRefPtrWillBeRawPtr<SVGAnimatedPropertyBase> attribute, SVGNumberNegativeValuesMode negativeValuesMode)
    {
        return adoptRef(new NumberSVGInterpolation(toInterpolableValue(start), toInterpolableValue(end), attribute, negativeValuesMode));
    }

    static bool canCreateFrom(SVGPropertyBase* value)
    {
        return true;
    }

    PassRefPtrWillBeRawPtr<SVGPropertyBase> interpolatedValue(SVGElement&) const final
    {
        return fromInterpolableValue(*m_cachedValue, m_negativeValuesMode);
    }

    static PassOwnPtr<InterpolableNumber> toInterpolableValue(SVGPropertyBase* value)
    {
        return InterpolableNumber::create(toSVGNumber(value)->value());
    }

    static PassRefPtrWillBeRawPtr<SVGNumber> fromInterpolableValue(const InterpolableValue&, SVGNumberNegativeValuesMode = AllowNegativeNumbers);

private:
    NumberSVGInterpolation(PassOwnPtr<InterpolableValue> start, PassOwnPtr<InterpolableValue> end, PassRefPtrWillBeRawPtr<SVGAnimatedPropertyBase> attribute, SVGNumberNegativeValuesMode negativeValuesMode)
        : SVGInterpolation(start, end, attribute)
        , m_negativeValuesMode(negativeValuesMode)
    {
    }

    const SVGNumberNegativeValuesMode m_negativeValuesMode;
};

}

#endif // NumberSVGInterpolation_h
