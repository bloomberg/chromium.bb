// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scroll/ScrollAnimatorCompositorCoordinator.h"

#include "cc/animation/scroll_offset_animation_curve.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/animation/CompositorAnimationPlayer.h"
#include "platform/animation/CompositorAnimationTimeline.h"
#include "platform/graphics/CompositorFactory.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/scroll/ScrollableArea.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCompositorSupport.h"

namespace blink {

ScrollAnimatorCompositorCoordinator::ScrollAnimatorCompositorCoordinator()
    : m_compositorAnimationAttachedToLayerId(0)
    , m_runState(RunState::Idle)
    , m_compositorAnimationId(0)
    , m_compositorAnimationGroupId(0)
{
    ThreadState::current()->registerPreFinalizer(this);
    m_compositorPlayer = adoptPtr(CompositorFactory::current().createAnimationPlayer());
    ASSERT(m_compositorPlayer);
    m_compositorPlayer->setAnimationDelegate(this);
}

ScrollAnimatorCompositorCoordinator::~ScrollAnimatorCompositorCoordinator()
{
}

void ScrollAnimatorCompositorCoordinator::dispose()
{
    m_compositorPlayer->setAnimationDelegate(nullptr);
    m_compositorPlayer.clear();
}

void ScrollAnimatorCompositorCoordinator::resetAnimationState()
{
    m_runState = RunState::Idle;
    m_compositorAnimationId = 0;
    m_compositorAnimationGroupId = 0;
}

bool ScrollAnimatorCompositorCoordinator::hasAnimationThatRequiresService() const
{
    switch (m_runState) {
    case RunState::Idle:
    case RunState::RunningOnCompositor:
        return false;
    case RunState::WaitingToCancelOnCompositorButNewScroll:
    case RunState::PostAnimationCleanup:
    case RunState::WaitingToSendToCompositor:
    case RunState::RunningOnMainThread:
    case RunState::RunningOnCompositorButNeedsUpdate:
    case RunState::RunningOnCompositorButNeedsTakeover:
    case RunState::WaitingToCancelOnCompositor:
        return true;
    }
    ASSERT_NOT_REACHED();
    return false;
}

bool ScrollAnimatorCompositorCoordinator::addAnimation(
    PassOwnPtr<CompositorAnimation> animation)
{
    if (m_compositorPlayer->isLayerAttached()) {
        m_compositorPlayer->addAnimation(animation.leakPtr());
        return true;
    }
    return false;
}

void ScrollAnimatorCompositorCoordinator::removeAnimation()
{
    if (m_compositorPlayer->isLayerAttached())
        m_compositorPlayer->removeAnimation(m_compositorAnimationId);
}

void ScrollAnimatorCompositorCoordinator::abortAnimation()
{
    if (m_compositorPlayer->isLayerAttached())
        m_compositorPlayer->abortAnimation(m_compositorAnimationId);
}

void ScrollAnimatorCompositorCoordinator::cancelAnimation()
{
    switch (m_runState) {
    case RunState::Idle:
    case RunState::WaitingToCancelOnCompositor:
    case RunState::PostAnimationCleanup:
        break;
    case RunState::RunningOnCompositorButNeedsTakeover:
    case RunState::WaitingToSendToCompositor:
        if (m_compositorAnimationId) {
            // We still have a previous animation running on the compositor.
            m_runState = RunState::WaitingToCancelOnCompositor;
        } else {
            resetAnimationState();
        }
        break;
    case RunState::RunningOnMainThread:
        m_runState = RunState::PostAnimationCleanup;
        break;
    case RunState::WaitingToCancelOnCompositorButNewScroll:
    case RunState::RunningOnCompositorButNeedsUpdate:
    case RunState::RunningOnCompositor:
        m_runState = RunState::WaitingToCancelOnCompositor;

        // Get serviced the next time compositor updates are allowed.
        getScrollableArea()->registerForAnimation();
    }
}

void ScrollAnimatorCompositorCoordinator::takeoverCompositorAnimation()
{
    switch (m_runState) {
    case RunState::Idle:
    case RunState::WaitingToCancelOnCompositor:
    case RunState::WaitingToCancelOnCompositorButNewScroll:
    case RunState::PostAnimationCleanup:
    case RunState::RunningOnCompositorButNeedsTakeover:
    case RunState::WaitingToSendToCompositor:
    case RunState::RunningOnMainThread:
        break;
    case RunState::RunningOnCompositorButNeedsUpdate:
    case RunState::RunningOnCompositor:
        // We call abortAnimation that makes changes to the animation running on
        // the compositor. Thus, this function should only be called when in
        // CompositingClean state.
        abortAnimation();

        m_runState = RunState::RunningOnCompositorButNeedsTakeover;

        // Get serviced the next time compositor updates are allowed.
        getScrollableArea()->registerForAnimation();
    }
}

void ScrollAnimatorCompositorCoordinator::compositorAnimationFinished(
    int groupId)
{
    if (m_compositorAnimationGroupId != groupId)
        return;

    m_compositorAnimationId = 0;
    m_compositorAnimationGroupId = 0;

    switch (m_runState) {
    case RunState::Idle:
    case RunState::PostAnimationCleanup:
    case RunState::RunningOnMainThread:
        ASSERT_NOT_REACHED();
        break;
    case RunState::WaitingToSendToCompositor:
    case RunState::WaitingToCancelOnCompositorButNewScroll:
        break;
    case RunState::RunningOnCompositor:
    case RunState::RunningOnCompositorButNeedsUpdate:
    case RunState::RunningOnCompositorButNeedsTakeover:
    case RunState::WaitingToCancelOnCompositor:
        m_runState = RunState::PostAnimationCleanup;
        // Get serviced the next time compositor updates are allowed.
        if (getScrollableArea())
            getScrollableArea()->registerForAnimation();
        else
            resetAnimationState();
    }
}

bool ScrollAnimatorCompositorCoordinator::reattachCompositorPlayerIfNeeded(
    CompositorAnimationTimeline* timeline)
{
    bool reattached = false;
    int compositorAnimationAttachedToLayerId = 0;
    if (getScrollableArea()->layerForScrolling())
        compositorAnimationAttachedToLayerId = getScrollableArea()->layerForScrolling()->platformLayer()->id();

    if (compositorAnimationAttachedToLayerId != m_compositorAnimationAttachedToLayerId) {
        if (m_compositorPlayer && timeline) {
            // Detach from old layer (if any).
            if (m_compositorAnimationAttachedToLayerId) {
                if (m_compositorPlayer->isLayerAttached())
                    m_compositorPlayer->detachLayer();
                timeline->playerDestroyed(*this);
            }
            // Attach to new layer (if any).
            if (compositorAnimationAttachedToLayerId) {
                ASSERT(!m_compositorPlayer->isLayerAttached());
                timeline->playerAttached(*this);
                m_compositorPlayer->attachLayer(
                    getScrollableArea()->layerForScrolling()->platformLayer());
                reattached = true;
            }
            m_compositorAnimationAttachedToLayerId = compositorAnimationAttachedToLayerId;
        }
    }

    return reattached;
}

void ScrollAnimatorCompositorCoordinator::notifyAnimationStarted(
    double monotonicTime, int group)
{
}

void ScrollAnimatorCompositorCoordinator::notifyAnimationFinished(
    double monotonicTime, int group)
{
    notifyCompositorAnimationFinished(group);
}

void ScrollAnimatorCompositorCoordinator::notifyAnimationAborted(
    double monotonicTime, int group)
{
    // An animation aborted by the compositor is treated as a finished
    // animation.
    notifyCompositorAnimationFinished(group);
}

CompositorAnimationPlayer* ScrollAnimatorCompositorCoordinator::compositorPlayer() const
{
    return m_compositorPlayer.get();
}

FloatPoint ScrollAnimatorCompositorCoordinator::compositorOffsetFromBlinkOffset(FloatPoint offset)
{
    offset.moveBy(getScrollableArea()->scrollOrigin());
    return offset;
}

FloatPoint ScrollAnimatorCompositorCoordinator::blinkOffsetFromCompositorOffset(FloatPoint offset)
{
    offset.moveBy(-getScrollableArea()->scrollOrigin());
    return offset;
}

String ScrollAnimatorCompositorCoordinator::runStateAsText() const
{
    switch (m_runState) {
    case RunState::Idle:
        return String("Idle");
    case RunState::WaitingToSendToCompositor:
        return String("WaitingToSendToCompositor");
    case RunState::RunningOnCompositor:
        return String("RunningOnCompositor");
    case RunState::RunningOnMainThread:
        return String("RunningOnMainThread");
    case RunState::RunningOnCompositorButNeedsUpdate:
        return String("RunningOnCompositorButNeedsUpdate");
    case RunState::WaitingToCancelOnCompositor:
        return String("WaitingToCancelOnCompositor");
    case RunState::PostAnimationCleanup:
        return String("PostAnimationCleanup");
    case RunState::RunningOnCompositorButNeedsTakeover:
        return String("RunningOnCompositorButNeedsTakeover");
    case RunState::WaitingToCancelOnCompositorButNewScroll:
        return String("WaitingToCancelOnCompositorButNewScroll");
    }
    ASSERT_NOT_REACHED();
    return String();
}

} // namespace blink
