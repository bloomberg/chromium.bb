// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/animation/Animation.h"

#include "bindings/v8/Dictionary.h"
#include "core/animation/AnimationTestHelper.h"
#include "core/animation/TimedItemTiming.h"
#include "platform/animation/TimingFunctionTestHelper.h"

#include <gtest/gtest.h>

namespace WebCore {

class AnimationAnimationTimingInputTest : public ::testing::Test {
protected:
    AnimationAnimationTimingInputTest()
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

    void populateTiming(Timing& timing, Dictionary timingInputDictionary)
    {
        Animation::populateTiming(timing, timingInputDictionary);
    }

    Timing applyTimingInputNumber(String timingProperty, double timingPropertyValue)
    {
        v8::Handle<v8::Object> timingInput = v8::Object::New(isolate);
        setV8ObjectPropertyAsNumber(timingInput, timingProperty, timingPropertyValue);
        Dictionary timingInputDictionary = Dictionary(v8::Handle<v8::Value>::Cast(timingInput), isolate);
        Timing timing;
        populateTiming(timing, timingInputDictionary);
        return timing;
    }

    Timing applyTimingInputString(String timingProperty, String timingPropertyValue)
    {
        v8::Handle<v8::Object> timingInput = v8::Object::New(isolate);
        setV8ObjectPropertyAsString(timingInput, timingProperty, timingPropertyValue);
        Dictionary timingInputDictionary = Dictionary(v8::Handle<v8::Value>::Cast(timingInput), isolate);
        Timing timing;
        populateTiming(timing, timingInputDictionary);
        return timing;
    }
};

TEST_F(AnimationAnimationTimingInputTest, TimingInputStartDelay)
{
    EXPECT_EQ(1.1, applyTimingInputNumber("delay", 1.1).startDelay);
    EXPECT_EQ(-1, applyTimingInputNumber("delay", -1).startDelay);
    EXPECT_EQ(1, applyTimingInputString("delay", "1").startDelay);
    EXPECT_EQ(0, applyTimingInputString("delay", "1s").startDelay);
    EXPECT_EQ(0, applyTimingInputString("delay", "Infinity").startDelay);
    EXPECT_EQ(0, applyTimingInputString("delay", "-Infinity").startDelay);
    EXPECT_EQ(0, applyTimingInputString("delay", "NaN").startDelay);
    EXPECT_EQ(0, applyTimingInputString("delay", "rubbish").startDelay);
}

TEST_F(AnimationAnimationTimingInputTest, TimingInputEndDelay)
{
    EXPECT_EQ(10, applyTimingInputNumber("endDelay", 10).endDelay);
    EXPECT_EQ(-2.5, applyTimingInputNumber("endDelay", -2.5).endDelay);
}

TEST_F(AnimationAnimationTimingInputTest, TimingInputFillMode)
{
    Timing::FillMode defaultFillMode = Timing::FillModeAuto;

    EXPECT_EQ(Timing::FillModeAuto, applyTimingInputString("fill", "auto").fillMode);
    EXPECT_EQ(Timing::FillModeForwards, applyTimingInputString("fill", "forwards").fillMode);
    EXPECT_EQ(Timing::FillModeNone, applyTimingInputString("fill", "none").fillMode);
    EXPECT_EQ(Timing::FillModeBackwards, applyTimingInputString("fill", "backwards").fillMode);
    EXPECT_EQ(Timing::FillModeBoth, applyTimingInputString("fill", "both").fillMode);
    EXPECT_EQ(defaultFillMode, applyTimingInputString("fill", "everything!").fillMode);
    EXPECT_EQ(defaultFillMode, applyTimingInputString("fill", "backwardsandforwards").fillMode);
    EXPECT_EQ(defaultFillMode, applyTimingInputNumber("fill", 2).fillMode);
}

TEST_F(AnimationAnimationTimingInputTest, TimingInputIterationStart)
{
    EXPECT_EQ(1.1, applyTimingInputNumber("iterationStart", 1.1).iterationStart);
    EXPECT_EQ(0, applyTimingInputNumber("iterationStart", -1).iterationStart);
    EXPECT_EQ(0, applyTimingInputString("iterationStart", "Infinity").iterationStart);
    EXPECT_EQ(0, applyTimingInputString("iterationStart", "-Infinity").iterationStart);
    EXPECT_EQ(0, applyTimingInputString("iterationStart", "NaN").iterationStart);
    EXPECT_EQ(0, applyTimingInputString("iterationStart", "rubbish").iterationStart);
}

TEST_F(AnimationAnimationTimingInputTest, TimingInputIterationCount)
{
    EXPECT_EQ(2.1, applyTimingInputNumber("iterations", 2.1).iterationCount);
    EXPECT_EQ(0, applyTimingInputNumber("iterations", -1).iterationCount);

    Timing timing = applyTimingInputString("iterations", "Infinity");
    EXPECT_TRUE(std::isinf(timing.iterationCount));
    EXPECT_GT(timing.iterationCount, 0);

    EXPECT_EQ(0, applyTimingInputString("iterations", "-Infinity").iterationCount);
    EXPECT_EQ(1, applyTimingInputString("iterations", "NaN").iterationCount);
    EXPECT_EQ(1, applyTimingInputString("iterations", "rubbish").iterationCount);
}

TEST_F(AnimationAnimationTimingInputTest, TimingInputIterationDuration)
{
    EXPECT_EQ(1.1, applyTimingInputNumber("duration", 1.1).iterationDuration);
    EXPECT_TRUE(std::isnan(applyTimingInputNumber("duration", -1).iterationDuration));
    EXPECT_EQ(1, applyTimingInputString("duration", "1").iterationDuration);

    Timing timing = applyTimingInputString("duration", "Infinity");
    EXPECT_TRUE(std::isinf(timing.iterationDuration));
    EXPECT_GT(timing.iterationDuration, 0);

    EXPECT_TRUE(std::isnan(applyTimingInputString("duration", "-Infinity").iterationDuration));
    EXPECT_TRUE(std::isnan(applyTimingInputString("duration", "NaN").iterationDuration));
    EXPECT_TRUE(std::isnan(applyTimingInputString("duration", "auto").iterationDuration));
    EXPECT_TRUE(std::isnan(applyTimingInputString("duration", "rubbish").iterationDuration));
}

TEST_F(AnimationAnimationTimingInputTest, TimingInputPlaybackRate)
{
    EXPECT_EQ(2.1, applyTimingInputNumber("playbackRate", 2.1).playbackRate);
    EXPECT_EQ(-1, applyTimingInputNumber("playbackRate", -1).playbackRate);
    EXPECT_EQ(1, applyTimingInputString("playbackRate", "Infinity").playbackRate);
    EXPECT_EQ(1, applyTimingInputString("playbackRate", "-Infinity").playbackRate);
    EXPECT_EQ(1, applyTimingInputString("playbackRate", "NaN").playbackRate);
    EXPECT_EQ(1, applyTimingInputString("playbackRate", "rubbish").playbackRate);
}

TEST_F(AnimationAnimationTimingInputTest, TimingInputDirection)
{
    Timing::PlaybackDirection defaultPlaybackDirection = Timing::PlaybackDirectionNormal;

    EXPECT_EQ(Timing::PlaybackDirectionNormal, applyTimingInputString("direction", "normal").direction);
    EXPECT_EQ(Timing::PlaybackDirectionReverse, applyTimingInputString("direction", "reverse").direction);
    EXPECT_EQ(Timing::PlaybackDirectionAlternate, applyTimingInputString("direction", "alternate").direction);
    EXPECT_EQ(Timing::PlaybackDirectionAlternateReverse, applyTimingInputString("direction", "alternate-reverse").direction);
    EXPECT_EQ(defaultPlaybackDirection, applyTimingInputString("direction", "rubbish").direction);
    EXPECT_EQ(defaultPlaybackDirection, applyTimingInputNumber("direction", 2).direction);
}

TEST_F(AnimationAnimationTimingInputTest, TimingInputTimingFunction)
{
    const RefPtr<TimingFunction> defaultTimingFunction = LinearTimingFunction::create();

    EXPECT_EQ(*CubicBezierTimingFunction::preset(CubicBezierTimingFunction::Ease), *applyTimingInputString("easing", "ease").timingFunction);
    EXPECT_EQ(*CubicBezierTimingFunction::preset(CubicBezierTimingFunction::EaseIn), *applyTimingInputString("easing", "ease-in").timingFunction);
    EXPECT_EQ(*CubicBezierTimingFunction::preset(CubicBezierTimingFunction::EaseOut), *applyTimingInputString("easing", "ease-out").timingFunction);
    EXPECT_EQ(*CubicBezierTimingFunction::preset(CubicBezierTimingFunction::EaseInOut), *applyTimingInputString("easing", "ease-in-out").timingFunction);
    EXPECT_EQ(*LinearTimingFunction::create(), *applyTimingInputString("easing", "linear").timingFunction);
    EXPECT_EQ(*defaultTimingFunction, *applyTimingInputString("easing", "initial").timingFunction);
    EXPECT_EQ(*StepsTimingFunction::preset(StepsTimingFunction::Start), *applyTimingInputString("easing", "step-start").timingFunction);
    EXPECT_EQ(*StepsTimingFunction::preset(StepsTimingFunction::End), *applyTimingInputString("easing", "step-end").timingFunction);
    EXPECT_EQ(*CubicBezierTimingFunction::create(1, 1, 0.3, 0.3), *applyTimingInputString("easing", "cubic-bezier(1, 1, 0.3, 0.3)").timingFunction);
    EXPECT_EQ(*StepsTimingFunction::create(3, true), *applyTimingInputString("easing", "steps(3, start)").timingFunction);
    EXPECT_EQ(*StepsTimingFunction::create(5, false), *applyTimingInputString("easing", "steps(5, end)").timingFunction);
    EXPECT_EQ(*defaultTimingFunction, *applyTimingInputString("easing", "steps(5.6, end)").timingFunction);
    // FIXME: Step-middle not yet implemented. Change this test when it is working.
    EXPECT_EQ(*defaultTimingFunction, *applyTimingInputString("easing", "steps(5, middle)").timingFunction);
    EXPECT_EQ(*defaultTimingFunction, *applyTimingInputString("easing", "cubic-bezier(2, 2, 0.3, 0.3)").timingFunction);
    EXPECT_EQ(*defaultTimingFunction, *applyTimingInputString("easing", "rubbish").timingFunction);
    EXPECT_EQ(*defaultTimingFunction, *applyTimingInputNumber("easing", 2).timingFunction);
}

TEST_F(AnimationAnimationTimingInputTest, TimingInputEmpty)
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
    EXPECT_EQ(*controlTiming.timingFunction, *updatedTiming.timingFunction);
}

} // namespace WebCore
