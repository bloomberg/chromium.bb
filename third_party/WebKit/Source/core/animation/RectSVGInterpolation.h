// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RectSVGInterpolation_h
#define RectSVGInterpolation_h

#include "core/animation/SVGInterpolation.h"
#include "core/svg/SVGRect.h"

namespace blink {

class SVGRect;

class RectSVGInterpolation : public SVGInterpolation {
public:
    static PassRefPtr<RectSVGInterpolation> create(SVGPropertyBase* start, SVGPropertyBase* end, PassRefPtrWillBeRawPtr<SVGAnimatedPropertyBase> attribute)
    {
        return adoptRef(new RectSVGInterpolation(toInterpolableValue(start), toInterpolableValue(end), attribute));
    }

    PassRefPtrWillBeRawPtr<SVGPropertyBase> interpolatedValue(SVGElement&) const final
    {
        return fromInterpolableValue(*m_cachedValue);
    }

private:
    RectSVGInterpolation(PassOwnPtr<InterpolableValue> start, PassOwnPtr<InterpolableValue> end, PassRefPtrWillBeRawPtr<SVGAnimatedPropertyBase> attribute)
        : SVGInterpolation(start, end, attribute)
    {
    }

    static PassOwnPtr<InterpolableValue> toInterpolableValue(SVGPropertyBase*);

    static PassRefPtrWillBeRawPtr<SVGRect> fromInterpolableValue(const InterpolableValue&);
};

}

#endif // RectSVGInterpolation_h
