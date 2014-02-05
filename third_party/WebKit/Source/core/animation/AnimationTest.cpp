// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/animation/Animation.h"

#include "bindings/v8/Dictionary.h"
#include "core/animation/AnimatableLength.h"
#include "core/animation/AnimationClock.h"
#include "core/animation/AnimationHelpers.h"
#include "core/animation/DocumentTimeline.h"
#include "core/animation/KeyframeEffectModel.h"
#include "platform/animation/TimingFunctionTestHelper.h"

#include <gtest/gtest.h>

namespace WebCore {

namespace {

v8::Handle<v8::Value> stringToV8Value(String string)
{
    return v8::Handle<v8::Value>::Cast(v8String(v8::Isolate::GetCurrent(), string));
}

v8::Handle<v8::Value> doubleToV8Value(double number)
{
    return v8::Handle<v8::Value>::Cast(v8::Number::New(v8::Isolate::GetCurrent(), number));
}

void setV8ObjectPropertyAsString(v8::Handle<v8::Object> object, String name, String value)
{
    object->Set(stringToV8Value(name), stringToV8Value(value));
}

void setV8ObjectPropertyAsNumber(v8::Handle<v8::Object> object, String name, double value)
{
    object->Set(stringToV8Value(name), doubleToV8Value(value));
}

} // namespace

class AnimationAnimationTest : public ::testing::Test {
protected:
    virtual void SetUp()
    {
        document = Document::create();
        document->animationClock().resetTimeForTesting();
        element = document->createElement("foo", ASSERT_NO_EXCEPTION);
        document->timeline()->setZeroTime(0);
        ASSERT_EQ(0, document->timeline()->currentTime());
    }

    RefPtr<Document> document;
    RefPtr<Element> element;

    PassRefPtr<Animation> createAnimation(Element* element, Vector<Dictionary> keyframeDictionaryVector, Dictionary timingInput)
    {
        return Animation::createUnsafe(element, keyframeDictionaryVector, timingInput);
    }

    PassRefPtr<Animation> createAnimation(Element* element, Vector<Dictionary> keyframeDictionaryVector, double timingInput)
    {
        return Animation::createUnsafe(element, keyframeDictionaryVector, timingInput);
    }

    PassRefPtr<Animation> createAnimation(Element* element, Vector<Dictionary> keyframeDictionaryVector)
    {
        return Animation::createUnsafe(element, keyframeDictionaryVector);
    }

    void populateTiming(Timing& timing, Dictionary timingInputDictionary)
    {
        Animation::populateTiming(timing, timingInputDictionary);
    }

    void applyTimingInputNumber(Timing& timing, v8::Isolate* isolate, String timingProperty, double timingPropertyValue)
    {
        v8::Handle<v8::Object> timingInput = v8::Object::New(isolate);
        setV8ObjectPropertyAsNumber(timingInput, timingProperty, timingPropertyValue);
        Dictionary timingInputDictionary = Dictionary(v8::Handle<v8::Value>::Cast(timingInput), isolate);
        populateTiming(timing, timingInputDictionary);
    }

    void applyTimingInputString(Timing& timing, v8::Isolate* isolate, String timingProperty, String timingPropertyValue)
    {
        v8::Handle<v8::Object> timingInput = v8::Object::New(isolate);
        setV8ObjectPropertyAsString(timingInput, timingProperty, timingPropertyValue);
        Dictionary timingInputDictionary = Dictionary(v8::Handle<v8::Value>::Cast(timingInput), isolate);
        populateTiming(timing, timingInputDictionary);
    }
};

TEST_F(AnimationAnimationTest, CanCreateAnAnimation)
{
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::HandleScope scope(isolate);
    v8::Local<v8::Context> context = v8::Context::New(isolate);
    v8::Context::Scope contextScope(context);

    Vector<Dictionary> jsKeyframes;
    v8::Handle<v8::Object> keyframe1 = v8::Object::New(isolate);
    v8::Handle<v8::Object> keyframe2 = v8::Object::New(isolate);

    setV8ObjectPropertyAsString(keyframe1, "width", "100px");
    setV8ObjectPropertyAsString(keyframe1, "offset", "0");
    setV8ObjectPropertyAsString(keyframe2, "width", "0px");
    setV8ObjectPropertyAsString(keyframe2, "offset", "1");

    jsKeyframes.append(Dictionary(keyframe1, isolate));
    jsKeyframes.append(Dictionary(keyframe2, isolate));

    String value1;
    ASSERT_TRUE(jsKeyframes[0].get("width", value1));
    ASSERT_EQ("100px", value1);

    String value2;
    ASSERT_TRUE(jsKeyframes[1].get("width", value2));
    ASSERT_EQ("0px", value2);

    RefPtr<Animation> animation = createAnimation(element.get(), jsKeyframes, 0);

    Element* target = animation->target();
    EXPECT_EQ(*element.get(), *target);

    const KeyframeEffectModel::KeyframeVector keyframes =
        toKeyframeEffectModel(animation->effect())->getFrames();

    EXPECT_EQ(0, keyframes[0]->offset());
    EXPECT_EQ(1, keyframes[1]->offset());

    const AnimatableValue* keyframe1Width = keyframes[0]->propertyValue(CSSPropertyWidth);
    const AnimatableValue* keyframe2Width = keyframes[1]->propertyValue(CSSPropertyWidth);
    ASSERT(keyframe1Width);
    ASSERT(keyframe2Width);

    EXPECT_TRUE(keyframe1Width->isLength());
    EXPECT_TRUE(keyframe2Width->isLength());

    EXPECT_EQ("100px", toAnimatableLength(keyframe1Width)->toCSSValue()->cssText());
    EXPECT_EQ("0px", toAnimatableLength(keyframe2Width)->toCSSValue()->cssText());
}

TEST_F(AnimationAnimationTest, CanSetDuration)
{
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::HandleScope scope(isolate);
    v8::Local<v8::Context> context = v8::Context::New(isolate);
    v8::Context::Scope contextScope(context);

    Vector<Dictionary, 0> jsKeyframes;
    double duration = 2;

    RefPtr<Animation> animation = createAnimation(element.get(), jsKeyframes, duration);

    EXPECT_TRUE(animation->specified().hasIterationDuration);
    EXPECT_EQ(duration, animation->specified().iterationDuration);
}

TEST_F(AnimationAnimationTest, CanOmitSpecifiedDuration)
{
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::HandleScope scope(isolate);
    v8::Local<v8::Context> context = v8::Context::New(isolate);
    v8::Context::Scope contextScope(context);

    Vector<Dictionary, 0> jsKeyframes;

    RefPtr<Animation> animation = createAnimation(element.get(), jsKeyframes);

    EXPECT_FALSE(animation->specified().hasIterationDuration);
    EXPECT_EQ(0, animation->specified().iterationDuration);
}

TEST_F(AnimationAnimationTest, ClipNegativeDurationToZero)
{
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::HandleScope scope(isolate);
    v8::Local<v8::Context> context = v8::Context::New(isolate);
    v8::Context::Scope contextScope(context);

    Vector<Dictionary, 0> jsKeyframes;

    RefPtr<Animation> animation = createAnimation(element.get(), jsKeyframes, -2);

    EXPECT_TRUE(animation->specified().hasIterationDuration);
    EXPECT_EQ(0, animation->specified().iterationDuration);
}

TEST_F(AnimationAnimationTest, TimingInputStartDelay)
{
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::HandleScope scope(isolate);
    v8::Local<v8::Context> context = v8::Context::New(isolate);
    v8::Context::Scope contextScope(context);

    Timing timing;
    EXPECT_EQ(0, timing.startDelay);

    applyTimingInputNumber(timing, isolate, "delay", 1.1);
    EXPECT_EQ(1.1, timing.startDelay);
    timing.startDelay = 0;

    applyTimingInputNumber(timing, isolate, "delay", -1);
    EXPECT_EQ(-1, timing.startDelay);
    timing.startDelay = 0;

    applyTimingInputString(timing, isolate, "delay", "1");
    EXPECT_EQ(1, timing.startDelay);
    timing.startDelay = 0;

    applyTimingInputString(timing, isolate, "delay", "1s");
    EXPECT_EQ(0, timing.startDelay);
    timing.startDelay = 0;

    applyTimingInputString(timing, isolate, "delay", "Infinity");
    EXPECT_EQ(0, timing.startDelay);
    timing.startDelay = 0;

    applyTimingInputString(timing, isolate, "delay", "-Infinity");
    EXPECT_EQ(0, timing.startDelay);
    timing.startDelay = 0;

    applyTimingInputString(timing, isolate, "delay", "NaN");
    EXPECT_EQ(0, timing.startDelay);
    timing.startDelay = 0;

    applyTimingInputString(timing, isolate, "delay", "rubbish");
    EXPECT_EQ(0, timing.startDelay);
    timing.startDelay = 0;
}

TEST_F(AnimationAnimationTest, TimingInputFillMode)
{
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::HandleScope scope(isolate);
    v8::Local<v8::Context> context = v8::Context::New(isolate);
    v8::Context::Scope contextScope(context);

    Timing timing;
    Timing::FillMode defaultFillMode = Timing::FillModeForwards;
    EXPECT_EQ(defaultFillMode, timing.fillMode);

    applyTimingInputString(timing, isolate, "fill", "forwards");
    EXPECT_EQ(Timing::FillModeForwards, timing.fillMode);
    timing.fillMode = defaultFillMode;

    applyTimingInputString(timing, isolate, "fill", "none");
    EXPECT_EQ(Timing::FillModeNone, timing.fillMode);
    timing.fillMode = defaultFillMode;

    applyTimingInputString(timing, isolate, "fill", "backwards");
    EXPECT_EQ(Timing::FillModeBackwards, timing.fillMode);
    timing.fillMode = defaultFillMode;

    applyTimingInputString(timing, isolate, "fill", "both");
    EXPECT_EQ(Timing::FillModeBoth, timing.fillMode);
    timing.fillMode = defaultFillMode;

    applyTimingInputString(timing, isolate, "fill", "everything!");
    EXPECT_EQ(defaultFillMode, timing.fillMode);
    timing.fillMode = defaultFillMode;

    applyTimingInputString(timing, isolate, "fill", "backwardsandforwards");
    EXPECT_EQ(defaultFillMode, timing.fillMode);
    timing.fillMode = defaultFillMode;

    applyTimingInputNumber(timing, isolate, "fill", 2);
    EXPECT_EQ(defaultFillMode, timing.fillMode);
    timing.fillMode = defaultFillMode;
}

TEST_F(AnimationAnimationTest, TimingInputIterationStart)
{
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::HandleScope scope(isolate);
    v8::Local<v8::Context> context = v8::Context::New(isolate);
    v8::Context::Scope contextScope(context);

    Timing timing;
    EXPECT_EQ(0, timing.iterationStart);

    applyTimingInputNumber(timing, isolate, "iterationStart", 1.1);
    EXPECT_EQ(1.1, timing.iterationStart);
    timing.iterationStart = 0;

    applyTimingInputNumber(timing, isolate, "iterationStart", -1);
    EXPECT_EQ(0, timing.iterationStart);
    timing.iterationStart = 0;

    applyTimingInputString(timing, isolate, "iterationStart", "Infinity");
    EXPECT_EQ(0, timing.iterationStart);
    timing.iterationStart = 0;

    applyTimingInputString(timing, isolate, "iterationStart", "-Infinity");
    EXPECT_EQ(0, timing.iterationStart);
    timing.iterationStart = 0;

    applyTimingInputString(timing, isolate, "iterationStart", "NaN");
    EXPECT_EQ(0, timing.iterationStart);
    timing.iterationStart = 0;

    applyTimingInputString(timing, isolate, "iterationStart", "rubbish");
    EXPECT_EQ(0, timing.iterationStart);
    timing.iterationStart = 0;
}

TEST_F(AnimationAnimationTest, TimingInputIterationCount)
{
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::HandleScope scope(isolate);
    v8::Local<v8::Context> context = v8::Context::New(isolate);
    v8::Context::Scope contextScope(context);

    Timing timing;
    EXPECT_EQ(1, timing.iterationCount);

    applyTimingInputNumber(timing, isolate, "iterations", 2.1);
    EXPECT_EQ(2.1, timing.iterationCount);
    timing.iterationCount = 1;

    applyTimingInputNumber(timing, isolate, "iterations", -1);
    EXPECT_EQ(0, timing.iterationCount);
    timing.iterationCount = 1;

    applyTimingInputString(timing, isolate, "iterations", "Infinity");
    EXPECT_TRUE(std::isinf(timing.iterationCount) && (timing.iterationCount > 0));
    timing.iterationCount = 1;

    applyTimingInputString(timing, isolate, "iterations", "-Infinity");
    EXPECT_EQ(0, timing.iterationCount);
    timing.iterationCount = 1;

    applyTimingInputString(timing, isolate, "iterations", "NaN");
    EXPECT_EQ(1, timing.iterationCount);
    timing.iterationCount = 1;

    applyTimingInputString(timing, isolate, "iterations", "rubbish");
    EXPECT_EQ(1, timing.iterationCount);
    timing.iterationCount = 1;
}

TEST_F(AnimationAnimationTest, TimingInputIterationDuration)
{
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::HandleScope scope(isolate);
    v8::Local<v8::Context> context = v8::Context::New(isolate);
    v8::Context::Scope contextScope(context);

    Timing timing;
    EXPECT_EQ(0, timing.iterationDuration);
    EXPECT_FALSE(timing.hasIterationDuration);

    applyTimingInputNumber(timing, isolate, "duration", 1.1);
    EXPECT_EQ(1.1, timing.iterationDuration);
    EXPECT_TRUE(timing.hasIterationDuration);
    timing.hasIterationDuration = false;
    timing.iterationDuration = 0;

    applyTimingInputNumber(timing, isolate, "duration", -1);
    EXPECT_EQ(0, timing.iterationDuration);
    EXPECT_FALSE(timing.hasIterationDuration);
    timing.hasIterationDuration = false;
    timing.iterationDuration = 0;

    applyTimingInputString(timing, isolate, "duration", "1");
    EXPECT_EQ(1, timing.iterationDuration);
    EXPECT_TRUE(timing.hasIterationDuration);
    timing.hasIterationDuration = false;
    timing.iterationDuration = 0;

    applyTimingInputString(timing, isolate, "duration", "Infinity");
    EXPECT_TRUE(std::isinf(timing.iterationDuration) && (timing.iterationDuration > 0));
    EXPECT_TRUE(timing.hasIterationDuration);
    timing.hasIterationDuration = false;
    timing.iterationDuration = 0;

    applyTimingInputString(timing, isolate, "duration", "-Infinity");
    EXPECT_EQ(0, timing.iterationDuration);
    EXPECT_FALSE(timing.hasIterationDuration);
    timing.hasIterationDuration = false;
    timing.iterationDuration = 0;

    applyTimingInputString(timing, isolate, "duration", "NaN");
    EXPECT_EQ(0, timing.iterationDuration);
    EXPECT_FALSE(timing.hasIterationDuration);
    timing.hasIterationDuration = false;
    timing.iterationDuration = 0;

    applyTimingInputString(timing, isolate, "duration", "auto");
    EXPECT_EQ(0, timing.iterationDuration);
    EXPECT_FALSE(timing.hasIterationDuration);
    timing.hasIterationDuration = false;
    timing.iterationDuration = 0;

    applyTimingInputString(timing, isolate, "duration", "rubbish");
    EXPECT_EQ(0, timing.iterationDuration);
    EXPECT_FALSE(timing.hasIterationDuration);
    timing.hasIterationDuration = false;
    timing.iterationDuration = 0;
}

TEST_F(AnimationAnimationTest, TimingInputPlaybackRate)
{
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::HandleScope scope(isolate);
    v8::Local<v8::Context> context = v8::Context::New(isolate);
    v8::Context::Scope contextScope(context);

    Timing timing;
    EXPECT_EQ(1, timing.playbackRate);

    applyTimingInputNumber(timing, isolate, "playbackRate", 2.1);
    EXPECT_EQ(2.1, timing.playbackRate);
    timing.playbackRate = 1;

    applyTimingInputNumber(timing, isolate, "playbackRate", -1);
    EXPECT_EQ(-1, timing.playbackRate);
    timing.playbackRate = 1;

    applyTimingInputString(timing, isolate, "playbackRate", "Infinity");
    EXPECT_EQ(1, timing.playbackRate);
    timing.playbackRate = 1;

    applyTimingInputString(timing, isolate, "playbackRate", "-Infinity");
    EXPECT_EQ(1, timing.playbackRate);
    timing.playbackRate = 1;

    applyTimingInputString(timing, isolate, "playbackRate", "NaN");
    EXPECT_EQ(1, timing.playbackRate);
    timing.playbackRate = 1;

    applyTimingInputString(timing, isolate, "playbackRate", "rubbish");
    EXPECT_EQ(1, timing.playbackRate);
    timing.playbackRate = 1;
}

TEST_F(AnimationAnimationTest, TimingInputDirection)
{
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::HandleScope scope(isolate);
    v8::Local<v8::Context> context = v8::Context::New(isolate);
    v8::Context::Scope contextScope(context);

    Timing timing;
    Timing::PlaybackDirection defaultPlaybackDirection = Timing::PlaybackDirectionNormal;
    EXPECT_EQ(defaultPlaybackDirection, timing.direction);

    applyTimingInputString(timing, isolate, "direction", "normal");
    EXPECT_EQ(Timing::PlaybackDirectionNormal, timing.direction);
    timing.direction = defaultPlaybackDirection;

    applyTimingInputString(timing, isolate, "direction", "reverse");
    EXPECT_EQ(Timing::PlaybackDirectionReverse, timing.direction);
    timing.direction = defaultPlaybackDirection;

    applyTimingInputString(timing, isolate, "direction", "alternate");
    EXPECT_EQ(Timing::PlaybackDirectionAlternate, timing.direction);
    timing.direction = defaultPlaybackDirection;

    applyTimingInputString(timing, isolate, "direction", "alternate-reverse");
    EXPECT_EQ(Timing::PlaybackDirectionAlternateReverse, timing.direction);
    timing.direction = defaultPlaybackDirection;

    applyTimingInputString(timing, isolate, "direction", "rubbish");
    EXPECT_EQ(defaultPlaybackDirection, timing.direction);
    timing.direction = defaultPlaybackDirection;

    applyTimingInputNumber(timing, isolate, "direction", 2);
    EXPECT_EQ(defaultPlaybackDirection, timing.direction);
    timing.direction = defaultPlaybackDirection;
}

TEST_F(AnimationAnimationTest, TimingInputTimingFunction)
{
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::HandleScope scope(isolate);
    v8::Local<v8::Context> context = v8::Context::New(isolate);
    v8::Context::Scope contextScope(context);

    Timing timing;
    const RefPtr<TimingFunction> defaultTimingFunction = LinearTimingFunction::create();
    EXPECT_EQ(*defaultTimingFunction.get(), *timing.timingFunction.get());

    applyTimingInputString(timing, isolate, "easing", "ease");
    EXPECT_EQ(*(CubicBezierTimingFunction::preset(CubicBezierTimingFunction::Ease)), *timing.timingFunction.get());
    timing.timingFunction = defaultTimingFunction;

    applyTimingInputString(timing, isolate, "easing", "ease-in");
    EXPECT_EQ(*(CubicBezierTimingFunction::preset(CubicBezierTimingFunction::EaseIn)), *timing.timingFunction.get());
    timing.timingFunction = defaultTimingFunction;

    applyTimingInputString(timing, isolate, "easing", "ease-out");
    EXPECT_EQ(*(CubicBezierTimingFunction::preset(CubicBezierTimingFunction::EaseOut)), *timing.timingFunction.get());
    timing.timingFunction = defaultTimingFunction;

    applyTimingInputString(timing, isolate, "easing", "ease-in-out");
    EXPECT_EQ(*(CubicBezierTimingFunction::preset(CubicBezierTimingFunction::EaseInOut)), *timing.timingFunction.get());
    timing.timingFunction = defaultTimingFunction;

    applyTimingInputString(timing, isolate, "easing", "linear");
    EXPECT_EQ(*(LinearTimingFunction::create()), *timing.timingFunction.get());
    timing.timingFunction = defaultTimingFunction;

    applyTimingInputString(timing, isolate, "easing", "initial");
    EXPECT_EQ(*defaultTimingFunction.get(), *timing.timingFunction.get());
    timing.timingFunction = defaultTimingFunction;

    applyTimingInputString(timing, isolate, "easing", "step-start");
    EXPECT_EQ(*(StepsTimingFunction::preset(StepsTimingFunction::Start)), *timing.timingFunction.get());
    timing.timingFunction = defaultTimingFunction;

    applyTimingInputString(timing, isolate, "easing", "step-end");
    EXPECT_EQ(*(StepsTimingFunction::preset(StepsTimingFunction::End)), *timing.timingFunction.get());
    timing.timingFunction = defaultTimingFunction;

    applyTimingInputString(timing, isolate, "easing", "cubic-bezier(1, 1, 0.3, 0.3)");
    EXPECT_EQ(*(CubicBezierTimingFunction::create(1, 1, 0.3, 0.3).get()), *timing.timingFunction.get());
    timing.timingFunction = defaultTimingFunction;

    applyTimingInputString(timing, isolate, "easing", "steps(3, start)");
    EXPECT_EQ(*(StepsTimingFunction::create(3, true).get()), *timing.timingFunction.get());
    timing.timingFunction = defaultTimingFunction;

    applyTimingInputString(timing, isolate, "easing", "steps(5, end)");
    EXPECT_EQ(*(StepsTimingFunction::create(5, false).get()), *timing.timingFunction.get());
    timing.timingFunction = defaultTimingFunction;

    applyTimingInputString(timing, isolate, "easing", "steps(5.6, end)");
    EXPECT_EQ(*defaultTimingFunction.get(), *timing.timingFunction.get());
    timing.timingFunction = defaultTimingFunction;

    // FIXME: Step-middle not yet implemented. Change this test when it is working.
    applyTimingInputString(timing, isolate, "easing", "steps(5, middle)");
    EXPECT_EQ(*defaultTimingFunction.get(), *timing.timingFunction.get());
    timing.timingFunction = defaultTimingFunction;

    applyTimingInputString(timing, isolate, "easing", "cubic-bezier(2, 2, 0.3, 0.3)");
    EXPECT_EQ(*defaultTimingFunction.get(), *timing.timingFunction.get());
    timing.timingFunction = defaultTimingFunction;

    applyTimingInputString(timing, isolate, "easing", "rubbish");
    EXPECT_EQ(*defaultTimingFunction.get(), *timing.timingFunction.get());
    timing.timingFunction = defaultTimingFunction;

    applyTimingInputNumber(timing, isolate, "easing", 2);
    EXPECT_EQ(*defaultTimingFunction.get(), *timing.timingFunction.get());
    timing.timingFunction = defaultTimingFunction;
}

TEST_F(AnimationAnimationTest, TimingInputEmpty)
{
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::HandleScope scope(isolate);
    v8::Local<v8::Context> context = v8::Context::New(isolate);
    v8::Context::Scope contextScope(context);

    Timing updatedTiming;
    Timing controlTiming;

    v8::Handle<v8::Object> timingInput = v8::Object::New(isolate);
    Dictionary timingInputDictionary = Dictionary(v8::Handle<v8::Value>::Cast(timingInput), isolate);
    populateTiming(updatedTiming, timingInputDictionary);

    EXPECT_EQ(controlTiming.startDelay, updatedTiming.startDelay);
    EXPECT_EQ(controlTiming.fillMode, updatedTiming.fillMode);
    EXPECT_EQ(controlTiming.iterationStart, updatedTiming.iterationStart);
    EXPECT_EQ(controlTiming.iterationCount, updatedTiming.iterationCount);
    EXPECT_EQ(controlTiming.iterationDuration, updatedTiming.iterationDuration);
    EXPECT_EQ(controlTiming.hasIterationDuration, updatedTiming.hasIterationDuration);
    EXPECT_EQ(controlTiming.playbackRate, updatedTiming.playbackRate);
    EXPECT_EQ(controlTiming.direction, updatedTiming.direction);
    EXPECT_EQ(*controlTiming.timingFunction.get(), *updatedTiming.timingFunction.get());
}

} // namespace WebCore
