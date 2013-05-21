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
#include "core/animation/TimedItem.h"

#include <gtest/gtest.h>

using namespace WebCore;

namespace {

class TestTimedItem : public TimedItem {
public:
    static PassRefPtr<TestTimedItem> create(const Timing& specified)
    {
        return adoptRef(new TestTimedItem(specified));
    }

    void updateInheritedTime(double time)
    {
        TimedItem::updateInheritedTime(time);
    }

    void updateChildrenAndEffects(bool wasActiveOrInEffect) const FINAL OVERRIDE {
    }

private:
    TestTimedItem(const Timing& specified)
        : TimedItem(specified)
    {
    }
};

TEST(TimedItem, Sanity)
{
    Timing timing;
    timing.hasIterationDuration = true;
    timing.iterationDuration = 2;
    RefPtr<TestTimedItem> timedItem = TestTimedItem::create(timing);

    ASSERT_FALSE(timedItem->isScheduled());
    ASSERT_FALSE(timedItem->isActive());
    ASSERT_FALSE(timedItem->isCurrent());
    ASSERT_FALSE(timedItem->isInEffect());
    ASSERT_TRUE(isNull(timedItem->currentIteration()));
    ASSERT_EQ(0, timedItem->startTime());
    ASSERT_TRUE(isNull(timedItem->activeDuration()));
    ASSERT_TRUE(isNull(timedItem->timeFraction()));

    timedItem->updateInheritedTime(0);

    ASSERT_FALSE(timedItem->isScheduled());
    ASSERT_TRUE(timedItem->isActive());
    ASSERT_TRUE(timedItem->isCurrent());
    ASSERT_TRUE(timedItem->isInEffect());
    ASSERT_EQ(0, timedItem->currentIteration());
    ASSERT_EQ(0, timedItem->startTime());
    ASSERT_EQ(2, timedItem->activeDuration());
    ASSERT_EQ(0, timedItem->timeFraction());

    timedItem->updateInheritedTime(1);

    ASSERT_FALSE(timedItem->isScheduled());
    ASSERT_TRUE(timedItem->isActive());
    ASSERT_TRUE(timedItem->isCurrent());
    ASSERT_TRUE(timedItem->isInEffect());
    ASSERT_EQ(0, timedItem->currentIteration());
    ASSERT_EQ(0, timedItem->startTime());
    ASSERT_EQ(2, timedItem->activeDuration());
    ASSERT_EQ(0.5, timedItem->timeFraction());

    timedItem->updateInheritedTime(2);

    ASSERT_FALSE(timedItem->isScheduled());
    ASSERT_FALSE(timedItem->isActive());
    ASSERT_FALSE(timedItem->isCurrent());
    ASSERT_TRUE(timedItem->isInEffect());
    ASSERT_EQ(0, timedItem->currentIteration());
    ASSERT_EQ(0, timedItem->startTime());
    ASSERT_EQ(2, timedItem->activeDuration());
    ASSERT_EQ(1, timedItem->timeFraction());

    timedItem->updateInheritedTime(3);

    ASSERT_FALSE(timedItem->isScheduled());
    ASSERT_FALSE(timedItem->isActive());
    ASSERT_FALSE(timedItem->isCurrent());
    ASSERT_TRUE(timedItem->isInEffect());
    ASSERT_EQ(0, timedItem->currentIteration());
    ASSERT_EQ(0, timedItem->startTime());
    ASSERT_EQ(2, timedItem->activeDuration());
    ASSERT_EQ(1, timedItem->timeFraction());
}

TEST(TimedItem, FillForwards)
{
    Timing timing;
    timing.hasIterationDuration = true;
    timing.iterationDuration = 1;
    RefPtr<TestTimedItem> timedItem = TestTimedItem::create(timing);

    timedItem->updateInheritedTime(-1);
    ASSERT_TRUE(isNull(timedItem->timeFraction()));

    timedItem->updateInheritedTime(2);
    ASSERT_EQ(1, timedItem->timeFraction());
}

TEST(TimedItem, FillBackwards)
{
    Timing timing;
    timing.hasIterationDuration = true;
    timing.iterationDuration = 1;
    timing.fillMode = Timing::FillModeBackwards;
    RefPtr<TestTimedItem> timedItem = TestTimedItem::create(timing);

    timedItem->updateInheritedTime(-1);
    ASSERT_EQ(0, timedItem->timeFraction());

    timedItem->updateInheritedTime(2);
    ASSERT_TRUE(isNull(timedItem->timeFraction()));
}

TEST(TimedItem, FillBoth)
{
    Timing timing;
    timing.hasIterationDuration = true;
    timing.iterationDuration = 1;
    timing.fillMode = Timing::FillModeBoth;
    RefPtr<TestTimedItem> timedItem = TestTimedItem::create(timing);

    timedItem->updateInheritedTime(-1);
    ASSERT_EQ(0, timedItem->timeFraction());

    timedItem->updateInheritedTime(2);
    ASSERT_EQ(1, timedItem->timeFraction());
}

TEST(TimedItem, StartDelay)
{
    Timing timing;
    timing.hasIterationDuration = true;
    timing.iterationDuration = 1;
    timing.startDelay = 0.5;
    RefPtr<TestTimedItem> timedItem = TestTimedItem::create(timing);

    timedItem->updateInheritedTime(0);
    ASSERT_TRUE(isNull(timedItem->timeFraction()));

    timedItem->updateInheritedTime(0.5);
    ASSERT_EQ(0, timedItem->timeFraction());

    timedItem->updateInheritedTime(1.5);
    ASSERT_EQ(1, timedItem->timeFraction());
}

TEST(TimedItem, InfiniteIteration)
{
    Timing timing;
    timing.hasIterationDuration = true;
    timing.iterationDuration = 1;
    timing.iterationCount = std::numeric_limits<double>::infinity();
    RefPtr<TestTimedItem> timedItem = TestTimedItem::create(timing);

    timedItem->updateInheritedTime(-1);
    ASSERT_TRUE(isNull(timedItem->currentIteration()));
    ASSERT_TRUE(isNull(timedItem->timeFraction()));

    ASSERT_EQ(std::numeric_limits<double>::infinity(), timedItem->activeDuration());

    timedItem->updateInheritedTime(0);
    ASSERT_EQ(0, timedItem->currentIteration());
    ASSERT_EQ(0, timedItem->timeFraction());
}

TEST(TimedItem, Iteration)
{
    Timing timing;
    timing.iterationCount = 2;
    timing.hasIterationDuration = true;
    timing.iterationDuration = 2;
    RefPtr<TestTimedItem> timedItem = TestTimedItem::create(timing);

    timedItem->updateInheritedTime(0);
    ASSERT_EQ(0, timedItem->currentIteration());
    ASSERT_EQ(0, timedItem->timeFraction());

    timedItem->updateInheritedTime(1);
    ASSERT_EQ(0, timedItem->currentIteration());
    ASSERT_EQ(0.5, timedItem->timeFraction());

    timedItem->updateInheritedTime(2);
    ASSERT_EQ(1, timedItem->currentIteration());
    ASSERT_EQ(0, timedItem->timeFraction());

    timedItem->updateInheritedTime(2);
    ASSERT_EQ(1, timedItem->currentIteration());
    ASSERT_EQ(0, timedItem->timeFraction());

    timedItem->updateInheritedTime(5);
    ASSERT_EQ(1, timedItem->currentIteration());
    ASSERT_EQ(1, timedItem->timeFraction());
}

TEST(TimedItem, IterationStart)
{
    Timing timing;
    timing.iterationStart = 1.2;
    timing.iterationCount = 2.2;
    timing.hasIterationDuration = true;
    timing.iterationDuration = 1;
    timing.fillMode = Timing::FillModeBoth;
    RefPtr<TestTimedItem> timedItem = TestTimedItem::create(timing);

    timedItem->updateInheritedTime(-1);
    ASSERT_EQ(1, timedItem->currentIteration());
    ASSERT_NEAR(0.2, timedItem->timeFraction(), 0.000000000000001);

    timedItem->updateInheritedTime(0);
    ASSERT_EQ(1, timedItem->currentIteration());
    ASSERT_NEAR(0.2, timedItem->timeFraction(), 0.000000000000001);

    timedItem->updateInheritedTime(10);
    ASSERT_EQ(3, timedItem->currentIteration());
    ASSERT_NEAR(0.4, timedItem->timeFraction(), 0.000000000000001);
}

TEST(TimedItem, IterationAlternate)
{
    Timing timing;
    timing.iterationCount = 10;
    timing.hasIterationDuration = true;
    timing.iterationDuration = 1;
    timing.direction = Timing::PlaybackDirectionAlternate;
    RefPtr<TestTimedItem> timedItem = TestTimedItem::create(timing);

    timedItem->updateInheritedTime(0.75);
    ASSERT_EQ(0, timedItem->currentIteration());
    ASSERT_EQ(0.75, timedItem->timeFraction());

    timedItem->updateInheritedTime(1.75);
    ASSERT_EQ(1, timedItem->currentIteration());
    ASSERT_EQ(0.25, timedItem->timeFraction());

    timedItem->updateInheritedTime(2.75);
    ASSERT_EQ(2, timedItem->currentIteration());
    ASSERT_EQ(0.75, timedItem->timeFraction());
}

TEST(TimedItem, IterationAlternateReverse)
{
    Timing timing;
    timing.iterationCount = 10;
    timing.hasIterationDuration = true;
    timing.iterationDuration = 1;
    timing.direction = Timing::PlaybackDirectionAlternateReverse;
    RefPtr<TestTimedItem> timedItem = TestTimedItem::create(timing);

    timedItem->updateInheritedTime(0.75);
    ASSERT_EQ(0, timedItem->currentIteration());
    ASSERT_EQ(0.25, timedItem->timeFraction());

    timedItem->updateInheritedTime(1.75);
    ASSERT_EQ(1, timedItem->currentIteration());
    ASSERT_EQ(0.75, timedItem->timeFraction());

    timedItem->updateInheritedTime(2.75);
    ASSERT_EQ(2, timedItem->currentIteration());
    ASSERT_EQ(0.25, timedItem->timeFraction());
}

TEST(TimedItem, ZeroDurationSanity)
{
    Timing timing;
    RefPtr<TestTimedItem> timedItem = TestTimedItem::create(timing);

    ASSERT_FALSE(timedItem->isScheduled());
    ASSERT_FALSE(timedItem->isActive());
    ASSERT_FALSE(timedItem->isCurrent());
    ASSERT_FALSE(timedItem->isInEffect());
    ASSERT_TRUE(isNull(timedItem->currentIteration()));
    ASSERT_EQ(0, timedItem->startTime());
    ASSERT_TRUE(isNull(timedItem->activeDuration()));
    ASSERT_TRUE(isNull(timedItem->timeFraction()));

    timedItem->updateInheritedTime(0);

    ASSERT_FALSE(timedItem->isScheduled());
    ASSERT_FALSE(timedItem->isActive());
    ASSERT_FALSE(timedItem->isCurrent());
    ASSERT_TRUE(timedItem->isInEffect());
    ASSERT_EQ(0, timedItem->currentIteration());
    ASSERT_EQ(0, timedItem->startTime());
    ASSERT_EQ(0, timedItem->activeDuration());
    ASSERT_EQ(1, timedItem->timeFraction());

    timedItem->updateInheritedTime(1);

    ASSERT_FALSE(timedItem->isScheduled());
    ASSERT_FALSE(timedItem->isActive());
    ASSERT_FALSE(timedItem->isCurrent());
    ASSERT_TRUE(timedItem->isInEffect());
    ASSERT_EQ(0, timedItem->currentIteration());
    ASSERT_EQ(0, timedItem->startTime());
    ASSERT_EQ(0, timedItem->activeDuration());
    ASSERT_EQ(1, timedItem->timeFraction());
}

TEST(TimedItem, ZeroDurationFillForwards)
{
    Timing timing;
    RefPtr<TestTimedItem> timedItem = TestTimedItem::create(timing);

    timedItem->updateInheritedTime(-1);
    ASSERT_TRUE(isNull(timedItem->timeFraction()));

    timedItem->updateInheritedTime(0);
    ASSERT_EQ(1, timedItem->timeFraction());

    timedItem->updateInheritedTime(1);
    ASSERT_EQ(1, timedItem->timeFraction());
}

TEST(TimedItem, ZeroDurationFillBackwards)
{
    Timing timing;
    timing.fillMode = Timing::FillModeBackwards;
    RefPtr<TestTimedItem> timedItem = TestTimedItem::create(timing);

    timedItem->updateInheritedTime(-1);
    ASSERT_EQ(0, timedItem->timeFraction());

    timedItem->updateInheritedTime(0);
    ASSERT_TRUE(isNull(timedItem->timeFraction()));

    timedItem->updateInheritedTime(1);
    ASSERT_TRUE(isNull(timedItem->timeFraction()));
}

TEST(TimedItem, ZeroDurationFillBoth)
{
    Timing timing;
    timing.fillMode = Timing::FillModeBoth;
    RefPtr<TestTimedItem> timedItem = TestTimedItem::create(timing);

    timedItem->updateInheritedTime(-1);
    ASSERT_EQ(0, timedItem->timeFraction());

    timedItem->updateInheritedTime(0);
    ASSERT_EQ(1, timedItem->timeFraction());

    timedItem->updateInheritedTime(1);
    ASSERT_EQ(1, timedItem->timeFraction());
}

TEST(TimedItem, ZeroDurationStartDelay)
{
    Timing timing;
    timing.startDelay = 0.5;
    RefPtr<TestTimedItem> timedItem = TestTimedItem::create(timing);

    timedItem->updateInheritedTime(0);
    ASSERT_TRUE(isNull(timedItem->timeFraction()));

    timedItem->updateInheritedTime(0.5);
    ASSERT_EQ(1, timedItem->timeFraction());

    timedItem->updateInheritedTime(1.5);
    ASSERT_EQ(1, timedItem->timeFraction());
}

// FIXME: Needs specification work -- ASSERTION FAILED: activeDuration >= 0
TEST(TimedItem, DISABLED_ZeroDurationInfiniteIteration)
{
    Timing timing;
    timing.iterationCount = std::numeric_limits<double>::infinity();
    RefPtr<TestTimedItem> timedItem = TestTimedItem::create(timing);

    timedItem->updateInheritedTime(-1);
    ASSERT_TRUE(isNull(timedItem->currentIteration()));
    ASSERT_TRUE(isNull(timedItem->timeFraction()));
    ASSERT_TRUE(isNull(timedItem->activeDuration()));

    timedItem->updateInheritedTime(0);
    ASSERT_EQ(std::numeric_limits<double>::infinity(), timedItem->currentIteration());
    ASSERT_EQ(1, timedItem->timeFraction());
}

TEST(TimedItem, ZeroDurationIteration)
{
    Timing timing;
    timing.iterationCount = 2;
    RefPtr<TestTimedItem> timedItem = TestTimedItem::create(timing);

    timedItem->updateInheritedTime(-1);
    ASSERT_TRUE(isNull(timedItem->currentIteration()));
    ASSERT_TRUE(isNull(timedItem->timeFraction()));

    timedItem->updateInheritedTime(0);
    ASSERT_EQ(1, timedItem->currentIteration());
    ASSERT_EQ(1, timedItem->timeFraction());

    timedItem->updateInheritedTime(1);
    ASSERT_EQ(1, timedItem->currentIteration());
    ASSERT_EQ(1, timedItem->timeFraction());
}

TEST(TimedItem, ZeroDurationIterationStart)
{
    Timing timing;
    timing.iterationStart = 1.2;
    timing.iterationCount = 2.2;
    timing.fillMode = Timing::FillModeBoth;
    RefPtr<TestTimedItem> timedItem = TestTimedItem::create(timing);

    timedItem->updateInheritedTime(-1);
    ASSERT_EQ(1, timedItem->currentIteration());
    ASSERT_NEAR(0.2, timedItem->timeFraction(), 0.000000000000001);

    timedItem->updateInheritedTime(0);
    ASSERT_EQ(3, timedItem->currentIteration());
    ASSERT_NEAR(0.4, timedItem->timeFraction(), 0.000000000000001);

    timedItem->updateInheritedTime(10);
    ASSERT_EQ(3, timedItem->currentIteration());
    ASSERT_NEAR(0.4, timedItem->timeFraction(), 0.000000000000001);
}

TEST(TimedItem, ZeroDurationIterationAlternate)
{
    Timing timing;
    timing.iterationCount = 2;
    timing.direction = Timing::PlaybackDirectionAlternate;
    RefPtr<TestTimedItem> timedItem = TestTimedItem::create(timing);

    timedItem->updateInheritedTime(-1);
    ASSERT_TRUE(isNull(timedItem->currentIteration()));
    ASSERT_TRUE(isNull(timedItem->timeFraction()));

    timedItem->updateInheritedTime(0);
    ASSERT_EQ(1, timedItem->currentIteration());
    ASSERT_EQ(0, timedItem->timeFraction());

    timedItem->updateInheritedTime(1);
    ASSERT_EQ(1, timedItem->currentIteration());
    ASSERT_EQ(0, timedItem->timeFraction());
}

TEST(TimedItem, ZeroDurationIterationAlternateReverse)
{
    Timing timing;
    timing.iterationCount = 2;
    timing.direction = Timing::PlaybackDirectionAlternateReverse;
    RefPtr<TestTimedItem> timedItem = TestTimedItem::create(timing);

    timedItem->updateInheritedTime(-1);
    ASSERT_TRUE(isNull(timedItem->currentIteration()));
    ASSERT_TRUE(isNull(timedItem->timeFraction()));

    timedItem->updateInheritedTime(0);
    ASSERT_EQ(1, timedItem->currentIteration());
    ASSERT_EQ(1, timedItem->timeFraction());

    timedItem->updateInheritedTime(1);
    ASSERT_EQ(1, timedItem->currentIteration());
    ASSERT_EQ(1, timedItem->timeFraction());
}
}
