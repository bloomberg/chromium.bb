// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/EffectStack.h"

#include <memory>
#include "core/animation/AnimationClock.h"
#include "core/animation/DocumentTimeline.h"
#include "core/animation/ElementAnimations.h"
#include "core/animation/KeyframeEffectModel.h"
#include "core/animation/LegacyStyleInterpolation.h"
#include "core/animation/PendingAnimations.h"
#include "core/animation/animatable/AnimatableDouble.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class AnimationEffectStackTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    page_holder = DummyPageHolder::Create();
    document = &page_holder->GetDocument();
    document->GetAnimationClock().ResetTimeForTesting();
    timeline = DocumentTimeline::Create(document.Get());
    element = document->createElement("foo");
  }

  Animation* Play(KeyframeEffect* effect, double start_time) {
    Animation* animation = timeline->Play(effect);
    animation->setStartTime(start_time * 1000, false);
    animation->Update(kTimingUpdateOnDemand);
    return animation;
  }

  void UpdateTimeline(double time) {
    document->GetAnimationClock().UpdateTime(document->Timeline().ZeroTime() +
                                             time);
    timeline->ServiceAnimations(kTimingUpdateForAnimationFrame);
  }

  size_t SampledEffectCount() {
    return element->EnsureElementAnimations()
        .GetEffectStack()
        .sampled_effects_.size();
  }

  EffectModel* MakeEffectModel(CSSPropertyID id,
                               RefPtr<AnimatableValue> value) {
    AnimatableValueKeyframeVector keyframes(2);
    keyframes[0] = AnimatableValueKeyframe::Create();
    keyframes[0]->SetOffset(0.0);
    keyframes[0]->SetPropertyValue(id, value.Get());
    keyframes[1] = AnimatableValueKeyframe::Create();
    keyframes[1]->SetOffset(1.0);
    keyframes[1]->SetPropertyValue(id, value.Get());
    return AnimatableValueKeyframeEffectModel::Create(keyframes);
  }

  InertEffect* MakeInertEffect(EffectModel* effect) {
    Timing timing;
    timing.fill_mode = Timing::FillMode::BOTH;
    return InertEffect::Create(effect, timing, false, 0);
  }

  KeyframeEffect* MakeKeyframeEffect(EffectModel* effect,
                                     double duration = 10) {
    Timing timing;
    timing.fill_mode = Timing::FillMode::BOTH;
    timing.iteration_duration = duration;
    return KeyframeEffect::Create(element.Get(), effect, timing);
  }

  double GetDoubleValue(const ActiveInterpolationsMap& active_interpolations,
                        CSSPropertyID id) {
    Interpolation& interpolation =
        *active_interpolations.at(PropertyHandle(id)).at(0);
    AnimatableValue* animatable_value =
        ToLegacyStyleInterpolation(interpolation).CurrentValue().Get();
    return ToAnimatableDouble(animatable_value)->ToDouble();
  }

  std::unique_ptr<DummyPageHolder> page_holder;
  Persistent<Document> document;
  Persistent<DocumentTimeline> timeline;
  Persistent<Element> element;
};

TEST_F(AnimationEffectStackTest, ElementAnimationsSorted) {
  Play(MakeKeyframeEffect(
           MakeEffectModel(CSSPropertyFontSize, AnimatableDouble::Create(1))),
       10);
  Play(MakeKeyframeEffect(
           MakeEffectModel(CSSPropertyFontSize, AnimatableDouble::Create(2))),
       15);
  Play(MakeKeyframeEffect(
           MakeEffectModel(CSSPropertyFontSize, AnimatableDouble::Create(3))),
       5);
  ActiveInterpolationsMap result = EffectStack::ActiveInterpolations(
      &element->GetElementAnimations()->GetEffectStack(), 0, 0,
      KeyframeEffectReadOnly::kDefaultPriority);
  EXPECT_EQ(1u, result.size());
  EXPECT_EQ(GetDoubleValue(result, CSSPropertyFontSize), 3);
}

TEST_F(AnimationEffectStackTest, NewAnimations) {
  Play(MakeKeyframeEffect(
           MakeEffectModel(CSSPropertyFontSize, AnimatableDouble::Create(1))),
       15);
  Play(MakeKeyframeEffect(
           MakeEffectModel(CSSPropertyZIndex, AnimatableDouble::Create(2))),
       10);
  HeapVector<Member<const InertEffect>> new_animations;
  InertEffect* inert1 = MakeInertEffect(
      MakeEffectModel(CSSPropertyFontSize, AnimatableDouble::Create(3)));
  InertEffect* inert2 = MakeInertEffect(
      MakeEffectModel(CSSPropertyZIndex, AnimatableDouble::Create(4)));
  new_animations.push_back(inert1);
  new_animations.push_back(inert2);
  ActiveInterpolationsMap result = EffectStack::ActiveInterpolations(
      &element->GetElementAnimations()->GetEffectStack(), &new_animations, 0,
      KeyframeEffectReadOnly::kDefaultPriority);
  EXPECT_EQ(2u, result.size());
  EXPECT_EQ(GetDoubleValue(result, CSSPropertyFontSize), 3);
  EXPECT_EQ(GetDoubleValue(result, CSSPropertyZIndex), 4);
}

TEST_F(AnimationEffectStackTest, CancelledAnimations) {
  HeapHashSet<Member<const Animation>> cancelled_animations;
  Animation* animation =
      Play(MakeKeyframeEffect(MakeEffectModel(CSSPropertyFontSize,
                                              AnimatableDouble::Create(1))),
           0);
  cancelled_animations.insert(animation);
  Play(MakeKeyframeEffect(
           MakeEffectModel(CSSPropertyZIndex, AnimatableDouble::Create(2))),
       0);
  ActiveInterpolationsMap result = EffectStack::ActiveInterpolations(
      &element->GetElementAnimations()->GetEffectStack(), 0,
      &cancelled_animations, KeyframeEffectReadOnly::kDefaultPriority);
  EXPECT_EQ(1u, result.size());
  EXPECT_EQ(GetDoubleValue(result, CSSPropertyZIndex), 2);
}

TEST_F(AnimationEffectStackTest, ClearedEffectsRemoved) {
  Animation* animation =
      Play(MakeKeyframeEffect(MakeEffectModel(CSSPropertyFontSize,
                                              AnimatableDouble::Create(1))),
           10);
  ActiveInterpolationsMap result = EffectStack::ActiveInterpolations(
      &element->GetElementAnimations()->GetEffectStack(), 0, 0,
      KeyframeEffectReadOnly::kDefaultPriority);
  EXPECT_EQ(1u, result.size());
  EXPECT_EQ(GetDoubleValue(result, CSSPropertyFontSize), 1);

  animation->setEffect(0);
  result = EffectStack::ActiveInterpolations(
      &element->GetElementAnimations()->GetEffectStack(), 0, 0,
      KeyframeEffectReadOnly::kDefaultPriority);
  EXPECT_EQ(0u, result.size());
}

TEST_F(AnimationEffectStackTest, ForwardsFillDiscarding) {
  Play(MakeKeyframeEffect(
           MakeEffectModel(CSSPropertyFontSize, AnimatableDouble::Create(1))),
       2);
  Play(MakeKeyframeEffect(
           MakeEffectModel(CSSPropertyFontSize, AnimatableDouble::Create(2))),
       6);
  Play(MakeKeyframeEffect(
           MakeEffectModel(CSSPropertyFontSize, AnimatableDouble::Create(3))),
       4);
  document->GetPendingAnimations().Update(Optional<CompositorElementIdSet>());
  ActiveInterpolationsMap interpolations;

  UpdateTimeline(11);
  ThreadState::Current()->CollectAllGarbage();
  interpolations = EffectStack::ActiveInterpolations(
      &element->GetElementAnimations()->GetEffectStack(), nullptr, nullptr,
      KeyframeEffectReadOnly::kDefaultPriority);
  EXPECT_EQ(1u, interpolations.size());
  EXPECT_EQ(GetDoubleValue(interpolations, CSSPropertyFontSize), 3);
  EXPECT_EQ(3u, SampledEffectCount());

  UpdateTimeline(13);
  ThreadState::Current()->CollectAllGarbage();
  interpolations = EffectStack::ActiveInterpolations(
      &element->GetElementAnimations()->GetEffectStack(), nullptr, nullptr,
      KeyframeEffectReadOnly::kDefaultPriority);
  EXPECT_EQ(1u, interpolations.size());
  EXPECT_EQ(GetDoubleValue(interpolations, CSSPropertyFontSize), 3);
  EXPECT_EQ(3u, SampledEffectCount());

  UpdateTimeline(15);
  ThreadState::Current()->CollectAllGarbage();
  interpolations = EffectStack::ActiveInterpolations(
      &element->GetElementAnimations()->GetEffectStack(), nullptr, nullptr,
      KeyframeEffectReadOnly::kDefaultPriority);
  EXPECT_EQ(1u, interpolations.size());
  EXPECT_EQ(GetDoubleValue(interpolations, CSSPropertyFontSize), 3);
  EXPECT_EQ(2u, SampledEffectCount());

  UpdateTimeline(17);
  ThreadState::Current()->CollectAllGarbage();
  interpolations = EffectStack::ActiveInterpolations(
      &element->GetElementAnimations()->GetEffectStack(), nullptr, nullptr,
      KeyframeEffectReadOnly::kDefaultPriority);
  EXPECT_EQ(1u, interpolations.size());
  EXPECT_EQ(GetDoubleValue(interpolations, CSSPropertyFontSize), 3);
  EXPECT_EQ(1u, SampledEffectCount());
}

}  // namespace blink
