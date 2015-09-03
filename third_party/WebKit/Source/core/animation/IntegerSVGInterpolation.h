// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IntegerSVGInterpolation_h
#define IntegerSVGInterpolation_h

#include "core/animation/SVGInterpolation.h"
#include "core/svg/SVGInteger.h"

namespace blink {

class IntegerSVGInterpolation : public SVGInterpolation {
public:
    static PassRefPtr<IntegerSVGInterpolation> create(SVGPropertyBase* start, SVGPropertyBase* end, PassRefPtrWillBeRawPtr<SVGAnimatedPropertyBase> attribute)
    {
        return adoptRef(new IntegerSVGInterpolation(toInterpolableValue(start), toInterpolableValue(end), attribute));
    }

    PassRefPtrWillBeRawPtr<SVGPropertyBase> interpolatedValue(SVGElement&) const final
    {
        return fromInterpolableValue(*m_cachedValue);
    }

private:
    IntegerSVGInterpolation(PassOwnPtr<InterpolableValue> start, PassOwnPtr<InterpolableValue> end, PassRefPtrWillBeRawPtr<SVGAnimatedPropertyBase> attribute)
        : SVGInterpolation(start, end, attribute)
    {
    }

    static PassOwnPtr<InterpolableValue> toInterpolableValue(SVGPropertyBase* value)
    {
        return InterpolableNumber::create(toSVGInteger(value)->value());
    }

    static PassRefPtrWillBeRawPtr<SVGInteger> fromInterpolableValue(const InterpolableValue& value)
    {
        return SVGInteger::create(clampTo<int>(roundf(toInterpolableNumber(value).value())));
    }
};

}

#endif // IntegerSVGInterpolation_h
