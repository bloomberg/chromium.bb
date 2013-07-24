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

#ifndef TimedItemCalculations_h
#define TimedItemCalculations_h

#include "core/animation/TimedItem.h"
#include "core/animation/Timing.h"
#include "core/platform/animation/AnimationUtilities.h"
#include "wtf/MathExtras.h"

namespace WebCore {

static inline TimedItem::Phase calculatePhase(double activeDuration, double localTime, const Timing& specified)
{
    ASSERT(activeDuration >= 0);
    if (isNull(localTime))
        return TimedItem::PhaseNone;
    if (localTime < specified.startDelay)
        return TimedItem::PhaseBefore;
    if (localTime >= specified.startDelay + activeDuration)
        return TimedItem::PhaseAfter;
    return TimedItem::PhaseActive;
}

static inline bool isActiveInParentPhase(TimedItem::Phase parentPhase, Timing::FillMode fillMode)
{
    switch (parentPhase) {
    case TimedItem::PhaseBefore:
        return fillMode == Timing::FillModeBackwards || fillMode == Timing::FillModeBoth;
    case TimedItem::PhaseActive:
        return true;
    case TimedItem::PhaseAfter:
        return fillMode == Timing::FillModeForwards || fillMode == Timing::FillModeBoth;
    default:
        ASSERT_NOT_REACHED();
        return false;
    }
}

static inline double calculateActiveTime(double activeDuration, double localTime, TimedItem::Phase parentPhase, TimedItem::Phase phase, const Timing& specified)
{
    ASSERT(activeDuration >= 0);
    ASSERT(phase == calculatePhase(activeDuration, localTime, specified));

    switch (phase) {
    case TimedItem::PhaseBefore:
        if (specified.fillMode == Timing::FillModeBackwards || specified.fillMode == Timing::FillModeBoth)
            return 0;
        return nullValue();
    case TimedItem::PhaseActive:
        if (isActiveInParentPhase(parentPhase, specified.fillMode))
            return localTime - specified.startDelay;
        return nullValue();
    case TimedItem::PhaseAfter:
        if (specified.fillMode == Timing::FillModeForwards || specified.fillMode == Timing::FillModeBoth)
            return activeDuration;
        return nullValue();
    case TimedItem::PhaseNone:
        ASSERT(isNull(localTime));
        return nullValue();
    default:
        ASSERT_NOT_REACHED();
        return nullValue();
    }
}

static inline double calculateScaledActiveTime(double activeDuration, double activeTime, double startOffset, const Timing& specified)
{
    ASSERT(activeDuration >= 0);
    ASSERT(startOffset >= 0);

    if (isNull(activeTime))
        return nullValue();

    ASSERT(activeTime >= 0);

    return (specified.playbackRate < 0 ? activeTime - activeDuration : activeTime) * specified.playbackRate + startOffset;
}

static inline bool endsOnIterationBoundary(double iterationCount, double iterationStart)
{
    return !fmod(iterationCount + iterationStart, 1);
}

static inline double calculateIterationTime(double iterationDuration, double repeatedDuration, double scaledActiveTime, double startOffset, const Timing& specified)
{
    ASSERT(iterationDuration >= 0);
    ASSERT(repeatedDuration == iterationDuration * specified.iterationCount);

    if (isNull(scaledActiveTime))
        return nullValue();

    ASSERT(scaledActiveTime >= 0);
    ASSERT(scaledActiveTime <= repeatedDuration + startOffset);

    if (!iterationDuration)
        return 0;

    if (scaledActiveTime - startOffset == repeatedDuration && specified.iterationCount && endsOnIterationBoundary(specified.iterationCount, specified.iterationStart))
        return iterationDuration;

    return fmod(scaledActiveTime, iterationDuration);
}

static inline double calculateCurrentIteration(double iterationDuration, double iterationTime, double scaledActiveTime, const Timing& specified)
{
    ASSERT(iterationDuration >= 0);
    ASSERT(isNull(iterationTime) || iterationTime >= 0);

    if (isNull(scaledActiveTime))
        return nullValue();

    ASSERT(iterationTime >= 0);
    ASSERT(iterationTime <= iterationDuration);
    ASSERT(scaledActiveTime >= 0);

    if (!scaledActiveTime)
        return 0;

    if (!iterationDuration)
        return floor(specified.iterationStart + specified.iterationCount);

    if (iterationTime == iterationDuration)
        return specified.iterationStart + specified.iterationCount - 1;

    return floor(scaledActiveTime / iterationDuration);
}

static inline double calculateDirectedTime(double currentIteration, double iterationDuration, double iterationTime, const Timing& specified)
{
    ASSERT(isNull(currentIteration) || currentIteration >= 0);
    ASSERT(iterationDuration > 0);

    if (isNull(iterationTime))
        return nullValue();

    ASSERT(currentIteration >= 0);
    ASSERT(iterationTime >= 0);
    ASSERT(iterationTime <= iterationDuration);

    const bool currentIterationIsOdd = fmod(currentIteration, 2) >= 1;
    const bool currentDirectionIsForwards = specified.direction == Timing::PlaybackDirectionNormal
        || (specified.direction == Timing::PlaybackDirectionAlternate && !currentIterationIsOdd)
        || (specified.direction == Timing::PlaybackDirectionAlternateReverse && currentIterationIsOdd);

    return currentDirectionIsForwards ? iterationTime : iterationDuration - iterationTime;
}

static inline double calculateTransformedTime(double currentIteration, double iterationDuration, double iterationTime, const Timing& specified)
{
    ASSERT(isNull(currentIteration) || currentIteration >= 0);
    ASSERT(iterationDuration > 0);
    ASSERT(isNull(iterationTime) || (iterationTime >= 0 && iterationTime <= iterationDuration));

    double directedTime = calculateDirectedTime(currentIteration, iterationDuration, iterationTime, specified);
    if (isNull(directedTime))
        return nullValue();
    return specified.timingFunction ?
        iterationDuration * specified.timingFunction->evaluate(directedTime / iterationDuration, accuracyForDuration(iterationDuration)) :
        directedTime;
}

} // namespace WebCore

#endif
