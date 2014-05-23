// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LengthStyleInterpolation_h
#define LengthStyleInterpolation_h

#include "core/animation/interpolation/StyleInterpolation.h"

namespace WebCore {

class LengthStyleInterpolation : public StyleInterpolation {
public:
    static PassRefPtrWillBeRawPtr<LengthStyleInterpolation> create(CSSValue* start, CSSValue* end, CSSPropertyID id)
    {
        return adoptRefWillBeNoop(new LengthStyleInterpolation(lengthToInterpolableValue(start), lengthToInterpolableValue(end), id));
    }

    static bool canCreateFrom(const CSSValue&);

    virtual void apply(StyleResolverState&) const OVERRIDE;

    virtual void trace(Visitor*) OVERRIDE;

private:
    LengthStyleInterpolation(PassOwnPtrWillBeRawPtr<InterpolableValue> start, PassOwnPtrWillBeRawPtr<InterpolableValue> end, CSSPropertyID id)
        : StyleInterpolation(start, end, id)
    { }

    static PassOwnPtrWillBeRawPtr<InterpolableValue> lengthToInterpolableValue(CSSValue*);
    static PassRefPtrWillBeRawPtr<CSSValue> interpolableValueToLength(InterpolableValue*);

    friend class AnimationLengthStyleInterpolationTest;
};

}

#endif
