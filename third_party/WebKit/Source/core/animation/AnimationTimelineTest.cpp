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
#include "core/animation/AnimationTimeline.h"

#include "core/animation/AnimationClock.h"
#include "core/animation/AnimationEffect.h"
#include "core/animation/KeyframeEffect.h"
#include "core/animation/KeyframeEffectModel.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/QualifiedName.h"
#include "platform/weborigin/KURL.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace blink {

class MockPlatformTiming : public AnimationTimeline::PlatformTiming {
public:

    MOCK_METHOD1(wakeAfter, void(double));
    MOCK_METHOD0(cancelWake, void());
    MOCK_METHOD0(serviceOnNextFrame, void());

    /**
     * AnimationTimelines should do one of the following things after servicing animations:
     *  - cancel the timer and not request to be woken again (expectNoMoreActions)
     *  - cancel the timer and request to be woken on the next frame (expectNextFrameAction)
     *  - cancel the timer and request to be woken at some point in the future (expectDelayedAction)
     */

    void expectNoMoreActions()
    {
        EXPECT_CALL(*this, cancelWake());
    }

    DEFINE_INLINE_TRACE()
    {
        AnimationTimeline::PlatformTiming::trace(visitor);
    }
};

class AnimationAnimationTimelineTest : public ::testing::Test {
protected:
    virtual void SetUp()
    {
        document = Document::create();
        document->animationClock().resetTimeForTesting();
        element = Element::create(QualifiedName::null() , document.get());
        platformTiming = new MockPlatformTiming;
        timeline = AnimationTimeline::create(document.get(), platformTiming);
        ASSERT_EQ(0, timeline->currentTimeInternal());
    }

    virtual void TearDown()
    {
        document.release();
        element.release();
        timeline.release();
#if ENABLE(OILPAN)
        Heap::collectAllGarbage();
#endif
    }

    void updateClockAndService(double time)
    {
        document->animationClock().updateTime(time);
        document->compositorPendingAnimations().update(false);
        timeline->serviceAnimations(TimingUpdateForAnimationFrame);
        timeline->scheduleNextService();
    }

    RefPtrWillBePersistent<Document> document;
    RefPtrWillBePersistent<Element> element;
    Persistent<AnimationTimeline> timeline;
    Timing timing;
    Persistent<MockPlatformTiming> platformTiming;

    void wake()
    {
        timeline->wake();
    }

    double minimumDelay()
    {
        return AnimationTimeline::s_minimumDelay;
    }
};

TEST_F(AnimationAnimationTimelineTest, HasStarted)
{
    timeline = AnimationTimeline::create(document.get());
}

TEST_F(AnimationAnimationTimelineTest, EmptyKeyframeAnimation)
{
    AnimatableValueKeyframeEffectModel* effect = AnimatableValueKeyframeEffectModel::create(AnimatableValueKeyframeVector());
    KeyframeEffect* keyframeEffect = KeyframeEffect::create(element.get(), effect, timing);

    timeline->play(keyframeEffect);

    platformTiming->expectNoMoreActions();
    updateClockAndService(0);
    EXPECT_FLOAT_EQ(0, timeline->currentTimeInternal());
    EXPECT_FALSE(keyframeEffect->isInEffect());

    platformTiming->expectNoMoreActions();
    updateClockAndService(100);
    EXPECT_FLOAT_EQ(100, timeline->currentTimeInternal());
}

TEST_F(AnimationAnimationTimelineTest, EmptyForwardsKeyframeAnimation)
{
    AnimatableValueKeyframeEffectModel* effect = AnimatableValueKeyframeEffectModel::create(AnimatableValueKeyframeVector());
    timing.fillMode = Timing::FillModeForwards;
    KeyframeEffect* keyframeEffect = KeyframeEffect::create(element.get(), effect, timing);

    timeline->play(keyframeEffect);

    platformTiming->expectNoMoreActions();
    updateClockAndService(0);
    EXPECT_FLOAT_EQ(0, timeline->currentTimeInternal());
    EXPECT_TRUE(keyframeEffect->isInEffect());

    platformTiming->expectNoMoreActions();
    updateClockAndService(100);
    EXPECT_FLOAT_EQ(100, timeline->currentTimeInternal());
}

TEST_F(AnimationAnimationTimelineTest, ZeroTime)
{
    timeline = AnimationTimeline::create(document.get());
    bool isNull;

    document->animationClock().updateTime(100);
    EXPECT_EQ(100, timeline->currentTimeInternal());
    EXPECT_EQ(100, timeline->currentTimeInternal(isNull));
    EXPECT_FALSE(isNull);

    document->animationClock().updateTime(200);
    EXPECT_EQ(200, timeline->currentTimeInternal());
    EXPECT_EQ(200, timeline->currentTimeInternal(isNull));
    EXPECT_FALSE(isNull);
}

TEST_F(AnimationAnimationTimelineTest, PlaybackRateNormal)
{
    timeline = AnimationTimeline::create(document.get());
    double zeroTime = timeline->zeroTime();
    bool isNull;

    timeline->setPlaybackRate(1.0);
    EXPECT_EQ(1.0, timeline->playbackRate());
    document->animationClock().updateTime(100);
    EXPECT_EQ(zeroTime, timeline->zeroTime());
    EXPECT_EQ(100, timeline->currentTimeInternal());
    EXPECT_EQ(100, timeline->currentTimeInternal(isNull));
    EXPECT_FALSE(isNull);

    document->animationClock().updateTime(200);
    EXPECT_EQ(zeroTime, timeline->zeroTime());
    EXPECT_EQ(200, timeline->currentTimeInternal());
    EXPECT_EQ(200, timeline->currentTimeInternal(isNull));
    EXPECT_FALSE(isNull);
}

TEST_F(AnimationAnimationTimelineTest, PlaybackRatePause)
{
    timeline = AnimationTimeline::create(document.get());
    bool isNull;

    document->animationClock().updateTime(100);
    EXPECT_EQ(0, timeline->zeroTime());
    EXPECT_EQ(100, timeline->currentTimeInternal());
    EXPECT_EQ(100, timeline->currentTimeInternal(isNull));
    EXPECT_FALSE(isNull);

    timeline->setPlaybackRate(0.0);
    EXPECT_EQ(0.0, timeline->playbackRate());
    document->animationClock().updateTime(200);
    EXPECT_EQ(100, timeline->zeroTime());
    EXPECT_EQ(100, timeline->currentTimeInternal());
    EXPECT_EQ(100, timeline->currentTimeInternal(isNull));

    timeline->setPlaybackRate(1.0);
    EXPECT_EQ(1.0, timeline->playbackRate());
    document->animationClock().updateTime(400);
    EXPECT_EQ(100, timeline->zeroTime());
    EXPECT_EQ(300, timeline->currentTimeInternal());
    EXPECT_EQ(300, timeline->currentTimeInternal(isNull));

    EXPECT_FALSE(isNull);
}

TEST_F(AnimationAnimationTimelineTest, PlaybackRateSlow)
{
    timeline = AnimationTimeline::create(document.get());
    bool isNull;

    document->animationClock().updateTime(100);
    EXPECT_EQ(0, timeline->zeroTime());
    EXPECT_EQ(100, timeline->currentTimeInternal());
    EXPECT_EQ(100, timeline->currentTimeInternal(isNull));
    EXPECT_FALSE(isNull);

    timeline->setPlaybackRate(0.5);
    EXPECT_EQ(0.5, timeline->playbackRate());
    document->animationClock().updateTime(300);
    EXPECT_EQ(-100, timeline->zeroTime());
    EXPECT_EQ(200, timeline->currentTimeInternal());
    EXPECT_EQ(200, timeline->currentTimeInternal(isNull));

    timeline->setPlaybackRate(1.0);
    EXPECT_EQ(1.0, timeline->playbackRate());
    document->animationClock().updateTime(400);
    EXPECT_EQ(100, timeline->zeroTime());
    EXPECT_EQ(300, timeline->currentTimeInternal());
    EXPECT_EQ(300, timeline->currentTimeInternal(isNull));

    EXPECT_FALSE(isNull);
}

TEST_F(AnimationAnimationTimelineTest, PlaybackRateFast)
{
    timeline = AnimationTimeline::create(document.get());
    bool isNull;

    document->animationClock().updateTime(100);
    EXPECT_EQ(0, timeline->zeroTime());
    EXPECT_EQ(100, timeline->currentTimeInternal());
    EXPECT_EQ(100, timeline->currentTimeInternal(isNull));
    EXPECT_FALSE(isNull);

    timeline->setPlaybackRate(2.0);
    EXPECT_EQ(2.0, timeline->playbackRate());
    document->animationClock().updateTime(300);
    EXPECT_EQ(50, timeline->zeroTime());
    EXPECT_EQ(500, timeline->currentTimeInternal());
    EXPECT_EQ(500, timeline->currentTimeInternal(isNull));

    timeline->setPlaybackRate(1.0);
    EXPECT_EQ(1.0, timeline->playbackRate());
    document->animationClock().updateTime(400);
    EXPECT_EQ(-200, timeline->zeroTime());
    EXPECT_EQ(600, timeline->currentTimeInternal());
    EXPECT_EQ(600, timeline->currentTimeInternal(isNull));

    EXPECT_FALSE(isNull);
}

TEST_F(AnimationAnimationTimelineTest, SetCurrentTime)
{
    timeline = AnimationTimeline::create(document.get());
    double zeroTime = timeline->zeroTime();

    document->animationClock().updateTime(100);
    EXPECT_EQ(zeroTime, timeline->zeroTime());
    EXPECT_EQ(100, timeline->currentTimeInternal());

    timeline->setCurrentTimeInternal(0);
    EXPECT_EQ(0, timeline->currentTimeInternal());
    EXPECT_EQ(zeroTime + 100, timeline->zeroTime());

    timeline->setCurrentTimeInternal(100);
    EXPECT_EQ(100, timeline->currentTimeInternal());
    EXPECT_EQ(zeroTime, timeline->zeroTime());

    timeline->setCurrentTimeInternal(200);
    EXPECT_EQ(200, timeline->currentTimeInternal());
    EXPECT_EQ(zeroTime - 100, timeline->zeroTime());

    document->animationClock().updateTime(200);
    EXPECT_EQ(300, timeline->currentTimeInternal());
    EXPECT_EQ(zeroTime - 100, timeline->zeroTime());

    timeline->setCurrentTimeInternal(0);
    EXPECT_EQ(0, timeline->currentTimeInternal());
    EXPECT_EQ(zeroTime + 200, timeline->zeroTime());

    timeline->setCurrentTimeInternal(100);
    EXPECT_EQ(100, timeline->currentTimeInternal());
    EXPECT_EQ(zeroTime + 100, timeline->zeroTime());

    timeline->setCurrentTimeInternal(200);
    EXPECT_EQ(200, timeline->currentTimeInternal());
    EXPECT_EQ(zeroTime, timeline->zeroTime());

    timeline->setCurrentTime(0);
    EXPECT_EQ(0, timeline->currentTime());
    EXPECT_EQ(zeroTime + 200, timeline->zeroTime());

    timeline->setCurrentTime(1000);
    EXPECT_EQ(1000, timeline->currentTime());
    EXPECT_EQ(zeroTime + 199, timeline->zeroTime());

    timeline->setCurrentTime(2000);
    EXPECT_EQ(2000, timeline->currentTime());
    EXPECT_EQ(zeroTime + 198, timeline->zeroTime());
}

TEST_F(AnimationAnimationTimelineTest, PauseForTesting)
{
    float seekTime = 1;
    timing.fillMode = Timing::FillModeForwards;
    KeyframeEffect* anim1 = KeyframeEffect::create(element.get(), AnimatableValueKeyframeEffectModel::create(AnimatableValueKeyframeVector()), timing);
    KeyframeEffect* anim2  = KeyframeEffect::create(element.get(), AnimatableValueKeyframeEffectModel::create(AnimatableValueKeyframeVector()), timing);
    Animation* animation1 = timeline->play(anim1);
    Animation* animation2 = timeline->play(anim2);
    timeline->pauseAnimationsForTesting(seekTime);

    EXPECT_FLOAT_EQ(seekTime, animation1->currentTime() / 1000.0);
    EXPECT_FLOAT_EQ(seekTime, animation2->currentTime() / 1000.0);
}

TEST_F(AnimationAnimationTimelineTest, DelayBeforeAnimationStart)
{
    timing.iterationDuration = 2;
    timing.startDelay = 5;

    KeyframeEffect* keyframeEffect = KeyframeEffect::create(element.get(), nullptr, timing);

    timeline->play(keyframeEffect);

    // TODO: Put the animation startTime in the future when we add the capability to change animation startTime
    EXPECT_CALL(*platformTiming, wakeAfter(timing.startDelay - minimumDelay()));
    updateClockAndService(0);

    EXPECT_CALL(*platformTiming, wakeAfter(timing.startDelay - minimumDelay() - 1.5));
    updateClockAndService(1.5);

    EXPECT_CALL(*platformTiming, serviceOnNextFrame());
    wake();

    EXPECT_CALL(*platformTiming, serviceOnNextFrame());
    updateClockAndService(4.98);
}

TEST_F(AnimationAnimationTimelineTest, PlayAfterDocumentDeref)
{
    timing.iterationDuration = 2;
    timing.startDelay = 5;

    timeline = &document->timeline();
    element = nullptr;
    document = nullptr;

    KeyframeEffect* keyframeEffect = KeyframeEffect::create(0, nullptr, timing);
    // Test passes if this does not crash.
    timeline->play(keyframeEffect);
}

TEST_F(AnimationAnimationTimelineTest, UseAnimationAfterTimelineDeref)
{
    Animation* animation = timeline->play(0);
    timeline.clear();
    // Test passes if this does not crash.
    animation->setStartTime(0);
}

}
