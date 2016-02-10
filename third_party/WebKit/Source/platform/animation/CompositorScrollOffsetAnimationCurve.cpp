// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/animation/CompositorScrollOffsetAnimationCurve.h"

#include "cc/animation/scroll_offset_animation_curve.h"
#include "cc/animation/timing_function.h"

using blink::CompositorScrollOffsetAnimationCurve;
using DurationBehavior = cc::ScrollOffsetAnimationCurve::DurationBehavior;

namespace blink {

static DurationBehavior GetDurationBehavior(CompositorScrollOffsetAnimationCurve::ScrollDurationBehavior webDurationBehavior)
{
    switch (webDurationBehavior) {
    case CompositorScrollOffsetAnimationCurve::ScrollDurationDeltaBased:
        return DurationBehavior::DELTA_BASED;

    case CompositorScrollOffsetAnimationCurve::ScrollDurationConstant:
        return DurationBehavior::CONSTANT;

    case CompositorScrollOffsetAnimationCurve::ScrollDurationInverseDelta:
        return DurationBehavior::INVERSE_DELTA;
    }
    NOTREACHED();
    return DurationBehavior::DELTA_BASED;
}

CompositorScrollOffsetAnimationCurve::CompositorScrollOffsetAnimationCurve(
    FloatPoint targetValue,
    TimingFunctionType timingFunction,
    ScrollDurationBehavior durationBehavior)
    : m_curve(cc::ScrollOffsetAnimationCurve::Create(
        gfx::ScrollOffset(targetValue.x(), targetValue.y()),
        createTimingFunction(timingFunction),
        GetDurationBehavior(durationBehavior)))
{
}

CompositorScrollOffsetAnimationCurve::~CompositorScrollOffsetAnimationCurve()
{
}

CompositorAnimationCurve::AnimationCurveType CompositorScrollOffsetAnimationCurve::type() const
{
    return CompositorAnimationCurve::AnimationCurveTypeScrollOffset;
}

void CompositorScrollOffsetAnimationCurve::setInitialValue(FloatPoint initialValue)
{
    m_curve->SetInitialValue(gfx::ScrollOffset(initialValue.x(), initialValue.y()));
}

FloatPoint CompositorScrollOffsetAnimationCurve::getValue(double time) const
{
    gfx::ScrollOffset value = m_curve->GetValue(base::TimeDelta::FromSecondsD(time));
    return FloatPoint(value.x(), value.y());
}

double CompositorScrollOffsetAnimationCurve::duration() const
{
    return m_curve->Duration().InSecondsF();
}

FloatPoint CompositorScrollOffsetAnimationCurve::targetValue() const
{
    gfx::ScrollOffset target = m_curve->target_value();
    return FloatPoint(target.x(), target.y());
}

void CompositorScrollOffsetAnimationCurve::updateTarget(double time, FloatPoint newTarget)
{
    m_curve->UpdateTarget(time, gfx::ScrollOffset(newTarget.x(), newTarget.y()));
}

scoped_ptr<cc::AnimationCurve> CompositorScrollOffsetAnimationCurve::cloneToAnimationCurve() const
{
    return m_curve->Clone();
}

} // namespace blink
