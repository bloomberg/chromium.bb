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

#include "core/animation/Animation.h"
#include "core/animation/DocumentTimeline.h"

namespace WebCore {

PassRefPtr<Player> Player::create(DocumentTimeline& timeline, TimedItem* content)
{
    return adoptRef(new Player(timeline, content));
}

Player::Player(DocumentTimeline& timeline, TimedItem* content)
    : m_playbackRate(1)
    , m_startTime(nullValue())
    , m_holdTime(nullValue())
    , m_storedTimeLag(0)
    , m_content(content)
    , m_timeline(timeline)
    , m_paused(false)
    , m_held(false)
    , m_isPausedForTesting(false)
{
    if (m_content)
        m_content->attach(this);
}

Player::~Player()
{
    if (m_content)
        m_content->detach();
}

double Player::sourceEnd() const
{
    return m_content ? m_content->endTime() : 0;
}

bool Player::limited(double currentTime) const
{
    return (m_playbackRate < 0 && currentTime <= 0) || (m_playbackRate > 0 && currentTime >= sourceEnd());
}

double Player::currentTimeWithoutLag() const
{
    if (isNull(m_startTime))
        return 0;
    double timelineTime = m_timeline.currentTime();
    if (isNull(timelineTime))
        timelineTime = 0;
    return (timelineTime - m_startTime) * m_playbackRate;
}

double Player::currentTimeWithLag() const
{
    ASSERT(!m_held);
    double time = currentTimeWithoutLag();
    return std::isinf(time) ? time : time - m_storedTimeLag;
}

void Player::updateTimingState(double newCurrentTime)
{
    ASSERT(!isNull(newCurrentTime));
    m_held = m_paused || !m_playbackRate || limited(newCurrentTime);
    if (m_held) {
        m_holdTime = newCurrentTime;
        m_storedTimeLag = nullValue();
    } else {
        m_holdTime = nullValue();
        m_storedTimeLag = currentTimeWithoutLag() - newCurrentTime;
    }
}

void Player::updateCurrentTimingState()
{
    if (m_held) {
        updateTimingState(m_holdTime);
    } else {
        updateTimingState(currentTimeWithLag());
        if (m_held && limited(m_holdTime))
            m_holdTime = m_playbackRate < 0 ? 0 : sourceEnd();
    }
}

double Player::currentTime()
{
    updateCurrentTimingState();
    if (m_held)
        return m_holdTime;
    return currentTimeWithLag();
}

void Player::setCurrentTime(double newCurrentTime)
{
    if (!std::isfinite(newCurrentTime))
        return;
    updateTimingState(newCurrentTime);
    m_timeline.serviceAnimations();
}

void Player::setStartTime(double newStartTime)
{
    if (!std::isfinite(newStartTime))
        return;
    updateCurrentTimingState(); // Update the value of held
    m_startTime = newStartTime;
    if (!m_held)
        updateCurrentTimingState();
    m_timeline.serviceAnimations();
}

void Player::setSource(TimedItem* newSource)
{
    if (m_content == newSource)
        return;
    double storedCurrentTime = currentTime();
    if (m_content)
        m_content->detach();
    if (newSource) {
        // FIXME: This logic needs to be updated once groups are implemented
        if (newSource->player())
            newSource->detach();
        newSource->attach(this);
    }
    m_content = newSource;
    updateTimingState(storedCurrentTime);
}

void Player::pause()
{
    if (m_paused)
        return;
    m_paused = true;
    updateTimingState(currentTime());
    // FIXME: resume compositor animation rather than pull back to main-thread
    cancelAnimationOnCompositor();
}

void Player::unpause()
{
    if (!m_paused)
        return;
    m_paused = false;
    updateTimingState(currentTime());
}

void Player::play()
{
    unpause();
    if (!m_content)
        return;
    double currentTime = this->currentTime();
    if (m_playbackRate > 0 && (currentTime < 0 || currentTime >= sourceEnd()))
        setCurrentTime(0);
    else if (m_playbackRate < 0 && (currentTime <= 0 || currentTime > sourceEnd()))
        setCurrentTime(sourceEnd());
}

void Player::reverse()
{
    if (!m_playbackRate)
        return;
    if (m_content) {
        if (m_playbackRate > 0 && currentTime() > sourceEnd())
            setCurrentTime(sourceEnd());
        else if (m_playbackRate < 0 && currentTime() < 0)
            setCurrentTime(0);
    }
    setPlaybackRate(-m_playbackRate);
    unpause();
}

void Player::setPlaybackRate(double playbackRate)
{
    if (!std::isfinite(playbackRate))
        return;
    double storedCurrentTime = currentTime();
    m_playbackRate = playbackRate;
    updateTimingState(storedCurrentTime);
    m_timeline.serviceAnimations();
}

bool Player::maybeStartAnimationOnCompositor()
{
    // FIXME: Support starting compositor animations that have a fixed
    // start time.
    ASSERT(!hasStartTime());
    if (!m_content || !m_content->isAnimation())
        return false;

    return toAnimation(m_content.get())->maybeStartAnimationOnCompositor();
}

bool Player::hasActiveAnimationsOnCompositor()
{
    if (!m_content || !m_content->isAnimation())
        return false;
    return toAnimation(m_content.get())->hasActiveAnimationsOnCompositor();
}

void Player::cancelAnimationOnCompositor()
{
    if (hasActiveAnimationsOnCompositor())
        toAnimation(m_content.get())->cancelAnimationOnCompositor();
}

bool Player::update(double* timeToEffectChange, bool* didTriggerStyleRecalc)
{
    if (!m_content) {
        if (timeToEffectChange)
            *timeToEffectChange = std::numeric_limits<double>::infinity();
        if (didTriggerStyleRecalc)
            *didTriggerStyleRecalc = false;
        return false;
    }

    double inheritedTime = isNull(m_timeline.currentTime()) ? nullValue() : currentTime();
    bool didTriggerStyleRecalcLocal = m_content->updateInheritedTime(inheritedTime);

    if (timeToEffectChange)
        *timeToEffectChange = m_content->timeToEffectChange();
    if (didTriggerStyleRecalc)
        *didTriggerStyleRecalc = didTriggerStyleRecalcLocal;
    return m_content->isCurrent() || m_content->isInEffect();
}

void Player::cancel()
{
    if (!m_content)
        return;

    ASSERT(m_content->player() == this);
    m_content->detach();
    m_content = 0;
}

void Player::pauseForTesting(double pauseTime)
{
    RELEASE_ASSERT(!paused());
    updateTimingState(pauseTime);
    if (!m_isPausedForTesting && hasActiveAnimationsOnCompositor())
        toAnimation(m_content.get())->pauseAnimationForTestingOnCompositor(currentTime());
    m_isPausedForTesting = true;
    pause();
}

} // namespace
