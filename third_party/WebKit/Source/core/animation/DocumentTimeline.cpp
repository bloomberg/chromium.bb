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
#include "core/animation/DocumentTimeline.h"

#include "core/animation/ActiveAnimations.h"
#include "core/animation/AnimationClock.h"
#include "core/dom/Document.h"
#include "core/frame/FrameView.h"

namespace WebCore {

// This value represents 1 frame at 30Hz plus a little bit of wiggle room.
// TODO: Plumb a nominal framerate through and derive this value from that.
const double DocumentTimeline::s_minimumDelay = 0.04;


PassRefPtr<DocumentTimeline> DocumentTimeline::create(Document* document, PassOwnPtr<PlatformTiming> timing)
{
    return adoptRef(new DocumentTimeline(document, timing));
}

DocumentTimeline::DocumentTimeline(Document* document, PassOwnPtr<PlatformTiming> timing)
    : m_zeroTime(nullValue())
    , m_document(document)
    , m_eventDistpachTimer(this, &DocumentTimeline::eventDispatchTimerFired)
{
    if (!timing)
        m_timing = adoptPtr(new DocumentTimelineTiming(this));
    else
        m_timing = timing;

    ASSERT(document);
}

Player* DocumentTimeline::createPlayer(TimedItem* child)
{
    RefPtr<Player> player = Player::create(*this, child);
    Player* result = player.get();
    m_players.append(player.release());
    if (m_document->view())
        m_timing->serviceOnNextFrame();
    return result;
}

Player* DocumentTimeline::play(TimedItem* child)
{
    Player* player = createPlayer(child);
    player->setStartTime(currentTime());
    return player;
}

void DocumentTimeline::wake()
{
    m_timing->serviceOnNextFrame();
}

bool DocumentTimeline::serviceAnimations()
{
    TRACE_EVENT0("webkit", "DocumentTimeline::serviceAnimations");

    m_timing->cancelWake();

    double timeToNextEffect = std::numeric_limits<double>::infinity();
    bool didTriggerStyleRecalc = false;
    for (int i = m_players.size() - 1; i >= 0; --i) {
        double playerNextEffect;
        bool playerDidTriggerStyleRecalc;
        if (!m_players[i]->update(&playerNextEffect, &playerDidTriggerStyleRecalc))
            m_players.remove(i);
        didTriggerStyleRecalc |= playerDidTriggerStyleRecalc;
        if (playerNextEffect < timeToNextEffect)
            timeToNextEffect = playerNextEffect;
    }

    if (!m_players.isEmpty()) {
        if (timeToNextEffect < s_minimumDelay)
            m_timing->serviceOnNextFrame();
        else if (timeToNextEffect != std::numeric_limits<double>::infinity())
            m_timing->wakeAfter(timeToNextEffect - s_minimumDelay);
    }

    return didTriggerStyleRecalc;
}

void DocumentTimeline::setZeroTime(double zeroTime)
{
    ASSERT(isNull(m_zeroTime));
    m_zeroTime = zeroTime;
    ASSERT(!isNull(m_zeroTime));
}

void DocumentTimeline::DocumentTimelineTiming::wakeAfter(double duration)
{
    m_timer.startOneShot(duration);
}

void DocumentTimeline::DocumentTimelineTiming::cancelWake()
{
    m_timer.stop();
}

void DocumentTimeline::DocumentTimelineTiming::serviceOnNextFrame()
{
    if (m_timeline->m_document->view())
        m_timeline->m_document->view()->scheduleAnimation();
}

double DocumentTimeline::currentTime()
{
    return m_document->animationClock().currentTime() - m_zeroTime;
}

void DocumentTimeline::pauseAnimationsForTesting(double pauseTime)
{
    for (size_t i = 0; i < m_players.size(); i++) {
        m_players[i]->pauseForTesting();
        m_players[i]->setCurrentTime(pauseTime);
    }
}

void DocumentTimeline::dispatchEvents()
{
    Vector<EventToDispatch> events = m_events;
    m_events.clear();
    for (size_t i = 0; i < events.size(); i++)
        events[i].target->dispatchEvent(events[i].event.release());
}

void DocumentTimeline::dispatchEventsAsync()
{
    if (m_events.isEmpty() || m_eventDistpachTimer.isActive())
        return;
    m_eventDistpachTimer.startOneShot(0);
}

void DocumentTimeline::eventDispatchTimerFired(Timer<DocumentTimeline>*)
{
    dispatchEvents();
}

size_t DocumentTimeline::numberOfActiveAnimationsForTesting() const
{
    if (isNull(m_zeroTime))
        return 0;
    // Includes all players whose directly associated timed items
    // are current or in effect.
    if (isNull(m_zeroTime))
        return 0;
    size_t count = 0;
    for (size_t i = 0; i < m_players.size(); ++i) {
        const TimedItem* timedItem = m_players[i]->source();
        if (m_players[i]->hasStartTime())
            count += (timedItem && (timedItem->isCurrent() || timedItem->isInEffect()));
    }
    return count;
}

} // namespace
