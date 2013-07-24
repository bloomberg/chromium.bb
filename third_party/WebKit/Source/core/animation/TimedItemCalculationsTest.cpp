/*
 * Copyright (c) 2013, Google Inc. All rights reserved.
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
#include "core/animation/TimedItemCalculations.h"

#include <gtest/gtest.h>

using namespace WebCore;

namespace {

TEST(TimedItemCalculations, ActiveTime)
{
    Timing timing;

    // calculateActiveTime(activeDuration, localTime, startTime)

    // Before Phase
    timing.startDelay = 10;
    timing.fillMode = Timing::FillModeForwards;
    ASSERT_TRUE(isNull(calculateActiveTime(20, 0, TimedItem::PhaseActive, TimedItem::PhaseBefore, timing)));
    timing.fillMode = Timing::FillModeNone;
    ASSERT_TRUE(isNull(calculateActiveTime(20, 0, TimedItem::PhaseActive, TimedItem::PhaseBefore, timing)));
    timing.fillMode = Timing::FillModeBackwards;
    ASSERT_EQ(0, calculateActiveTime(20, 0, TimedItem::PhaseActive, TimedItem::PhaseBefore, timing));
    timing.fillMode = Timing::FillModeBoth;
    ASSERT_EQ(0, calculateActiveTime(20, 0, TimedItem::PhaseActive, TimedItem::PhaseBefore, timing));

    // Active Phase
    timing.startDelay = 10;
    // Active, and parent Before
    timing.fillMode = Timing::FillModeNone;
    ASSERT_TRUE(isNull(calculateActiveTime(20, 15, TimedItem::PhaseBefore, TimedItem::PhaseActive, timing)));
    timing.fillMode = Timing::FillModeForwards;
    ASSERT_TRUE(isNull(calculateActiveTime(20, 15, TimedItem::PhaseBefore, TimedItem::PhaseActive, timing)));
    // Active, and parent After
    timing.fillMode = Timing::FillModeNone;
    ASSERT_TRUE(isNull(calculateActiveTime(20, 15, TimedItem::PhaseAfter, TimedItem::PhaseActive, timing)));
    timing.fillMode = Timing::FillModeBackwards;
    ASSERT_TRUE(isNull(calculateActiveTime(20, 15, TimedItem::PhaseAfter, TimedItem::PhaseActive, timing)));
    // Active, and parent Active
    timing.fillMode = Timing::FillModeForwards;
    ASSERT_EQ(5, calculateActiveTime(20, 15, TimedItem::PhaseActive, TimedItem::PhaseActive, timing));

    // After Phase
    timing.startDelay = 10;
    timing.fillMode = Timing::FillModeForwards;
    ASSERT_EQ(21, calculateActiveTime(21, 45, TimedItem::PhaseActive, TimedItem::PhaseAfter, timing));
    timing.fillMode = Timing::FillModeBoth;
    ASSERT_EQ(21, calculateActiveTime(21, 45, TimedItem::PhaseActive, TimedItem::PhaseAfter, timing));
    timing.fillMode = Timing::FillModeBackwards;
    ASSERT_TRUE(isNull(calculateActiveTime(21, 45, TimedItem::PhaseActive, TimedItem::PhaseAfter, timing)));
    timing.fillMode = Timing::FillModeNone;
    ASSERT_TRUE(isNull(calculateActiveTime(21, 45, TimedItem::PhaseActive, TimedItem::PhaseAfter, timing)));

    // None
    ASSERT_TRUE(isNull(calculateActiveTime(32, nullValue(), TimedItem::PhaseNone, TimedItem::PhaseNone, timing)));
}

TEST(TimedItemCalculations, ScaledActiveTime)
{
    Timing timing;

    // calculateScaledActiveTime(activeDuration, activeTime, startOffset)

    // if the active time is null
    ASSERT_TRUE(isNull(calculateScaledActiveTime(4, nullValue(), 5, timing)));

    // if the playback rate is negative
    timing.playbackRate = -1;
    ASSERT_EQ(-5, calculateScaledActiveTime(10, 20, 5, timing));

    // otherwise
    timing.playbackRate = 0;
    ASSERT_EQ(5, calculateScaledActiveTime(10, 20, 5, timing));
    timing.playbackRate = 1;
    ASSERT_EQ(25, calculateScaledActiveTime(10, 20, 5, timing));
}

TEST(TimedItemCalculations, IterationTime)
{
    Timing timing;

    // calculateIterationTime(iterationDuration, repeatedDuration, scaledActiveTime, startOffset)

    // if the scaled active time is null
    ASSERT_TRUE(isNull(calculateIterationTime(1, 1, nullValue(), 1, timing)));

    // if the iteration duration is zero
    ASSERT_EQ(0, calculateIterationTime(0, 0, 0, 4, timing));

    // if (complex-conditions)...
    ASSERT_EQ(12, calculateIterationTime(12, 12, 12, 0, timing));

    // otherwise
    timing.iterationCount = 10;
    ASSERT_EQ(5, calculateIterationTime(10, 100, 25, 4, timing));
    ASSERT_EQ(7, calculateIterationTime(11, 110, 29, 1, timing));
    timing.iterationStart = 1.1;
    ASSERT_EQ(8, calculateIterationTime(12, 120, 20, 7, timing));
}

TEST(TimedItemCalculations, CurrentIteration)
{
    Timing timing;

    // calculateCurrentIteration(iterationDuration, iterationTime, scaledActiveTime)

    // if the scaled active time is null
    ASSERT_TRUE(isNull(calculateCurrentIteration(1, 1, nullValue(), timing)));

    // if the scaled active time is zero
    ASSERT_EQ(0, calculateCurrentIteration(1, 1, 0, timing));

    // if iterationDuration is zero
    ASSERT_EQ(1, calculateCurrentIteration(0, 0, 9, timing));

    // if the iteration time equals the iteration duration
    timing.iterationStart = 4;
    timing.iterationCount = 7;
    ASSERT_EQ(10, calculateCurrentIteration(5, 5, 9, timing));

    // otherwise
    ASSERT_EQ(3, calculateCurrentIteration(3.2, 3.1, 10, timing));
}

TEST(TimedItemCalculations, DirectedTime)
{
    Timing timing;

    // calculateDirectedTime(currentIteration, iterationDuration, iterationTime)

    // if the iteration time is null
    ASSERT_TRUE(isNull(calculateDirectedTime(1, 2, nullValue(), timing)));

    // forwards
    ASSERT_EQ(17, calculateDirectedTime(0, 20, 17, timing));
    ASSERT_EQ(17, calculateDirectedTime(1, 20, 17, timing));
    timing.direction = Timing::PlaybackDirectionAlternate;
    ASSERT_EQ(17, calculateDirectedTime(0, 20, 17, timing));
    ASSERT_EQ(17, calculateDirectedTime(2, 20, 17, timing));
    timing.direction = Timing::PlaybackDirectionAlternateReverse;
    ASSERT_EQ(17, calculateDirectedTime(1, 20, 17, timing));
    ASSERT_EQ(17, calculateDirectedTime(3, 20, 17, timing));

    // reverse
    timing.direction = Timing::PlaybackDirectionReverse;
    ASSERT_EQ(3, calculateDirectedTime(0, 20, 17, timing));
    ASSERT_EQ(3, calculateDirectedTime(1, 20, 17, timing));
    timing.direction = Timing::PlaybackDirectionAlternate;
    ASSERT_EQ(3, calculateDirectedTime(1, 20, 17, timing));
    ASSERT_EQ(3, calculateDirectedTime(3, 20, 17, timing));
    timing.direction = Timing::PlaybackDirectionAlternateReverse;
    ASSERT_EQ(3, calculateDirectedTime(0, 20, 17, timing));
    ASSERT_EQ(3, calculateDirectedTime(2, 20, 17, timing));
}

TEST(TimedItemCalculations, TransformedTime)
{
    Timing timing;

    // calculateTransformedTime(currentIteration, iterationDuration, iterationTime)

    // Iteration time is null
    ASSERT_TRUE(isNull(calculateTransformedTime(1, 2, nullValue(), timing)));

    // PlaybackDirectionForwards
    ASSERT_EQ(12, calculateTransformedTime(0, 20, 12, timing));
    ASSERT_EQ(12, calculateTransformedTime(1, 20, 12, timing));

    // PlaybackDirectionForwards with timing function
    timing.timingFunction = StepsTimingFunction::create(4, false /* stepAtStart */);
    ASSERT_EQ(10, calculateTransformedTime(0, 20, 12, timing));
    ASSERT_EQ(10, calculateTransformedTime(1, 20, 12, timing));

    // PlaybackDirectionReverse
    timing.timingFunction = 0;
    timing.direction = Timing::PlaybackDirectionReverse;
    ASSERT_EQ(8, calculateTransformedTime(0, 20, 12, timing));
    ASSERT_EQ(8, calculateTransformedTime(1, 20, 12, timing));

    // PlaybackDirectionReverse with timing function
    timing.timingFunction = StepsTimingFunction::create(4, false /* stepAtStart */);
    ASSERT_EQ(5, calculateTransformedTime(0, 20, 12, timing));
    ASSERT_EQ(5, calculateTransformedTime(1, 20, 12, timing));

    // Timing function when directed time is null.
    ASSERT_TRUE(isNull(calculateTransformedTime(1, 2, nullValue(), timing)));
}

}
