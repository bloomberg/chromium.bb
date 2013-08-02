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
#include "core/animation/Player.h"

#include "core/animation/DocumentTimeline.h"
#include "core/animation/TimedItem.h"

namespace WebCore {

PassRefPtr<Player> Player::create(DocumentTimeline* timeline, TimedItem* content)
{
    ASSERT(timeline);
    return adoptRef(new Player(timeline, content));
}

Player::Player(DocumentTimeline* timeline, TimedItem* content)
    : m_pauseStartTime(nullValue())
    , m_playbackRate(1)
    , m_timeDrift(0)
    , m_startTime(effectiveTime(timeline->currentTime()))
    , m_content(content)
    , m_timeline(timeline)
{
    ASSERT(m_startTime >= 0);
    if (m_content)
        m_content->attach(this);
    update();
}

Player::~Player()
{
    if (m_content)
        m_content->detach();
}

double Player::currentTimeBeforeDrift() const
{
    return (effectiveTime(m_timeline->currentTime()) - m_startTime) * m_playbackRate;
}

double Player::pausedTimeDrift() const
{
    ASSERT(paused());
    return currentTimeBeforeDrift() - m_pauseStartTime;
}

double Player::timeDrift() const
{
    return paused() ? pausedTimeDrift() : m_timeDrift;
}

double Player::currentTime() const
{
    return currentTimeBeforeDrift() - timeDrift();
}

bool Player::update()
{
    if (!m_content)
        return false;

    double newTime = isNull(m_timeline->currentTime()) ? nullValue() : currentTime();
    m_content->updateInheritedTime(newTime);
    return m_content->isCurrent() || m_content->isInEffect();
}

void Player::cancel()
{
    if (!m_content)
        return;

    m_content->detach();
    m_content = 0;
}

void Player::setCurrentTime(double seekTime)
{
    if (paused())
        m_pauseStartTime = seekTime;
    else
        m_timeDrift = currentTimeBeforeDrift() - seekTime;

    update();
}

void Player::setPaused(bool newValue)
{
    if (paused() == newValue)
        return;

    if (newValue)
        m_pauseStartTime = currentTime();
    else {
        m_timeDrift = pausedTimeDrift();
        m_pauseStartTime = nullValue();
    }
}

void Player::setPlaybackRate(double newRate)
{
    double previousTime = currentTime();
    m_playbackRate = newRate;
    setCurrentTime(previousTime);
}

} // namespace
