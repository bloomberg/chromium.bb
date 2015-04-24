// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TransformSVGInterpolation_h
#define TransformSVGInterpolation_h

#include "core/animation/SVGInterpolation.h"
#include "core/svg/SVGTransformList.h"

namespace blink {

class TransformSVGInterpolation {
public:
    typedef SVGTransformList ListType;
    typedef SVGTransformType NonInterpolableType;

    static bool canCreateFrom(SVGTransform* start, SVGTransform* end);

    static PassOwnPtrWillBeRawPtr<InterpolableValue> toInterpolableValue(SVGTransform*, SVGTransformType*);

    static PassRefPtrWillBeRawPtr<SVGTransform> fromInterpolableValue(const InterpolableValue&, SVGTransformType);
};

}

#endif // TransformSVGInterpolation_h
