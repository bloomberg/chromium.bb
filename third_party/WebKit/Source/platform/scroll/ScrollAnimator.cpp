/*
 * Copyright (c) 2011, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/scroll/ScrollAnimator.h"

#include "platform/TraceEvent.h"
#include "platform/animation/CompositorAnimation.h"
#include "platform/graphics/CompositorFactory.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/scroll/ScrollableArea.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCompositorSupport.h"
#include "wtf/CurrentTime.h"
#include "wtf/PassRefPtr.h"

namespace blink {

PassOwnPtrWillBeRawPtr<ScrollAnimatorBase> ScrollAnimatorBase::create(ScrollableArea* scrollableArea)
{
    if (scrollableArea && scrollableArea->scrollAnimatorEnabled())
        return adoptPtrWillBeNoop(new ScrollAnimator(scrollableArea));
    return adoptPtrWillBeNoop(new ScrollAnimatorBase(scrollableArea));
}

ScrollAnimator::ScrollAnimator(ScrollableArea* scrollableArea, WTF::TimeFunction timeFunction)
    : ScrollAnimatorBase(scrollableArea)
    , m_timeFunction(timeFunction)
    , m_lastGranularity(ScrollByPixel)
{
}

ScrollAnimator::~ScrollAnimator()
{
}

FloatPoint ScrollAnimator::desiredTargetPosition() const
{
    return (m_animationCurve || m_runState == RunState::WaitingToSendToCompositor)
        ? m_targetOffset : currentPosition();
}

bool ScrollAnimator::hasRunningAnimation() const
{
    return (m_animationCurve || m_runState == RunState::WaitingToSendToCompositor);
}

float ScrollAnimator::computeDeltaToConsume(
    ScrollbarOrientation orientation, float pixelDelta) const
{
    FloatPoint pos = desiredTargetPosition();
    float currentPos = (orientation == HorizontalScrollbar) ? pos.x() : pos.y();
    float newPos = clampScrollPosition(orientation, currentPos + pixelDelta);
    return (currentPos == newPos) ? 0.0f : (newPos - currentPos);
}

void ScrollAnimator::resetAnimationState()
{
    ScrollAnimatorCompositorCoordinator::resetAnimationState();
    if (m_animationCurve)
        m_animationCurve.clear();
    m_startTime = 0.0;
}

ScrollResultOneDimensional ScrollAnimator::userScroll(
    ScrollbarOrientation orientation, ScrollGranularity granularity, float step, float delta)
{
    if (!m_scrollableArea->scrollAnimatorEnabled())
        return ScrollAnimatorBase::userScroll(orientation, granularity, step, delta);

    TRACE_EVENT0("blink", "ScrollAnimator::scroll");

    if (granularity == ScrollByPrecisePixel) {
        // Cancel scroll animation because asked to instant scroll.
        if (hasRunningAnimation())
            cancelAnimation();
        return ScrollAnimatorBase::userScroll(orientation, granularity, step, delta);
    }

    float usedPixelDelta = computeDeltaToConsume(orientation, step * delta);
    FloatPoint pixelDelta = (orientation == VerticalScrollbar
        ? FloatPoint(0, usedPixelDelta) : FloatPoint(usedPixelDelta, 0));

    FloatPoint targetPos = desiredTargetPosition();
    targetPos.moveBy(pixelDelta);

    if (m_runState == RunState::PostAnimationCleanup)
        resetAnimationState();

    if (m_animationCurve && m_runState != RunState::WaitingToCancelOnCompositor) {
        if ((targetPos - m_targetOffset).isZero()) {
            // Report unused delta only if there is no animation running. See
            // comment below regarding scroll latching.
            return ScrollResultOneDimensional(/* didScroll */ true, /* unusedScrollDelta */ 0);
        }

        m_targetOffset = targetPos;
        ASSERT(m_runState == RunState::RunningOnMainThread
            || m_runState == RunState::RunningOnCompositor
            || m_runState == RunState::RunningOnCompositorButNeedsUpdate);

        if (m_runState == RunState::RunningOnCompositor
            || m_runState == RunState::RunningOnCompositorButNeedsUpdate) {
            if (registerAndScheduleAnimation())
                m_runState = RunState::RunningOnCompositorButNeedsUpdate;
            return ScrollResultOneDimensional(/* didScroll */ true, /* unusedScrollDelta */ 0);
        }

        // Running on the main thread, simply update the target offset instead
        // of sending to the compositor.
        m_animationCurve->updateTarget(m_timeFunction() - m_startTime, targetPos);
        return ScrollResultOneDimensional(/* didScroll */ true, /* unusedScrollDelta */ 0);
    }

    if ((targetPos - currentPosition()).isZero()) {
        // Report unused delta only if there is no animation and we are not
        // starting one. This ensures we latch for the duration of the
        // animation rather than animating multiple scrollers at the same time.
        return ScrollResultOneDimensional(/* didScroll */ false, delta);
    }

    m_targetOffset = targetPos;
    m_startTime = m_timeFunction();
    m_lastGranularity = granularity;

    if (registerAndScheduleAnimation())
        m_runState = RunState::WaitingToSendToCompositor;

    return ScrollResultOneDimensional(/* didScroll */ true, /* unusedScrollDelta */ 0);
}

void ScrollAnimator::scrollToOffsetWithoutAnimation(const FloatPoint& offset)
{
    m_currentPosX = offset.x();
    m_currentPosY = offset.y();

    resetAnimationState();
    notifyPositionChanged();
}

void ScrollAnimator::tickAnimation(double monotonicTime)
{
    if (m_runState != RunState::RunningOnMainThread)
        return;

    TRACE_EVENT0("blink", "ScrollAnimator::tickAnimation");
    double elapsedTime = monotonicTime - m_startTime;

    bool isFinished = (elapsedTime > m_animationCurve->duration());
    FloatPoint offset = isFinished ? m_animationCurve->targetValue()
        : m_animationCurve->getValue(elapsedTime);

    offset = FloatPoint(m_scrollableArea->clampScrollPosition(offset));

    m_currentPosX = offset.x();
    m_currentPosY = offset.y();

    if (isFinished)
        m_runState = RunState::PostAnimationCleanup;
    else
        scrollableArea()->scheduleAnimation();

    TRACE_EVENT0("blink", "ScrollAnimator::notifyPositionChanged");
    notifyPositionChanged();
}

void ScrollAnimator::updateCompositorAnimations()
{
    if (m_runState == RunState::PostAnimationCleanup)
        return resetAnimationState();

    if (m_compositorAnimationId && m_runState != RunState::RunningOnCompositor
        && m_runState != RunState::RunningOnCompositorButNeedsUpdate) {
        // If the current run state is WaitingToSendToCompositor but we have a
        // non-zero compositor animation id, there's a currently running
        // compositor animation that needs to be removed here before the new
        // animation is added below.
        ASSERT(m_runState == RunState::WaitingToCancelOnCompositor
            || m_runState == RunState::WaitingToSendToCompositor);

        abortAnimation();

        m_compositorAnimationId = 0;
        m_compositorAnimationGroupId = 0;
        if (m_runState == RunState::WaitingToCancelOnCompositor) {
            return resetAnimationState();
        }
    }

    if (m_runState == RunState::WaitingToSendToCompositor
        || m_runState == RunState::RunningOnCompositorButNeedsUpdate) {
        if (m_runState == RunState::RunningOnCompositorButNeedsUpdate) {
            // Abort the running animation before a new one with an updated
            // target is added.
            abortAnimation();

            m_compositorAnimationId = 0;
            m_compositorAnimationGroupId = 0;

            m_animationCurve->updateTarget(m_timeFunction() - m_startTime,
                m_targetOffset);
            m_runState = RunState::WaitingToSendToCompositor;
        }

        if (!m_animationCurve) {
            m_animationCurve = adoptPtr(CompositorFactory::current().createScrollOffsetAnimationCurve(
                m_targetOffset,
                CompositorAnimationCurve::TimingFunctionTypeEaseInOut,
                m_lastGranularity == ScrollByPixel ?
                    CompositorScrollOffsetAnimationCurve::ScrollDurationInverseDelta :
                    CompositorScrollOffsetAnimationCurve::ScrollDurationConstant));
            m_animationCurve->setInitialValue(currentPosition());
        }

        bool sentToCompositor = false;
        if (!m_scrollableArea->shouldScrollOnMainThread()) {
            OwnPtr<CompositorAnimation> animation = adoptPtr(
                CompositorFactory::current().createAnimation(
                    *m_animationCurve,
                    CompositorAnimation::TargetPropertyScrollOffset));
            // Being here means that either there is an animation that needs
            // to be sent to the compositor, or an animation that needs to
            // be updated (a new scroll event before the previous animation
            // is finished). In either case, the start time is when the
            // first animation was initiated. This re-targets the animation
            // using the current time on main thread.
            animation->setStartTime(m_startTime);

            int animationId = animation->id();
            int animationGroupId = animation->group();

            sentToCompositor = addAnimation(animation.release());
            if (sentToCompositor) {
                m_runState = RunState::RunningOnCompositor;
                m_compositorAnimationId = animationId;
                m_compositorAnimationGroupId = animationGroupId;
            }
        }

        if (!sentToCompositor) {
            if (registerAndScheduleAnimation())
                m_runState = RunState::RunningOnMainThread;
        }
    }
}

void ScrollAnimator::notifyCompositorAnimationAborted(int groupId)
{
    // An animation aborted by the compositor is treated as a finished
    // animation.
    ScrollAnimatorCompositorCoordinator::compositorAnimationFinished(groupId);
}

void ScrollAnimator::notifyCompositorAnimationFinished(int groupId)
{
    ScrollAnimatorCompositorCoordinator::compositorAnimationFinished(groupId);
}

void ScrollAnimator::cancelAnimation()
{
    ScrollAnimatorCompositorCoordinator::cancelAnimation();
}

void ScrollAnimator::layerForCompositedScrollingDidChange(
    CompositorAnimationTimeline* timeline)
{
    reattachCompositorPlayerIfNeeded(timeline);
}

bool ScrollAnimator::registerAndScheduleAnimation()
{
    scrollableArea()->registerForAnimation();
    if (!m_scrollableArea->scheduleAnimation()) {
        scrollToOffsetWithoutAnimation(m_targetOffset);
        resetAnimationState();
        return false;
    }
    return true;
}

DEFINE_TRACE(ScrollAnimator)
{
    ScrollAnimatorBase::trace(visitor);
}

} // namespace blink
