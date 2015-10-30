// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LengthSVGInterpolation_h
#define LengthSVGInterpolation_h

#include "core/animation/SVGInterpolation.h"
#include "core/svg/SVGLength.h"
#include "core/svg/SVGLengthList.h"

namespace blink {

class LengthSVGInterpolation : public SVGInterpolation {
public:
    typedef SVGLengthList ListType;
    struct NonInterpolableType {
        DISALLOW_NEW();
        SVGLengthMode unitMode;
        SVGLengthNegativeValuesMode negativeValuesMode;
    };

    static bool canCreateFrom(SVGLength*, SVGLength*)
    {
        return true;
    }

    static PassRefPtrWillBeRawPtr<SVGLengthList> createList(const SVGAnimatedPropertyBase&);

    static PassRefPtr<LengthSVGInterpolation> create(SVGPropertyBase* start, SVGPropertyBase* end, PassRefPtrWillBeRawPtr<SVGAnimatedPropertyBase> attribute);

    static PassOwnPtr<InterpolableValue> toInterpolableValue(SVGLength*, const SVGAnimatedPropertyBase*, NonInterpolableType*);

    static PassRefPtrWillBeRawPtr<SVGLength> fromInterpolableValue(const InterpolableValue&, const NonInterpolableType&, const SVGElement*);

    PassRefPtrWillBeRawPtr<SVGPropertyBase> interpolatedValue(SVGElement&) const final;

private:
    LengthSVGInterpolation(PassOwnPtr<InterpolableValue> start, PassOwnPtr<InterpolableValue> end, PassRefPtrWillBeRawPtr<SVGAnimatedPropertyBase> attribute, const NonInterpolableType& modeData)
        : SVGInterpolation(start, end, attribute)
        , m_modeData(modeData)
    {
    }

    const NonInterpolableType m_modeData;
};

}

#endif // LengthSVGInterpolation_h
