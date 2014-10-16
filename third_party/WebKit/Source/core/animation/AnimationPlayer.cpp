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
#include "platform/TraceEvent.h"

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
    player->uncancel();
    timeline.document()->compositorPendingAnimations().add(player.get());
    player->suspendIfNeeded();
    return player.release();
}

AnimationPlayer::AnimationPlayer(ExecutionContext* executionContext, AnimationTimeline& timeline, AnimationNode* content)
    : ActiveDOMObject(executionContext)
    , m_playState(Idle)
    , m_playbackRate(1)
    , m_startTime(nullValue())
    , m_holdTime(0)
    , m_sequenceNumber(nextSequenceNumber())
    , m_content(content)
    , m_timeline(&timeline)
    , m_paused(false)
    , m_held(true)
    , m_isPausedForTesting(false)
    , m_outdated(true)
    , m_finished(true)
    , m_compositorState(nullptr)
    , m_compositorPending(true)
    , m_currentTimePending(false)
{
    if (m_content) {
        if (m_content->player()) {
            m_content->player()->cancel();
            m_content->player()->setSource(0);
        }
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

void AnimationPlayer::setCurrentTime(double newCurrentTime)
{
    UseCounter::count(executionContext(), UseCounter::AnimationPlayerSetCurrentTime);
    if (!std::isfinite(newCurrentTime))
        return;

    PlayStateUpdateScope updateScope(*this, TimingUpdateOnDemand);

    m_currentTimePending = false;
    setCurrentTimeInternal(newCurrentTime / 1000, TimingUpdateOnDemand);
}

void AnimationPlayer::setCurrentTimeInternal(double newCurrentTime, TimingUpdateReason reason)
{
    ASSERT(std::isfinite(newCurrentTime));

    bool oldHeld = m_held;
    bool outdated = false;
    bool isLimited = limited(newCurrentTime);
    m_held = m_paused || !m_playbackRate || isLimited || std::isnan(m_startTime);
    if (m_held) {
        if (!oldHeld || m_holdTime != newCurrentTime)
            outdated = true;
        m_holdTime = newCurrentTime;
        if (m_paused || !m_playbackRate) {
            m_startTime = nullValue();
        } else if (isLimited && std::isnan(m_startTime) && reason == TimingUpdateForAnimationFrame) {
            m_startTime = calculateStartTime(newCurrentTime);
        }
    } else {
        m_holdTime = nullValue();
        m_startTime = calculateStartTime(newCurrentTime);
        m_finished = false;
        outdated = true;
    }

    if (outdated) {
        setOutdated();
    }
}

// Update timing to reflect updated animation clock due to tick
void AnimationPlayer::updateCurrentTimingState(TimingUpdateReason reason)
{
    if (m_held) {
        setCurrentTimeInternal(m_holdTime, reason);
        return;
    }
    if (!limited(calculateCurrentTime()))
        return;
    m_held = true;
    m_holdTime = m_playbackRate < 0 ? 0 : sourceEnd();
}

double AnimationPlayer::startTime(bool& isNull) const
{
    double result = startTime();
    isNull = std::isnan(result);
    return result;
}

double AnimationPlayer::startTime() const
{
    UseCounter::count(executionContext(), UseCounter::AnimationPlayerGetStartTime);
    return m_startTime * 1000;
}

double AnimationPlayer::currentTime(bool& isNull)
{
    double result = currentTime();
    isNull = std::isnan(result);
    return result;
}

double AnimationPlayer::currentTime()
{
    PlayStateUpdateScope updateScope(*this, TimingUpdateOnDemand);

    UseCounter::count(executionContext(), UseCounter::AnimationPlayerGetCurrentTime);
    if (m_currentTimePending || playStateInternal() == Idle)
        return std::numeric_limits<double>::quiet_NaN();

    return currentTimeInternal() * 1000;
}

double AnimationPlayer::currentTimeInternal() const
{
    double result = m_held ? m_holdTime : calculateCurrentTime();
#if ENABLE(ASSERT)
    const_cast<AnimationPlayer*>(this)->updateCurrentTimingState(TimingUpdateOnDemand);
    ASSERT(result == (m_held ? m_holdTime : calculateCurrentTime()));
#endif
    return result;
}

void AnimationPlayer::preCommit(bool startOnCompositor)
{
    if (m_compositorState && m_compositorState->pendingAction == Start) {
        // Still waiting for a start time.
        return;
    }

    PlayStateUpdateScope updateScope(*this, TimingUpdateOnDemand, DoNotSetCompositorPending);

    bool softChange = m_compositorState && (paused() || m_compositorState->playbackRate != m_playbackRate);
    bool hardChange = m_compositorState && (m_compositorState->sourceChanged || (m_compositorState->startTime != m_startTime && !std::isnan(m_compositorState->startTime) && !std::isnan(m_startTime)));

    // FIXME: softChange && !hardChange should generate a Pause/ThenStart,
    // not a Cancel, but we can't communicate these to the compositor yet.

    bool changed = softChange || hardChange;
    bool shouldCancel = (!playing() && m_compositorState) || changed;
    bool shouldStart = playing() && (!m_compositorState || changed);

    if (shouldCancel) {
        cancelAnimationOnCompositor();
        m_compositorState = nullptr;
    }

    if (!shouldStart) {
        m_currentTimePending = false;
    }

    if (shouldStart && startOnCompositor && maybeStartAnimationOnCompositor()) {
        m_compositorState = adoptPtr(new CompositorState(*this));
    }
}

void AnimationPlayer::postCommit(double timelineTime)
{
    PlayStateUpdateScope updateScope(*this, TimingUpdateOnDemand, DoNotSetCompositorPending);

    m_compositorPending = false;

    if (!m_compositorState || m_compositorState->pendingAction == None)
        return;

    switch (m_compositorState->pendingAction) {
    case Start:
        if (!std::isnan(m_compositorState->startTime)) {
            ASSERT(m_startTime == m_compositorState->startTime);
            m_compositorState->pendingAction = None;
        }
        break;
    case Pause:
    case PauseThenStart:
        ASSERT(std::isnan(m_startTime));
        m_compositorState->pendingAction = None;
        setCurrentTimeInternal((timelineTime - m_compositorState->startTime) * m_playbackRate, TimingUpdateForAnimationFrame);
        m_currentTimePending = false;
        break;
    default:
        ASSERT_NOT_REACHED();
    }
}

void AnimationPlayer::notifyCompositorStartTime(double timelineTime)
{
    PlayStateUpdateScope updateScope(*this, TimingUpdateOnDemand);

    if (m_compositorState) {
        ASSERT(m_compositorState->pendingAction == Start);
        ASSERT(std::isnan(m_compositorState->startTime));

        double initialCompositorHoldTime = m_compositorState->holdTime;
        m_compositorState->pendingAction = None;
        m_compositorState->startTime = timelineTime;

        if (m_startTime == timelineTime) {
            // The start time was set to the incoming compositor start time.
            // Unlikely, but possible.
            // FIXME: Depending on what changed above this might still be pending.
            // Maybe...
            m_currentTimePending = false;
            return;
        }

        if (!std::isnan(m_startTime) || currentTimeInternal() != initialCompositorHoldTime) {
            // A new start time or current time was set while starting.
            setCompositorPending(true);
            return;
        }
    }

    notifyStartTime(timelineTime);
}

void AnimationPlayer::notifyStartTime(double timelineTime)
{
    if (playing()) {
        ASSERT(std::isnan(m_startTime));
        ASSERT(m_held);

        if (m_playbackRate == 0) {
            setStartTimeInternal(timelineTime);
        } else {
            setStartTimeInternal(timelineTime + currentTimeInternal() / -m_playbackRate);
        }

        // FIXME: This avoids marking this player as outdated needlessly when a start time
        // is notified, but we should refactor how outdating works to avoid this.
        m_outdated = false;

        m_currentTimePending = false;
    }
}

double AnimationPlayer::calculateStartTime(double currentTime) const
{
    return m_timeline->effectiveTime() - currentTime / m_playbackRate;
}

double AnimationPlayer::calculateCurrentTime() const
{
    ASSERT(!m_held);
    if (isNull(m_startTime) || !m_timeline)
        return 0;
    return (m_timeline->effectiveTime() - m_startTime) * m_playbackRate;
}

void AnimationPlayer::setStartTime(double startTime)
{
    PlayStateUpdateScope updateScope(*this, TimingUpdateOnDemand);

    UseCounter::count(executionContext(), UseCounter::AnimationPlayerSetStartTime);
    if (m_paused || playStateInternal() == Idle)
        return;
    if (!std::isfinite(startTime))
        return;
    if (startTime == m_startTime)
        return;

    m_currentTimePending = false;
    setStartTimeInternal(startTime / 1000);
}

void AnimationPlayer::setStartTimeInternal(double newStartTime)
{
    ASSERT(!m_paused);
    ASSERT(std::isfinite(newStartTime));
    ASSERT(newStartTime != m_startTime);

    bool hadStartTime = hasStartTime();
    double previousCurrentTime = currentTimeInternal();
    m_startTime = newStartTime;
    if (m_held && m_playbackRate) {
        // If held, the start time would still be derrived from the hold time.
        // Force a new, limited, current time.
        m_held = false;
        double currentTime = calculateCurrentTime();
        if (m_playbackRate > 0 && currentTime > sourceEnd()) {
            currentTime = sourceEnd();
        } else if (m_playbackRate < 0 && currentTime < 0) {
            currentTime = 0;
        }
        setCurrentTimeInternal(currentTime, TimingUpdateOnDemand);
    }
    updateCurrentTimingState(TimingUpdateOnDemand);
    double newCurrentTime = currentTimeInternal();

    if (previousCurrentTime != newCurrentTime) {
        setOutdated();
    } else if (!hadStartTime && m_timeline) {
        // Even though this player is not outdated, time to effect change is
        // infinity until start time is set.
        m_timeline->wake();
    }
}

void AnimationPlayer::setSource(AnimationNode* newSource)
{
    if (m_content == newSource)
        return;

    PlayStateUpdateScope updateScope(*this, TimingUpdateOnDemand, SetCompositorPendingWithSourceChanged);

    double storedCurrentTime = currentTimeInternal();
    if (m_content)
        m_content->detach();
    m_content = newSource;
    if (newSource) {
        // FIXME: This logic needs to be updated once groups are implemented
        if (newSource->player()) {
            newSource->player()->cancel();
            newSource->player()->setSource(0);
        }
        newSource->attach(this);
        setOutdated();
    }
    setCurrentTimeInternal(storedCurrentTime, TimingUpdateOnDemand);
}

const char* AnimationPlayer::playStateString(AnimationPlayState playState)
{
    switch (playState) {
    case Idle:
        return "idle";
    case Pending:
        return "pending";
    case Running:
        return "running";
    case Paused:
        return "paused";
    case Finished:
        return "finished";
    default:
        ASSERT_NOT_REACHED();
        return "";
    }
}

AnimationPlayer::AnimationPlayState AnimationPlayer::playStateInternal() const
{
    return m_playState;
}

AnimationPlayer::AnimationPlayState AnimationPlayer::calculatePlayState()
{
    if (m_playState == Idle)
        return Idle;
    if (m_currentTimePending || (isNull(m_startTime) && !m_paused && m_playbackRate != 0))
        return Pending;
    if (m_paused)
        return Paused;
    if (finished())
        return Finished;
    return Running;
}

void AnimationPlayer::pause()
{
    if (m_paused)
        return;

    PlayStateUpdateScope updateScope(*this, TimingUpdateOnDemand);

    if (playing()) {
        m_currentTimePending = true;
    }
    m_paused = true;
    setCurrentTimeInternal(currentTimeInternal(), TimingUpdateOnDemand);
}

void AnimationPlayer::unpause()
{
    if (!m_paused)
        return;

    PlayStateUpdateScope updateScope(*this, TimingUpdateOnDemand);

    m_currentTimePending = true;
    unpauseInternal();
}

void AnimationPlayer::unpauseInternal()
{
    if (!m_paused)
        return;
    m_paused = false;
    setCurrentTimeInternal(currentTimeInternal(), TimingUpdateOnDemand);
}

void AnimationPlayer::play()
{
    PlayStateUpdateScope updateScope(*this, TimingUpdateOnDemand);

    if (!playing())
        m_startTime = nullValue();

    uncancel();
    unpauseInternal();
    if (!m_content)
        return;
    double currentTime = this->currentTimeInternal();
    if (m_playbackRate > 0 && (currentTime < 0 || currentTime >= sourceEnd()))
        setCurrentTimeInternal(0, TimingUpdateOnDemand);
    else if (m_playbackRate < 0 && (currentTime <= 0 || currentTime > sourceEnd()))
        setCurrentTimeInternal(sourceEnd(), TimingUpdateOnDemand);
    m_finished = false;
}

void AnimationPlayer::reverse()
{
    if (!m_playbackRate) {
        return;
    }

    uncancel();
    setPlaybackRateInternal(-m_playbackRate);
    play();
}

void AnimationPlayer::finish(ExceptionState& exceptionState)
{
    PlayStateUpdateScope updateScope(*this, TimingUpdateOnDemand);

    if (!m_playbackRate || playStateInternal() == Idle) {
        return;
    }
    if (m_playbackRate > 0 && sourceEnd() == std::numeric_limits<double>::infinity()) {
        exceptionState.throwDOMException(InvalidStateError, "AnimationPlayer has source content whose end time is infinity.");
        return;
    }

    uncancel();

    double newCurrentTime = m_playbackRate < 0 ? 0 : sourceEnd();
    setCurrentTimeInternal(newCurrentTime, TimingUpdateOnDemand);
    if (!paused()) {
        m_startTime = calculateStartTime(newCurrentTime);
    }

    m_currentTimePending = false;
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
    PlayStateUpdateScope updateScope(*this, TimingUpdateOnDemand);

    m_finished = true;
    m_pendingFinishedEvent = nullptr;
}

bool AnimationPlayer::dispatchEvent(PassRefPtrWillBeRawPtr<Event> event)
{
    if (m_pendingFinishedEvent == event)
        m_pendingFinishedEvent = nullptr;
    return EventTargetWithInlineData::dispatchEvent(event);
}

double AnimationPlayer::playbackRate() const
{
    UseCounter::count(executionContext(), UseCounter::AnimationPlayerGetPlaybackRate);
    return m_playbackRate;
}

void AnimationPlayer::setPlaybackRate(double playbackRate)
{
    UseCounter::count(executionContext(), UseCounter::AnimationPlayerSetPlaybackRate);
    if (!std::isfinite(playbackRate))
        return;
    if (playbackRate == m_playbackRate)
        return;

    PlayStateUpdateScope updateScope(*this, TimingUpdateOnDemand);

    setPlaybackRateInternal(playbackRate);
}

void AnimationPlayer::setPlaybackRateInternal(double playbackRate)
{
    ASSERT(std::isfinite(playbackRate));
    ASSERT(playbackRate != m_playbackRate);

    if (!finished() && !paused() && hasStartTime())
        m_currentTimePending = true;

    double storedCurrentTime = currentTimeInternal();
    if ((m_playbackRate < 0 && playbackRate >= 0) || (m_playbackRate > 0 && playbackRate <= 0))
        m_finished = false;

    m_playbackRate = playbackRate;
    m_startTime = std::numeric_limits<double>::quiet_NaN();
    setCurrentTimeInternal(storedCurrentTime, TimingUpdateOnDemand);
}

void AnimationPlayer::setOutdated()
{
    m_outdated = true;
    if (m_timeline)
        m_timeline->setOutdatedAnimationPlayer(this);
}

bool AnimationPlayer::canStartAnimationOnCompositor()
{
    if (m_playbackRate == 0)
        return false;

    return m_timeline && m_content && m_content->isAnimation() && playing();
}

bool AnimationPlayer::maybeStartAnimationOnCompositor()
{
    if (!canStartAnimationOnCompositor())
        return false;

    double startTime = timeline()->zeroTime() + startTimeInternal();
    double timeOffset = 0;
    if (std::isnan(startTime)) {
        timeOffset = m_playbackRate < 0 ? sourceEnd() - currentTimeInternal() : currentTimeInternal();
    }
    return toAnimation(m_content.get())->maybeStartAnimationOnCompositor(startTime, timeOffset, m_playbackRate);
}

void AnimationPlayer::setCompositorPending(bool sourceChanged)
{
    // FIXME: Animation could notify this directly?
    if (!hasActiveAnimationsOnCompositor()) {
        m_compositorState.release();
    }
    if (sourceChanged && m_compositorState) {
        m_compositorState->sourceChanged = true;
    }
    if (m_compositorPending || m_isPausedForTesting) {
        return;
    }

    if (sourceChanged || !m_compositorState
        || !playing() || m_compositorState->playbackRate != m_playbackRate
        || m_compositorState->startTime != m_startTime) {
        m_compositorPending = true;
        timeline()->document()->compositorPendingAnimations().add(this);
    }
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

    PlayStateUpdateScope updateScope(*this, reason, DoNotSetCompositorPending);

    m_outdated = false;
    bool idle = playStateInternal() == Idle;

    if (m_content) {
        double inheritedTime = idle || isNull(m_timeline->currentTimeInternal()) ? nullValue() : currentTimeInternal();
        // Special case for end-exclusivity when playing backwards.
        if (inheritedTime == 0 && m_playbackRate < 0)
            inheritedTime = -1;
        m_content->updateInheritedTime(inheritedTime, reason);
    }

    if ((idle || finished()) && !m_finished) {
        if (reason == TimingUpdateForAnimationFrame && (idle || hasStartTime())) {
            const AtomicString& eventType = EventTypeNames::finish;
            if (executionContext() && hasEventListeners(eventType)) {
                double eventCurrentTime = currentTimeInternal() * 1000;
                m_pendingFinishedEvent = AnimationPlayerEvent::create(eventType, eventCurrentTime, timeline()->currentTime());
                m_pendingFinishedEvent->setTarget(this);
                m_pendingFinishedEvent->setCurrentTarget(this);
                m_timeline->document()->enqueueAnimationFrameEvent(m_pendingFinishedEvent);
            }
            m_finished = true;
        }
    }
    ASSERT(!m_outdated);
    return !m_finished;
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
    PlayStateUpdateScope updateScope(*this, TimingUpdateOnDemand);

    if (playStateInternal() == Idle)
        return;

    m_holdTime = currentTimeInternal();
    m_held = true;
    // TODO
    m_playState = Idle;
    m_startTime = nullValue();
    m_currentTimePending = false;
}

void AnimationPlayer::uncancel()
{
    PlayStateUpdateScope updateScope(*this, TimingUpdateOnDemand);

    if (playStateInternal() != Idle)
        return;

    // We may not go into the pending state, but setting it to something other
    // than Idle here will force an update.
    ASSERT(isNull(m_startTime));
    m_playState = Pending;
    m_held = true;
    m_holdTime = 0;
    m_finished = false;
}

AnimationPlayer::PlayStateUpdateScope::PlayStateUpdateScope(AnimationPlayer& player, TimingUpdateReason reason, CompositorPendingChange compositorPendingChange)
    : m_player(player)
    , m_initial(player.playStateInternal())
    , m_compositorPendingChange(compositorPendingChange)
{
    m_player.updateCurrentTimingState(reason);
}

AnimationPlayer::PlayStateUpdateScope::~PlayStateUpdateScope()
{
    AnimationPlayState oldPlayState = m_initial;
    AnimationPlayState newPlayState = m_player.calculatePlayState();
    if (oldPlayState != newPlayState) {
        bool wasActive = oldPlayState == Pending || oldPlayState == Running;
        bool isActive = newPlayState == Pending || newPlayState == Running;
        if (!wasActive && isActive) {
            if (m_player.m_content) {
                TRACE_EVENT_ASYNC_BEGIN1("blink", "Animation", &m_player, "Name", TRACE_STR_COPY(m_player.m_content->name().utf8().data()));
            } else {
                TRACE_EVENT_ASYNC_BEGIN0("blink", "Animation", &m_player);
            }
        } else if (wasActive && !isActive) {
            if (oldPlayState != Idle && oldPlayState != Finished) {
                TRACE_EVENT_ASYNC_END0("blink", "Animation", &m_player);
            }
        }
        if (isActive) {
            TRACE_EVENT_ASYNC_STEP_INTO0("blink", "Animation", &m_player, playStateString(newPlayState));
        }
    }

    m_player.m_playState = newPlayState;

#if ENABLE(ASSERT)
    // Verify that current time is up to date.
    m_player.currentTimeInternal();
#endif

    switch (m_compositorPendingChange) {
    case SetCompositorPending:
        m_player.setCompositorPending();
        break;
    case SetCompositorPendingWithSourceChanged:
        m_player.setCompositorPending(true);
        break;
    case DoNotSetCompositorPending:
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }
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
    setCurrentTimeInternal(pauseTime, TimingUpdateOnDemand);
    if (hasActiveAnimationsOnCompositor())
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
