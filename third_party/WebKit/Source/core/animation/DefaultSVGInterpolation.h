// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DefaultSVGInterpolation_h
#define DefaultSVGInterpolation_h

#include "core/animation/SVGInterpolation.h"

namespace blink {

class DefaultSVGInterpolation : public SVGInterpolation {
public:
    static PassRefPtr<DefaultSVGInterpolation> create(SVGPropertyBase* start, SVGPropertyBase* end, PassRefPtrWillBeRawPtr<SVGAnimatedPropertyBase> attribute)
    {
        return adoptRef(new DefaultSVGInterpolation(start, end, attribute));
    }

    PassRefPtrWillBeRawPtr<SVGPropertyBase> interpolatedValue(SVGElement&) const final;

private:
    DefaultSVGInterpolation(SVGPropertyBase* start, SVGPropertyBase* end, PassRefPtrWillBeRawPtr<SVGAnimatedPropertyBase> attribute);

    RefPtrWillBePersistent<SVGPropertyBase> m_start;
    RefPtrWillBePersistent<SVGPropertyBase> m_end;
};

}

#endif // DefaultSVGInterpolation_h
