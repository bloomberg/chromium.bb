// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DoubleStyleInterpolation_h
#define DoubleStyleInterpolation_h

#include "core/animation/StyleInterpolation.h"

namespace blink {

enum ClampRange {
    NoClamp,
    ClampOpacity
};

class DoubleStyleInterpolation : public StyleInterpolation {
public:
    static PassRefPtrWillBeRawPtr<DoubleStyleInterpolation> create(const CSSValue& start, const CSSValue& end, CSSPropertyID id, ClampRange clamp)
    {
        return adoptRefWillBeNoop(new DoubleStyleInterpolation(doubleToInterpolableValue(start), doubleToInterpolableValue(end), id, clamp, false));
    }

    static PassRefPtrWillBeRawPtr<DoubleStyleInterpolation> maybeCreateFromMotionRotation(const CSSValue& start, const CSSValue& end, CSSPropertyID);

    static bool canCreateFrom(const CSSValue&);

    virtual void apply(StyleResolverState&) const override;

    virtual void trace(Visitor*) override;

private:
    DoubleStyleInterpolation(PassOwnPtrWillBeRawPtr<InterpolableValue> start, PassOwnPtrWillBeRawPtr<InterpolableValue> end, CSSPropertyID id, ClampRange clamp, bool flag)
        : StyleInterpolation(start, end, id)
        , m_clamp(clamp)
        , m_flag(flag)
    { }

    static PassOwnPtrWillBeRawPtr<InterpolableValue> doubleToInterpolableValue(const CSSValue&);
    static PassOwnPtrWillBeRawPtr<InterpolableValue> motionRotationToInterpolableValue(const CSSValue&);
    static PassRefPtrWillBeRawPtr<CSSValue> interpolableValueToDouble(InterpolableValue*, ClampRange);
    static PassRefPtrWillBeRawPtr<CSSValue> interpolableValueToMotionRotation(InterpolableValue*, bool);

    ClampRange m_clamp;
    bool m_flag;

    friend class AnimationDoubleStyleInterpolationTest;
};
}
#endif
