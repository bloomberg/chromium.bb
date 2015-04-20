// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IntegerOptionalIntegerSVGInterpolation_h
#define IntegerOptionalIntegerSVGInterpolation_h

#include "core/animation/SVGInterpolation.h"
#include "core/svg/SVGIntegerOptionalInteger.h"

namespace blink {

class SVGIntegerOptionalInteger;

class IntegerOptionalIntegerSVGInterpolation : public SVGInterpolation {
public:
    static PassRefPtrWillBeRawPtr<IntegerOptionalIntegerSVGInterpolation> create(SVGPropertyBase* start, SVGPropertyBase* end, PassRefPtrWillBeRawPtr<SVGAnimatedPropertyBase> attribute)
    {
        return adoptRefWillBeNoop(new IntegerOptionalIntegerSVGInterpolation(toInterpolableValue(start), toInterpolableValue(end), attribute));
    }

    virtual PassRefPtrWillBeRawPtr<SVGPropertyBase> interpolatedValue(SVGElement&) const override final
    {
        return fromInterpolableValue(*m_cachedValue, m_min);
    }

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        SVGInterpolation::trace(visitor);
    }

private:
    IntegerOptionalIntegerSVGInterpolation(PassOwnPtrWillBeRawPtr<InterpolableValue> start, PassOwnPtrWillBeRawPtr<InterpolableValue> end, PassRefPtrWillBeRawPtr<SVGAnimatedPropertyBase> attribute);

    static PassOwnPtrWillBeRawPtr<InterpolableValue> toInterpolableValue(SVGPropertyBase*);

    static PassRefPtrWillBeRawPtr<SVGIntegerOptionalInteger> fromInterpolableValue(const InterpolableValue&, int);

    const int m_min;
};

}

#endif // IntegerOptionalIntegerSVGInterpolation_h
