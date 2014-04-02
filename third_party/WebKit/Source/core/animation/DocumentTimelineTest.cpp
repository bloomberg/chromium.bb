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
#include "core/animation/DocumentTimeline.h"

#include "core/animation/Animation.h"
#include "core/animation/AnimationClock.h"
#include "core/animation/KeyframeEffectModel.h"
#include "core/animation/TimedItem.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/QualifiedName.h"
#include "platform/weborigin/KURL.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace WebCore {

class MockPlatformTiming : public DocumentTimeline::PlatformTiming {
public:

    MOCK_METHOD1(wakeAfter, void(double));
    MOCK_METHOD0(cancelWake, void());
    MOCK_METHOD0(serviceOnNextFrame, void());

    /**
     * DocumentTimelines should do one of the following things after servicing animations:
     *  - cancel the timer and not request to be woken again (expectNoMoreActions)
     *  - cancel the timer and request to be woken on the next frame (expectNextFrameAction)
     *  - cancel the timer and request to be woken at some point in the future (expectDelayedAction)
     */

    void expectNoMoreActions()
    {
        EXPECT_CALL(*this, cancelWake());
    }

    void expectNextFrameAction()
    {
        ::testing::Sequence sequence;
        EXPECT_CALL(*this, cancelWake()).InSequence(sequence);
        EXPECT_CALL(*this, serviceOnNextFrame()).InSequence(sequence);
    }

    void expectDelayedAction(double when)
    {
        ::testing::Sequence sequence;
        EXPECT_CALL(*this, cancelWake()).InSequence(sequence);
        EXPECT_CALL(*this, wakeAfter(when)).InSequence(sequence);
    }
};

class AnimationDocumentTimelineTest : public ::testing::Test {
protected:
    virtual void SetUp()
    {
        document = Document::create();
        document->animationClock().resetTimeForTesting();
        element = Element::create(nullQName() , document.get());
        platformTiming = new MockPlatformTiming;
        timeline = DocumentTimeline::create(document.get(), adoptPtr(platformTiming));
        timeline->setZeroTime(0);
        ASSERT_EQ(0, timeline->currentTime());
    }

    virtual void TearDown()
    {
        timeline.release();
        document.release();
        element.release();
    }

    void updateClockAndService(double time)
    {
        document->animationClock().updateTime(time);
        timeline->serviceAnimations();
    }

    RefPtr<Document> document;
    RefPtr<Element> element;
    RefPtr<DocumentTimeline> timeline;
    Timing timing;
    MockPlatformTiming* platformTiming;

    void wake()
    {
        timeline->wake();
    }

    double minimumDelay()
    {
        return DocumentTimeline::s_minimumDelay;
    }
};

TEST_F(AnimationDocumentTimelineTest, HasStarted)
{
    timeline = DocumentTimeline::create(document.get());
    EXPECT_FALSE(timeline->hasStarted());
    timeline->setZeroTime(0);
    EXPECT_TRUE(timeline->hasStarted());
}

TEST_F(AnimationDocumentTimelineTest, EmptyKeyframeAnimation)
{
    RefPtrWillBeRawPtr<AnimatableValueKeyframeEffectModel> effect = AnimatableValueKeyframeEffectModel::create(AnimatableValueKeyframeVector());
    RefPtr<Animation> anim = Animation::create(element.get(), effect, timing);

    timeline->play(anim.get());

    platformTiming->expectNoMoreActions();
    updateClockAndService(0);
    EXPECT_FLOAT_EQ(0, timeline->currentTime());
    EXPECT_FALSE(anim->isInEffect());

    platformTiming->expectNoMoreActions();
    updateClockAndService(100);
    EXPECT_FLOAT_EQ(100, timeline->currentTime());
}

TEST_F(AnimationDocumentTimelineTest, EmptyForwardsKeyframeAnimation)
{
    RefPtrWillBeRawPtr<AnimatableValueKeyframeEffectModel> effect = AnimatableValueKeyframeEffectModel::create(AnimatableValueKeyframeVector());
    timing.fillMode = Timing::FillModeForwards;
    RefPtr<Animation> anim = Animation::create(element.get(), effect, timing);

    timeline->play(anim.get());

    platformTiming->expectNoMoreActions();
    updateClockAndService(0);
    EXPECT_FLOAT_EQ(0, timeline->currentTime());
    EXPECT_TRUE(anim->isInEffect());
    EXPECT_TRUE(anim->activeInterpolations().isEmpty());

    platformTiming->expectNoMoreActions();
    updateClockAndService(100);
    EXPECT_FLOAT_EQ(100, timeline->currentTime());
}

TEST_F(AnimationDocumentTimelineTest, ZeroTime)
{
    timeline = DocumentTimeline::create(document.get());
    bool isNull;

    document->animationClock().updateTime(100);
    EXPECT_TRUE(std::isnan(timeline->currentTime()));
    EXPECT_TRUE(std::isnan(timeline->currentTime(isNull)));
    EXPECT_TRUE(isNull);

    document->animationClock().updateTime(200);
    EXPECT_TRUE(std::isnan(timeline->currentTime()));

    timeline->setZeroTime(300);
    document->animationClock().updateTime(300);
    EXPECT_EQ(0, timeline->currentTime());
    EXPECT_EQ(0, timeline->currentTime(isNull));
    EXPECT_FALSE(isNull);

    document->animationClock().updateTime(400);
    EXPECT_EQ(100, timeline->currentTime());
}

TEST_F(AnimationDocumentTimelineTest, PauseForTesting)
{
    float seekTime = 1;
    timing.fillMode = Timing::FillModeForwards;
    RefPtr<Animation> anim1 = Animation::create(element.get(), AnimatableValueKeyframeEffectModel::create(AnimatableValueKeyframeVector()), timing);
    RefPtr<Animation> anim2  = Animation::create(element.get(), AnimatableValueKeyframeEffectModel::create(AnimatableValueKeyframeVector()), timing);
    AnimationPlayer* player1 = timeline->play(anim1.get());
    AnimationPlayer* player2 = timeline->play(anim2.get());
    timeline->pauseAnimationsForTesting(seekTime);

    EXPECT_FLOAT_EQ(seekTime, player1->currentTime());
    EXPECT_FLOAT_EQ(seekTime, player2->currentTime());
}

TEST_F(AnimationDocumentTimelineTest, NumberOfActiveAnimations)
{
    Timing timingForwardFill;
    timingForwardFill.iterationDuration = 2;
    timingForwardFill.fillMode = Timing::FillModeForwards;

    Timing timingNoFill;
    timingNoFill.iterationDuration = 2;
    timingNoFill.fillMode = Timing::FillModeNone;

    Timing timingBackwardFillDelay;
    timingBackwardFillDelay.iterationDuration = 1;
    timingBackwardFillDelay.fillMode = Timing::FillModeBackwards;
    timingBackwardFillDelay.startDelay = 1;

    Timing timingNoFillDelay;
    timingNoFillDelay.iterationDuration = 1;
    timingNoFillDelay.fillMode = Timing::FillModeNone;
    timingNoFillDelay.startDelay = 1;

    Timing timingAutoFill;
    timingAutoFill.iterationDuration = 2;

    RefPtr<Animation> anim1 = Animation::create(element.get(), AnimatableValueKeyframeEffectModel::create(AnimatableValueKeyframeVector()), timingForwardFill);
    RefPtr<Animation> anim2 = Animation::create(element.get(), AnimatableValueKeyframeEffectModel::create(AnimatableValueKeyframeVector()), timingNoFill);
    RefPtr<Animation> anim3 = Animation::create(element.get(), AnimatableValueKeyframeEffectModel::create(AnimatableValueKeyframeVector()), timingBackwardFillDelay);
    RefPtr<Animation> anim4 = Animation::create(element.get(), AnimatableValueKeyframeEffectModel::create(AnimatableValueKeyframeVector()), timingNoFillDelay);
    RefPtr<Animation> anim5 = Animation::create(element.get(), AnimatableValueKeyframeEffectModel::create(AnimatableValueKeyframeVector()), timingAutoFill);

    timeline->play(anim1.get());
    timeline->play(anim2.get());
    timeline->play(anim3.get());
    timeline->play(anim4.get());
    timeline->play(anim5.get());

    platformTiming->expectNextFrameAction();
    updateClockAndService(0);
    EXPECT_EQ(5U, timeline->numberOfActiveAnimationsForTesting());
    platformTiming->expectNextFrameAction();
    updateClockAndService(0.5);
    EXPECT_EQ(5U, timeline->numberOfActiveAnimationsForTesting());
    platformTiming->expectNextFrameAction();
    updateClockAndService(1.5);
    EXPECT_EQ(5U, timeline->numberOfActiveAnimationsForTesting());
    platformTiming->expectNoMoreActions();
    updateClockAndService(3);
    EXPECT_EQ(1U, timeline->numberOfActiveAnimationsForTesting());
}

TEST_F(AnimationDocumentTimelineTest, DelayBeforeAnimationStart)
{
    timing.iterationDuration = 2;
    timing.startDelay = 5;

    RefPtr<Animation> anim = Animation::create(element.get(), nullptr, timing);

    timeline->play(anim.get());

    // TODO: Put the player startTime in the future when we add the capability to change player startTime
    platformTiming->expectDelayedAction(timing.startDelay - minimumDelay());
    updateClockAndService(0);

    platformTiming->expectDelayedAction(timing.startDelay - minimumDelay() - 1.5);
    updateClockAndService(1.5);

    EXPECT_CALL(*platformTiming, serviceOnNextFrame());
    wake();

    platformTiming->expectNextFrameAction();
    updateClockAndService(4.98);
}

TEST_F(AnimationDocumentTimelineTest, PlayAfterDocumentDeref)
{
    timing.iterationDuration = 2;
    timing.startDelay = 5;

    timeline = &document->timeline();
    element = nullptr;
    document = nullptr;

    RefPtr<Animation> anim = Animation::create(nullptr, nullptr, timing);
    // Test passes if this does not crash.
    timeline->play(anim.get());
}

TEST_F(AnimationDocumentTimelineTest, UseAnimationPlayerAfterTimelineDeref)
{
    RefPtr<AnimationPlayer> player = timeline->createAnimationPlayer(0);
    timeline.clear();
    // Test passes if this does not crash.
    player->setStartTime(0);
}

}
