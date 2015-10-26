// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PathSVGInterpolation_h
#define PathSVGInterpolation_h

#include "core/animation/SVGInterpolation.h"
#include "core/svg/SVGPathData.h"

namespace blink {

class PathSVGInterpolation : public SVGInterpolation {
public:
    static PassRefPtr<PathSVGInterpolation> maybeCreate(SVGPropertyBase* start, SVGPropertyBase* end, PassRefPtrWillBeRawPtr<SVGAnimatedPropertyBase> attribute);

    PassRefPtrWillBeRawPtr<SVGPropertyBase> interpolatedValue(SVGElement&) const final;

private:
    PathSVGInterpolation(PassOwnPtr<InterpolableValue> start, PassOwnPtr<InterpolableValue> end, PassRefPtrWillBeRawPtr<SVGAnimatedPropertyBase> attribute,
        Vector<SVGPathSegType> pathSegTypes)
        : SVGInterpolation(start, end, attribute)
    {
        m_pathSegTypes.swap(pathSegTypes);
    }

    static PassRefPtrWillBeRawPtr<SVGPropertyBase> fromInterpolableValue(const InterpolableValue&, const Vector<SVGPathSegType>&);

    Vector<SVGPathSegType> m_pathSegTypes;
};

}

#endif // NumberSVGInterpolation_h
