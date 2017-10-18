// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/EffectStack.h"

#include <memory>
#include "core/animation/AnimationClock.h"
#include "core/animation/AnimationTestHelper.h"
#include "core/animation/DocumentTimeline.h"
#include "core/animation/ElementAnimations.h"
#include "core/animation/InvalidatableInterpolation.h"
#include "core/animation/KeyframeEffectModel.h"
#include "core/animation/PendingAnimations.h"
#include "core/animation/StringKeyframe.h"
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

  EffectModel* MakeEffectModel(CSSPropertyID id, const String& value) {
    StringKeyframeVector keyframes(2);
    keyframes[0] = StringKeyframe::Create();
    keyframes[0]->SetOffset(0.0);
    keyframes[0]->SetCSSPropertyValue(id, value, nullptr);
    keyframes[1] = StringKeyframe::Create();
    keyframes[1]->SetOffset(1.0);
    keyframes[1]->SetCSSPropertyValue(id, value, nullptr);
    return StringKeyframeEffectModel::Create(keyframes);
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

  double GetFontSizeValue(
      const ActiveInterpolationsMap& active_interpolations) {
    const ActiveInterpolations& interpolations =
        active_interpolations.at(PropertyHandle(CSSPropertyFontSize));
    EnsureInterpolatedValueCached(interpolations, *document, element);

    const TypedInterpolationValue* typed_value =
        ToInvalidatableInterpolation(*interpolations.at(0))
            .GetCachedValueForTesting();
    // font-size is stored as an array of length values; here we assume pixels.
    EXPECT_TRUE(typed_value->GetInterpolableValue().IsList());
    const InterpolableList* list =
        ToInterpolableList(&typed_value->GetInterpolableValue());
    return ToInterpolableNumber(list->Get(0))->Value();
  }

  double GetZIndexValue(const ActiveInterpolationsMap& active_interpolations) {
    const ActiveInterpolations& interpolations =
        active_interpolations.at(PropertyHandle(CSSPropertyZIndex));
    EnsureInterpolatedValueCached(interpolations, *document, element);

    const TypedInterpolationValue* typed_value =
        ToInvalidatableInterpolation(*interpolations.at(0))
            .GetCachedValueForTesting();
    // z-index is stored as a straight number value.
    EXPECT_TRUE(typed_value->GetInterpolableValue().IsNumber());
    return ToInterpolableNumber(&typed_value->GetInterpolableValue())->Value();
  }

  std::unique_ptr<DummyPageHolder> page_holder;
  Persistent<Document> document;
  Persistent<DocumentTimeline> timeline;
  Persistent<Element> element;
};

TEST_F(AnimationEffectStackTest, ElementAnimationsSorted) {
  Play(MakeKeyframeEffect(MakeEffectModel(CSSPropertyFontSize, "1px")), 10);
  Play(MakeKeyframeEffect(MakeEffectModel(CSSPropertyFontSize, "2px")), 15);
  Play(MakeKeyframeEffect(MakeEffectModel(CSSPropertyFontSize, "3px")), 5);
  ActiveInterpolationsMap result = EffectStack::ActiveInterpolations(
      &element->GetElementAnimations()->GetEffectStack(), 0, 0,
      KeyframeEffectReadOnly::kDefaultPriority);
  EXPECT_EQ(1u, result.size());
  EXPECT_EQ(GetFontSizeValue(result), 3);
}

TEST_F(AnimationEffectStackTest, NewAnimations) {
  Play(MakeKeyframeEffect(MakeEffectModel(CSSPropertyFontSize, "1px")), 15);
  Play(MakeKeyframeEffect(MakeEffectModel(CSSPropertyZIndex, "2")), 10);
  HeapVector<Member<const InertEffect>> new_animations;
  InertEffect* inert1 =
      MakeInertEffect(MakeEffectModel(CSSPropertyFontSize, "3px"));
  InertEffect* inert2 =
      MakeInertEffect(MakeEffectModel(CSSPropertyZIndex, "4"));
  new_animations.push_back(inert1);
  new_animations.push_back(inert2);
  ActiveInterpolationsMap result = EffectStack::ActiveInterpolations(
      &element->GetElementAnimations()->GetEffectStack(), &new_animations, 0,
      KeyframeEffectReadOnly::kDefaultPriority);
  EXPECT_EQ(2u, result.size());
  EXPECT_EQ(GetFontSizeValue(result), 3);
  EXPECT_EQ(GetZIndexValue(result), 4);
}

TEST_F(AnimationEffectStackTest, CancelledAnimations) {
  HeapHashSet<Member<const Animation>> cancelled_animations;
  Animation* animation =
      Play(MakeKeyframeEffect(MakeEffectModel(CSSPropertyFontSize, "1px")), 0);
  cancelled_animations.insert(animation);
  Play(MakeKeyframeEffect(MakeEffectModel(CSSPropertyZIndex, "2")), 0);
  ActiveInterpolationsMap result = EffectStack::ActiveInterpolations(
      &element->GetElementAnimations()->GetEffectStack(), 0,
      &cancelled_animations, KeyframeEffectReadOnly::kDefaultPriority);
  EXPECT_EQ(1u, result.size());
  EXPECT_EQ(GetZIndexValue(result), 2);
}

TEST_F(AnimationEffectStackTest, ClearedEffectsRemoved) {
  Animation* animation =
      Play(MakeKeyframeEffect(MakeEffectModel(CSSPropertyFontSize, "1px")), 10);
  ActiveInterpolationsMap result = EffectStack::ActiveInterpolations(
      &element->GetElementAnimations()->GetEffectStack(), 0, 0,
      KeyframeEffectReadOnly::kDefaultPriority);
  EXPECT_EQ(1u, result.size());
  EXPECT_EQ(GetFontSizeValue(result), 1);

  animation->setEffect(0);
  result = EffectStack::ActiveInterpolations(
      &element->GetElementAnimations()->GetEffectStack(), 0, 0,
      KeyframeEffectReadOnly::kDefaultPriority);
  EXPECT_EQ(0u, result.size());
}

TEST_F(AnimationEffectStackTest, ForwardsFillDiscarding) {
  Play(MakeKeyframeEffect(MakeEffectModel(CSSPropertyFontSize, "1px")), 2);
  Play(MakeKeyframeEffect(MakeEffectModel(CSSPropertyFontSize, "2px")), 6);
  Play(MakeKeyframeEffect(MakeEffectModel(CSSPropertyFontSize, "3px")), 4);
  document->GetPendingAnimations().Update(Optional<CompositorElementIdSet>());
  ActiveInterpolationsMap interpolations;

  UpdateTimeline(11);
  ThreadState::Current()->CollectAllGarbage();
  interpolations = EffectStack::ActiveInterpolations(
      &element->GetElementAnimations()->GetEffectStack(), nullptr, nullptr,
      KeyframeEffectReadOnly::kDefaultPriority);
  EXPECT_EQ(1u, interpolations.size());
  EXPECT_EQ(GetFontSizeValue(interpolations), 3);
  EXPECT_EQ(3u, SampledEffectCount());

  UpdateTimeline(13);
  ThreadState::Current()->CollectAllGarbage();
  interpolations = EffectStack::ActiveInterpolations(
      &element->GetElementAnimations()->GetEffectStack(), nullptr, nullptr,
      KeyframeEffectReadOnly::kDefaultPriority);
  EXPECT_EQ(1u, interpolations.size());
  EXPECT_EQ(GetFontSizeValue(interpolations), 3);
  EXPECT_EQ(3u, SampledEffectCount());

  UpdateTimeline(15);
  ThreadState::Current()->CollectAllGarbage();
  interpolations = EffectStack::ActiveInterpolations(
      &element->GetElementAnimations()->GetEffectStack(), nullptr, nullptr,
      KeyframeEffectReadOnly::kDefaultPriority);
  EXPECT_EQ(1u, interpolations.size());
  EXPECT_EQ(GetFontSizeValue(interpolations), 3);
  EXPECT_EQ(2u, SampledEffectCount());

  UpdateTimeline(17);
  ThreadState::Current()->CollectAllGarbage();
  interpolations = EffectStack::ActiveInterpolations(
      &element->GetElementAnimations()->GetEffectStack(), nullptr, nullptr,
      KeyframeEffectReadOnly::kDefaultPriority);
  EXPECT_EQ(1u, interpolations.size());
  EXPECT_EQ(GetFontSizeValue(interpolations), 3);
  EXPECT_EQ(1u, SampledEffectCount());
}

}  // namespace blink
