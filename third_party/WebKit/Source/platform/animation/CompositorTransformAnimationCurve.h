// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorTransformAnimationCurve_h
#define CompositorTransformAnimationCurve_h

#include "base/memory/scoped_ptr.h"
#include "platform/PlatformExport.h"
#include "platform/animation/CompositorAnimationCurve.h"
#include "platform/animation/CompositorTransformKeyframe.h"
#include "wtf/Noncopyable.h"

namespace cc {
class AnimationCurve;
class KeyframedTransformAnimationCurve;
}

namespace blink {
class CompositorTransformKeyframe;
}

namespace blink {

// A keyframed transform animation curve.
class PLATFORM_EXPORT CompositorTransformAnimationCurve : public CompositorAnimationCurve {
    WTF_MAKE_NONCOPYABLE(CompositorTransformAnimationCurve);
public:
    CompositorTransformAnimationCurve();
    ~CompositorTransformAnimationCurve() override;

    // Adds the keyframe with the default timing function (ease).
    virtual void add(const CompositorTransformKeyframe&);
    virtual void add(const CompositorTransformKeyframe&, TimingFunctionType);
    // Adds the keyframe with a custom, bezier timing function. Note, it is
    // assumed that x0 = y0, and x3 = y3 = 1.
    virtual void add(const CompositorTransformKeyframe&, double x1, double y1, double x2, double y2);
    // Adds the keyframe with a steps timing function.
    virtual void add(const CompositorTransformKeyframe&, int steps, float stepsStartOffset);

    virtual void setLinearTimingFunction();
    virtual void setCubicBezierTimingFunction(TimingFunctionType);
    virtual void setCubicBezierTimingFunction(double x1, double y1, double x2, double y2);
    virtual void setStepsTimingFunction(int numberOfSteps, float stepsStartOffset);

    // CompositorAnimationCurve implementation.
    AnimationCurveType type() const override;

    scoped_ptr<cc::AnimationCurve> cloneToAnimationCurve() const;

private:
    scoped_ptr<cc::KeyframedTransformAnimationCurve> m_curve;
};

} // namespace blink

#endif // CompositorTransformAnimationCurve_h
