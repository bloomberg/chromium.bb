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
#include "core/dom/ExceptionCode.h"
#include "core/events/AnimationPlayerEvent.h"
#include "core/frame/UseCounter.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/inspector/InspectorTraceEvents.h"
#include "platform/TraceEvent.h"
#include "wtf/MathExtras.h"

namespace blink {

namespace {

static unsigned nextSequenceNumber()
{
    static unsigned next = 0;
    return ++next;
}

}

PassRefPtrWillBeRawPtr<AnimationPlayer> AnimationPlayer::create(AnimationNode* source, AnimationTimeline* timeline)
{
    if (!timeline) {
        // FIXME: Support creating players without a timeline.
        return nullptr;
    }

    RefPtrWillBeRawPtr<AnimationPlayer> player = adoptRefWillBeNoop(new AnimationPlayer(timeline->document()->contextDocument().get(), *timeline, source));
    player->suspendIfNeeded();

    if (timeline) {
        timeline->playerAttached(*player);
    }

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
    , m_outdated(false)
    , m_finished(true)
    , m_compositorState(nullptr)
    , m_compositorPending(false)
    , m_compositorGroup(0)
    , m_currentTimePending(false)
    , m_stateIsBeingUpdated(false)
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

    PlayStateUpdateScope updateScope(*this, TimingUpdateOnDemand);

    m_currentTimePending = false;
    setCurrentTimeInternal(newCurrentTime / 1000, TimingUpdateOnDemand);

    if (calculatePlayState() == Finished)
        m_startTime = calculateStartTime(newCurrentTime);
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
        double newCurrentTime = m_holdTime;
        if (playStateInternal() == Finished && !isNull(m_startTime) && m_timeline) {
            // Add hystersis due to floating point error accumulation
            if (!limited(calculateCurrentTime() + 0.001 * m_playbackRate)) {
                // The current time became unlimited, eg. due to a backwards
                // seek of the timeline.
                newCurrentTime = calculateCurrentTime();
            } else if (!limited(m_holdTime)) {
                // The hold time became unlimited, eg. due to the source content
                // becoming longer.
                newCurrentTime = clampTo<double>(calculateCurrentTime(), 0, sourceEnd());
            }
        }
        setCurrentTimeInternal(newCurrentTime, reason);
    } else if (limited(calculateCurrentTime())) {
        m_held = true;
        m_holdTime = m_playbackRate < 0 ? 0 : sourceEnd();
    }
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

double AnimationPlayer::unlimitedCurrentTimeInternal() const
{
#if ENABLE(ASSERT)
    currentTimeInternal();
#endif
    return playStateInternal() == Paused || isNull(m_startTime)
        ? currentTimeInternal()
        : calculateCurrentTime();
}

void AnimationPlayer::preCommit(int compositorGroup, bool startOnCompositor)
{
    PlayStateUpdateScope updateScope(*this, TimingUpdateOnDemand, DoNotSetCompositorPending);

    bool softChange = m_compositorState && (paused() || m_compositorState->playbackRate != m_playbackRate);
    bool hardChange = m_compositorState && (m_compositorState->sourceChanged || m_compositorState->startTime != m_startTime);

    // FIXME: softChange && !hardChange should generate a Pause/ThenStart,
    // not a Cancel, but we can't communicate these to the compositor yet.

    bool changed = softChange || hardChange;
    bool shouldCancel = (!playing() && m_compositorState) || changed;
    bool shouldStart = playing() && (!m_compositorState || changed);

    if (shouldCancel) {
        cancelAnimationOnCompositor();
        m_compositorState = nullptr;
    }

    if (m_compositorState && m_compositorState->pendingAction == Start) {
        // Still waiting for a start time.
        return;
    }

    ASSERT(!m_compositorState || !std::isnan(m_compositorState->startTime));

    if (!shouldStart) {
        m_currentTimePending = false;
    }

    if (shouldStart) {
        m_compositorGroup = compositorGroup;
        if (startOnCompositor) {
            if (maybeStartAnimationOnCompositor())
                m_compositorState = adoptPtr(new CompositorState(*this));
            else
                cancelIncompatibleAnimationsOnCompositor();
        }
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
    PlayStateUpdateScope updateScope(*this, TimingUpdateOnDemand, DoNotSetCompositorPending);

    if (m_compositorState) {
        ASSERT(m_compositorState->pendingAction == Start);
        ASSERT(std::isnan(m_compositorState->startTime));

        double initialCompositorHoldTime = m_compositorState->holdTime;
        m_compositorState->pendingAction = None;
        m_compositorState->startTime = timelineTime + currentTimeInternal() / -m_playbackRate;

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

bool AnimationPlayer::affects(const Element& element, CSSPropertyID property) const
{
    if (!m_content || !m_content->isAnimation())
        return false;

    const Animation* animation = toAnimation(m_content.get());
    return (animation->target() == &element) && animation->affects(property);
}

double AnimationPlayer::calculateStartTime(double currentTime) const
{
    return m_timeline->effectiveTime() - currentTime / m_playbackRate;
}

double AnimationPlayer::calculateCurrentTime() const
{
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
    if (limited())
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

    if (playStateInternal() == Idle) {
        // We may not go into the pending state, but setting it to something other
        // than Idle here will force an update.
        ASSERT(isNull(m_startTime));
        m_playState = Pending;
        m_held = true;
        m_holdTime = 0;
    }

    m_finished = false;
    unpauseInternal();
    if (!m_content)
        return;
    double currentTime = this->currentTimeInternal();
    if (m_playbackRate > 0 && (currentTime < 0 || currentTime >= sourceEnd()))
        setCurrentTimeInternal(0, TimingUpdateOnDemand);
    else if (m_playbackRate < 0 && (currentTime <= 0 || currentTime > sourceEnd()))
        setCurrentTimeInternal(sourceEnd(), TimingUpdateOnDemand);
}

void AnimationPlayer::reverse()
{
    if (!m_playbackRate) {
        return;
    }

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

    double newCurrentTime = m_playbackRate < 0 ? 0 : sourceEnd();
    setCurrentTimeInternal(newCurrentTime, TimingUpdateOnDemand);
    if (!paused()) {
        m_startTime = calculateStartTime(newCurrentTime);
    }

    m_currentTimePending = false;
    ASSERT(playStateInternal() != Idle);
    ASSERT(limited());
}

ScriptPromise AnimationPlayer::finished(ScriptState* scriptState)
{
    if (!m_finishedPromise) {
        m_finishedPromise = new AnimationPlayerPromise(scriptState->executionContext(), this, AnimationPlayerPromise::Finished);
        if (playStateInternal() == Finished)
            m_finishedPromise->resolve(this);
    }
    return m_finishedPromise->promise(scriptState->world());
}

ScriptPromise AnimationPlayer::ready(ScriptState* scriptState)
{
    if (!m_readyPromise) {
        m_readyPromise = new AnimationPlayerPromise(scriptState->executionContext(), this, AnimationPlayerPromise::Ready);
        if (playStateInternal() != Pending)
            m_readyPromise->resolve(this);
    }
    return m_readyPromise->promise(scriptState->world());
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
    if (playbackRate == m_playbackRate)
        return;

    PlayStateUpdateScope updateScope(*this, TimingUpdateOnDemand);

    setPlaybackRateInternal(playbackRate);
}

void AnimationPlayer::setPlaybackRateInternal(double playbackRate)
{
    ASSERT(std::isfinite(playbackRate));
    ASSERT(playbackRate != m_playbackRate);

    if (!limited() && !paused() && hasStartTime())
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
    // FIXME: Timeline playback rates should be compositable
    if (m_playbackRate == 0 || (std::isinf(sourceEnd()) && m_playbackRate < 0) || (timeline() && timeline()->playbackRate() != 1))
        return false;

    return m_timeline && m_content && m_content->isAnimation() && playing();
}

bool AnimationPlayer::maybeStartAnimationOnCompositor()
{
    if (!canStartAnimationOnCompositor())
        return false;

    bool reversed = m_playbackRate < 0;

    double startTime = timeline()->zeroTime() + startTimeInternal();
    if (reversed) {
        startTime -= sourceEnd() / fabs(m_playbackRate);
    }

    double timeOffset = 0;
    if (std::isnan(startTime)) {
        timeOffset = reversed ? sourceEnd() - currentTimeInternal() : currentTimeInternal();
        timeOffset = timeOffset / fabs(m_playbackRate);
    }
    ASSERT(m_compositorGroup != 0);
    return toAnimation(m_content.get())->maybeStartAnimationOnCompositor(m_compositorGroup, startTime, timeOffset, m_playbackRate);
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

void AnimationPlayer::cancelAnimationOnCompositor()
{
    if (hasActiveAnimationsOnCompositor())
        toAnimation(m_content.get())->cancelAnimationOnCompositor();
}

void AnimationPlayer::restartAnimationOnCompositor()
{
    if (hasActiveAnimationsOnCompositor())
        toAnimation(m_content.get())->restartAnimationOnCompositor();
}

void AnimationPlayer::cancelIncompatibleAnimationsOnCompositor()
{
    if (m_content && m_content->isAnimation())
        toAnimation(m_content.get())->cancelIncompatibleAnimationsOnCompositor();
}

bool AnimationPlayer::hasActiveAnimationsOnCompositor()
{
    if (!m_content || !m_content->isAnimation())
        return false;

    return toAnimation(m_content.get())->hasActiveAnimationsOnCompositor();
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

    if ((idle || limited()) && !m_finished) {
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
    double result = m_playbackRate > 0
        ? m_content->timeToForwardsEffectChange() / m_playbackRate
        : m_content->timeToReverseEffectChange() / -m_playbackRate;
    return !hasActiveAnimationsOnCompositor() && m_content->phase() == AnimationNode::PhaseActive
        ? 0
        : result;
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

    // after cancelation, transitions must be downgraded or they'll fail
    // to be considered when retriggering themselves. This can happen if
    // the transition is captured through getAnimationPlayers then played.
    if (m_content && m_content->isAnimation())
        toAnimation(m_content.get())->downgradeToNormalAnimation();
}

void AnimationPlayer::beginUpdatingState()
{
    // Nested calls are not allowed!
    ASSERT(!m_stateIsBeingUpdated);
    m_stateIsBeingUpdated = true;
}

void AnimationPlayer::endUpdatingState()
{
    ASSERT(m_stateIsBeingUpdated);
    m_stateIsBeingUpdated = false;
}

AnimationPlayer::PlayStateUpdateScope::PlayStateUpdateScope(AnimationPlayer& player, TimingUpdateReason reason, CompositorPendingChange compositorPendingChange)
    : m_player(player)
    , m_initialPlayState(m_player->playStateInternal())
    , m_compositorPendingChange(compositorPendingChange)
{
    m_player->beginUpdatingState();
    m_player->updateCurrentTimingState(reason);
}

AnimationPlayer::PlayStateUpdateScope::~PlayStateUpdateScope()
{
    AnimationPlayState oldPlayState = m_initialPlayState;
    AnimationPlayState newPlayState = m_player->calculatePlayState();

    m_player->m_playState = newPlayState;
    if (oldPlayState != newPlayState) {
        bool wasActive = oldPlayState == Pending || oldPlayState == Running;
        bool isActive = newPlayState == Pending || newPlayState == Running;
        if (!wasActive && isActive)
            TRACE_EVENT_NESTABLE_ASYNC_BEGIN1("blink.animations," TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "Animation", m_player, "data", InspectorAnimationEvent::data(*m_player));
        else if (wasActive && !isActive)
            TRACE_EVENT_NESTABLE_ASYNC_END1("blink.animations," TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "Animation", m_player, "endData", InspectorAnimationStateEvent::data(*m_player));
        else
            TRACE_EVENT_NESTABLE_ASYNC_INSTANT1("blink.animations," TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "Animation", m_player, "data", InspectorAnimationStateEvent::data(*m_player));
    }

    // Ordering is important, the ready promise should resolve/reject before
    // the finished promise.
    if (m_player->m_readyPromise && newPlayState != oldPlayState) {
        if (newPlayState == Idle) {
            if (m_player->m_readyPromise->state() == AnimationPlayerPromise::Pending) {
                m_player->m_readyPromise->reject(DOMException::create(AbortError));
            }
            m_player->m_readyPromise->reset();
            m_player->m_readyPromise->resolve(m_player);
        } else if (oldPlayState == Pending) {
            m_player->m_readyPromise->resolve(m_player);
        } else if (newPlayState == Pending) {
            ASSERT(m_player->m_readyPromise->state() != AnimationPlayerPromise::Pending);
            m_player->m_readyPromise->reset();
        }
    }

    if (m_player->m_finishedPromise && newPlayState != oldPlayState) {
        if (newPlayState == Idle) {
            if (m_player->m_finishedPromise->state() == AnimationPlayerPromise::Pending) {
                m_player->m_finishedPromise->reject(DOMException::create(AbortError));
            }
            m_player->m_finishedPromise->reset();
        } else if (newPlayState == Finished) {
            m_player->m_finishedPromise->resolve(m_player);
        } else if (oldPlayState == Finished) {
            m_player->m_finishedPromise->reset();
        }
    }

    if (oldPlayState != newPlayState && (oldPlayState == Idle || newPlayState == Idle)) {
        m_player->setOutdated();
    }

#if ENABLE(ASSERT)
    // Verify that current time is up to date.
    m_player->currentTimeInternal();
#endif

    switch (m_compositorPendingChange) {
    case SetCompositorPending:
        m_player->setCompositorPending();
        break;
    case SetCompositorPendingWithSourceChanged:
        m_player->setCompositorPending(true);
        break;
    case DoNotSetCompositorPending:
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }
    m_player->endUpdatingState();

    if (oldPlayState != newPlayState && newPlayState == Running)
        InspectorInstrumentation::didCreateAnimationPlayer(m_player->timeline()->document(), *m_player);
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

DEFINE_TRACE(AnimationPlayer)
{
    visitor->trace(m_content);
    visitor->trace(m_timeline);
    visitor->trace(m_pendingFinishedEvent);
    visitor->trace(m_finishedPromise);
    visitor->trace(m_readyPromise);
    EventTargetWithInlineData::trace(visitor);
    ActiveDOMObject::trace(visitor);
}

} // namespace
