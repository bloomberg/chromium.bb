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
#include "core/animation/AnimationPlayer.h"

#include "core/animation/Animation.h"
#include "core/animation/AnimationTimeline.h"
#include "core/dom/Document.h"
#include "core/events/AnimationPlayerEvent.h"
#include "core/frame/UseCounter.h"

namespace blink {

namespace {

static unsigned nextSequenceNumber()
{
    static unsigned next = 0;
    return ++next;
}

}

PassRefPtrWillBeRawPtr<AnimationPlayer> AnimationPlayer::create(ExecutionContext* executionContext, AnimationTimeline& timeline, AnimationNode* content)
{
    RefPtrWillBeRawPtr<AnimationPlayer> player = adoptRefWillBeNoop(new AnimationPlayer(executionContext, timeline, content));
    player->suspendIfNeeded();
    return player.release();
}

AnimationPlayer::AnimationPlayer(ExecutionContext* executionContext, AnimationTimeline& timeline, AnimationNode* content)
    : ActiveDOMObject(executionContext)
    , m_playbackRate(1)
    , m_startTime(nullValue())
    , m_holdTime(nullValue())
    , m_sortInfo(nextSequenceNumber())
    , m_content(content)
    , m_timeline(&timeline)
    , m_paused(false)
    , m_held(false)
    , m_isPausedForTesting(false)
    , m_outdated(true)
    , m_finished(false)
{
    ScriptWrappable::init(this);
    if (m_content) {
        if (m_content->player())
            m_content->player()->cancel();
        m_content->attach(this);
    }
}

AnimationPlayer::~AnimationPlayer()
{
#if !ENABLE(OILPAN)
    if (m_content)
        m_content->detach();
    if (m_timeline)
        m_timeline->playerDestroyed(this);
#endif
}

double AnimationPlayer::sourceEnd() const
{
    return m_content ? m_content->endTimeInternal() : 0;
}

bool AnimationPlayer::limited(double currentTime) const
{
    return (m_playbackRate < 0 && currentTime <= 0) || (m_playbackRate > 0 && currentTime >= sourceEnd());
}

void AnimationPlayer::setCurrentTimeInternal(double newCurrentTime)
{
    ASSERT(std::isfinite(newCurrentTime));

    bool oldHeld = m_held;
    bool outdated = false;
    m_held = m_paused || !m_playbackRate || limited(newCurrentTime);

    if (m_held) {
        if (!oldHeld || m_holdTime != newCurrentTime)
            outdated = true;
        m_holdTime = newCurrentTime;
        if (m_paused || !m_playbackRate)
            m_startTime = nullValue();
    } else {
        m_holdTime = nullValue();
        m_startTime = m_timeline->effectiveTime() - newCurrentTime / m_playbackRate;
        m_finished = false;
        outdated = true;
    }

    if (outdated) {
        setOutdated();
        cancelAnimationOnCompositor();
        if (!m_held)
            schedulePendingAnimationOnCompositor();
    }
}

// Update timing to reflect updated animation clock due to tick
void AnimationPlayer::updateCurrentTimingState()
{
    if (m_held) {
        setCurrentTimeInternal(m_holdTime);
        return;
    }
    if (!limited(calculateCurrentTime()))
        return;
    m_held = true;
    m_holdTime = m_playbackRate < 0 ? 0 : sourceEnd();
}

double AnimationPlayer::currentTime()
{
    return currentTimeInternal() * 1000;
}

double AnimationPlayer::currentTimeInternal()
{
    updateCurrentTimingState();
    if (m_held)
        return m_holdTime;
    return calculateCurrentTime();
}

double AnimationPlayer::calculateCurrentTime() const
{
    ASSERT(!m_held);
    if (isNull(m_startTime) || !m_timeline)
        return 0;
    return (m_timeline->effectiveTime() - m_startTime) * m_playbackRate;
}

void AnimationPlayer::setCurrentTime(double newCurrentTime)
{
    if (!std::isfinite(newCurrentTime))
        return;
    setCurrentTimeInternal(newCurrentTime / 1000);
}

void AnimationPlayer::setStartTimeInternal(double newStartTime, bool isUpdateFromCompositor)
{
    ASSERT(!isUpdateFromCompositor || !hasStartTime());

    if (m_paused)
        return;
    if (!std::isfinite(newStartTime))
        return;
    if (newStartTime == m_startTime)
        return;

    bool hadStartTime = hasStartTime();
    double previousCurrentTime = currentTimeInternal();
    m_startTime = newStartTime;
    double newCurrentTime = currentTimeInternal();

    if (previousCurrentTime != newCurrentTime) {
        setOutdated();
    } else if (!hadStartTime && m_timeline) {
        // Even though this player is not outdated, time to effect change is
        // infinity until start time is set.
        m_timeline->wake();
    }
    if (!isUpdateFromCompositor) {
        cancelAnimationOnCompositor();
        schedulePendingAnimationOnCompositor();
    }
}

void AnimationPlayer::setSource(AnimationNode* newSource)
{
    if (m_content == newSource)
        return;
    cancelAnimationOnCompositor();
    double storedCurrentTime = currentTimeInternal();
    if (m_content)
        m_content->detach();
    m_content = newSource;
    if (newSource) {
        // FIXME: This logic needs to be updated once groups are implemented
        if (newSource->player())
            newSource->player()->cancel();
        newSource->attach(this);
    }
    setCurrentTimeInternal(storedCurrentTime);
    schedulePendingAnimationOnCompositor();
}

void AnimationPlayer::pause()
{
    if (m_paused)
        return;
    m_paused = true;
    setCurrentTimeInternal(currentTimeInternal());
    cancelAnimationOnCompositor();
}

void AnimationPlayer::unpause()
{
    if (!m_paused)
        return;
    m_paused = false;
    setCurrentTimeInternal(currentTimeInternal());
}

void AnimationPlayer::play()
{
    cancelAnimationOnCompositor();
    unpause();
    if (!m_content)
        return;
    double currentTime = this->currentTimeInternal();
    if (m_playbackRate > 0 && (currentTime < 0 || currentTime >= sourceEnd()))
        setCurrentTimeInternal(0);
    else if (m_playbackRate < 0 && (currentTime <= 0 || currentTime > sourceEnd()))
        setCurrentTimeInternal(sourceEnd());
    m_finished = false;
}

void AnimationPlayer::reverse()
{
    if (!m_playbackRate)
        return;
    if (m_content) {
        if (m_playbackRate > 0 && currentTimeInternal() > sourceEnd())
            setCurrentTimeInternal(sourceEnd());
        else if (m_playbackRate < 0 && currentTimeInternal() < 0)
            setCurrentTimeInternal(0);
    }
    setPlaybackRate(-m_playbackRate);
    unpause();
}

void AnimationPlayer::finish(ExceptionState& exceptionState)
{
    if (!m_playbackRate)
        return;
    if (m_playbackRate < 0) {
        setCurrentTimeInternal(0);
    } else {
        if (sourceEnd() == std::numeric_limits<double>::infinity()) {
            exceptionState.throwDOMException(InvalidStateError, "AnimationPlayer has source content whose end time is infinity.");
            return;
        }
        setCurrentTimeInternal(sourceEnd());
    }
    ASSERT(finished());
}

const AtomicString& AnimationPlayer::interfaceName() const
{
    return EventTargetNames::AnimationPlayer;
}

ExecutionContext* AnimationPlayer::executionContext() const
{
    return ActiveDOMObject::executionContext();
}

bool AnimationPlayer::hasPendingActivity() const
{
    return m_pendingFinishedEvent || (!m_finished && hasEventListeners(EventTypeNames::finish));
}

void AnimationPlayer::stop()
{
    m_finished = true;
    m_pendingFinishedEvent = nullptr;
}

bool AnimationPlayer::dispatchEvent(PassRefPtrWillBeRawPtr<Event> event)
{
    if (m_pendingFinishedEvent == event)
        m_pendingFinishedEvent = nullptr;
    return EventTargetWithInlineData::dispatchEvent(event);
}

void AnimationPlayer::setPlaybackRate(double playbackRate)
{
    if (!std::isfinite(playbackRate))
        return;
    double storedCurrentTime = currentTimeInternal();
    if ((m_playbackRate < 0 && playbackRate >= 0) || (m_playbackRate > 0 && playbackRate <= 0))
        m_finished = false;
    m_playbackRate = playbackRate;
    setCurrentTimeInternal(storedCurrentTime);
}

void AnimationPlayer::setOutdated()
{
    m_outdated = true;
    if (m_timeline)
        m_timeline->setOutdatedAnimationPlayer(this);
}

bool AnimationPlayer::canStartAnimationOnCompositor()
{
    // FIXME: Need compositor support for playback rate != 1.
    if (playbackRate() != 1)
        return false;

    return m_timeline && m_content && m_content->isAnimation() && !m_held;
}

bool AnimationPlayer::maybeStartAnimationOnCompositor()
{
    if (!canStartAnimationOnCompositor())
        return false;

    return toAnimation(m_content.get())->maybeStartAnimationOnCompositor(timeline()->zeroTime() + startTimeInternal());
}

void AnimationPlayer::schedulePendingAnimationOnCompositor()
{
    ASSERT(!hasActiveAnimationsOnCompositor());

    if (canStartAnimationOnCompositor())
        timeline()->document()->compositorPendingAnimations().add(this);
}

bool AnimationPlayer::hasActiveAnimationsOnCompositor()
{
    if (!m_content || !m_content->isAnimation())
        return false;

    return toAnimation(m_content.get())->hasActiveAnimationsOnCompositor();
}

void AnimationPlayer::cancelAnimationOnCompositor()
{
    if (hasActiveAnimationsOnCompositor())
        toAnimation(m_content.get())->cancelAnimationOnCompositor();
}

bool AnimationPlayer::update(TimingUpdateReason reason)
{
    if (!m_timeline)
        return false;

    updateCurrentTimingState();
    m_outdated = false;

    if (m_content) {
        double inheritedTime = isNull(m_timeline->currentTimeInternal()) ? nullValue() : currentTimeInternal();
        m_content->updateInheritedTime(inheritedTime, reason);
    }

    if (finished() && !m_finished) {
        if (reason == TimingUpdateForAnimationFrame && hasStartTime()) {
            const AtomicString& eventType = EventTypeNames::finish;
            if (executionContext() && hasEventListeners(eventType)) {
                m_pendingFinishedEvent = AnimationPlayerEvent::create(eventType, currentTime(), timeline()->currentTime());
                m_pendingFinishedEvent->setTarget(this);
                m_pendingFinishedEvent->setCurrentTarget(this);
                m_timeline->document()->enqueueAnimationFrameEvent(m_pendingFinishedEvent);
            }
            m_finished = true;
        }
    }
    ASSERT(!m_outdated);
    return !m_finished || !finished();
}

double AnimationPlayer::timeToEffectChange()
{
    ASSERT(!m_outdated);
    if (m_held || !hasStartTime())
        return std::numeric_limits<double>::infinity();
    if (!m_content)
        return -currentTimeInternal() / m_playbackRate;
    if (m_playbackRate > 0)
        return m_content->timeToForwardsEffectChange() / m_playbackRate;
    return m_content->timeToReverseEffectChange() / -m_playbackRate;
}

void AnimationPlayer::cancel()
{
    setSource(0);
}

#if !ENABLE(OILPAN)
bool AnimationPlayer::canFree() const
{
    ASSERT(m_content);
    return hasOneRef() && m_content->isAnimation() && m_content->hasOneRef();
}
#endif

bool AnimationPlayer::addEventListener(const AtomicString& eventType, PassRefPtr<EventListener> listener, bool useCapture)
{
    if (eventType == EventTypeNames::finish)
        UseCounter::count(executionContext(), UseCounter::AnimationPlayerFinishEvent);
    return EventTargetWithInlineData::addEventListener(eventType, listener, useCapture);
}

void AnimationPlayer::pauseForTesting(double pauseTime)
{
    RELEASE_ASSERT(!paused());
    setCurrentTimeInternal(pauseTime);
    if (!m_isPausedForTesting && hasActiveAnimationsOnCompositor())
        toAnimation(m_content.get())->pauseAnimationForTestingOnCompositor(currentTimeInternal());
    m_isPausedForTesting = true;
    pause();
}

void AnimationPlayer::trace(Visitor* visitor)
{
    visitor->trace(m_content);
    visitor->trace(m_timeline);
    visitor->trace(m_pendingFinishedEvent);
    EventTargetWithInlineData::trace(visitor);
}

} // namespace
