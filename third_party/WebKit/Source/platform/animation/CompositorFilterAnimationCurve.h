// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorFilterAnimationCurve_h
#define CompositorFilterAnimationCurve_h

#include "platform/PlatformExport.h"
#include "platform/animation/CompositorAnimationCurve.h"
#include "platform/animation/CompositorFilterKeyframe.h"
#include "platform/animation/TimingFunction.h"
#include "wtf/Noncopyable.h"

#include <memory>

namespace cc {
class AnimationCurve;
class KeyframedFilterAnimationCurve;
}

namespace blink {
class CompositorFilterKeyframe;
}

namespace blink {

// A keyframed filter animation curve.
class PLATFORM_EXPORT CompositorFilterAnimationCurve : public CompositorAnimationCurve {
    WTF_MAKE_NONCOPYABLE(CompositorFilterAnimationCurve);
public:
    CompositorFilterAnimationCurve();
    ~CompositorFilterAnimationCurve() override;

    virtual void add(const CompositorFilterKeyframe&, TimingFunctionType = TimingFunctionTypeEase);
    // Adds the keyframe with a custom, bezier timing function. Note, it is
    // assumed that x0 = y0, and x3 = y3 = 1.
    virtual void add(const CompositorFilterKeyframe&, double x1, double y1, double x2, double y2);
    // Adds the keyframe with a steps timing function.
    virtual void add(const CompositorFilterKeyframe&, int steps, StepsTimingFunction::StepPosition);

    virtual void setLinearTimingFunction();
    virtual void setCubicBezierTimingFunction(TimingFunctionType);
    virtual void setCubicBezierTimingFunction(double x1, double y1, double x2, double y2);
    virtual void setStepsTimingFunction(int numberOfSteps, StepsTimingFunction::StepPosition);

    // blink::CompositorAnimationCurve implementation.
    AnimationCurveType type() const override;

    std::unique_ptr<cc::AnimationCurve> cloneToAnimationCurve() const;

private:
    std::unique_ptr<cc::KeyframedFilterAnimationCurve> m_curve;
};

} // namespace blink

#endif // CompositorFilterAnimationCurve_h
