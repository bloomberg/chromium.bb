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

TimedItem::TimedItem(const Timing& timing)
    : m_parent(0)
    , m_startTime(0)
    , m_specified(timing)
    , m_calculated()
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

    const double activeTime = calculateActiveTime(activeDuration, localTime, m_startTime, m_specified);

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
        const double newLocalTime = localTime < m_specified.startDelay ? m_specified.startDelay - 1 : activeDuration;
        const double activeTime = calculateActiveTime(activeDuration, newLocalTime, m_startTime, m_specified);
        const double startOffset = m_specified.iterationStart * iterationDuration;
        const double scaledActiveTime = calculateScaledActiveTime(activeDuration, activeTime, startOffset, m_specified);
        const double iterationTime = calculateIterationTime(iterationDuration, repeatedDuration, scaledActiveTime, startOffset, m_specified);

        currentIteration = calculateCurrentIteration(iterationDuration, iterationTime, scaledActiveTime, m_specified);
        timeFraction = calculateTransformedTime(currentIteration, iterationDuration, iterationTime, m_specified);
    }

    m_calculated.currentIteration = currentIteration;
    m_calculated.activeDuration = activeDuration;
    m_calculated.timeFraction = timeFraction;

    const bool wasActiveOrInEffect = isActive() || isInEffect();
    m_calculated.isScheduled = (!isNull(localTime) && localTime < m_specified.startDelay) || (m_parent && m_parent->isScheduled());
    m_calculated.isActive = !isNull(localTime) && localTime >= m_specified.startDelay && localTime < m_specified.startDelay + activeDuration;
    m_calculated.isCurrent = isScheduled() || isActive() || (m_parent && m_parent->isCurrent());
    m_calculated.isInEffect = !isNull(activeTime);

    // FIXME: This probably shouldn't be recursive.
    updateChildrenAndEffects(wasActiveOrInEffect);
}

TimedItem::CalculatedTiming::CalculatedTiming()
    : activeDuration(nullValue())
    , currentIteration(nullValue())
    , timeFraction(nullValue())
    , isScheduled(false)
    , isActive(false)
    , isCurrent(false)
    , isInEffect(false)
{
}

} // namespace WebCore
