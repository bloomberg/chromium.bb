// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/animation/AnimationStack.h"

#include "core/animation/ActiveAnimations.h"
#include "core/animation/AnimatableDouble.h"
#include "core/animation/AnimationClock.h"
#include "core/animation/DocumentTimeline.h"
#include "core/animation/KeyframeEffectModel.h"
#include <gtest/gtest.h>

using namespace WebCore;

namespace {

class AnimationAnimationStackTest : public ::testing::Test {
protected:
    virtual void SetUp()
    {
        document = Document::create();
        document->animationClock().resetTimeForTesting();
        timeline = DocumentTimeline::create(document.get());
        timeline->setZeroTime(0);
        element = document->createElement("foo", ASSERT_NO_EXCEPTION);
    }

    AnimationPlayer* play(Animation* animation, double startTime)
    {
        AnimationPlayer* player = timeline->createAnimationPlayer(animation);
        player->setStartTime(startTime);
        player->update(AnimationPlayer::UpdateOnDemand);
        return player;
    }

    PassRefPtrWillBeRawPtr<AnimationEffect> makeAnimationEffect(CSSPropertyID id, PassRefPtrWillBeRawPtr<AnimatableValue> value)
    {
        AnimatableValueKeyframeVector keyframes(2);
        keyframes[0] = AnimatableValueKeyframe::create();
        keyframes[0]->setOffset(0.0);
        keyframes[0]->setPropertyValue(id, value.get());
        keyframes[1] = AnimatableValueKeyframe::create();
        keyframes[1]->setOffset(1.0);
        keyframes[1]->setPropertyValue(id, value.get());
        return AnimatableValueKeyframeEffectModel::create(keyframes);
    }

    PassRefPtr<InertAnimation> makeInertAnimation(PassRefPtrWillBeRawPtr<AnimationEffect> effect)
    {
        Timing timing;
        timing.fillMode = Timing::FillModeBoth;
        return InertAnimation::create(effect, timing, false);
    }

    PassRefPtr<Animation> makeAnimation(PassRefPtrWillBeRawPtr<AnimationEffect> effect)
    {
        Timing timing;
        timing.fillMode = Timing::FillModeBoth;
        return Animation::create(element, effect, timing);
    }

    AnimatableValue* interpolationValue(Interpolation* interpolation)
    {
        return toLegacyStyleInterpolation(interpolation)->currentValue().get();
    }

    RefPtr<Document> document;
    RefPtr<DocumentTimeline> timeline;
    RefPtr<Element> element;
};

TEST_F(AnimationAnimationStackTest, ActiveAnimationsSorted)
{
    play(makeAnimation(makeAnimationEffect(CSSPropertyFontSize, AnimatableDouble::create(1))).get(), 10);
    play(makeAnimation(makeAnimationEffect(CSSPropertyFontSize, AnimatableDouble::create(2))).get(), 15);
    play(makeAnimation(makeAnimationEffect(CSSPropertyFontSize, AnimatableDouble::create(3))).get(), 5);
    WillBeHeapHashMap<CSSPropertyID, RefPtrWillBeMember<Interpolation> > result = AnimationStack::activeInterpolations(&element->activeAnimations()->defaultStack(), 0, 0, Animation::DefaultPriority, 0);
    EXPECT_EQ(1u, result.size());
    EXPECT_TRUE(interpolationValue(result.get(CSSPropertyFontSize))->equals(AnimatableDouble::create(2).get()));
}

TEST_F(AnimationAnimationStackTest, NewAnimations)
{
    play(makeAnimation(makeAnimationEffect(CSSPropertyFontSize, AnimatableDouble::create(1))).get(), 15);
    play(makeAnimation(makeAnimationEffect(CSSPropertyZIndex, AnimatableDouble::create(2))).get(), 10);
    Vector<InertAnimation*> newAnimations;
    RefPtr<InertAnimation> inert1 = makeInertAnimation(makeAnimationEffect(CSSPropertyFontSize, AnimatableDouble::create(3)));
    RefPtr<InertAnimation> inert2 = makeInertAnimation(makeAnimationEffect(CSSPropertyZIndex, AnimatableDouble::create(4)));
    newAnimations.append(inert1.get());
    newAnimations.append(inert2.get());
    WillBeHeapHashMap<CSSPropertyID, RefPtrWillBeMember<Interpolation> > result = AnimationStack::activeInterpolations(&element->activeAnimations()->defaultStack(), &newAnimations, 0, Animation::DefaultPriority, 10);
    EXPECT_EQ(2u, result.size());
    EXPECT_TRUE(interpolationValue(result.get(CSSPropertyFontSize))->equals(AnimatableDouble::create(1).get()));
    EXPECT_TRUE(interpolationValue(result.get(CSSPropertyZIndex))->equals(AnimatableDouble::create(4).get()));
}

TEST_F(AnimationAnimationStackTest, CancelledAnimationPlayers)
{
    HashSet<const AnimationPlayer*> cancelledAnimationPlayers;
    cancelledAnimationPlayers.add(play(makeAnimation(makeAnimationEffect(CSSPropertyFontSize, AnimatableDouble::create(1))).get(), 0));
    play(makeAnimation(makeAnimationEffect(CSSPropertyZIndex, AnimatableDouble::create(2))).get(), 0);
    WillBeHeapHashMap<CSSPropertyID, RefPtrWillBeMember<Interpolation> > result = AnimationStack::activeInterpolations(&element->activeAnimations()->defaultStack(), 0, &cancelledAnimationPlayers, Animation::DefaultPriority, 0);
    EXPECT_EQ(1u, result.size());
    EXPECT_TRUE(interpolationValue(result.get(CSSPropertyZIndex))->equals(AnimatableDouble::create(2).get()));
}

}
