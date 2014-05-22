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
#include "core/animation/AnimationSource.h"

#include <gtest/gtest.h>

using namespace WebCore;

namespace {

class TestAnimationSourceEventDelegate : public AnimationSource::EventDelegate {
public:
    virtual void onEventCondition(const AnimationSource* animationSource) OVERRIDE
    {
        m_eventTriggered = true;

    }
    void reset()
    {
        m_eventTriggered = false;
    }
    bool eventTriggered() { return m_eventTriggered; }

private:
    bool m_eventTriggered;
};

class TestAnimationSource : public AnimationSource {
public:
    static PassRefPtrWillBeRawPtr<TestAnimationSource> create(const Timing& specified)
    {
        return adoptRefWillBeNoop(new TestAnimationSource(specified, new TestAnimationSourceEventDelegate()));
    }

    void updateInheritedTime(double time)
    {
        updateInheritedTime(time, TimingUpdateForAnimationFrame);
    }

    void updateInheritedTime(double time, TimingUpdateReason reason)
    {
        m_eventDelegate->reset();
        AnimationSource::updateInheritedTime(time, reason);
    }

    virtual void updateChildrenAndEffects() const OVERRIDE { }
    void willDetach() { }
    TestAnimationSourceEventDelegate* eventDelegate() { return m_eventDelegate; }
    virtual double calculateTimeToEffectChange(bool forwards, double localTime, double timeToNextIteration) const OVERRIDE
    {
        m_localTime = localTime;
        m_timeToNextIteration = timeToNextIteration;
        return -1;
    }
    double takeLocalTime()
    {
        const double result = m_localTime;
        m_localTime = nullValue();
        return result;
    }

    double takeTimeToNextIteration()
    {
        const double result = m_timeToNextIteration;
        m_timeToNextIteration = nullValue();
        return result;
    }

private:
    TestAnimationSource(const Timing& specified, TestAnimationSourceEventDelegate* eventDelegate)
        : AnimationSource(specified, adoptPtr(eventDelegate))
        , m_eventDelegate(eventDelegate)
    {
    }

    TestAnimationSourceEventDelegate* m_eventDelegate;
    mutable double m_localTime;
    mutable double m_timeToNextIteration;
};

TEST(AnimationAnimationSourceTest, Sanity)
{
    Timing timing;
    timing.iterationDuration = 2;
    RefPtrWillBeRawPtr<TestAnimationSource> animationSource = TestAnimationSource::create(timing);

    EXPECT_EQ(0, animationSource->startTime());

    animationSource->updateInheritedTime(0);

    EXPECT_EQ(AnimationSource::PhaseActive, animationSource->phase());
    EXPECT_TRUE(animationSource->isInPlay());
    EXPECT_TRUE(animationSource->isCurrent());
    EXPECT_TRUE(animationSource->isInEffect());
    EXPECT_EQ(0, animationSource->currentIteration());
    EXPECT_EQ(0, animationSource->startTime());
    EXPECT_EQ(2, animationSource->activeDurationInternal());
    EXPECT_EQ(0, animationSource->timeFraction());

    animationSource->updateInheritedTime(1);

    EXPECT_EQ(AnimationSource::PhaseActive, animationSource->phase());
    EXPECT_TRUE(animationSource->isInPlay());
    EXPECT_TRUE(animationSource->isCurrent());
    EXPECT_TRUE(animationSource->isInEffect());
    EXPECT_EQ(0, animationSource->currentIteration());
    EXPECT_EQ(0, animationSource->startTime());
    EXPECT_EQ(2, animationSource->activeDurationInternal());
    EXPECT_EQ(0.5, animationSource->timeFraction());

    animationSource->updateInheritedTime(2);

    EXPECT_EQ(AnimationSource::PhaseAfter, animationSource->phase());
    EXPECT_FALSE(animationSource->isInPlay());
    EXPECT_FALSE(animationSource->isCurrent());
    EXPECT_TRUE(animationSource->isInEffect());
    EXPECT_EQ(0, animationSource->currentIteration());
    EXPECT_EQ(0, animationSource->startTime());
    EXPECT_EQ(2, animationSource->activeDurationInternal());
    EXPECT_EQ(1, animationSource->timeFraction());

    animationSource->updateInheritedTime(3);

    EXPECT_EQ(AnimationSource::PhaseAfter, animationSource->phase());
    EXPECT_FALSE(animationSource->isInPlay());
    EXPECT_FALSE(animationSource->isCurrent());
    EXPECT_TRUE(animationSource->isInEffect());
    EXPECT_EQ(0, animationSource->currentIteration());
    EXPECT_EQ(0, animationSource->startTime());
    EXPECT_EQ(2, animationSource->activeDurationInternal());
    EXPECT_EQ(1, animationSource->timeFraction());
}

TEST(AnimationAnimationSourceTest, FillAuto)
{
    Timing timing;
    timing.iterationDuration = 1;
    RefPtrWillBeRawPtr<TestAnimationSource> animationSource = TestAnimationSource::create(timing);

    animationSource->updateInheritedTime(-1);
    EXPECT_EQ(0, animationSource->timeFraction());

    animationSource->updateInheritedTime(2);
    EXPECT_EQ(1, animationSource->timeFraction());
}

TEST(AnimationAnimationSourceTest, FillForwards)
{
    Timing timing;
    timing.iterationDuration = 1;
    timing.fillMode = Timing::FillModeForwards;
    RefPtrWillBeRawPtr<TestAnimationSource> animationSource = TestAnimationSource::create(timing);

    animationSource->updateInheritedTime(-1);
    EXPECT_TRUE(isNull(animationSource->timeFraction()));

    animationSource->updateInheritedTime(2);
    EXPECT_EQ(1, animationSource->timeFraction());
}

TEST(AnimationAnimationSourceTest, FillBackwards)
{
    Timing timing;
    timing.iterationDuration = 1;
    timing.fillMode = Timing::FillModeBackwards;
    RefPtrWillBeRawPtr<TestAnimationSource> animationSource = TestAnimationSource::create(timing);

    animationSource->updateInheritedTime(-1);
    EXPECT_EQ(0, animationSource->timeFraction());

    animationSource->updateInheritedTime(2);
    EXPECT_TRUE(isNull(animationSource->timeFraction()));
}

TEST(AnimationAnimationSourceTest, FillBoth)
{
    Timing timing;
    timing.iterationDuration = 1;
    timing.fillMode = Timing::FillModeBoth;
    RefPtrWillBeRawPtr<TestAnimationSource> animationSource = TestAnimationSource::create(timing);

    animationSource->updateInheritedTime(-1);
    EXPECT_EQ(0, animationSource->timeFraction());

    animationSource->updateInheritedTime(2);
    EXPECT_EQ(1, animationSource->timeFraction());
}

TEST(AnimationAnimationSourceTest, StartDelay)
{
    Timing timing;
    timing.iterationDuration = 1;
    timing.fillMode = Timing::FillModeForwards;
    timing.startDelay = 0.5;
    RefPtrWillBeRawPtr<TestAnimationSource> animationSource = TestAnimationSource::create(timing);

    animationSource->updateInheritedTime(0);
    EXPECT_TRUE(isNull(animationSource->timeFraction()));

    animationSource->updateInheritedTime(0.5);
    EXPECT_EQ(0, animationSource->timeFraction());

    animationSource->updateInheritedTime(1.5);
    EXPECT_EQ(1, animationSource->timeFraction());
}

TEST(AnimationAnimationSourceTest, ZeroIteration)
{
    Timing timing;
    timing.iterationDuration = 1;
    timing.fillMode = Timing::FillModeForwards;
    timing.iterationCount = 0;
    RefPtrWillBeRawPtr<TestAnimationSource> animationSource = TestAnimationSource::create(timing);

    animationSource->updateInheritedTime(-1);
    EXPECT_EQ(0, animationSource->activeDurationInternal());
    EXPECT_TRUE(isNull(animationSource->currentIteration()));
    EXPECT_TRUE(isNull(animationSource->timeFraction()));

    animationSource->updateInheritedTime(0);
    EXPECT_EQ(0, animationSource->activeDurationInternal());
    EXPECT_EQ(0, animationSource->currentIteration());
    EXPECT_EQ(0, animationSource->timeFraction());
}

TEST(AnimationAnimationSourceTest, InfiniteIteration)
{
    Timing timing;
    timing.iterationDuration = 1;
    timing.fillMode = Timing::FillModeForwards;
    timing.iterationCount = std::numeric_limits<double>::infinity();
    RefPtrWillBeRawPtr<TestAnimationSource> animationSource = TestAnimationSource::create(timing);

    animationSource->updateInheritedTime(-1);
    EXPECT_TRUE(isNull(animationSource->currentIteration()));
    EXPECT_TRUE(isNull(animationSource->timeFraction()));

    EXPECT_EQ(std::numeric_limits<double>::infinity(), animationSource->activeDurationInternal());

    animationSource->updateInheritedTime(0);
    EXPECT_EQ(0, animationSource->currentIteration());
    EXPECT_EQ(0, animationSource->timeFraction());
}

TEST(AnimationAnimationSourceTest, Iteration)
{
    Timing timing;
    timing.iterationCount = 2;
    timing.iterationDuration = 2;
    RefPtrWillBeRawPtr<TestAnimationSource> animationSource = TestAnimationSource::create(timing);

    animationSource->updateInheritedTime(0);
    EXPECT_EQ(0, animationSource->currentIteration());
    EXPECT_EQ(0, animationSource->timeFraction());

    animationSource->updateInheritedTime(1);
    EXPECT_EQ(0, animationSource->currentIteration());
    EXPECT_EQ(0.5, animationSource->timeFraction());

    animationSource->updateInheritedTime(2);
    EXPECT_EQ(1, animationSource->currentIteration());
    EXPECT_EQ(0, animationSource->timeFraction());

    animationSource->updateInheritedTime(2);
    EXPECT_EQ(1, animationSource->currentIteration());
    EXPECT_EQ(0, animationSource->timeFraction());

    animationSource->updateInheritedTime(5);
    EXPECT_EQ(1, animationSource->currentIteration());
    EXPECT_EQ(1, animationSource->timeFraction());
}

TEST(AnimationAnimationSourceTest, IterationStart)
{
    Timing timing;
    timing.iterationStart = 1.2;
    timing.iterationCount = 2.2;
    timing.iterationDuration = 1;
    timing.fillMode = Timing::FillModeBoth;
    RefPtrWillBeRawPtr<TestAnimationSource> animationSource = TestAnimationSource::create(timing);

    animationSource->updateInheritedTime(-1);
    EXPECT_EQ(1, animationSource->currentIteration());
    EXPECT_NEAR(0.2, animationSource->timeFraction(), 0.000000000000001);

    animationSource->updateInheritedTime(0);
    EXPECT_EQ(1, animationSource->currentIteration());
    EXPECT_NEAR(0.2, animationSource->timeFraction(), 0.000000000000001);

    animationSource->updateInheritedTime(10);
    EXPECT_EQ(3, animationSource->currentIteration());
    EXPECT_NEAR(0.4, animationSource->timeFraction(), 0.000000000000001);
}

TEST(AnimationAnimationSourceTest, IterationAlternate)
{
    Timing timing;
    timing.iterationCount = 10;
    timing.iterationDuration = 1;
    timing.direction = Timing::PlaybackDirectionAlternate;
    RefPtrWillBeRawPtr<TestAnimationSource> animationSource = TestAnimationSource::create(timing);

    animationSource->updateInheritedTime(0.75);
    EXPECT_EQ(0, animationSource->currentIteration());
    EXPECT_EQ(0.75, animationSource->timeFraction());

    animationSource->updateInheritedTime(1.75);
    EXPECT_EQ(1, animationSource->currentIteration());
    EXPECT_EQ(0.25, animationSource->timeFraction());

    animationSource->updateInheritedTime(2.75);
    EXPECT_EQ(2, animationSource->currentIteration());
    EXPECT_EQ(0.75, animationSource->timeFraction());
}

TEST(AnimationAnimationSourceTest, IterationAlternateReverse)
{
    Timing timing;
    timing.iterationCount = 10;
    timing.iterationDuration = 1;
    timing.direction = Timing::PlaybackDirectionAlternateReverse;
    RefPtrWillBeRawPtr<TestAnimationSource> animationSource = TestAnimationSource::create(timing);

    animationSource->updateInheritedTime(0.75);
    EXPECT_EQ(0, animationSource->currentIteration());
    EXPECT_EQ(0.25, animationSource->timeFraction());

    animationSource->updateInheritedTime(1.75);
    EXPECT_EQ(1, animationSource->currentIteration());
    EXPECT_EQ(0.75, animationSource->timeFraction());

    animationSource->updateInheritedTime(2.75);
    EXPECT_EQ(2, animationSource->currentIteration());
    EXPECT_EQ(0.25, animationSource->timeFraction());
}

TEST(AnimationAnimationSourceTest, ZeroDurationSanity)
{
    Timing timing;
    RefPtrWillBeRawPtr<TestAnimationSource> animationSource = TestAnimationSource::create(timing);

    EXPECT_EQ(0, animationSource->startTime());

    animationSource->updateInheritedTime(0);

    EXPECT_EQ(AnimationSource::PhaseAfter, animationSource->phase());
    EXPECT_FALSE(animationSource->isInPlay());
    EXPECT_FALSE(animationSource->isCurrent());
    EXPECT_TRUE(animationSource->isInEffect());
    EXPECT_EQ(0, animationSource->currentIteration());
    EXPECT_EQ(0, animationSource->startTime());
    EXPECT_EQ(0, animationSource->activeDurationInternal());
    EXPECT_EQ(1, animationSource->timeFraction());

    animationSource->updateInheritedTime(1);

    EXPECT_EQ(AnimationSource::PhaseAfter, animationSource->phase());
    EXPECT_FALSE(animationSource->isInPlay());
    EXPECT_FALSE(animationSource->isCurrent());
    EXPECT_TRUE(animationSource->isInEffect());
    EXPECT_EQ(0, animationSource->currentIteration());
    EXPECT_EQ(0, animationSource->startTime());
    EXPECT_EQ(0, animationSource->activeDurationInternal());
    EXPECT_EQ(1, animationSource->timeFraction());
}

TEST(AnimationAnimationSourceTest, ZeroDurationFillForwards)
{
    Timing timing;
    timing.fillMode = Timing::FillModeForwards;
    RefPtrWillBeRawPtr<TestAnimationSource> animationSource = TestAnimationSource::create(timing);

    animationSource->updateInheritedTime(-1);
    EXPECT_TRUE(isNull(animationSource->timeFraction()));

    animationSource->updateInheritedTime(0);
    EXPECT_EQ(1, animationSource->timeFraction());

    animationSource->updateInheritedTime(1);
    EXPECT_EQ(1, animationSource->timeFraction());
}

TEST(AnimationAnimationSourceTest, ZeroDurationFillBackwards)
{
    Timing timing;
    timing.fillMode = Timing::FillModeBackwards;
    RefPtrWillBeRawPtr<TestAnimationSource> animationSource = TestAnimationSource::create(timing);

    animationSource->updateInheritedTime(-1);
    EXPECT_EQ(0, animationSource->timeFraction());

    animationSource->updateInheritedTime(0);
    EXPECT_TRUE(isNull(animationSource->timeFraction()));

    animationSource->updateInheritedTime(1);
    EXPECT_TRUE(isNull(animationSource->timeFraction()));
}

TEST(AnimationAnimationSourceTest, ZeroDurationFillBoth)
{
    Timing timing;
    timing.fillMode = Timing::FillModeBoth;
    RefPtrWillBeRawPtr<TestAnimationSource> animationSource = TestAnimationSource::create(timing);

    animationSource->updateInheritedTime(-1);
    EXPECT_EQ(0, animationSource->timeFraction());

    animationSource->updateInheritedTime(0);
    EXPECT_EQ(1, animationSource->timeFraction());

    animationSource->updateInheritedTime(1);
    EXPECT_EQ(1, animationSource->timeFraction());
}

TEST(AnimationAnimationSourceTest, ZeroDurationStartDelay)
{
    Timing timing;
    timing.fillMode = Timing::FillModeForwards;
    timing.startDelay = 0.5;
    RefPtrWillBeRawPtr<TestAnimationSource> animationSource = TestAnimationSource::create(timing);

    animationSource->updateInheritedTime(0);
    EXPECT_TRUE(isNull(animationSource->timeFraction()));

    animationSource->updateInheritedTime(0.5);
    EXPECT_EQ(1, animationSource->timeFraction());

    animationSource->updateInheritedTime(1.5);
    EXPECT_EQ(1, animationSource->timeFraction());
}

TEST(AnimationAnimationSourceTest, ZeroDurationIterationStartAndCount)
{
    Timing timing;
    timing.iterationStart = 0.1;
    timing.iterationCount = 0.2;
    timing.fillMode = Timing::FillModeBoth;
    timing.startDelay = 0.3;
    RefPtrWillBeRawPtr<TestAnimationSource> animationSource = TestAnimationSource::create(timing);

    animationSource->updateInheritedTime(0);
    EXPECT_EQ(0.1, animationSource->timeFraction());

    animationSource->updateInheritedTime(0.3);
    EXPECT_DOUBLE_EQ(0.3, animationSource->timeFraction());

    animationSource->updateInheritedTime(1);
    EXPECT_DOUBLE_EQ(0.3, animationSource->timeFraction());
}

// FIXME: Needs specification work.
TEST(AnimationAnimationSourceTest, ZeroDurationInfiniteIteration)
{
    Timing timing;
    timing.fillMode = Timing::FillModeForwards;
    timing.iterationCount = std::numeric_limits<double>::infinity();
    RefPtrWillBeRawPtr<TestAnimationSource> animationSource = TestAnimationSource::create(timing);

    animationSource->updateInheritedTime(-1);
    EXPECT_EQ(0, animationSource->activeDurationInternal());
    EXPECT_TRUE(isNull(animationSource->currentIteration()));
    EXPECT_TRUE(isNull(animationSource->timeFraction()));

    animationSource->updateInheritedTime(0);
    EXPECT_EQ(0, animationSource->activeDurationInternal());
    EXPECT_EQ(std::numeric_limits<double>::infinity(), animationSource->currentIteration());
    EXPECT_EQ(1, animationSource->timeFraction());
}

TEST(AnimationAnimationSourceTest, ZeroDurationIteration)
{
    Timing timing;
    timing.fillMode = Timing::FillModeForwards;
    timing.iterationCount = 2;
    RefPtrWillBeRawPtr<TestAnimationSource> animationSource = TestAnimationSource::create(timing);

    animationSource->updateInheritedTime(-1);
    EXPECT_TRUE(isNull(animationSource->currentIteration()));
    EXPECT_TRUE(isNull(animationSource->timeFraction()));

    animationSource->updateInheritedTime(0);
    EXPECT_EQ(1, animationSource->currentIteration());
    EXPECT_EQ(1, animationSource->timeFraction());

    animationSource->updateInheritedTime(1);
    EXPECT_EQ(1, animationSource->currentIteration());
    EXPECT_EQ(1, animationSource->timeFraction());
}

TEST(AnimationAnimationSourceTest, ZeroDurationIterationStart)
{
    Timing timing;
    timing.iterationStart = 1.2;
    timing.iterationCount = 2.2;
    timing.fillMode = Timing::FillModeBoth;
    RefPtrWillBeRawPtr<TestAnimationSource> animationSource = TestAnimationSource::create(timing);

    animationSource->updateInheritedTime(-1);
    EXPECT_EQ(1, animationSource->currentIteration());
    EXPECT_NEAR(0.2, animationSource->timeFraction(), 0.000000000000001);

    animationSource->updateInheritedTime(0);
    EXPECT_EQ(3, animationSource->currentIteration());
    EXPECT_NEAR(0.4, animationSource->timeFraction(), 0.000000000000001);

    animationSource->updateInheritedTime(10);
    EXPECT_EQ(3, animationSource->currentIteration());
    EXPECT_NEAR(0.4, animationSource->timeFraction(), 0.000000000000001);
}

TEST(AnimationAnimationSourceTest, ZeroDurationIterationAlternate)
{
    Timing timing;
    timing.fillMode = Timing::FillModeForwards;
    timing.iterationCount = 2;
    timing.direction = Timing::PlaybackDirectionAlternate;
    RefPtrWillBeRawPtr<TestAnimationSource> animationSource = TestAnimationSource::create(timing);

    animationSource->updateInheritedTime(-1);
    EXPECT_TRUE(isNull(animationSource->currentIteration()));
    EXPECT_TRUE(isNull(animationSource->timeFraction()));

    animationSource->updateInheritedTime(0);
    EXPECT_EQ(1, animationSource->currentIteration());
    EXPECT_EQ(0, animationSource->timeFraction());

    animationSource->updateInheritedTime(1);
    EXPECT_EQ(1, animationSource->currentIteration());
    EXPECT_EQ(0, animationSource->timeFraction());
}

TEST(AnimationAnimationSourceTest, ZeroDurationIterationAlternateReverse)
{
    Timing timing;
    timing.fillMode = Timing::FillModeForwards;
    timing.iterationCount = 2;
    timing.direction = Timing::PlaybackDirectionAlternateReverse;
    RefPtrWillBeRawPtr<TestAnimationSource> animationSource = TestAnimationSource::create(timing);

    animationSource->updateInheritedTime(-1);
    EXPECT_TRUE(isNull(animationSource->currentIteration()));
    EXPECT_TRUE(isNull(animationSource->timeFraction()));

    animationSource->updateInheritedTime(0);
    EXPECT_EQ(1, animationSource->currentIteration());
    EXPECT_EQ(1, animationSource->timeFraction());

    animationSource->updateInheritedTime(1);
    EXPECT_EQ(1, animationSource->currentIteration());
    EXPECT_EQ(1, animationSource->timeFraction());
}

TEST(AnimationAnimationSourceTest, InfiniteDurationSanity)
{
    Timing timing;
    timing.iterationDuration = std::numeric_limits<double>::infinity();
    timing.iterationCount = 1;
    RefPtrWillBeRawPtr<TestAnimationSource> animationSource = TestAnimationSource::create(timing);

    EXPECT_EQ(0, animationSource->startTime());

    animationSource->updateInheritedTime(0);

    EXPECT_EQ(std::numeric_limits<double>::infinity(), animationSource->activeDurationInternal());
    EXPECT_EQ(AnimationSource::PhaseActive, animationSource->phase());
    EXPECT_TRUE(animationSource->isInPlay());
    EXPECT_TRUE(animationSource->isCurrent());
    EXPECT_TRUE(animationSource->isInEffect());
    EXPECT_EQ(0, animationSource->currentIteration());
    EXPECT_EQ(0, animationSource->timeFraction());

    animationSource->updateInheritedTime(1);

    EXPECT_EQ(std::numeric_limits<double>::infinity(), animationSource->activeDurationInternal());
    EXPECT_EQ(AnimationSource::PhaseActive, animationSource->phase());
    EXPECT_TRUE(animationSource->isInPlay());
    EXPECT_TRUE(animationSource->isCurrent());
    EXPECT_TRUE(animationSource->isInEffect());
    EXPECT_EQ(0, animationSource->currentIteration());
    EXPECT_EQ(0, animationSource->timeFraction());
}

// FIXME: Needs specification work.
TEST(AnimationAnimationSourceTest, InfiniteDurationZeroIterations)
{
    Timing timing;
    timing.iterationDuration = std::numeric_limits<double>::infinity();
    timing.iterationCount = 0;
    RefPtrWillBeRawPtr<TestAnimationSource> animationSource = TestAnimationSource::create(timing);

    EXPECT_EQ(0, animationSource->startTime());

    animationSource->updateInheritedTime(0);

    EXPECT_EQ(0, animationSource->activeDurationInternal());
    EXPECT_EQ(AnimationSource::PhaseAfter, animationSource->phase());
    EXPECT_FALSE(animationSource->isInPlay());
    EXPECT_FALSE(animationSource->isCurrent());
    EXPECT_TRUE(animationSource->isInEffect());
    EXPECT_EQ(0, animationSource->currentIteration());
    EXPECT_EQ(0, animationSource->timeFraction());

    animationSource->updateInheritedTime(1);

    EXPECT_EQ(AnimationSource::PhaseAfter, animationSource->phase());
    EXPECT_EQ(AnimationSource::PhaseAfter, animationSource->phase());
    EXPECT_FALSE(animationSource->isInPlay());
    EXPECT_FALSE(animationSource->isCurrent());
    EXPECT_TRUE(animationSource->isInEffect());
    EXPECT_EQ(0, animationSource->currentIteration());
    EXPECT_EQ(0, animationSource->timeFraction());
}

TEST(AnimationAnimationSourceTest, InfiniteDurationInfiniteIterations)
{
    Timing timing;
    timing.iterationDuration = std::numeric_limits<double>::infinity();
    timing.iterationCount = std::numeric_limits<double>::infinity();
    RefPtrWillBeRawPtr<TestAnimationSource> animationSource = TestAnimationSource::create(timing);

    EXPECT_EQ(0, animationSource->startTime());

    animationSource->updateInheritedTime(0);

    EXPECT_EQ(std::numeric_limits<double>::infinity(), animationSource->activeDurationInternal());
    EXPECT_EQ(AnimationSource::PhaseActive, animationSource->phase());
    EXPECT_TRUE(animationSource->isInPlay());
    EXPECT_TRUE(animationSource->isCurrent());
    EXPECT_TRUE(animationSource->isInEffect());
    EXPECT_EQ(0, animationSource->currentIteration());
    EXPECT_EQ(0, animationSource->timeFraction());

    animationSource->updateInheritedTime(1);

    EXPECT_EQ(std::numeric_limits<double>::infinity(), animationSource->activeDurationInternal());
    EXPECT_EQ(AnimationSource::PhaseActive, animationSource->phase());
    EXPECT_TRUE(animationSource->isInPlay());
    EXPECT_TRUE(animationSource->isCurrent());
    EXPECT_TRUE(animationSource->isInEffect());
    EXPECT_EQ(0, animationSource->currentIteration());
    EXPECT_EQ(0, animationSource->timeFraction());
}

TEST(AnimationAnimationSourceTest, InfiniteDurationZeroPlaybackRate)
{
    Timing timing;
    timing.iterationDuration = std::numeric_limits<double>::infinity();
    timing.playbackRate = 0;
    RefPtrWillBeRawPtr<TestAnimationSource> animationSource = TestAnimationSource::create(timing);

    EXPECT_EQ(0, animationSource->startTime());

    animationSource->updateInheritedTime(0);

    EXPECT_EQ(std::numeric_limits<double>::infinity(), animationSource->activeDurationInternal());
    EXPECT_EQ(AnimationSource::PhaseActive, animationSource->phase());
    EXPECT_TRUE(animationSource->isInPlay());
    EXPECT_TRUE(animationSource->isCurrent());
    EXPECT_TRUE(animationSource->isInEffect());
    EXPECT_EQ(0, animationSource->currentIteration());
    EXPECT_EQ(0, animationSource->timeFraction());

    animationSource->updateInheritedTime(std::numeric_limits<double>::infinity());

    EXPECT_EQ(std::numeric_limits<double>::infinity(), animationSource->activeDurationInternal());
    EXPECT_EQ(AnimationSource::PhaseAfter, animationSource->phase());
    EXPECT_FALSE(animationSource->isInPlay());
    EXPECT_FALSE(animationSource->isCurrent());
    EXPECT_TRUE(animationSource->isInEffect());
    EXPECT_EQ(0, animationSource->currentIteration());
    EXPECT_EQ(0, animationSource->timeFraction());
}

TEST(AnimationAnimationSourceTest, EndTime)
{
    Timing timing;
    timing.startDelay = 1;
    timing.endDelay = 2;
    timing.iterationDuration = 4;
    timing.iterationCount = 2;
    RefPtrWillBeRawPtr<TestAnimationSource> animationSource = TestAnimationSource::create(timing);
    EXPECT_EQ(11, animationSource->endTimeInternal());
}

TEST(AnimationAnimationSourceTest, Events)
{
    Timing timing;
    timing.iterationDuration = 1;
    timing.fillMode = Timing::FillModeForwards;
    timing.iterationCount = 2;
    timing.startDelay = 1;
    RefPtrWillBeRawPtr<TestAnimationSource> animationSource = TestAnimationSource::create(timing);

    animationSource->updateInheritedTime(0.0, TimingUpdateOnDemand);
    EXPECT_FALSE(animationSource->eventDelegate()->eventTriggered());

    animationSource->updateInheritedTime(0.0, TimingUpdateForAnimationFrame);
    EXPECT_TRUE(animationSource->eventDelegate()->eventTriggered());

    animationSource->updateInheritedTime(1.5, TimingUpdateOnDemand);
    EXPECT_FALSE(animationSource->eventDelegate()->eventTriggered());

    animationSource->updateInheritedTime(1.5, TimingUpdateForAnimationFrame);
    EXPECT_TRUE(animationSource->eventDelegate()->eventTriggered());

}

TEST(AnimationAnimationSourceTest, TimeToEffectChange)
{
    Timing timing;
    timing.iterationDuration = 1;
    timing.fillMode = Timing::FillModeForwards;
    timing.iterationStart = 0.2;
    timing.iterationCount = 2.5;
    timing.startDelay = 1;
    timing.direction = Timing::PlaybackDirectionAlternate;
    RefPtrWillBeRawPtr<TestAnimationSource> animationSource = TestAnimationSource::create(timing);

    animationSource->updateInheritedTime(0);
    EXPECT_EQ(0, animationSource->takeLocalTime());
    EXPECT_TRUE(std::isinf(animationSource->takeTimeToNextIteration()));

    // Normal iteration.
    animationSource->updateInheritedTime(1.75);
    EXPECT_EQ(1.75, animationSource->takeLocalTime());
    EXPECT_NEAR(0.05, animationSource->takeTimeToNextIteration(), 0.000000000000001);

    // Reverse iteration.
    animationSource->updateInheritedTime(2.75);
    EXPECT_EQ(2.75, animationSource->takeLocalTime());
    EXPECT_NEAR(0.05, animationSource->takeTimeToNextIteration(), 0.000000000000001);

    // Item ends before iteration finishes.
    animationSource->updateInheritedTime(3.4);
    EXPECT_EQ(AnimationSource::PhaseActive, animationSource->phase());
    EXPECT_EQ(3.4, animationSource->takeLocalTime());
    EXPECT_TRUE(std::isinf(animationSource->takeTimeToNextIteration()));

    // Item has finished.
    animationSource->updateInheritedTime(3.5);
    EXPECT_EQ(AnimationSource::PhaseAfter, animationSource->phase());
    EXPECT_EQ(3.5, animationSource->takeLocalTime());
    EXPECT_TRUE(std::isinf(animationSource->takeTimeToNextIteration()));
}

}
