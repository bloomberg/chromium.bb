// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/animation/CompositorTransformAnimationCurve.h"

#include "cc/animation/keyframed_animation_curve.h"
#include "cc/animation/timing_function.h"
#include "cc/animation/transform_operations.h"
#include "platform/animation/CompositorTransformOperations.h"

using blink::CompositorTransformKeyframe;

namespace blink {

CompositorTransformAnimationCurve::CompositorTransformAnimationCurve()
    : m_curve(cc::KeyframedTransformAnimationCurve::Create())
{
}

CompositorTransformAnimationCurve::~CompositorTransformAnimationCurve()
{
}

void CompositorTransformAnimationCurve::addLinearKeyframe(const CompositorTransformKeyframe& keyframe)
{
    const cc::TransformOperations& transformOperations = keyframe.value().asTransformOperations();
    m_curve->AddKeyframe(cc::TransformKeyframe::Create(
        base::TimeDelta::FromSecondsD(keyframe.time()), transformOperations, nullptr));
}

void CompositorTransformAnimationCurve::addCubicBezierKeyframe(const CompositorTransformKeyframe& keyframe, const TimingFunction& timingFunction)
{
    const cc::TransformOperations& transformOperations = keyframe.value().asTransformOperations();
    m_curve->AddKeyframe(cc::TransformKeyframe::Create(
        base::TimeDelta::FromSecondsD(keyframe.time()), transformOperations,
        timingFunction.cloneToCC()));
}

void CompositorTransformAnimationCurve::addStepsKeyframe(const CompositorTransformKeyframe& keyframe, const TimingFunction& timingFunction)
{
    const cc::TransformOperations& transformOperations = keyframe.value().asTransformOperations();
    m_curve->AddKeyframe(cc::TransformKeyframe::Create(
        base::TimeDelta::FromSecondsD(keyframe.time()), transformOperations,
        timingFunction.cloneToCC()));
}

void CompositorTransformAnimationCurve::setLinearTimingFunction()
{
    m_curve->SetTimingFunction(nullptr);
}

void CompositorTransformAnimationCurve::setCubicBezierTimingFunction(const TimingFunction& timingFunction)
{
    m_curve->SetTimingFunction(timingFunction.cloneToCC());
}

void CompositorTransformAnimationCurve::setStepsTimingFunction(const TimingFunction& timingFunction)
{
    m_curve->SetTimingFunction(timingFunction.cloneToCC());
}

std::unique_ptr<cc::AnimationCurve> CompositorTransformAnimationCurve::cloneToAnimationCurve() const
{
    return m_curve->Clone();
}

} // namespace blink
