// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/animation/CompositorFloatAnimationCurve.h"

#include "cc/animation/animation_curve.h"
#include "cc/animation/keyframed_animation_curve.h"
#include "cc/animation/timing_function.h"

using blink::CompositorFloatKeyframe;

namespace blink {

CompositorFloatAnimationCurve::CompositorFloatAnimationCurve()
    : m_curve(cc::KeyframedFloatAnimationCurve::Create())
{
}

CompositorFloatAnimationCurve::~CompositorFloatAnimationCurve()
{
}

CompositorAnimationCurve::AnimationCurveType CompositorFloatAnimationCurve::type() const
{
    return CompositorAnimationCurve::AnimationCurveTypeFloat;
}

void CompositorFloatAnimationCurve::add(const CompositorFloatKeyframe& keyframe)
{
    add(keyframe, TimingFunctionTypeEase);
}

void CompositorFloatAnimationCurve::add(const CompositorFloatKeyframe& keyframe,
    TimingFunctionType type)
{
    m_curve->AddKeyframe(
        cc::FloatKeyframe::Create(base::TimeDelta::FromSecondsD(keyframe.time),
            keyframe.value, createTimingFunction(type)));
}

void CompositorFloatAnimationCurve::add(const CompositorFloatKeyframe& keyframe, double x1, double y1, double x2, double y2)
{
    m_curve->AddKeyframe(cc::FloatKeyframe::Create(
        base::TimeDelta::FromSecondsD(keyframe.time), keyframe.value,
        cc::CubicBezierTimingFunction::Create(x1, y1, x2, y2)));
}

void CompositorFloatAnimationCurve::add(const CompositorFloatKeyframe& keyframe, int steps, float stepsStartOffset)
{
    m_curve->AddKeyframe(cc::FloatKeyframe::Create(
        base::TimeDelta::FromSecondsD(keyframe.time), keyframe.value,
        cc::StepsTimingFunction::Create(steps, stepsStartOffset)));
}

void CompositorFloatAnimationCurve::setLinearTimingFunction()
{
    m_curve->SetTimingFunction(nullptr);
}

void CompositorFloatAnimationCurve::setCubicBezierTimingFunction(TimingFunctionType type)
{
    m_curve->SetTimingFunction(createTimingFunction(type));
}

void CompositorFloatAnimationCurve::setCubicBezierTimingFunction(double x1, double y1, double x2, double y2)
{
    m_curve->SetTimingFunction(cc::CubicBezierTimingFunction::Create(x1, y1, x2, y2));
}

void CompositorFloatAnimationCurve::setStepsTimingFunction(int numberOfSteps, float stepsStartOffset)
{
    m_curve->SetTimingFunction(cc::StepsTimingFunction::Create(numberOfSteps, stepsStartOffset));
}

float CompositorFloatAnimationCurve::getValue(double time) const
{
    return m_curve->GetValue(base::TimeDelta::FromSecondsD(time));
}

scoped_ptr<cc::AnimationCurve> CompositorFloatAnimationCurve::cloneToAnimationCurve() const
{
    return m_curve->Clone();
}

} // namespace blink
