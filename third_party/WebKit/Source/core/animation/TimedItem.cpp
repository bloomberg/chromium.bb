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
#include "core/animation/TimedItem.h"

#include "core/animation/AnimationPlayer.h"
#include "core/animation/TimedItemCalculations.h"
#include "core/animation/TimedItemTiming.h"

namespace WebCore {

namespace {

Timing::FillMode resolvedFillMode(Timing::FillMode fillMode, bool isAnimation)
{
    if (fillMode != Timing::FillModeAuto)
        return fillMode;
    if (isAnimation)
        return Timing::FillModeNone;
    return Timing::FillModeBoth;
}

} // namespace

TimedItem::TimedItem(const Timing& timing, PassOwnPtr<EventDelegate> eventDelegate)
    : m_parent(0)
    , m_startTime(0)
    , m_player(0)
    , m_specified(timing)
    , m_eventDelegate(eventDelegate)
    , m_calculated()
    , m_isFirstSample(true)
    , m_needsUpdate(true)
    , m_lastUpdateTime(nullValue())
{
    m_specified.assertValid();
}

double TimedItem::iterationDuration() const
{
    double result = std::isnan(m_specified.iterationDuration) ? intrinsicIterationDuration() : m_specified.iterationDuration;
    ASSERT(result >= 0);
    return result;
}

double TimedItem::repeatedDuration() const
{
    const double result = multiplyZeroAlwaysGivesZero(iterationDuration(), m_specified.iterationCount);
    ASSERT(result >= 0);
    return result;
}

double TimedItem::activeDuration() const
{
    const double result = m_specified.playbackRate
        ? repeatedDuration() / std::abs(m_specified.playbackRate)
        : std::numeric_limits<double>::infinity();
    ASSERT(result >= 0);
    return result;
}

void TimedItem::updateSpecifiedTiming(const Timing& timing)
{
    m_specified = timing;
    invalidate();
    if (m_player)
        m_player->setOutdated();
}

void TimedItem::updateInheritedTime(double inheritedTime) const
{
    bool needsUpdate = m_needsUpdate || (m_lastUpdateTime != inheritedTime && !(isNull(m_lastUpdateTime) && isNull(inheritedTime)));
    m_needsUpdate = false;
    m_lastUpdateTime = inheritedTime;

    const double previousIteration = m_calculated.currentIteration;
    const Phase previousPhase = m_calculated.phase;

    const double localTime = inheritedTime - m_startTime;
    double timeToNextIteration = std::numeric_limits<double>::infinity();
    if (needsUpdate) {
        const double activeDuration = this->activeDuration();

        const Phase currentPhase = calculatePhase(activeDuration, localTime, m_specified);
        // FIXME: parentPhase depends on groups being implemented.
        const TimedItem::Phase parentPhase = TimedItem::PhaseActive;
        const double activeTime = calculateActiveTime(activeDuration, resolvedFillMode(m_specified.fillMode, isAnimation()), localTime, parentPhase, currentPhase, m_specified);

        double currentIteration;
        double timeFraction;
        if (const double iterationDuration = this->iterationDuration()) {
            const double startOffset = multiplyZeroAlwaysGivesZero(m_specified.iterationStart, iterationDuration);
            ASSERT(startOffset >= 0);
            const double scaledActiveTime = calculateScaledActiveTime(activeDuration, activeTime, startOffset, m_specified);
            const double iterationTime = calculateIterationTime(iterationDuration, repeatedDuration(), scaledActiveTime, startOffset, m_specified);

            currentIteration = calculateCurrentIteration(iterationDuration, iterationTime, scaledActiveTime, m_specified);
            timeFraction = calculateTransformedTime(currentIteration, iterationDuration, iterationTime, m_specified) / iterationDuration;

            if (!isNull(iterationTime)) {
                timeToNextIteration = (iterationDuration - iterationTime) / std::abs(m_specified.playbackRate);
                if (activeDuration - activeTime < timeToNextIteration)
                    timeToNextIteration = std::numeric_limits<double>::infinity();
            }
        } else {
            const double localIterationDuration = 1;
            const double localRepeatedDuration = localIterationDuration * m_specified.iterationCount;
            ASSERT(localRepeatedDuration >= 0);
            const double localActiveDuration = m_specified.playbackRate ? localRepeatedDuration / std::abs(m_specified.playbackRate) : std::numeric_limits<double>::infinity();
            ASSERT(localActiveDuration >= 0);
            const double localLocalTime = localTime < m_specified.startDelay ? localTime : localActiveDuration + m_specified.startDelay;
            const TimedItem::Phase localCurrentPhase = calculatePhase(localActiveDuration, localLocalTime, m_specified);
            const double localActiveTime = calculateActiveTime(localActiveDuration, resolvedFillMode(m_specified.fillMode, isAnimation()), localLocalTime, parentPhase, localCurrentPhase, m_specified);
            const double startOffset = m_specified.iterationStart * localIterationDuration;
            ASSERT(startOffset >= 0);
            const double scaledActiveTime = calculateScaledActiveTime(localActiveDuration, localActiveTime, startOffset, m_specified);
            const double iterationTime = calculateIterationTime(localIterationDuration, localRepeatedDuration, scaledActiveTime, startOffset, m_specified);

            currentIteration = calculateCurrentIteration(localIterationDuration, iterationTime, scaledActiveTime, m_specified);
            timeFraction = calculateTransformedTime(currentIteration, localIterationDuration, iterationTime, m_specified);
        }

        m_calculated.currentIteration = currentIteration;
        m_calculated.timeFraction = timeFraction;

        m_calculated.phase = currentPhase;
        m_calculated.isInEffect = !isNull(activeTime);
        m_calculated.isInPlay = phase() == PhaseActive && (!m_parent || m_parent->isInPlay());
        m_calculated.isCurrent = phase() == PhaseBefore || isInPlay() || (m_parent && m_parent->isCurrent());
        m_calculated.localTime = m_lastUpdateTime - m_startTime;
    }

    // Test for events even if timing didn't need an update as the player may have gained a start time.
    // FIXME: Refactor so that we can ASSERT(m_player) here, this is currently required to be nullable for testing.
    if (!m_player || m_player->hasStartTime()) {
        // This logic is specific to CSS animation events and assumes that all
        // animations start after the DocumentTimeline has started.
        if (m_eventDelegate && (m_isFirstSample || previousPhase != phase() || (phase() == PhaseActive && previousIteration != m_calculated.currentIteration)))
            m_eventDelegate->onEventCondition(this, m_isFirstSample, previousPhase, previousIteration);
        m_isFirstSample = false;
    }

    if (needsUpdate)  {
        // FIXME: This probably shouldn't be recursive.
        updateChildrenAndEffects();
        m_calculated.timeToForwardsEffectChange = calculateTimeToEffectChange(true, localTime, timeToNextIteration);
        m_calculated.timeToReverseEffectChange = calculateTimeToEffectChange(false, localTime, timeToNextIteration);
    }
}

const TimedItem::CalculatedTiming& TimedItem::ensureCalculated() const
{
    if (!m_player)
        return m_calculated;
    if (m_player->outdated())
        m_player->update(AnimationPlayer::UpdateOnDemand);
    ASSERT(!m_player->outdated());
    return m_calculated;
}

PassRefPtr<TimedItemTiming> TimedItem::specified()
{
    return TimedItemTiming::create(this);
}

} // namespace WebCore
