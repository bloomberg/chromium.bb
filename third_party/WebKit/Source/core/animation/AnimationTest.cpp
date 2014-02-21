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
    AnimationAnimationTest()
        : document(Document::create())
        , element(document->createElement("foo", ASSERT_NO_EXCEPTION))
    {
        document->animationClock().resetTimeForTesting();
        document->timeline()->setZeroTime(0);
        EXPECT_EQ(0, document->timeline()->currentTime());
    }

    RefPtr<Document> document;
    RefPtr<Element> element;
};

class AnimationAnimationV8Test : public AnimationAnimationTest {
protected:
    AnimationAnimationV8Test()
        : isolate(v8::Isolate::GetCurrent())
        , scope(isolate)
        , context(v8::Context::New(isolate))
        , contextScope(context)
    {
    }

    v8::Isolate* isolate;
    v8::HandleScope scope;
    v8::Local<v8::Context> context;
    v8::Context::Scope contextScope;

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

    void applyTimingInputNumber(Timing& timing, String timingProperty, double timingPropertyValue)
    {
        v8::Handle<v8::Object> timingInput = v8::Object::New(isolate);
        setV8ObjectPropertyAsNumber(timingInput, timingProperty, timingPropertyValue);
        Dictionary timingInputDictionary = Dictionary(v8::Handle<v8::Value>::Cast(timingInput), isolate);
        populateTiming(timing, timingInputDictionary);
    }

    void applyTimingInputString(Timing& timing, String timingProperty, String timingPropertyValue)
    {
        v8::Handle<v8::Object> timingInput = v8::Object::New(isolate);
        setV8ObjectPropertyAsString(timingInput, timingProperty, timingPropertyValue);
        Dictionary timingInputDictionary = Dictionary(v8::Handle<v8::Value>::Cast(timingInput), isolate);
        populateTiming(timing, timingInputDictionary);
    }
};

TEST_F(AnimationAnimationV8Test, CanCreateAnAnimation)
{
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

TEST_F(AnimationAnimationV8Test, CanSetDuration)
{
    Vector<Dictionary, 0> jsKeyframes;
    double duration = 2;

    RefPtr<Animation> animation = createAnimation(element.get(), jsKeyframes, duration);

    EXPECT_EQ(duration, animation->specifiedTiming().iterationDuration);
}

TEST_F(AnimationAnimationV8Test, CanOmitSpecifiedDuration)
{
    Vector<Dictionary, 0> jsKeyframes;
    RefPtr<Animation> animation = createAnimation(element.get(), jsKeyframes);
    EXPECT_TRUE(std::isnan(animation->specifiedTiming().iterationDuration));
}

TEST_F(AnimationAnimationV8Test, ClipNegativeDurationToZero)
{
    Vector<Dictionary, 0> jsKeyframes;
    RefPtr<Animation> animation = createAnimation(element.get(), jsKeyframes, -2);
    EXPECT_EQ(0, animation->specifiedTiming().iterationDuration);
}

TEST_F(AnimationAnimationV8Test, SpecifiedGetters)
{
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

TEST_F(AnimationAnimationV8Test, SpecifiedDurationGetter)
{
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
    specifiedWithDuration->getDuration("duration", isNumber, numberDuration, isString, stringDuration);
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
    specifiedNoDuration->getDuration("duration", isNumber, numberDuration, isString, stringDuration);
    EXPECT_FALSE(isNumber);
    EXPECT_TRUE(std::isnan(numberDuration));
    EXPECT_TRUE(isString);
    EXPECT_EQ("auto", stringDuration);
}

TEST_F(AnimationAnimationV8Test, SpecifiedSetters)
{
    Vector<Dictionary, 0> jsKeyframes;
    v8::Handle<v8::Object> timingInput = v8::Object::New(isolate);
    Dictionary timingInputDictionary = Dictionary(v8::Handle<v8::Value>::Cast(timingInput), isolate);
    RefPtr<Animation> animation = createAnimation(element.get(), jsKeyframes, timingInputDictionary);

    RefPtr<TimedItemTiming> specified = animation->specified();

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
    specified->setIterationStart(2);
    EXPECT_EQ(2, specified->iterationStart());

    EXPECT_EQ(1, specified->iterations());
    specified->setIterations(10);
    EXPECT_EQ(10, specified->iterations());

    EXPECT_EQ(1, specified->playbackRate());
    specified->setPlaybackRate(2);
    EXPECT_EQ(2, specified->playbackRate());

    EXPECT_EQ("normal", specified->direction());
    specified->setDirection("reverse");
    EXPECT_EQ("reverse", specified->direction());

    EXPECT_EQ("linear", specified->easing());
    specified->setEasing("step-start");
    EXPECT_EQ("step-start", specified->easing());
}

TEST_F(AnimationAnimationV8Test, SetSpecifiedDuration)
{
    Vector<Dictionary, 0> jsKeyframes;
    v8::Handle<v8::Object> timingInput = v8::Object::New(isolate);
    Dictionary timingInputDictionary = Dictionary(v8::Handle<v8::Value>::Cast(timingInput), isolate);
    RefPtr<Animation> animation = createAnimation(element.get(), jsKeyframes, timingInputDictionary);

    RefPtr<TimedItemTiming> specified = animation->specified();

    bool isNumber = false;
    double numberDuration = std::numeric_limits<double>::quiet_NaN();
    bool isString = false;
    String stringDuration = "";
    specified->getDuration("duration", isNumber, numberDuration, isString, stringDuration);
    EXPECT_FALSE(isNumber);
    EXPECT_TRUE(std::isnan(numberDuration));
    EXPECT_TRUE(isString);
    EXPECT_EQ("auto", stringDuration);

    specified->setDuration("duration", 2.5);
    isNumber = false;
    numberDuration = std::numeric_limits<double>::quiet_NaN();
    isString = false;
    stringDuration = "";
    specified->getDuration("duration", isNumber, numberDuration, isString, stringDuration);
    EXPECT_TRUE(isNumber);
    EXPECT_EQ(2.5, numberDuration);
    EXPECT_FALSE(isString);
    EXPECT_EQ("", stringDuration);
}

TEST_F(AnimationAnimationV8Test, TimingInputStartDelay)
{
    Timing timing;
    EXPECT_EQ(0, timing.startDelay);

    applyTimingInputNumber(timing, "delay", 1.1);
    EXPECT_EQ(1.1, timing.startDelay);
    timing.startDelay = 0;

    applyTimingInputNumber(timing, "delay", -1);
    EXPECT_EQ(-1, timing.startDelay);
    timing.startDelay = 0;

    applyTimingInputString(timing, "delay", "1");
    EXPECT_EQ(1, timing.startDelay);
    timing.startDelay = 0;

    applyTimingInputString(timing, "delay", "1s");
    EXPECT_EQ(0, timing.startDelay);
    timing.startDelay = 0;

    applyTimingInputString(timing, "delay", "Infinity");
    EXPECT_EQ(0, timing.startDelay);
    timing.startDelay = 0;

    applyTimingInputString(timing, "delay", "-Infinity");
    EXPECT_EQ(0, timing.startDelay);
    timing.startDelay = 0;

    applyTimingInputString(timing, "delay", "NaN");
    EXPECT_EQ(0, timing.startDelay);
    timing.startDelay = 0;

    applyTimingInputString(timing, "delay", "rubbish");
    EXPECT_EQ(0, timing.startDelay);
    timing.startDelay = 0;
}

TEST_F(AnimationAnimationV8Test, TimingInputEndDelay)
{
    Timing timing;
    applyTimingInputNumber(timing, "endDelay", 10);
    EXPECT_EQ(10, timing.endDelay);
    applyTimingInputNumber(timing, "endDelay", -2.5);
    EXPECT_EQ(-2.5, timing.endDelay);
}

TEST_F(AnimationAnimationV8Test, TimingInputFillMode)
{
    Timing timing;
    Timing::FillMode defaultFillMode = Timing::FillModeAuto;
    EXPECT_EQ(defaultFillMode, timing.fillMode);

    applyTimingInputString(timing, "fill", "auto");
    EXPECT_EQ(Timing::FillModeAuto, timing.fillMode);
    timing.fillMode = defaultFillMode;

    applyTimingInputString(timing, "fill", "forwards");
    EXPECT_EQ(Timing::FillModeForwards, timing.fillMode);
    timing.fillMode = defaultFillMode;

    applyTimingInputString(timing, "fill", "none");
    EXPECT_EQ(Timing::FillModeNone, timing.fillMode);
    timing.fillMode = defaultFillMode;

    applyTimingInputString(timing, "fill", "backwards");
    EXPECT_EQ(Timing::FillModeBackwards, timing.fillMode);
    timing.fillMode = defaultFillMode;

    applyTimingInputString(timing, "fill", "both");
    EXPECT_EQ(Timing::FillModeBoth, timing.fillMode);
    timing.fillMode = defaultFillMode;

    applyTimingInputString(timing, "fill", "everything!");
    EXPECT_EQ(defaultFillMode, timing.fillMode);
    timing.fillMode = defaultFillMode;

    applyTimingInputString(timing, "fill", "backwardsandforwards");
    EXPECT_EQ(defaultFillMode, timing.fillMode);
    timing.fillMode = defaultFillMode;

    applyTimingInputNumber(timing, "fill", 2);
    EXPECT_EQ(defaultFillMode, timing.fillMode);
    timing.fillMode = defaultFillMode;
}

TEST_F(AnimationAnimationV8Test, TimingInputIterationStart)
{
    Timing timing;
    EXPECT_EQ(0, timing.iterationStart);

    applyTimingInputNumber(timing, "iterationStart", 1.1);
    EXPECT_EQ(1.1, timing.iterationStart);
    timing.iterationStart = 0;

    applyTimingInputNumber(timing, "iterationStart", -1);
    EXPECT_EQ(0, timing.iterationStart);
    timing.iterationStart = 0;

    applyTimingInputString(timing, "iterationStart", "Infinity");
    EXPECT_EQ(0, timing.iterationStart);
    timing.iterationStart = 0;

    applyTimingInputString(timing, "iterationStart", "-Infinity");
    EXPECT_EQ(0, timing.iterationStart);
    timing.iterationStart = 0;

    applyTimingInputString(timing, "iterationStart", "NaN");
    EXPECT_EQ(0, timing.iterationStart);
    timing.iterationStart = 0;

    applyTimingInputString(timing, "iterationStart", "rubbish");
    EXPECT_EQ(0, timing.iterationStart);
    timing.iterationStart = 0;
}

TEST_F(AnimationAnimationV8Test, TimingInputIterationCount)
{
    Timing timing;
    EXPECT_EQ(1, timing.iterationCount);

    applyTimingInputNumber(timing, "iterations", 2.1);
    EXPECT_EQ(2.1, timing.iterationCount);
    timing.iterationCount = 1;

    applyTimingInputNumber(timing, "iterations", -1);
    EXPECT_EQ(0, timing.iterationCount);
    timing.iterationCount = 1;

    applyTimingInputString(timing, "iterations", "Infinity");
    EXPECT_TRUE(std::isinf(timing.iterationCount) && (timing.iterationCount > 0));
    timing.iterationCount = 1;

    applyTimingInputString(timing, "iterations", "-Infinity");
    EXPECT_EQ(0, timing.iterationCount);
    timing.iterationCount = 1;

    applyTimingInputString(timing, "iterations", "NaN");
    EXPECT_EQ(1, timing.iterationCount);
    timing.iterationCount = 1;

    applyTimingInputString(timing, "iterations", "rubbish");
    EXPECT_EQ(1, timing.iterationCount);
    timing.iterationCount = 1;
}

TEST_F(AnimationAnimationV8Test, TimingInputIterationDuration)
{
    Timing timing;
    EXPECT_TRUE(std::isnan(timing.iterationDuration));

    applyTimingInputNumber(timing, "duration", 1.1);
    EXPECT_EQ(1.1, timing.iterationDuration);
    timing.iterationDuration = std::numeric_limits<double>::quiet_NaN();

    applyTimingInputNumber(timing, "duration", -1);
    EXPECT_TRUE(std::isnan(timing.iterationDuration));
    timing.iterationDuration = std::numeric_limits<double>::quiet_NaN();

    applyTimingInputString(timing, "duration", "1");
    EXPECT_EQ(1, timing.iterationDuration);
    timing.iterationDuration = std::numeric_limits<double>::quiet_NaN();

    applyTimingInputString(timing, "duration", "Infinity");
    EXPECT_TRUE(std::isinf(timing.iterationDuration) && (timing.iterationDuration > 0));
    timing.iterationDuration = std::numeric_limits<double>::quiet_NaN();

    applyTimingInputString(timing, "duration", "-Infinity");
    EXPECT_TRUE(std::isnan(timing.iterationDuration));
    timing.iterationDuration = std::numeric_limits<double>::quiet_NaN();

    applyTimingInputString(timing, "duration", "NaN");
    EXPECT_TRUE(std::isnan(timing.iterationDuration));
    timing.iterationDuration = std::numeric_limits<double>::quiet_NaN();

    applyTimingInputString(timing, "duration", "auto");
    EXPECT_TRUE(std::isnan(timing.iterationDuration));
    timing.iterationDuration = std::numeric_limits<double>::quiet_NaN();

    applyTimingInputString(timing, "duration", "rubbish");
    EXPECT_TRUE(std::isnan(timing.iterationDuration));
    timing.iterationDuration = std::numeric_limits<double>::quiet_NaN();
}

TEST_F(AnimationAnimationV8Test, TimingInputPlaybackRate)
{
    Timing timing;
    EXPECT_EQ(1, timing.playbackRate);

    applyTimingInputNumber(timing, "playbackRate", 2.1);
    EXPECT_EQ(2.1, timing.playbackRate);
    timing.playbackRate = 1;

    applyTimingInputNumber(timing, "playbackRate", -1);
    EXPECT_EQ(-1, timing.playbackRate);
    timing.playbackRate = 1;

    applyTimingInputString(timing, "playbackRate", "Infinity");
    EXPECT_EQ(1, timing.playbackRate);
    timing.playbackRate = 1;

    applyTimingInputString(timing, "playbackRate", "-Infinity");
    EXPECT_EQ(1, timing.playbackRate);
    timing.playbackRate = 1;

    applyTimingInputString(timing, "playbackRate", "NaN");
    EXPECT_EQ(1, timing.playbackRate);
    timing.playbackRate = 1;

    applyTimingInputString(timing, "playbackRate", "rubbish");
    EXPECT_EQ(1, timing.playbackRate);
    timing.playbackRate = 1;
}

TEST_F(AnimationAnimationV8Test, TimingInputDirection)
{
    Timing timing;
    Timing::PlaybackDirection defaultPlaybackDirection = Timing::PlaybackDirectionNormal;
    EXPECT_EQ(defaultPlaybackDirection, timing.direction);

    applyTimingInputString(timing, "direction", "normal");
    EXPECT_EQ(Timing::PlaybackDirectionNormal, timing.direction);
    timing.direction = defaultPlaybackDirection;

    applyTimingInputString(timing, "direction", "reverse");
    EXPECT_EQ(Timing::PlaybackDirectionReverse, timing.direction);
    timing.direction = defaultPlaybackDirection;

    applyTimingInputString(timing, "direction", "alternate");
    EXPECT_EQ(Timing::PlaybackDirectionAlternate, timing.direction);
    timing.direction = defaultPlaybackDirection;

    applyTimingInputString(timing, "direction", "alternate-reverse");
    EXPECT_EQ(Timing::PlaybackDirectionAlternateReverse, timing.direction);
    timing.direction = defaultPlaybackDirection;

    applyTimingInputString(timing, "direction", "rubbish");
    EXPECT_EQ(defaultPlaybackDirection, timing.direction);
    timing.direction = defaultPlaybackDirection;

    applyTimingInputNumber(timing, "direction", 2);
    EXPECT_EQ(defaultPlaybackDirection, timing.direction);
    timing.direction = defaultPlaybackDirection;
}

TEST_F(AnimationAnimationV8Test, TimingInputTimingFunction)
{
    Timing timing;
    const RefPtr<TimingFunction> defaultTimingFunction = LinearTimingFunction::create();
    EXPECT_EQ(*defaultTimingFunction.get(), *timing.timingFunction.get());

    applyTimingInputString(timing, "easing", "ease");
    EXPECT_EQ(*(CubicBezierTimingFunction::preset(CubicBezierTimingFunction::Ease)), *timing.timingFunction.get());
    timing.timingFunction = defaultTimingFunction;

    applyTimingInputString(timing, "easing", "ease-in");
    EXPECT_EQ(*(CubicBezierTimingFunction::preset(CubicBezierTimingFunction::EaseIn)), *timing.timingFunction.get());
    timing.timingFunction = defaultTimingFunction;

    applyTimingInputString(timing, "easing", "ease-out");
    EXPECT_EQ(*(CubicBezierTimingFunction::preset(CubicBezierTimingFunction::EaseOut)), *timing.timingFunction.get());
    timing.timingFunction = defaultTimingFunction;

    applyTimingInputString(timing, "easing", "ease-in-out");
    EXPECT_EQ(*(CubicBezierTimingFunction::preset(CubicBezierTimingFunction::EaseInOut)), *timing.timingFunction.get());
    timing.timingFunction = defaultTimingFunction;

    applyTimingInputString(timing, "easing", "linear");
    EXPECT_EQ(*(LinearTimingFunction::create()), *timing.timingFunction.get());
    timing.timingFunction = defaultTimingFunction;

    applyTimingInputString(timing, "easing", "initial");
    EXPECT_EQ(*defaultTimingFunction.get(), *timing.timingFunction.get());
    timing.timingFunction = defaultTimingFunction;

    applyTimingInputString(timing, "easing", "step-start");
    EXPECT_EQ(*(StepsTimingFunction::preset(StepsTimingFunction::Start)), *timing.timingFunction.get());
    timing.timingFunction = defaultTimingFunction;

    applyTimingInputString(timing, "easing", "step-end");
    EXPECT_EQ(*(StepsTimingFunction::preset(StepsTimingFunction::End)), *timing.timingFunction.get());
    timing.timingFunction = defaultTimingFunction;

    applyTimingInputString(timing, "easing", "cubic-bezier(1, 1, 0.3, 0.3)");
    EXPECT_EQ(*(CubicBezierTimingFunction::create(1, 1, 0.3, 0.3).get()), *timing.timingFunction.get());
    timing.timingFunction = defaultTimingFunction;

    applyTimingInputString(timing, "easing", "steps(3, start)");
    EXPECT_EQ(*(StepsTimingFunction::create(3, true).get()), *timing.timingFunction.get());
    timing.timingFunction = defaultTimingFunction;

    applyTimingInputString(timing, "easing", "steps(5, end)");
    EXPECT_EQ(*(StepsTimingFunction::create(5, false).get()), *timing.timingFunction.get());
    timing.timingFunction = defaultTimingFunction;

    applyTimingInputString(timing, "easing", "steps(5.6, end)");
    EXPECT_EQ(*defaultTimingFunction.get(), *timing.timingFunction.get());
    timing.timingFunction = defaultTimingFunction;

    // FIXME: Step-middle not yet implemented. Change this test when it is working.
    applyTimingInputString(timing, "easing", "steps(5, middle)");
    EXPECT_EQ(*defaultTimingFunction.get(), *timing.timingFunction.get());
    timing.timingFunction = defaultTimingFunction;

    applyTimingInputString(timing, "easing", "cubic-bezier(2, 2, 0.3, 0.3)");
    EXPECT_EQ(*defaultTimingFunction.get(), *timing.timingFunction.get());
    timing.timingFunction = defaultTimingFunction;

    applyTimingInputString(timing, "easing", "rubbish");
    EXPECT_EQ(*defaultTimingFunction.get(), *timing.timingFunction.get());
    timing.timingFunction = defaultTimingFunction;

    applyTimingInputNumber(timing, "easing", 2);
    EXPECT_EQ(*defaultTimingFunction.get(), *timing.timingFunction.get());
    timing.timingFunction = defaultTimingFunction;
}

TEST_F(AnimationAnimationV8Test, TimingInputEmpty)
{
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
    RefPtr<Animation> animation = Animation::create(nullptr, nullptr, timing);
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
    RefPtr<Animation> animation = Animation::create(nullptr, nullptr, timing);
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
    RefPtr<Animation> animation = Animation::create(nullptr, nullptr, timing);
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
