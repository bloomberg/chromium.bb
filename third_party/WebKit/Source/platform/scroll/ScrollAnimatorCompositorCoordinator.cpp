// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scroll/ScrollAnimatorCompositorCoordinator.h"

#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/scroll/ScrollableArea.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCompositorAnimationPlayer.h"
#include "public/platform/WebCompositorAnimationTimeline.h"
#include "public/platform/WebCompositorSupport.h"

namespace blink {

ScrollAnimatorCompositorCoordinator::ScrollAnimatorCompositorCoordinator()
    : m_compositorAnimationAttachedToLayerId(0)
    , m_runState(RunState::Idle)
    , m_compositorAnimationId(0)
    , m_compositorAnimationGroupId(0)
{
    if (RuntimeEnabledFeatures::compositorAnimationTimelinesEnabled()) {
        ASSERT(Platform::current()->compositorSupport());
        m_compositorPlayer = adoptPtr(Platform::current()->compositorSupport()->createAnimationPlayer());
        ASSERT(m_compositorPlayer);
        m_compositorPlayer->setAnimationDelegate(this);
    }
}

ScrollAnimatorCompositorCoordinator::~ScrollAnimatorCompositorCoordinator()
{
    if (m_compositorPlayer) {
        m_compositorPlayer->setAnimationDelegate(nullptr);
        m_compositorPlayer.clear();
    }
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
    case RunState::PostAnimationCleanup:
    case RunState::WaitingToSendToCompositor:
    case RunState::RunningOnMainThread:
    case RunState::RunningOnCompositorButNeedsUpdate:
    case RunState::WaitingToCancelOnCompositor:
        return true;
    }
    ASSERT_NOT_REACHED();
    return false;
}

bool ScrollAnimatorCompositorCoordinator::addAnimation(
    PassOwnPtr<WebCompositorAnimation> animation)
{
    if (m_compositorPlayer) {
        if (m_compositorPlayer->isLayerAttached()) {
            m_compositorPlayer->addAnimation(animation.leakPtr());
            return true;
        }
    } else {
        return scrollableArea()->layerForScrolling()->addAnimation(animation);
    }
    return false;
}

void ScrollAnimatorCompositorCoordinator::removeAnimation()
{
    if (m_compositorPlayer) {
        if (m_compositorPlayer->isLayerAttached())
            m_compositorPlayer->removeAnimation(m_compositorAnimationId);
    } else {
        if (GraphicsLayer* layer = scrollableArea()->layerForScrolling())
            layer->removeAnimation(m_compositorAnimationId);
    }
}

void ScrollAnimatorCompositorCoordinator::abortAnimation()
{
    if (m_compositorPlayer) {
        if (m_compositorPlayer->isLayerAttached())
            m_compositorPlayer->abortAnimation(m_compositorAnimationId);
    } else {
        if (GraphicsLayer* layer = scrollableArea()->layerForScrolling())
            layer->abortAnimation(m_compositorAnimationId);
    }
}

void ScrollAnimatorCompositorCoordinator::cancelAnimation()
{
    switch (m_runState) {
    case RunState::Idle:
    case RunState::WaitingToCancelOnCompositor:
    case RunState::PostAnimationCleanup:
        break;
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
    case RunState::RunningOnCompositorButNeedsUpdate:
    case RunState::RunningOnCompositor:
        m_runState = RunState::WaitingToCancelOnCompositor;

        // Get serviced the next time compositor updates are allowed.
        scrollableArea()->registerForAnimation();
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
        break;
    case RunState::RunningOnCompositor:
    case RunState::RunningOnCompositorButNeedsUpdate:
    case RunState::WaitingToCancelOnCompositor:
        m_runState = RunState::PostAnimationCleanup;

        // Get serviced the next time compositor updates are allowed.
        scrollableArea()->registerForAnimation();
    }
}

void ScrollAnimatorCompositorCoordinator::reattachCompositorPlayerIfNeeded(
    WebCompositorAnimationTimeline* timeline)
{
    int compositorAnimationAttachedToLayerId = 0;
    if (scrollableArea()->layerForScrolling())
        compositorAnimationAttachedToLayerId = scrollableArea()->layerForScrolling()->platformLayer()->id();

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
                    scrollableArea()->layerForScrolling()->platformLayer());
            }
            m_compositorAnimationAttachedToLayerId = compositorAnimationAttachedToLayerId;
        }
    }
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

WebCompositorAnimationPlayer* ScrollAnimatorCompositorCoordinator::compositorPlayer() const
{
    return m_compositorPlayer.get();
}

} // namespace blink
