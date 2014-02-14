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
#include "core/animation/TimedItemTiming.h"
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
    setV8ObjectPropertyAsString(keyframe1, "easing", "ease-in-out");
    setV8ObjectPropertyAsString(keyframe2, "width", "0px");
    setV8ObjectPropertyAsString(keyframe2, "offset", "1");
    setV8ObjectPropertyAsString(keyframe2, "easing", "cubic-bezier(1, 1, 0.3, 0.3)");

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

    EXPECT_EQ(*(CubicBezierTimingFunction::preset(CubicBezierTimingFunction::EaseInOut)), *keyframes[0]->easing());
    EXPECT_EQ(*(CubicBezierTimingFunction::create(1, 1, 0.3, 0.3).get()), *keyframes[1]->easing());
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

    EXPECT_EQ(duration, animation->specifiedTiming().iterationDuration);
}

TEST_F(AnimationAnimationTest, CanOmitSpecifiedDuration)
{
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::HandleScope scope(isolate);
    v8::Local<v8::Context> context = v8::Context::New(isolate);
    v8::Context::Scope contextScope(context);

    Vector<Dictionary, 0> jsKeyframes;

    RefPtr<Animation> animation = createAnimation(element.get(), jsKeyframes);

    EXPECT_TRUE(std::isnan(animation->specifiedTiming().iterationDuration));
}

TEST_F(AnimationAnimationTest, ClipNegativeDurationToZero)
{
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::HandleScope scope(isolate);
    v8::Local<v8::Context> context = v8::Context::New(isolate);
    v8::Context::Scope contextScope(context);

    Vector<Dictionary, 0> jsKeyframes;

    RefPtr<Animation> animation = createAnimation(element.get(), jsKeyframes, -2);

    EXPECT_EQ(0, animation->specifiedTiming().iterationDuration);
}

TEST_F(AnimationAnimationTest, SpecifiedGetters)
{
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::HandleScope scope(isolate);
    v8::Local<v8::Context> context = v8::Context::New(isolate);
    v8::Context::Scope contextScope(context);

    Vector<Dictionary, 0> jsKeyframes;

    v8::Handle<v8::Object> timingInput = v8::Object::New(isolate);
    setV8ObjectPropertyAsNumber(timingInput, "delay", 2);
    setV8ObjectPropertyAsNumber(timingInput, "endDelay", 0.5);
    setV8ObjectPropertyAsString(timingInput, "fill", "backwards");
    setV8ObjectPropertyAsNumber(timingInput, "iterationStart", 2);
    setV8ObjectPropertyAsNumber(timingInput, "iterations", 10);
    setV8ObjectPropertyAsNumber(timingInput, "playbackRate", 2);
    setV8ObjectPropertyAsString(timingInput, "direction", "reverse");
    setV8ObjectPropertyAsString(timingInput, "easing", "step-start");
    Dictionary timingInputDictionary = Dictionary(v8::Handle<v8::Value>::Cast(timingInput), isolate);

    RefPtr<Animation> animation = createAnimation(element.get(), jsKeyframes, timingInputDictionary);

    RefPtr<TimedItemTiming> specified = animation->specified();
    EXPECT_EQ(2, specified->delay());
    EXPECT_EQ(0.5, specified->endDelay());
    EXPECT_EQ("backwards", specified->fill());
    EXPECT_EQ(2, specified->iterationStart());
    EXPECT_EQ(10, specified->iterations());
    EXPECT_EQ(2, specified->playbackRate());
    EXPECT_EQ("reverse", specified->direction());
    EXPECT_EQ("step-start", specified->easing());
}

TEST_F(AnimationAnimationTest, SpecifiedDurationGetter)
{
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::HandleScope scope(isolate);
    v8::Local<v8::Context> context = v8::Context::New(isolate);
    v8::Context::Scope contextScope(context);

    Vector<Dictionary, 0> jsKeyframes;

    v8::Handle<v8::Object> timingInputWithDuration = v8::Object::New(isolate);
    setV8ObjectPropertyAsNumber(timingInputWithDuration, "duration", 2.5);
    Dictionary timingInputDictionaryWithDuration = Dictionary(v8::Handle<v8::Value>::Cast(timingInputWithDuration), isolate);

    RefPtr<Animation> animationWithDuration = createAnimation(element.get(), jsKeyframes, timingInputDictionaryWithDuration);

    RefPtr<TimedItemTiming> specifiedWithDuration = animationWithDuration->specified();
    bool isNumber = false;
    double numberDuration = std::numeric_limits<double>::quiet_NaN();
    bool isString = false;
    String stringDuration = "";
    specifiedWithDuration->duration("duration", isNumber, numberDuration, isString, stringDuration);
    EXPECT_TRUE(isNumber);
    EXPECT_EQ(2.5, numberDuration);
    EXPECT_FALSE(isString);
    EXPECT_EQ("", stringDuration);


    v8::Handle<v8::Object> timingInputNoDuration = v8::Object::New(isolate);
    Dictionary timingInputDictionaryNoDuration = Dictionary(v8::Handle<v8::Value>::Cast(timingInputNoDuration), isolate);

    RefPtr<Animation> animationNoDuration = createAnimation(element.get(), jsKeyframes, timingInputDictionaryNoDuration);

    RefPtr<TimedItemTiming> specifiedNoDuration = animationNoDuration->specified();
    isNumber = false;
    numberDuration = std::numeric_limits<double>::quiet_NaN();
    isString = false;
    stringDuration = "";
    specifiedNoDuration->duration("duration", isNumber, numberDuration, isString, stringDuration);
    EXPECT_FALSE(isNumber);
    EXPECT_TRUE(std::isnan(numberDuration));
    EXPECT_TRUE(isString);
    EXPECT_EQ("auto", stringDuration);
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

TEST_F(AnimationAnimationTest, TimingInputEndDelay)
{
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::HandleScope scope(isolate);
    v8::Local<v8::Context> context = v8::Context::New(isolate);
    v8::Context::Scope contextScope(context);

    Timing timing;
    applyTimingInputNumber(timing, isolate, "endDelay", 10);
    EXPECT_EQ(10, timing.endDelay);
    applyTimingInputNumber(timing, isolate, "endDelay", -2.5);
    EXPECT_EQ(-2.5, timing.endDelay);
}

TEST_F(AnimationAnimationTest, TimingInputFillMode)
{
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::HandleScope scope(isolate);
    v8::Local<v8::Context> context = v8::Context::New(isolate);
    v8::Context::Scope contextScope(context);

    Timing timing;
    Timing::FillMode defaultFillMode = Timing::FillModeAuto;
    EXPECT_EQ(defaultFillMode, timing.fillMode);

    applyTimingInputString(timing, isolate, "fill", "auto");
    EXPECT_EQ(Timing::FillModeAuto, timing.fillMode);
    timing.fillMode = defaultFillMode;

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
    EXPECT_TRUE(std::isnan(timing.iterationDuration));

    applyTimingInputNumber(timing, isolate, "duration", 1.1);
    EXPECT_EQ(1.1, timing.iterationDuration);
    timing.iterationDuration = std::numeric_limits<double>::quiet_NaN();

    applyTimingInputNumber(timing, isolate, "duration", -1);
    EXPECT_TRUE(std::isnan(timing.iterationDuration));
    timing.iterationDuration = std::numeric_limits<double>::quiet_NaN();

    applyTimingInputString(timing, isolate, "duration", "1");
    EXPECT_EQ(1, timing.iterationDuration);
    timing.iterationDuration = std::numeric_limits<double>::quiet_NaN();

    applyTimingInputString(timing, isolate, "duration", "Infinity");
    EXPECT_TRUE(std::isinf(timing.iterationDuration) && (timing.iterationDuration > 0));
    timing.iterationDuration = std::numeric_limits<double>::quiet_NaN();

    applyTimingInputString(timing, isolate, "duration", "-Infinity");
    EXPECT_TRUE(std::isnan(timing.iterationDuration));
    timing.iterationDuration = std::numeric_limits<double>::quiet_NaN();

    applyTimingInputString(timing, isolate, "duration", "NaN");
    EXPECT_TRUE(std::isnan(timing.iterationDuration));
    timing.iterationDuration = std::numeric_limits<double>::quiet_NaN();

    applyTimingInputString(timing, isolate, "duration", "auto");
    EXPECT_TRUE(std::isnan(timing.iterationDuration));
    timing.iterationDuration = std::numeric_limits<double>::quiet_NaN();

    applyTimingInputString(timing, isolate, "duration", "rubbish");
    EXPECT_TRUE(std::isnan(timing.iterationDuration));
    timing.iterationDuration = std::numeric_limits<double>::quiet_NaN();
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
    EXPECT_TRUE(std::isnan(updatedTiming.iterationDuration));
    EXPECT_EQ(controlTiming.playbackRate, updatedTiming.playbackRate);
    EXPECT_EQ(controlTiming.direction, updatedTiming.direction);
    EXPECT_EQ(*controlTiming.timingFunction.get(), *updatedTiming.timingFunction.get());
}

TEST_F(AnimationAnimationTest, TimeToEffectChange)
{
    Timing timing;
    timing.iterationDuration = 100;
    timing.startDelay = 100;
    timing.endDelay = 100;
    timing.fillMode = Timing::FillModeNone;
    RefPtr<Animation> animation = Animation::create(0, 0, timing);
    RefPtr<Player> player = document->timeline()->play(animation.get());
    double inf = std::numeric_limits<double>::infinity();

    EXPECT_EQ(100, animation->timeToForwardsEffectChange());
    EXPECT_EQ(inf, animation->timeToReverseEffectChange());

    player->setCurrentTime(100);
    EXPECT_EQ(0, animation->timeToForwardsEffectChange());
    EXPECT_EQ(0, animation->timeToReverseEffectChange());

    player->setCurrentTime(199);
    EXPECT_EQ(0, animation->timeToForwardsEffectChange());
    EXPECT_EQ(0, animation->timeToReverseEffectChange());

    player->setCurrentTime(200);
    // End-exclusive.
    EXPECT_EQ(inf, animation->timeToForwardsEffectChange());
    EXPECT_EQ(0, animation->timeToReverseEffectChange());

    player->setCurrentTime(300);
    EXPECT_EQ(inf, animation->timeToForwardsEffectChange());
    EXPECT_EQ(100, animation->timeToReverseEffectChange());
}

TEST_F(AnimationAnimationTest, TimeToEffectChangeWithPlaybackRate)
{
    Timing timing;
    timing.iterationDuration = 100;
    timing.startDelay = 100;
    timing.endDelay = 100;
    timing.playbackRate = 2;
    timing.fillMode = Timing::FillModeNone;
    RefPtr<Animation> animation = Animation::create(0, 0, timing);
    RefPtr<Player> player = document->timeline()->play(animation.get());
    double inf = std::numeric_limits<double>::infinity();

    EXPECT_EQ(100, animation->timeToForwardsEffectChange());
    EXPECT_EQ(inf, animation->timeToReverseEffectChange());

    player->setCurrentTime(100);
    EXPECT_EQ(0, animation->timeToForwardsEffectChange());
    EXPECT_EQ(0, animation->timeToReverseEffectChange());

    player->setCurrentTime(149);
    EXPECT_EQ(0, animation->timeToForwardsEffectChange());
    EXPECT_EQ(0, animation->timeToReverseEffectChange());

    player->setCurrentTime(150);
    // End-exclusive.
    EXPECT_EQ(inf, animation->timeToForwardsEffectChange());
    EXPECT_EQ(0, animation->timeToReverseEffectChange());

    player->setCurrentTime(200);
    EXPECT_EQ(inf, animation->timeToForwardsEffectChange());
    EXPECT_EQ(50, animation->timeToReverseEffectChange());
}

TEST_F(AnimationAnimationTest, TimeToEffectChangeWithNegativePlaybackRate)
{
    Timing timing;
    timing.iterationDuration = 100;
    timing.startDelay = 100;
    timing.endDelay = 100;
    timing.playbackRate = -2;
    timing.fillMode = Timing::FillModeNone;
    RefPtr<Animation> animation = Animation::create(0, 0, timing);
    RefPtr<Player> player = document->timeline()->play(animation.get());
    double inf = std::numeric_limits<double>::infinity();

    EXPECT_EQ(100, animation->timeToForwardsEffectChange());
    EXPECT_EQ(inf, animation->timeToReverseEffectChange());

    player->setCurrentTime(100);
    EXPECT_EQ(0, animation->timeToForwardsEffectChange());
    EXPECT_EQ(0, animation->timeToReverseEffectChange());

    player->setCurrentTime(149);
    EXPECT_EQ(0, animation->timeToForwardsEffectChange());
    EXPECT_EQ(0, animation->timeToReverseEffectChange());

    player->setCurrentTime(150);
    EXPECT_EQ(inf, animation->timeToForwardsEffectChange());
    EXPECT_EQ(0, animation->timeToReverseEffectChange());

    player->setCurrentTime(200);
    EXPECT_EQ(inf, animation->timeToForwardsEffectChange());
    EXPECT_EQ(50, animation->timeToReverseEffectChange());
}

} // namespace WebCore
