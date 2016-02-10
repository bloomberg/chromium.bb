// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorScrollOffsetAnimationCurve_h
#define CompositorScrollOffsetAnimationCurve_h

#include "base/memory/scoped_ptr.h"
#include "platform/PlatformExport.h"
#include "platform/animation/CompositorAnimationCurve.h"
#include "platform/geometry/FloatPoint.h"
#include "wtf/Noncopyable.h"

namespace cc {
class AnimationCurve;
class ScrollOffsetAnimationCurve;
}

namespace blink {

class PLATFORM_EXPORT CompositorScrollOffsetAnimationCurve : public CompositorAnimationCurve {
    WTF_MAKE_NONCOPYABLE(CompositorScrollOffsetAnimationCurve);
public:
    enum ScrollDurationBehavior {
        ScrollDurationDeltaBased = 0,
        ScrollDurationConstant,
        ScrollDurationInverseDelta
    };

    CompositorScrollOffsetAnimationCurve(FloatPoint, TimingFunctionType, ScrollDurationBehavior);
    ~CompositorScrollOffsetAnimationCurve() override;

    // CompositorAnimationCurve implementation.
    AnimationCurveType type() const override;

    virtual void setInitialValue(FloatPoint);
    virtual FloatPoint getValue(double time) const;
    virtual double duration() const;
    virtual FloatPoint targetValue() const;
    virtual void updateTarget(double time, FloatPoint newTarget);

    scoped_ptr<cc::AnimationCurve> cloneToAnimationCurve() const;

private:
    scoped_ptr<cc::ScrollOffsetAnimationCurve> m_curve;
};

} // namespace blink

#endif // CompositorScrollOffsetAnimationCurve_h
