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
        return adoptRefWillBeNoop(new DoubleStyleInterpolation(doubleToInterpolableValue(start), doubleToInterpolableValue(end), id, clamp));
    }

    static bool canCreateFrom(const CSSValue&);

    virtual void apply(StyleResolverState&) const override;

    virtual void trace(Visitor*) override;

private:
    DoubleStyleInterpolation(PassOwnPtrWillBeRawPtr<InterpolableValue> start, PassOwnPtrWillBeRawPtr<InterpolableValue> end, CSSPropertyID id, ClampRange clamp)
        : StyleInterpolation(start, end, id)
        , m_clamp(clamp)
    { }

    static PassOwnPtrWillBeRawPtr<InterpolableValue> doubleToInterpolableValue(const CSSValue&);
    static PassRefPtrWillBeRawPtr<CSSValue> interpolableValueToDouble(InterpolableValue*, ClampRange);

    ClampRange m_clamp;

    friend class AnimationDoubleStyleInterpolationTest;
};
}
#endif
