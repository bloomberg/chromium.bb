// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/animationworklet/worklet_animation.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/bindings/modules/v8/animation_effect_or_animation_effect_sequence.h"
#include "third_party/blink/renderer/core/animation/element_animations.h"
#include "third_party/blink/renderer/core/animation/keyframe_effect.h"
#include "third_party/blink/renderer/core/animation/keyframe_effect_model.h"
#include "third_party/blink/renderer/core/animation/scroll_timeline.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"

namespace blink {

namespace {

KeyframeEffectModelBase* CreateEffectModel() {
  StringKeyframeVector frames_mixed_properties;
  Persistent<StringKeyframe> keyframe = StringKeyframe::Create();
  keyframe->SetOffset(0);
  keyframe->SetCSSPropertyValue(CSSPropertyOpacity, "0",
                                SecureContextMode::kInsecureContext, nullptr);
  frames_mixed_properties.push_back(keyframe);
  keyframe = StringKeyframe::Create();
  keyframe->SetOffset(1);
  keyframe->SetCSSPropertyValue(CSSPropertyOpacity, "1",
                                SecureContextMode::kInsecureContext, nullptr);
  frames_mixed_properties.push_back(keyframe);
  return StringKeyframeEffectModel::Create(frames_mixed_properties);
}

KeyframeEffect* CreateKeyframeEffect(Element* element) {
  Timing timing;
  timing.iteration_duration = AnimationTimeDelta::FromSecondsD(30);
  return KeyframeEffect::Create(element, CreateEffectModel(), timing);
}

WorkletAnimation* CreateWorkletAnimation(
    ScriptState* script_state,
    Element* element,
    ScrollTimeline* scroll_timeline = nullptr) {
  AnimationEffectOrAnimationEffectSequence effects;
  AnimationEffect* effect = CreateKeyframeEffect(element);
  effects.SetAnimationEffect(effect);
  DocumentTimelineOrScrollTimeline timeline;
  if (scroll_timeline)
    timeline.SetScrollTimeline(scroll_timeline);
  scoped_refptr<SerializedScriptValue> options;

  ScriptState::Scope scope(script_state);
  DummyExceptionStateForTesting exception_state;
  return WorkletAnimation::Create(script_state, "WorkletAnimation", effects,
                                  timeline, std::move(options),
                                  exception_state);
}

}  // namespace

class WorkletAnimationTest : public RenderingTest {
 public:
  WorkletAnimationTest()
      : RenderingTest(SingleChildLocalFrameClient::Create()) {}

  void SetUp() override {
    RenderingTest::SetUp();
    element_ = GetDocument().CreateElementForBinding("test");
    worklet_animation_ = CreateWorkletAnimation(GetScriptState(), element_);
  }

  ScriptState* GetScriptState() {
    return ToScriptStateForMainWorld(&GetFrame());
  }

  Persistent<Element> element_;
  Persistent<WorkletAnimation> worklet_animation_;
};

TEST_F(WorkletAnimationTest, WorkletAnimationInElementAnimations) {
  DummyExceptionStateForTesting exception_state;
  worklet_animation_->play(exception_state);
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
  DummyExceptionStateForTesting exception_state;
  worklet_animation_->play(exception_state);
  element_->EnsureElementAnimations().UpdateAnimationFlags(*style);
  EXPECT_EQ(true, style->HasCurrentOpacityAnimation());
}

TEST_F(WorkletAnimationTest,
       CurrentTimeFromDocumentTimelineIsOffsetByStartTime) {
  GetDocument().GetAnimationClock().ResetTimeForTesting();
  double error = base::TimeDelta::FromMicrosecondsD(1).InMillisecondsF();
  WorkletAnimationId id = worklet_animation_->GetWorkletAnimationId();
  base::TimeTicks first_ticks =
      base::TimeTicks() + base::TimeDelta::FromMillisecondsD(111);
  base::TimeTicks second_ticks =
      base::TimeTicks() + base::TimeDelta::FromMillisecondsD(111 + 123.4);

  GetDocument().GetAnimationClock().UpdateTime(first_ticks);
  DummyExceptionStateForTesting exception_state;
  worklet_animation_->play(exception_state);
  worklet_animation_->UpdateCompositingState();

  std::unique_ptr<AnimationWorkletDispatcherInput> state =
      std::make_unique<AnimationWorkletDispatcherInput>();
  worklet_animation_->UpdateInputState(state.get());
  // First state request sets the start time and thus current time should be 0.
  std::unique_ptr<AnimationWorkletInput> input =
      state->TakeWorkletState(id.scope_id);
  EXPECT_NEAR(0, input->added_and_updated_animations[0].current_time, error);
  state.reset(new AnimationWorkletDispatcherInput);
  GetDocument().GetAnimationClock().UpdateTime(second_ticks);
  worklet_animation_->UpdateInputState(state.get());
  input = state->TakeWorkletState(id.scope_id);
  EXPECT_NEAR(123.4, input->updated_animations[0].current_time, error);
}

TEST_F(WorkletAnimationTest,
       CurrentTimeFromScrollTimelineNotOffsetByStartTime) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #scroller { overflow: scroll; width: 100px; height: 100px; }
      #spacer { width: 200px; height: 200px; }
    </style>
    <div id='scroller'>
      <div id ='spacer'></div>
    </div>
  )HTML");

  LayoutBoxModelObject* scroller =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("scroller"));
  ASSERT_TRUE(scroller);
  ASSERT_TRUE(scroller->HasOverflowClip());
  PaintLayerScrollableArea* scrollable_area = scroller->GetScrollableArea();
  ASSERT_TRUE(scrollable_area);
  scrollable_area->SetScrollOffset(ScrollOffset(0, 20), kProgrammaticScroll);
  ScrollTimelineOptions* options = ScrollTimelineOptions::Create();
  DoubleOrScrollTimelineAutoKeyword time_range =
      DoubleOrScrollTimelineAutoKeyword::FromDouble(100);
  options->setTimeRange(time_range);
  options->setScrollSource(GetElementById("scroller"));
  ScrollTimeline* scroll_timeline =
      ScrollTimeline::Create(GetDocument(), options, ASSERT_NO_EXCEPTION);
  WorkletAnimation* worklet_animation =
      CreateWorkletAnimation(GetScriptState(), element_, scroll_timeline);
  WorkletAnimationId id = worklet_animation->GetWorkletAnimationId();

  DummyExceptionStateForTesting exception_state;
  worklet_animation->play(exception_state);
  worklet_animation->UpdateCompositingState();

  double error = base::TimeDelta::FromMicrosecondsD(1).InMillisecondsF();
  scrollable_area->SetScrollOffset(ScrollOffset(0, 40), kProgrammaticScroll);
  std::unique_ptr<AnimationWorkletDispatcherInput> state =
      std::make_unique<AnimationWorkletDispatcherInput>();
  worklet_animation->UpdateInputState(state.get());
  std::unique_ptr<AnimationWorkletInput> input =
      state->TakeWorkletState(id.scope_id);

  EXPECT_NEAR(40, input->added_and_updated_animations[0].current_time, error);
  state.reset(new AnimationWorkletDispatcherInput);

  scrollable_area->SetScrollOffset(ScrollOffset(0, 70), kProgrammaticScroll);
  worklet_animation->UpdateInputState(state.get());
  input = state->TakeWorkletState(id.scope_id);
  EXPECT_NEAR(70, input->updated_animations[0].current_time, error);
}

}  //  namespace blink
