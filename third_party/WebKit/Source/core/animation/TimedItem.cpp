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
    , m_player(0)
    , m_startTime(0)
    , m_specified(timing)
    , m_calculated()
    , m_eventDelegate(eventDelegate)
    , m_isFirstSample(true)
{
    timing.assertValid();
}

void TimedItem::updateInheritedTime(double inheritedTime) const
{
    const double localTime = inheritedTime - m_startTime;
    const double iterationDuration = m_specified.hasIterationDuration
        ? m_specified.iterationDuration
        : intrinsicIterationDuration();

    const double repeatedDuration = iterationDuration * m_specified.iterationCount;
    const double activeDuration = m_specified.playbackRate
        ? repeatedDuration / abs(m_specified.playbackRate)
        : std::numeric_limits<double>::infinity();

    const Phase currentPhase = calculatePhase(activeDuration, localTime, m_specified);
    // FIXME: parentPhase depends on groups being implemented.
    const TimedItem::Phase parentPhase = TimedItem::PhaseActive;
    const double activeTime = calculateActiveTime(activeDuration, localTime, parentPhase, currentPhase, m_specified);

    double currentIteration = nullValue();
    double timeFraction = nullValue();
    ASSERT(iterationDuration >= 0);
    if (iterationDuration) {
        const double startOffset = m_specified.iterationStart * iterationDuration;
        const double scaledActiveTime = calculateScaledActiveTime(activeDuration, activeTime, startOffset, m_specified);
        const double iterationTime = calculateIterationTime(iterationDuration, repeatedDuration, scaledActiveTime, startOffset, m_specified);

        currentIteration = calculateCurrentIteration(iterationDuration, iterationTime, scaledActiveTime, m_specified);
        timeFraction = calculateTransformedTime(currentIteration, iterationDuration, iterationTime, m_specified) / iterationDuration;
    } else {
        const double iterationDuration = 1;
        const double repeatedDuration = iterationDuration * m_specified.iterationCount;
        const double activeDuration = m_specified.playbackRate ? repeatedDuration / abs(m_specified.playbackRate) : std::numeric_limits<double>::infinity();
        const double newLocalTime = localTime < m_specified.startDelay ? m_specified.startDelay - 1 : activeDuration + m_specified.startDelay;
        const TimedItem::Phase localPhase = calculatePhase(activeDuration, newLocalTime, m_specified);
        const double activeTime = calculateActiveTime(activeDuration, newLocalTime, parentPhase, localPhase, m_specified);
        const double startOffset = m_specified.iterationStart * iterationDuration;
        const double scaledActiveTime = calculateScaledActiveTime(activeDuration, activeTime, startOffset, m_specified);
        const double iterationTime = calculateIterationTime(iterationDuration, repeatedDuration, scaledActiveTime, startOffset, m_specified);

        currentIteration = calculateCurrentIteration(iterationDuration, iterationTime, scaledActiveTime, m_specified);
        timeFraction = calculateTransformedTime(currentIteration, iterationDuration, iterationTime, m_specified);
    }

    const double previousIteration = m_calculated.currentIteration;
    m_calculated.currentIteration = currentIteration;
    m_calculated.activeDuration = activeDuration;
    m_calculated.timeFraction = timeFraction;

    const Phase previousPhase = m_calculated.phase;
    const bool wasInEffect = m_calculated.isInEffect;
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
    updateChildrenAndEffects(wasInEffect);

    m_calculated.timeToEffectChange = calculateTimeToEffectChange(localTime, m_startTime + m_specified.startDelay, m_calculated.phase);

}

} // namespace WebCore
