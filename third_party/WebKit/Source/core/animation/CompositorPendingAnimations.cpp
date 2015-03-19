/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "config.h"
#include "core/animation/CompositorPendingAnimations.h"

#include "core/animation/Animation.h"
#include "core/animation/AnimationTimeline.h"
#include "core/dom/Document.h"
#include "core/frame/FrameView.h"
#include "core/page/Page.h"
#include "platform/TraceEvent.h"

namespace blink {

void CompositorPendingAnimations::add(AnimationPlayer* player)
{
    ASSERT(player);
    ASSERT(m_pending.find(player) == kNotFound);
    m_pending.append(player);

    Document* document = player->timeline()->document();
    if (document->view())
        document->view()->scheduleAnimation();

    bool visible = document->page() && document->page()->visibilityState() == PageVisibilityStateVisible;
    if (!visible && !m_timer.isActive()) {
        m_timer.startOneShot(0, FROM_HERE);
    }
}

bool CompositorPendingAnimations::update(bool startOnCompositor)
{
    WillBeHeapVector<RawPtrWillBeMember<AnimationPlayer>> waitingForStartTime;
    bool startedSynchronizedOnCompositor = false;

    WillBeHeapVector<RefPtrWillBeMember<AnimationPlayer>> players;
    players.swap(m_pending);
    int compositorGroup = ++m_compositorGroup;
    if (compositorGroup == 0) {
        // Wrap around, skipping 0.
        compositorGroup = ++m_compositorGroup;
    }

    for (auto& player : players) {
        bool hadCompositorAnimation = player->hasActiveAnimationsOnCompositor();
        player->preCommit(compositorGroup, startOnCompositor);
        if (player->hasActiveAnimationsOnCompositor() && !hadCompositorAnimation) {
            startedSynchronizedOnCompositor = true;
        }

        if (player->playing() && !player->hasStartTime()) {
            waitingForStartTime.append(player.get());
        }
    }

    // If any synchronized animations were started on the compositor, all
    // remaning synchronized animations need to wait for the synchronized
    // start time. Otherwise they may start immediately.
    if (startedSynchronizedOnCompositor) {
        for (auto& player : waitingForStartTime) {
            if (!player->hasStartTime()) {
                m_waitingForCompositorAnimationStart.append(player);
            }
        }
    } else {
        for (auto& player : waitingForStartTime) {
            if (!player->hasStartTime()) {
                player->notifyCompositorStartTime(player->timeline()->currentTimeInternal());
            }
        }
    }

    // FIXME: The postCommit should happen *after* the commit, not before.
    for (auto& player : players)
        player->postCommit(player->timeline()->currentTimeInternal());

    ASSERT(m_pending.isEmpty());

    if (startedSynchronizedOnCompositor)
        return true;

    if (m_waitingForCompositorAnimationStart.isEmpty())
        return false;

    // Check if we're still waiting for any compositor animations to start.
    for (auto& player : m_waitingForCompositorAnimationStart) {
        if (player->hasActiveAnimationsOnCompositor())
            return true;
    }

    // If not, go ahead and start any animations that were waiting.
    notifyCompositorAnimationStarted(monotonicallyIncreasingTime());

    ASSERT(m_pending.isEmpty());
    return false;
}

void CompositorPendingAnimations::notifyCompositorAnimationStarted(double monotonicAnimationStartTime, int compositorGroup)
{
    TRACE_EVENT0("blink", "CompositorPendingAnimations::notifyCompositorAnimationStarted");
    WillBeHeapVector<RefPtrWillBeMember<AnimationPlayer>> players;
    players.swap(m_waitingForCompositorAnimationStart);

    for (auto player : players) {
        if (player->hasStartTime() || player->playStateInternal() != AnimationPlayer::Pending) {
            // Already started or no longer relevant.
            continue;
        }
        if (compositorGroup && player->compositorGroup() != compositorGroup) {
            // Still waiting.
            m_waitingForCompositorAnimationStart.append(player);
            continue;
        }
        player->notifyCompositorStartTime(monotonicAnimationStartTime - player->timeline()->zeroTime());
    }

}

DEFINE_TRACE(CompositorPendingAnimations)
{
    visitor->trace(m_pending);
    visitor->trace(m_waitingForCompositorAnimationStart);
}

} // namespace
