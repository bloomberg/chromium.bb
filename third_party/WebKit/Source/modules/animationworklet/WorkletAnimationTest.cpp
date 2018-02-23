// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/animationworklet/WorkletAnimation.h"

#include "bindings/modules/v8/animation_effect_read_only_or_animation_effect_read_only_sequence.h"
#include "core/animation/ElementAnimations.h"
#include "core/animation/KeyframeEffect.h"
#include "core/animation/KeyframeEffectModel.h"
#include "core/dom/Document.h"
#include "core/layout/LayoutTestHelper.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

KeyframeEffectModelBase* CreateEffectModel() {
  StringKeyframeVector frames_mixed_properties;
  scoped_refptr<StringKeyframe> keyframe = StringKeyframe::Create();
  keyframe->SetOffset(0);
  keyframe->SetCSSPropertyValue(CSSPropertyOpacity, "0",
                                SecureContextMode::kInsecureContext, nullptr);
  frames_mixed_properties.push_back(std::move(keyframe));
  keyframe = StringKeyframe::Create();
  keyframe->SetOffset(1);
  keyframe->SetCSSPropertyValue(CSSPropertyOpacity, "1",
                                SecureContextMode::kInsecureContext, nullptr);
  frames_mixed_properties.push_back(std::move(keyframe));
  return StringKeyframeEffectModel::Create(frames_mixed_properties);
}

KeyframeEffect* CreateKeyframeEffect(Element* element) {
  Timing timing;
  timing.iteration_duration = 30;
  timing.playback_rate = 1;
  return KeyframeEffect::Create(element, CreateEffectModel(), timing);
}

WorkletAnimation* CreateWorkletAnimation(Element* element) {
  AnimationEffectReadOnlyOrAnimationEffectReadOnlySequence effects;
  AnimationEffectReadOnly* effect = CreateKeyframeEffect(element);
  effects.SetAnimationEffectReadOnly(effect);
  DocumentTimelineOrScrollTimeline timeline;
  scoped_refptr<SerializedScriptValue> options;
  DummyExceptionStateForTesting exception_state;
  return WorkletAnimation::Create("WorkletAnimation", effects, timeline,
                                  std::move(options), exception_state);
}

}  // namespace

class WorkletAnimationTest : public RenderingTest {
 public:
  WorkletAnimationTest()
      : RenderingTest(SingleChildLocalFrameClient::Create()) {}

  void SetUp() override {
    RenderingTest::SetUp();
    element_ = GetDocument().CreateElementForBinding("test");
    worklet_animation_ = CreateWorkletAnimation(element_);
  }

  Persistent<Element> element_;
  Persistent<WorkletAnimation> worklet_animation_;
};

TEST_F(WorkletAnimationTest, WorkletAnimationInElementAnimations) {
  worklet_animation_->play();
  EXPECT_EQ(1u,
            element_->EnsureElementAnimations().GetWorkletAnimations().size());
  worklet_animation_->cancel();
  EXPECT_EQ(0u,
            element_->EnsureElementAnimations().GetWorkletAnimations().size());
}

TEST_F(WorkletAnimationTest, StyleHasCurrentAnimation) {
  scoped_refptr<ComputedStyle> style =
      GetDocument().EnsureStyleResolver().StyleForElement(element_).get();
  EXPECT_EQ(false, style->HasCurrentOpacityAnimation());
  worklet_animation_->play();
  element_->EnsureElementAnimations().UpdateAnimationFlags(*style);
  EXPECT_EQ(true, style->HasCurrentOpacityAnimation());
}

}  //  namespace blink
