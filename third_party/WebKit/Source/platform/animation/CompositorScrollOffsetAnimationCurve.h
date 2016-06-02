// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorScrollOffsetAnimationCurve_h
#define CompositorScrollOffsetAnimationCurve_h

#include "platform/PlatformExport.h"
#include "platform/animation/CompositorAnimationCurve.h"
#include "platform/geometry/FloatPoint.h"
#include "wtf/Noncopyable.h"

namespace cc {
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

    CompositorScrollOffsetAnimationCurve(FloatPoint, ScrollDurationBehavior);
    CompositorScrollOffsetAnimationCurve(cc::ScrollOffsetAnimationCurve*);
    ~CompositorScrollOffsetAnimationCurve() override;

    void setInitialValue(FloatPoint);
    FloatPoint getValue(double time) const;
    double duration() const;
    FloatPoint targetValue() const;
    void applyAdjustment(IntSize);
    void updateTarget(double time, FloatPoint newTarget);

    // CompositorAnimationCurve implementation.
    std::unique_ptr<cc::AnimationCurve> cloneToAnimationCurve() const override;

private:
    std::unique_ptr<cc::ScrollOffsetAnimationCurve> m_curve;
};

} // namespace blink

#endif // CompositorScrollOffsetAnimationCurve_h
