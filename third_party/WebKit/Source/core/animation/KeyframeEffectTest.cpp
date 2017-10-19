// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/KeyframeEffect.h"

#include <memory>

#include "bindings/core/v8/Dictionary.h"
#include "bindings/core/v8/V8BindingForTesting.h"
#include "bindings/core/v8/V8KeyframeEffectOptions.h"
#include "bindings/core/v8/dictionary_sequence_or_dictionary.h"
#include "bindings/core/v8/unrestricted_double_or_keyframe_effect_options.h"
#include "core/animation/Animation.h"
#include "core/animation/AnimationClock.h"
#include "core/animation/AnimationEffectTiming.h"
#include "core/animation/AnimationTestHelper.h"
#include "core/animation/DocumentTimeline.h"
#include "core/animation/KeyframeEffectModel.h"
#include "core/animation/Timing.h"
#include "core/dom/Document.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "v8/include/v8.h"

namespace blink {

class KeyframeEffectTest : public ::testing::Test {
 protected:
  KeyframeEffectTest() : page_holder(DummyPageHolder::Create()) {
    element = GetDocument().createElement("foo");

    GetDocument().GetAnimationClock().ResetTimeForTesting(
        GetDocument().Timeline().ZeroTime());
    GetDocument().documentElement()->AppendChild(element.Get());
    EXPECT_EQ(0, GetDocument().Timeline().currentTime());
  }

  Document& GetDocument() const { return page_holder->GetDocument(); }

  std::unique_ptr<DummyPageHolder> page_holder;
  Persistent<Element> element;
};

class AnimationKeyframeEffectV8Test : public KeyframeEffectTest {
 protected:
  static KeyframeEffect* CreateAnimation(
      Element* element,
      Vector<Dictionary> keyframe_dictionary_vector,
      double timing_input) {
    NonThrowableExceptionState exception_state;
    return KeyframeEffect::Create(
        nullptr, element,
        DictionarySequenceOrDictionary::FromDictionarySequence(
            keyframe_dictionary_vector),
        UnrestrictedDoubleOrKeyframeEffectOptions::FromUnrestrictedDouble(
            timing_input),
        exception_state);
  }
  static KeyframeEffect* CreateAnimation(
      Element* element,
      Vector<Dictionary> keyframe_dictionary_vector,
      const KeyframeEffectOptions& timing_input) {
    NonThrowableExceptionState exception_state;
    return KeyframeEffect::Create(
        nullptr, element,
        DictionarySequenceOrDictionary::FromDictionarySequence(
            keyframe_dictionary_vector),
        UnrestrictedDoubleOrKeyframeEffectOptions::FromKeyframeEffectOptions(
            timing_input),
        exception_state);
  }
  static KeyframeEffect* CreateAnimation(
      Element* element,
      Vector<Dictionary> keyframe_dictionary_vector) {
    NonThrowableExceptionState exception_state;
    return KeyframeEffect::Create(
        nullptr, element,
        DictionarySequenceOrDictionary::FromDictionarySequence(
            keyframe_dictionary_vector),
        exception_state);
  }
};

TEST_F(AnimationKeyframeEffectV8Test, CanCreateAnAnimation) {
  V8TestingScope scope;
  NonThrowableExceptionState exception_state;

  Vector<Dictionary> js_keyframes;
  v8::Local<v8::Object> keyframe1 = v8::Object::New(scope.GetIsolate());
  v8::Local<v8::Object> keyframe2 = v8::Object::New(scope.GetIsolate());

  SetV8ObjectPropertyAsString(scope.GetIsolate(), keyframe1, "width", "100px");
  SetV8ObjectPropertyAsString(scope.GetIsolate(), keyframe1, "offset", "0");
  SetV8ObjectPropertyAsString(scope.GetIsolate(), keyframe1, "easing",
                              "ease-in-out");
  SetV8ObjectPropertyAsString(scope.GetIsolate(), keyframe2, "width", "0px");
  SetV8ObjectPropertyAsString(scope.GetIsolate(), keyframe2, "offset", "1");
  SetV8ObjectPropertyAsString(scope.GetIsolate(), keyframe2, "easing",
                              "cubic-bezier(1, 1, 0.3, 0.3)");

  js_keyframes.push_back(
      Dictionary(scope.GetIsolate(), keyframe1, exception_state));
  js_keyframes.push_back(
      Dictionary(scope.GetIsolate(), keyframe2, exception_state));

  String value1;
  ASSERT_TRUE(DictionaryHelper::Get(js_keyframes[0], "width", value1));
  ASSERT_EQ("100px", value1);

  String value2;
  ASSERT_TRUE(DictionaryHelper::Get(js_keyframes[1], "width", value2));
  ASSERT_EQ("0px", value2);

  KeyframeEffect* animation = CreateAnimation(element.Get(), js_keyframes, 0);

  Element* target = animation->Target();
  EXPECT_EQ(*element.Get(), *target);

  const KeyframeVector keyframes =
      ToKeyframeEffectModelBase(animation->Model())->GetFrames();

  EXPECT_EQ(0, keyframes[0]->Offset());
  EXPECT_EQ(1, keyframes[1]->Offset());

  const CSSValue& keyframe1_width =
      ToStringKeyframe(keyframes[0].get())
          ->CssPropertyValue(PropertyHandle(CSSPropertyWidth));
  const CSSValue& keyframe2_width =
      ToStringKeyframe(keyframes[1].get())
          ->CssPropertyValue(PropertyHandle(CSSPropertyWidth));

  EXPECT_EQ("100px", keyframe1_width.CssText());
  EXPECT_EQ("0px", keyframe2_width.CssText());

  EXPECT_EQ(*(CubicBezierTimingFunction::Preset(
                CubicBezierTimingFunction::EaseType::EASE_IN_OUT)),
            keyframes[0]->Easing());
  EXPECT_EQ(*(CubicBezierTimingFunction::Create(1, 1, 0.3, 0.3).get()),
            keyframes[1]->Easing());
}

TEST_F(AnimationKeyframeEffectV8Test, CanSetDuration) {
  Vector<Dictionary, 0> js_keyframes;
  double duration = 2000;

  KeyframeEffect* animation =
      CreateAnimation(element.Get(), js_keyframes, duration);

  EXPECT_EQ(duration / 1000, animation->SpecifiedTiming().iteration_duration);
}

TEST_F(AnimationKeyframeEffectV8Test, CanOmitSpecifiedDuration) {
  Vector<Dictionary, 0> js_keyframes;
  KeyframeEffect* animation = CreateAnimation(element.Get(), js_keyframes);
  EXPECT_TRUE(std::isnan(animation->SpecifiedTiming().iteration_duration));
}

TEST_F(AnimationKeyframeEffectV8Test, SpecifiedGetters) {
  V8TestingScope scope;
  Vector<Dictionary, 0> js_keyframes;

  v8::Local<v8::Object> timing_input = v8::Object::New(scope.GetIsolate());
  SetV8ObjectPropertyAsNumber(scope.GetIsolate(), timing_input, "delay", 2);
  SetV8ObjectPropertyAsNumber(scope.GetIsolate(), timing_input, "endDelay",
                              0.5);
  SetV8ObjectPropertyAsString(scope.GetIsolate(), timing_input, "fill",
                              "backwards");
  SetV8ObjectPropertyAsNumber(scope.GetIsolate(), timing_input,
                              "iterationStart", 2);
  SetV8ObjectPropertyAsNumber(scope.GetIsolate(), timing_input, "iterations",
                              10);
  SetV8ObjectPropertyAsString(scope.GetIsolate(), timing_input, "direction",
                              "reverse");
  SetV8ObjectPropertyAsString(scope.GetIsolate(), timing_input, "easing",
                              "ease-in-out");
  KeyframeEffectOptions timing_input_dictionary;
  DummyExceptionStateForTesting exception_state;
  V8KeyframeEffectOptions::ToImpl(scope.GetIsolate(), timing_input,
                                  timing_input_dictionary, exception_state);
  EXPECT_FALSE(exception_state.HadException());

  KeyframeEffect* animation =
      CreateAnimation(element.Get(), js_keyframes, timing_input_dictionary);

  AnimationEffectTiming* specified = animation->timing();
  EXPECT_EQ(2, specified->delay());
  EXPECT_EQ(0.5, specified->endDelay());
  EXPECT_EQ("backwards", specified->fill());
  EXPECT_EQ(2, specified->iterationStart());
  EXPECT_EQ(10, specified->iterations());
  EXPECT_EQ("reverse", specified->direction());
  EXPECT_EQ("ease-in-out", specified->easing());
}

TEST_F(AnimationKeyframeEffectV8Test, SpecifiedDurationGetter) {
  V8TestingScope scope;
  Vector<Dictionary, 0> js_keyframes;

  v8::Local<v8::Object> timing_input_with_duration =
      v8::Object::New(scope.GetIsolate());
  SetV8ObjectPropertyAsNumber(scope.GetIsolate(), timing_input_with_duration,
                              "duration", 2.5);
  KeyframeEffectOptions timing_input_dictionary_with_duration;
  DummyExceptionStateForTesting exception_state;
  V8KeyframeEffectOptions::ToImpl(
      scope.GetIsolate(), timing_input_with_duration,
      timing_input_dictionary_with_duration, exception_state);
  EXPECT_FALSE(exception_state.HadException());

  KeyframeEffect* animation_with_duration = CreateAnimation(
      element.Get(), js_keyframes, timing_input_dictionary_with_duration);

  AnimationEffectTiming* specified_with_duration =
      animation_with_duration->timing();
  UnrestrictedDoubleOrString duration;
  specified_with_duration->duration(duration);
  EXPECT_TRUE(duration.IsUnrestrictedDouble());
  EXPECT_EQ(2.5, duration.GetAsUnrestrictedDouble());
  EXPECT_FALSE(duration.IsString());

  v8::Local<v8::Object> timing_input_no_duration =
      v8::Object::New(scope.GetIsolate());
  KeyframeEffectOptions timing_input_dictionary_no_duration;
  V8KeyframeEffectOptions::ToImpl(scope.GetIsolate(), timing_input_no_duration,
                                  timing_input_dictionary_no_duration,
                                  exception_state);
  EXPECT_FALSE(exception_state.HadException());

  KeyframeEffect* animation_no_duration = CreateAnimation(
      element.Get(), js_keyframes, timing_input_dictionary_no_duration);

  AnimationEffectTiming* specified_no_duration =
      animation_no_duration->timing();
  UnrestrictedDoubleOrString duration2;
  specified_no_duration->duration(duration2);
  EXPECT_FALSE(duration2.IsUnrestrictedDouble());
  EXPECT_TRUE(duration2.IsString());
  EXPECT_EQ("auto", duration2.GetAsString());
}

TEST_F(AnimationKeyframeEffectV8Test, SpecifiedSetters) {
  V8TestingScope scope;
  Vector<Dictionary, 0> js_keyframes;
  v8::Local<v8::Object> timing_input = v8::Object::New(scope.GetIsolate());
  KeyframeEffectOptions timing_input_dictionary;
  DummyExceptionStateForTesting exception_state;
  V8KeyframeEffectOptions::ToImpl(scope.GetIsolate(), timing_input,
                                  timing_input_dictionary, exception_state);
  EXPECT_FALSE(exception_state.HadException());
  KeyframeEffect* animation =
      CreateAnimation(element.Get(), js_keyframes, timing_input_dictionary);

  AnimationEffectTiming* specified = animation->timing();

  EXPECT_EQ(0, specified->delay());
  specified->setDelay(2);
  EXPECT_EQ(2, specified->delay());

  EXPECT_EQ(0, specified->endDelay());
  specified->setEndDelay(0.5);
  EXPECT_EQ(0.5, specified->endDelay());

  EXPECT_EQ("auto", specified->fill());
  specified->setFill("backwards");
  EXPECT_EQ("backwards", specified->fill());

  EXPECT_EQ(0, specified->iterationStart());
  specified->setIterationStart(2, exception_state);
  ASSERT_FALSE(exception_state.HadException());
  EXPECT_EQ(2, specified->iterationStart());

  EXPECT_EQ(1, specified->iterations());
  specified->setIterations(10, exception_state);
  ASSERT_FALSE(exception_state.HadException());
  EXPECT_EQ(10, specified->iterations());

  EXPECT_EQ(1, specified->PlaybackRate());
  specified->SetPlaybackRate(2);
  EXPECT_EQ(2, specified->PlaybackRate());

  EXPECT_EQ("normal", specified->direction());
  specified->setDirection("reverse");
  EXPECT_EQ("reverse", specified->direction());

  EXPECT_EQ("linear", specified->easing());
  specified->setEasing("ease-in-out", exception_state);
  ASSERT_FALSE(exception_state.HadException());
  EXPECT_EQ("ease-in-out", specified->easing());
}

TEST_F(AnimationKeyframeEffectV8Test, SetSpecifiedDuration) {
  V8TestingScope scope;
  Vector<Dictionary, 0> js_keyframes;
  v8::Local<v8::Object> timing_input = v8::Object::New(scope.GetIsolate());
  KeyframeEffectOptions timing_input_dictionary;
  DummyExceptionStateForTesting exception_state;
  V8KeyframeEffectOptions::ToImpl(scope.GetIsolate(), timing_input,
                                  timing_input_dictionary, exception_state);
  EXPECT_FALSE(exception_state.HadException());
  KeyframeEffect* animation =
      CreateAnimation(element.Get(), js_keyframes, timing_input_dictionary);

  AnimationEffectTiming* specified = animation->timing();

  UnrestrictedDoubleOrString duration;
  specified->duration(duration);
  EXPECT_FALSE(duration.IsUnrestrictedDouble());
  EXPECT_TRUE(duration.IsString());
  EXPECT_EQ("auto", duration.GetAsString());

  UnrestrictedDoubleOrString in_duration;
  in_duration.SetUnrestrictedDouble(2.5);
  specified->setDuration(in_duration, exception_state);
  ASSERT_FALSE(exception_state.HadException());
  UnrestrictedDoubleOrString duration2;
  specified->duration(duration2);
  EXPECT_TRUE(duration2.IsUnrestrictedDouble());
  EXPECT_EQ(2.5, duration2.GetAsUnrestrictedDouble());
  EXPECT_FALSE(duration2.IsString());
}

TEST_F(KeyframeEffectTest, TimeToEffectChange) {
  Timing timing;
  timing.iteration_duration = 100;
  timing.start_delay = 100;
  timing.end_delay = 100;
  timing.fill_mode = Timing::FillMode::NONE;
  KeyframeEffect* animation = KeyframeEffect::Create(nullptr, nullptr, timing);
  Animation* player = GetDocument().Timeline().Play(animation);
  double inf = std::numeric_limits<double>::infinity();

  EXPECT_EQ(100, animation->TimeToForwardsEffectChange());
  EXPECT_EQ(inf, animation->TimeToReverseEffectChange());

  player->SetCurrentTimeInternal(100);
  EXPECT_EQ(100, animation->TimeToForwardsEffectChange());
  EXPECT_EQ(0, animation->TimeToReverseEffectChange());

  player->SetCurrentTimeInternal(199);
  EXPECT_EQ(1, animation->TimeToForwardsEffectChange());
  EXPECT_EQ(0, animation->TimeToReverseEffectChange());

  player->SetCurrentTimeInternal(200);
  // End-exclusive.
  EXPECT_EQ(inf, animation->TimeToForwardsEffectChange());
  EXPECT_EQ(0, animation->TimeToReverseEffectChange());

  player->SetCurrentTimeInternal(300);
  EXPECT_EQ(inf, animation->TimeToForwardsEffectChange());
  EXPECT_EQ(100, animation->TimeToReverseEffectChange());
}

TEST_F(KeyframeEffectTest, TimeToEffectChangeWithPlaybackRate) {
  Timing timing;
  timing.iteration_duration = 100;
  timing.start_delay = 100;
  timing.end_delay = 100;
  timing.playback_rate = 2;
  timing.fill_mode = Timing::FillMode::NONE;
  KeyframeEffect* animation = KeyframeEffect::Create(nullptr, nullptr, timing);
  Animation* player = GetDocument().Timeline().Play(animation);
  double inf = std::numeric_limits<double>::infinity();

  EXPECT_EQ(100, animation->TimeToForwardsEffectChange());
  EXPECT_EQ(inf, animation->TimeToReverseEffectChange());

  player->SetCurrentTimeInternal(100);
  EXPECT_EQ(50, animation->TimeToForwardsEffectChange());
  EXPECT_EQ(0, animation->TimeToReverseEffectChange());

  player->SetCurrentTimeInternal(149);
  EXPECT_EQ(1, animation->TimeToForwardsEffectChange());
  EXPECT_EQ(0, animation->TimeToReverseEffectChange());

  player->SetCurrentTimeInternal(150);
  // End-exclusive.
  EXPECT_EQ(inf, animation->TimeToForwardsEffectChange());
  EXPECT_EQ(0, animation->TimeToReverseEffectChange());

  player->SetCurrentTimeInternal(200);
  EXPECT_EQ(inf, animation->TimeToForwardsEffectChange());
  EXPECT_EQ(50, animation->TimeToReverseEffectChange());
}

TEST_F(KeyframeEffectTest, TimeToEffectChangeWithNegativePlaybackRate) {
  Timing timing;
  timing.iteration_duration = 100;
  timing.start_delay = 100;
  timing.end_delay = 100;
  timing.playback_rate = -2;
  timing.fill_mode = Timing::FillMode::NONE;
  KeyframeEffect* animation = KeyframeEffect::Create(nullptr, nullptr, timing);
  Animation* player = GetDocument().Timeline().Play(animation);
  double inf = std::numeric_limits<double>::infinity();

  EXPECT_EQ(100, animation->TimeToForwardsEffectChange());
  EXPECT_EQ(inf, animation->TimeToReverseEffectChange());

  player->SetCurrentTimeInternal(100);
  EXPECT_EQ(50, animation->TimeToForwardsEffectChange());
  EXPECT_EQ(0, animation->TimeToReverseEffectChange());

  player->SetCurrentTimeInternal(149);
  EXPECT_EQ(1, animation->TimeToForwardsEffectChange());
  EXPECT_EQ(0, animation->TimeToReverseEffectChange());

  player->SetCurrentTimeInternal(150);
  EXPECT_EQ(inf, animation->TimeToForwardsEffectChange());
  EXPECT_EQ(0, animation->TimeToReverseEffectChange());

  player->SetCurrentTimeInternal(200);
  EXPECT_EQ(inf, animation->TimeToForwardsEffectChange());
  EXPECT_EQ(50, animation->TimeToReverseEffectChange());
}

TEST_F(KeyframeEffectTest, ElementDestructorClearsAnimationTarget) {
  // This test expects incorrect behaviour should be removed once Element
  // and KeyframeEffect are moved to Oilpan. See crbug.com/362404 for context.
  Timing timing;
  timing.iteration_duration = 5;
  KeyframeEffect* animation =
      KeyframeEffect::Create(element.Get(), nullptr, timing);
  EXPECT_EQ(element.Get(), animation->Target());
  GetDocument().Timeline().Play(animation);
  page_holder.reset();
  element.Clear();
}

}  // namespace blink
