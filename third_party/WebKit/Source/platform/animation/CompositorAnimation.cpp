// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/animation/CompositorAnimation.h"

#include "cc/animation/animation.h"
#include "cc/animation/animation_curve.h"
#include "cc/animation/animation_id_provider.h"
#include "platform/animation/CompositorAnimationCurve.h"
#include "platform/animation/CompositorFilterAnimationCurve.h"
#include "platform/animation/CompositorFloatAnimationCurve.h"
#include "platform/animation/CompositorScrollOffsetAnimationCurve.h"
#include "platform/animation/CompositorTransformAnimationCurve.h"

using cc::Animation;
using cc::AnimationIdProvider;

using blink::CompositorAnimation;
using blink::CompositorAnimationCurve;

namespace blink {

#define STATIC_ASSERT_ENUM(a, b)                              \
    static_assert(static_cast<int>(a) == static_cast<int>(b), \
        "mismatching enums: " #a)

// TargetProperty
STATIC_ASSERT_ENUM(CompositorAnimation::TargetPropertyTransform, cc::Animation::TRANSFORM);
STATIC_ASSERT_ENUM(CompositorAnimation::TargetPropertyOpacity, cc::Animation::OPACITY);
STATIC_ASSERT_ENUM(CompositorAnimation::TargetPropertyFilter, cc::Animation::FILTER);
STATIC_ASSERT_ENUM(CompositorAnimation::TargetPropertyScrollOffset, cc::Animation::SCROLL_OFFSET);

CompositorAnimation::CompositorAnimation(const CompositorAnimationCurve& webCurve, TargetProperty targetProperty, int animationId, int groupId)
{
    if (!animationId)
        animationId = AnimationIdProvider::NextAnimationId();
    if (!groupId)
        groupId = AnimationIdProvider::NextGroupId();

    CompositorAnimationCurve::AnimationCurveType curveType = webCurve.type();
    scoped_ptr<cc::AnimationCurve> curve;
    switch (curveType) {
    case CompositorAnimationCurve::AnimationCurveTypeFloat: {
        const blink::CompositorFloatAnimationCurve* floatCurve = static_cast<const blink::CompositorFloatAnimationCurve*>(&webCurve);
        curve = floatCurve->cloneToAnimationCurve();
        break;
    }
    case CompositorAnimationCurve::AnimationCurveTypeTransform: {
        const blink::CompositorTransformAnimationCurve* transformCurve = static_cast<const blink::CompositorTransformAnimationCurve*>(&webCurve);
        curve = transformCurve->cloneToAnimationCurve();
        break;
    }
    case CompositorAnimationCurve::AnimationCurveTypeFilter: {
        const blink::CompositorFilterAnimationCurve* filterCurve = static_cast<const blink::CompositorFilterAnimationCurve*>(&webCurve);
        curve = filterCurve->cloneToAnimationCurve();
        break;
    }
    case CompositorAnimationCurve::AnimationCurveTypeScrollOffset: {
        const blink::CompositorScrollOffsetAnimationCurve* scrollCurve = static_cast<const blink::CompositorScrollOffsetAnimationCurve*>(&webCurve);
        curve = scrollCurve->cloneToAnimationCurve();
        break;
    }
    }
    m_animation = Animation::Create(
        std::move(curve), animationId, groupId,
        static_cast<cc::Animation::TargetProperty>(targetProperty));
}

CompositorAnimation::CompositorAnimation() {}

CompositorAnimation::~CompositorAnimation() {}

int CompositorAnimation::id()
{
    return m_animation->id();
}

int CompositorAnimation::group()
{
    return m_animation->group();
}

blink::CompositorAnimation::TargetProperty CompositorAnimation::targetProperty() const
{
    return static_cast<CompositorAnimation::TargetProperty>(m_animation->target_property());
}

double CompositorAnimation::iterations() const
{
    return m_animation->iterations();
}

void CompositorAnimation::setIterations(double n)
{
    m_animation->set_iterations(n);
}

double CompositorAnimation::iterationStart() const
{
    return m_animation->iteration_start();
}

void CompositorAnimation::setIterationStart(double iterationStart)
{
    m_animation->set_iteration_start(iterationStart);
}

double CompositorAnimation::startTime() const
{
    return (m_animation->start_time() - base::TimeTicks()).InSecondsF();
}

void CompositorAnimation::setStartTime(double monotonicTime)
{
    m_animation->set_start_time(base::TimeTicks::FromInternalValue(
        monotonicTime * base::Time::kMicrosecondsPerSecond));
}

double CompositorAnimation::timeOffset() const
{
    return m_animation->time_offset().InSecondsF();
}

void CompositorAnimation::setTimeOffset(double monotonicTime)
{
    m_animation->set_time_offset(base::TimeDelta::FromSecondsD(monotonicTime));
}

blink::CompositorAnimation::Direction CompositorAnimation::direction() const
{
    switch (m_animation->direction()) {
    case cc::Animation::DIRECTION_NORMAL:
        return DirectionNormal;
    case cc::Animation::DIRECTION_REVERSE:
        return DirectionReverse;
    case cc::Animation::DIRECTION_ALTERNATE:
        return DirectionAlternate;
    case cc::Animation::DIRECTION_ALTERNATE_REVERSE:
        return DirectionAlternateReverse;
    default:
        NOTREACHED();
    }
    return DirectionNormal;
}

void CompositorAnimation::setDirection(Direction direction)
{
    switch (direction) {
    case DirectionNormal:
        m_animation->set_direction(cc::Animation::DIRECTION_NORMAL);
        break;
    case DirectionReverse:
        m_animation->set_direction(cc::Animation::DIRECTION_REVERSE);
        break;
    case DirectionAlternate:
        m_animation->set_direction(cc::Animation::DIRECTION_ALTERNATE);
        break;
    case DirectionAlternateReverse:
        m_animation->set_direction(cc::Animation::DIRECTION_ALTERNATE_REVERSE);
        break;
    }
}

double CompositorAnimation::playbackRate() const
{
    return m_animation->playback_rate();
}

void CompositorAnimation::setPlaybackRate(double playbackRate)
{
    m_animation->set_playback_rate(playbackRate);
}

blink::CompositorAnimation::FillMode CompositorAnimation::fillMode() const
{
    switch (m_animation->fill_mode()) {
    case cc::Animation::FILL_MODE_NONE:
        return FillModeNone;
    case cc::Animation::FILL_MODE_FORWARDS:
        return FillModeForwards;
    case cc::Animation::FILL_MODE_BACKWARDS:
        return FillModeBackwards;
    case cc::Animation::FILL_MODE_BOTH:
        return FillModeBoth;
    default:
        NOTREACHED();
    }
    return FillModeNone;
}

void CompositorAnimation::setFillMode(FillMode fillMode)
{
    switch (fillMode) {
    case FillModeNone:
        m_animation->set_fill_mode(cc::Animation::FILL_MODE_NONE);
        break;
    case FillModeForwards:
        m_animation->set_fill_mode(cc::Animation::FILL_MODE_FORWARDS);
        break;
    case FillModeBackwards:
        m_animation->set_fill_mode(cc::Animation::FILL_MODE_BACKWARDS);
        break;
    case FillModeBoth:
        m_animation->set_fill_mode(cc::Animation::FILL_MODE_BOTH);
        break;
    }
}

scoped_ptr<cc::Animation> CompositorAnimation::passAnimation()
{
    m_animation->set_needs_synchronized_start_time(true);
    return std::move(m_animation);
}

cc::Animation* CompositorAnimation::releaseCCAnimation()
{
    m_animation->set_needs_synchronized_start_time(true);
    return m_animation.release();
}

} // namespace blink
