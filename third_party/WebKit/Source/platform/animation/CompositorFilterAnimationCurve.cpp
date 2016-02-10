// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/animation/CompositorFilterAnimationCurve.h"

#include "cc/animation/keyframed_animation_curve.h"
#include "cc/animation/timing_function.h"
#include "cc/output/filter_operations.h"
#include "platform/graphics/CompositorFilterOperations.h"

using blink::CompositorFilterKeyframe;

namespace blink {

CompositorFilterAnimationCurve::CompositorFilterAnimationCurve()
    : m_curve(cc::KeyframedFilterAnimationCurve::Create())
{
}

CompositorFilterAnimationCurve::~CompositorFilterAnimationCurve()
{
}

blink::CompositorAnimationCurve::AnimationCurveType CompositorFilterAnimationCurve::type() const
{
    return CompositorAnimationCurve::AnimationCurveTypeFilter;
}

void CompositorFilterAnimationCurve::add(const CompositorFilterKeyframe& keyframe, TimingFunctionType type)
{
    const cc::FilterOperations& filterOperations = keyframe.value().asFilterOperations();
    m_curve->AddKeyframe(cc::FilterKeyframe::Create(
        base::TimeDelta::FromSecondsD(keyframe.time()), filterOperations,
        createTimingFunction(type)));
}

void CompositorFilterAnimationCurve::add(const CompositorFilterKeyframe& keyframe, double x1, double y1, double x2, double y2)
{
    const cc::FilterOperations& filterOperations = keyframe.value().asFilterOperations();
    m_curve->AddKeyframe(cc::FilterKeyframe::Create(
        base::TimeDelta::FromSecondsD(keyframe.time()), filterOperations,
        cc::CubicBezierTimingFunction::Create(x1, y1, x2, y2)));
}

void CompositorFilterAnimationCurve::add(const CompositorFilterKeyframe& keyframe, int steps, float stepsStartOffset)
{
    const cc::FilterOperations& filterOperations = keyframe.value().asFilterOperations();
    m_curve->AddKeyframe(cc::FilterKeyframe::Create(
        base::TimeDelta::FromSecondsD(keyframe.time()), filterOperations,
        cc::StepsTimingFunction::Create(steps, stepsStartOffset)));
}

void CompositorFilterAnimationCurve::setLinearTimingFunction()
{
    m_curve->SetTimingFunction(nullptr);
}

void CompositorFilterAnimationCurve::setCubicBezierTimingFunction(TimingFunctionType type)
{
    m_curve->SetTimingFunction(createTimingFunction(type));
}

void CompositorFilterAnimationCurve::setCubicBezierTimingFunction(double x1, double y1, double x2, double y2)
{
    m_curve->SetTimingFunction(cc::CubicBezierTimingFunction::Create(x1, y1, x2, y2));
}

void CompositorFilterAnimationCurve::setStepsTimingFunction(int numberOfSteps, float stepsStartOffset)
{
    m_curve->SetTimingFunction(cc::StepsTimingFunction::Create(numberOfSteps, stepsStartOffset));
}

scoped_ptr<cc::AnimationCurve> CompositorFilterAnimationCurve::cloneToAnimationCurve() const
{
    return m_curve->Clone();
}

} // namespace blink
