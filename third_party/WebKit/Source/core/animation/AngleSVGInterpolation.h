// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AngleSVGInterpolation_h
#define AngleSVGInterpolation_h

#include "core/animation/SVGInterpolation.h"
#include "core/svg/SVGAngle.h"

namespace blink {

class SVGInteger;

class AngleSVGInterpolation : public SVGInterpolation {
public:
    static PassRefPtr<AngleSVGInterpolation> create(SVGPropertyBase* start, SVGPropertyBase* end, PassRefPtrWillBeRawPtr<SVGAnimatedPropertyBase> attribute)
    {
        return adoptRef(new AngleSVGInterpolation(toInterpolableValue(start), toInterpolableValue(end), attribute));
    }

    static bool canCreateFrom(SVGPropertyBase* value)
    {
        return toSVGAngle(value)->orientType()->enumValue() == SVGMarkerOrientAngle;
    }

    PassRefPtrWillBeRawPtr<SVGPropertyBase> interpolatedValue(SVGElement&) const final
    {
        return fromInterpolableValue(m_cachedValue.get());
    }

private:
    AngleSVGInterpolation(PassOwnPtr<InterpolableValue> start, PassOwnPtr<InterpolableValue> end, PassRefPtrWillBeRawPtr<SVGAnimatedPropertyBase> attribute)
        : SVGInterpolation(start, end, attribute)
    {
    }

    static PassOwnPtr<InterpolableValue> toInterpolableValue(SVGPropertyBase* value)
    {
        return InterpolableNumber::create(toSVGAngle(value)->value());
    }

    static PassRefPtrWillBeRawPtr<SVGAngle> fromInterpolableValue(InterpolableValue* value)
    {
        double doubleValue = toInterpolableNumber(value)->value();
        RefPtrWillBeRawPtr<SVGAngle> result = SVGAngle::create();
        result->newValueSpecifiedUnits(SVGAngle::SVG_ANGLETYPE_DEG, doubleValue);
        return result.release();
    }
};

}

#endif // AngleSVGInterpolation_h
