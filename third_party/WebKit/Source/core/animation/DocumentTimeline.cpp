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

#include "core/animation/Player.h"
#include "core/dom/Document.h"
#include "core/page/FrameView.h"

namespace WebCore {

PassRefPtr<DocumentTimeline> DocumentTimeline::create(Document* document)
{
    return adoptRef(new DocumentTimeline(document));
}

DocumentTimeline::DocumentTimeline(Document* document)
    : m_currentTime(nullValue())
    , m_document(document)
    , m_zeroTimeAsPerfTime(nullValue())
{
    ASSERT(document);
}

PassRefPtr<Player> DocumentTimeline::play(TimedItem* child)
{
    RefPtr<Player> player = Player::create(this, child);
    m_players.append(player);

    if (m_document->view())
        m_document->view()->scheduleAnimation();

    return player.release();
}

void DocumentTimeline::serviceAnimations(double monotonicAnimationStartTime)
{
    // FIXME: The below ASSERT fires on Windows when running chrome.exe.
    // Does not fire with --single-process, or on Linux, or in
    // content_shell.exe. The assert condition has been moved up into the
    // outer 'if' to work around this. http://crbug.com/280439.
    if (!isNull(m_zeroTimeAsPerfTime) && (m_currentTime <= monotonicAnimationStartTime - m_zeroTimeAsPerfTime)) {
        ASSERT(m_currentTime <= monotonicAnimationStartTime - m_zeroTimeAsPerfTime);
        m_currentTime = monotonicAnimationStartTime - m_zeroTimeAsPerfTime;
    }

    for (int i = m_players.size() - 1; i >= 0; --i) {
        if (!m_players[i]->update())
            m_players.remove(i);
    }

    if (m_document->view() && !m_players.isEmpty())
        m_document->view()->scheduleAnimation();

    dispatchEvents();
}

void DocumentTimeline::setZeroTimeAsPerfTime(double zeroTime)
{
    ASSERT(isNull(m_zeroTimeAsPerfTime));
    m_zeroTimeAsPerfTime = zeroTime;
    ASSERT(!isNull(m_zeroTimeAsPerfTime));
    m_currentTime = 0;
}

void DocumentTimeline::pauseAnimationsForTesting(double pauseTime)
{
    for (size_t i = 0; i < m_players.size(); i++) {
        m_players[i]->setPaused(true);
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

size_t DocumentTimeline::numberOfActiveAnimationsForTesting() const
{
    // Includes all players whose directly associated timed items
    // are current or in effect.
    return isNull(m_currentTime) ? 0 : m_players.size();
}

} // namespace
