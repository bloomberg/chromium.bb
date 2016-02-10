// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorFloatAnimationCurve_h
#define CompositorFloatAnimationCurve_h

#include "base/memory/scoped_ptr.h"
#include "platform/PlatformExport.h"
#include "platform/animation/CompositorAnimationCurve.h"
#include "platform/animation/CompositorFloatKeyframe.h"
#include "wtf/Noncopyable.h"

namespace cc {
class AnimationCurve;
class KeyframedFloatAnimationCurve;
}

namespace blink {
struct CompositorFloatKeyframe;
}

namespace blink {

// A keyframed float animation curve.
class PLATFORM_EXPORT CompositorFloatAnimationCurve : public CompositorAnimationCurve {
    WTF_MAKE_NONCOPYABLE(CompositorFloatAnimationCurve);
public:
    CompositorFloatAnimationCurve();
    ~CompositorFloatAnimationCurve() override;

    // Adds the keyframe with the default timing function (ease).
    virtual void add(const CompositorFloatKeyframe&);
    virtual void add(const CompositorFloatKeyframe&, TimingFunctionType);
    // Adds the keyframe with a custom, bezier timing function. Note, it is
    // assumed that x0 = y0 , and x3 = y3 = 1.
    virtual void add(const CompositorFloatKeyframe&, double x1, double y1, double x2, double y2);
    // Adds the keyframe with a steps timing function.
    virtual void add(const CompositorFloatKeyframe&, int steps, float stepsStartOffset);

    virtual void setLinearTimingFunction();
    virtual void setCubicBezierTimingFunction(TimingFunctionType);
    virtual void setCubicBezierTimingFunction(double x1, double y1, double x2, double y2);
    virtual void setStepsTimingFunction(int numberOfSteps, float stepsStartOffset);

    virtual float getValue(double time) const;

    // CompositorAnimationCurve implementation.
    AnimationCurveType type() const override;

    scoped_ptr<cc::AnimationCurve> cloneToAnimationCurve() const;

private:
    scoped_ptr<cc::KeyframedFloatAnimationCurve> m_curve;
};

} // namespace blink

#endif // CompositorFloatAnimationCurve_h
