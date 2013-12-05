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
#include "core/animation/TimedItemCalculations.h"

namespace WebCore {

TimedItem::TimedItem(const Timing& timing, PassOwnPtr<EventDelegate> eventDelegate)
    : m_parent(0)
    , m_startTime(0)
    , m_player(0)
    , m_specified(timing)
    , m_eventDelegate(eventDelegate)
    , m_calculated()
    , m_isFirstSample(true)
{
    m_specified.assertValid();
}

bool TimedItem::updateInheritedTime(double inheritedTime) const
{
    const double localTime = inheritedTime - m_startTime;
    const double iterationDuration = m_specified.hasIterationDuration
        ? m_specified.iterationDuration
        : intrinsicIterationDuration();
    ASSERT(iterationDuration >= 0);

    // When iterationDuration = 0 and iterationCount = infinity, or vice-
    // versa, repeatedDuration should be 0, not NaN as operator*() would give.
    // FIXME: The spec is unclear about this.
    const double repeatedDuration = multiplyZeroAlwaysGivesZero(iterationDuration, m_specified.iterationCount);
    ASSERT(repeatedDuration >= 0);
    const double activeDuration = m_specified.playbackRate
        ? repeatedDuration / abs(m_specified.playbackRate)
        : std::numeric_limits<double>::infinity();
    ASSERT(activeDuration >= 0);

    const Phase currentPhase = calculatePhase(activeDuration, localTime, m_specified);
    // FIXME: parentPhase depends on groups being implemented.
    const TimedItem::Phase parentPhase = TimedItem::PhaseActive;
    const double activeTime = calculateActiveTime(activeDuration, localTime, parentPhase, currentPhase, m_specified);

    double currentIteration;
    double timeFraction;
    double timeToNextIteration = std::numeric_limits<double>::infinity();
    if (iterationDuration) {
        const double startOffset = multiplyZeroAlwaysGivesZero(m_specified.iterationStart, iterationDuration);
        ASSERT(startOffset >= 0);
        const double scaledActiveTime = calculateScaledActiveTime(activeDuration, activeTime, startOffset, m_specified);
        const double iterationTime = calculateIterationTime(iterationDuration, repeatedDuration, scaledActiveTime, startOffset, m_specified);

        currentIteration = calculateCurrentIteration(iterationDuration, iterationTime, scaledActiveTime, m_specified);
        timeFraction = calculateTransformedTime(currentIteration, iterationDuration, iterationTime, m_specified) / iterationDuration;
        if (!isNull(iterationTime))
            timeToNextIteration = (iterationDuration - iterationTime) / abs(m_specified.playbackRate);
    } else {
        const double localIterationDuration = 1;
        const double localRepeatedDuration = localIterationDuration * m_specified.iterationCount;
        ASSERT(localRepeatedDuration >= 0);
        const double localActiveDuration = m_specified.playbackRate ? localRepeatedDuration / abs(m_specified.playbackRate) : std::numeric_limits<double>::infinity();
        ASSERT(localActiveDuration >= 0);
        const double localLocalTime = localTime < m_specified.startDelay ? localTime : localActiveDuration + m_specified.startDelay;
        const TimedItem::Phase localCurrentPhase = calculatePhase(localActiveDuration, localLocalTime, m_specified);
        const double localActiveTime = calculateActiveTime(localActiveDuration, localLocalTime, parentPhase, localCurrentPhase, m_specified);
        const double startOffset = m_specified.iterationStart * localIterationDuration;
        ASSERT(startOffset >= 0);
        const double scaledActiveTime = calculateScaledActiveTime(localActiveDuration, localActiveTime, startOffset, m_specified);
        const double iterationTime = calculateIterationTime(localIterationDuration, localRepeatedDuration, scaledActiveTime, startOffset, m_specified);

        currentIteration = calculateCurrentIteration(localIterationDuration, iterationTime, scaledActiveTime, m_specified);
        timeFraction = calculateTransformedTime(currentIteration, localIterationDuration, iterationTime, m_specified);
    }

    const double previousIteration = m_calculated.currentIteration;
    m_calculated.currentIteration = currentIteration;
    m_calculated.activeDuration = activeDuration;
    m_calculated.timeFraction = timeFraction;

    const Phase previousPhase = m_calculated.phase;
    m_calculated.phase = currentPhase;
    m_calculated.isInEffect = !isNull(activeTime);
    m_calculated.isInPlay = phase() == PhaseActive && (!m_parent || m_parent->isInPlay());
    m_calculated.isCurrent = phase() == PhaseBefore || isInPlay() || (m_parent && m_parent->isCurrent());

    // This logic is specific to CSS animation events and assumes that all
    // animations start after the DocumentTimeline has started.
    if (m_eventDelegate && (m_isFirstSample || previousPhase != phase() || (phase() == PhaseActive && previousIteration != currentIteration)))
        m_eventDelegate->onEventCondition(this, m_isFirstSample, previousPhase, previousIteration);
    m_isFirstSample = false;

    // FIXME: This probably shouldn't be recursive.
    bool didTriggerStyleRecalc = updateChildrenAndEffects();

    if (activeDuration - activeTime < timeToNextIteration)
        timeToNextIteration = std::numeric_limits<double>::infinity();

    m_calculated.timeToEffectChange = calculateTimeToEffectChange(localTime, timeToNextIteration);
    return didTriggerStyleRecalc;
}

} // namespace WebCore
