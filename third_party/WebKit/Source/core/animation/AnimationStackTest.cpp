// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/animation/AnimationStack.h"

#include "core/animation/AnimationClock.h"
#include "core/animation/AnimationTimeline.h"
#include "core/animation/ElementAnimations.h"
#include "core/animation/KeyframeEffectModel.h"
#include "core/animation/LegacyStyleInterpolation.h"
#include "core/animation/animatable/AnimatableDouble.h"
#include <gtest/gtest.h>

namespace blink {

class AnimationAnimationStackTest : public ::testing::Test {
protected:
    virtual void SetUp()
    {
        document = Document::create();
        document->animationClock().resetTimeForTesting();
        timeline = AnimationTimeline::create(document.get());
        element = document->createElement("foo", ASSERT_NO_EXCEPTION);
    }

    Animation* play(KeyframeEffect* effect, double startTime)
    {
        Animation* animation = timeline->play(effect);
        animation->setStartTime(startTime * 1000);
        animation->update(TimingUpdateOnDemand);
        return animation;
    }

    void updateTimeline(double time)
    {
        document->animationClock().updateTime(time);
        timeline->serviceAnimations(TimingUpdateForAnimationFrame);
    }

    const WillBeHeapVector<OwnPtrWillBeMember<SampledEffect>>& effects()
    {
        return element->ensureElementAnimations().defaultStack().m_effects;
    }

    PassRefPtrWillBeRawPtr<EffectModel> makeEffectModel(CSSPropertyID id, PassRefPtrWillBeRawPtr<AnimatableValue> value)
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

    PassRefPtrWillBeRawPtr<InertEffect> makeInertEffect(PassRefPtrWillBeRawPtr<EffectModel> effect)
    {
        Timing timing;
        timing.fillMode = Timing::FillModeBoth;
        return InertEffect::create(effect, timing, false, 0);
    }

    PassRefPtrWillBeRawPtr<KeyframeEffect> makeKeyframeEffect(PassRefPtrWillBeRawPtr<EffectModel> effect, double duration = 10)
    {
        Timing timing;
        timing.fillMode = Timing::FillModeBoth;
        timing.iterationDuration = duration;
        return KeyframeEffect::create(element.get(), effect, timing);
    }

    AnimatableValue* interpolationValue(const ActiveInterpolationMap& activeInterpolations, CSSPropertyID id)
    {
        Interpolation& interpolation = *activeInterpolations.get(PropertyHandle(id));
        return toLegacyStyleInterpolation(interpolation).currentValue().get();
    }

    RefPtrWillBePersistent<Document> document;
    RefPtrWillBePersistent<AnimationTimeline> timeline;
    RefPtrWillBePersistent<Element> element;
};

TEST_F(AnimationAnimationStackTest, ElementAnimationsSorted)
{
    play(makeKeyframeEffect(makeEffectModel(CSSPropertyFontSize, AnimatableDouble::create(1))).get(), 10);
    play(makeKeyframeEffect(makeEffectModel(CSSPropertyFontSize, AnimatableDouble::create(2))).get(), 15);
    play(makeKeyframeEffect(makeEffectModel(CSSPropertyFontSize, AnimatableDouble::create(3))).get(), 5);
    ActiveInterpolationMap result = AnimationStack::activeInterpolations(&element->elementAnimations()->defaultStack(), 0, 0, KeyframeEffect::DefaultPriority, 0);
    EXPECT_EQ(1u, result.size());
    EXPECT_TRUE(interpolationValue(result, CSSPropertyFontSize)->equals(AnimatableDouble::create(3).get()));
}

TEST_F(AnimationAnimationStackTest, NewAnimations)
{
    play(makeKeyframeEffect(makeEffectModel(CSSPropertyFontSize, AnimatableDouble::create(1))).get(), 15);
    play(makeKeyframeEffect(makeEffectModel(CSSPropertyZIndex, AnimatableDouble::create(2))).get(), 10);
    WillBeHeapVector<RawPtrWillBeMember<InertEffect>> newAnimations;
    RefPtrWillBeRawPtr<InertEffect> inert1 = makeInertEffect(makeEffectModel(CSSPropertyFontSize, AnimatableDouble::create(3)));
    RefPtrWillBeRawPtr<InertEffect> inert2 = makeInertEffect(makeEffectModel(CSSPropertyZIndex, AnimatableDouble::create(4)));
    newAnimations.append(inert1.get());
    newAnimations.append(inert2.get());
    ActiveInterpolationMap result = AnimationStack::activeInterpolations(&element->elementAnimations()->defaultStack(), &newAnimations, 0, KeyframeEffect::DefaultPriority, 10);
    EXPECT_EQ(2u, result.size());
    EXPECT_TRUE(interpolationValue(result, CSSPropertyFontSize)->equals(AnimatableDouble::create(3).get()));
    EXPECT_TRUE(interpolationValue(result, CSSPropertyZIndex)->equals(AnimatableDouble::create(4).get()));
}

TEST_F(AnimationAnimationStackTest, CancelledAnimations)
{
    WillBeHeapHashSet<RawPtrWillBeMember<const Animation>> cancelledAnimations;
    RefPtrWillBeRawPtr<Animation> animation = play(makeKeyframeEffect(makeEffectModel(CSSPropertyFontSize, AnimatableDouble::create(1))).get(), 0);
    cancelledAnimations.add(animation.get());
    play(makeKeyframeEffect(makeEffectModel(CSSPropertyZIndex, AnimatableDouble::create(2))).get(), 0);
    ActiveInterpolationMap result = AnimationStack::activeInterpolations(&element->elementAnimations()->defaultStack(), 0, &cancelledAnimations, KeyframeEffect::DefaultPriority, 0);
    EXPECT_EQ(1u, result.size());
    EXPECT_TRUE(interpolationValue(result, CSSPropertyZIndex)->equals(AnimatableDouble::create(2).get()));
}

TEST_F(AnimationAnimationStackTest, ClearedEffectsRemoved)
{
    RefPtrWillBeRawPtr<Animation> animation = play(makeKeyframeEffect(makeEffectModel(CSSPropertyFontSize, AnimatableDouble::create(1))).get(), 10);
    ActiveInterpolationMap result = AnimationStack::activeInterpolations(&element->elementAnimations()->defaultStack(), 0, 0, KeyframeEffect::DefaultPriority, 0);
    EXPECT_EQ(1u, result.size());
    EXPECT_TRUE(interpolationValue(result, CSSPropertyFontSize)->equals(AnimatableDouble::create(1).get()));

    animation->setSource(0);
    result = AnimationStack::activeInterpolations(&element->elementAnimations()->defaultStack(), 0, 0, KeyframeEffect::DefaultPriority, 0);
    EXPECT_EQ(0u, result.size());
}

}
