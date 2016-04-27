// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/TimingInput.h"

#include "bindings/core/v8/V8BindingForTesting.h"
#include "bindings/core/v8/V8KeyframeEffectOptions.h"
#include "core/animation/AnimationEffectTiming.h"
#include "core/animation/AnimationTestHelper.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <v8.h>

namespace blink {

class AnimationTimingInputTest : public ::testing::Test {
protected:
    AnimationTimingInputTest()
        : m_isolate(v8::Isolate::GetCurrent())
        , m_scope(m_isolate)
    {
    }

    Timing applyTimingInputNumber(String timingProperty, double timingPropertyValue, bool& timingConversionSuccess)
    {
        v8::Local<v8::Object> timingInput = v8::Object::New(m_isolate);
        setV8ObjectPropertyAsNumber(m_isolate, timingInput, timingProperty, timingPropertyValue);
        KeyframeEffectOptions timingInputDictionary;
        TrackExceptionState exceptionState;
        V8KeyframeEffectOptions::toImpl(m_isolate, timingInput, timingInputDictionary, exceptionState);
        Timing result;
        timingConversionSuccess = TimingInput::convert(timingInputDictionary, result, nullptr, exceptionState) && !exceptionState.hadException();
        return result;
    }

    Timing applyTimingInputString(String timingProperty, String timingPropertyValue, bool& timingConversionSuccess)
    {
        v8::Local<v8::Object> timingInput = v8::Object::New(m_isolate);
        setV8ObjectPropertyAsString(m_isolate, timingInput, timingProperty, timingPropertyValue);
        KeyframeEffectOptions timingInputDictionary;
        TrackExceptionState exceptionState;
        V8KeyframeEffectOptions::toImpl(m_isolate, timingInput, timingInputDictionary, exceptionState);
        Timing result;
        timingConversionSuccess = TimingInput::convert(timingInputDictionary, result, nullptr, exceptionState) && !exceptionState.hadException();
        return result;
    }

    v8::Isolate* m_isolate;

private:
    V8TestingScope m_scope;
};

TEST_F(AnimationTimingInputTest, TimingInputStartDelay)
{
    bool ignoredSuccess;
    EXPECT_EQ(1.1, applyTimingInputNumber("delay", 1100, ignoredSuccess).startDelay);
    EXPECT_EQ(-1, applyTimingInputNumber("delay", -1000, ignoredSuccess).startDelay);
    EXPECT_EQ(1, applyTimingInputString("delay", "1000", ignoredSuccess).startDelay);
    EXPECT_EQ(0, applyTimingInputString("delay", "1s", ignoredSuccess).startDelay);
    EXPECT_EQ(0, applyTimingInputString("delay", "Infinity", ignoredSuccess).startDelay);
    EXPECT_EQ(0, applyTimingInputString("delay", "-Infinity", ignoredSuccess).startDelay);
    EXPECT_EQ(0, applyTimingInputString("delay", "NaN", ignoredSuccess).startDelay);
    EXPECT_EQ(0, applyTimingInputString("delay", "rubbish", ignoredSuccess).startDelay);
}

TEST_F(AnimationTimingInputTest, TimingInputEndDelay)
{
    bool ignoredSuccess;
    EXPECT_EQ(10, applyTimingInputNumber("endDelay", 10000, ignoredSuccess).endDelay);
    EXPECT_EQ(-2.5, applyTimingInputNumber("endDelay", -2500, ignoredSuccess).endDelay);
}

TEST_F(AnimationTimingInputTest, TimingInputFillMode)
{
    Timing::FillMode defaultFillMode = Timing::FillModeAuto;
    bool ignoredSuccess;

    EXPECT_EQ(Timing::FillModeAuto, applyTimingInputString("fill", "auto", ignoredSuccess).fillMode);
    EXPECT_EQ(Timing::FillModeForwards, applyTimingInputString("fill", "forwards", ignoredSuccess).fillMode);
    EXPECT_EQ(Timing::FillModeNone, applyTimingInputString("fill", "none", ignoredSuccess).fillMode);
    EXPECT_EQ(Timing::FillModeBackwards, applyTimingInputString("fill", "backwards", ignoredSuccess).fillMode);
    EXPECT_EQ(Timing::FillModeBoth, applyTimingInputString("fill", "both", ignoredSuccess).fillMode);
    EXPECT_EQ(defaultFillMode, applyTimingInputString("fill", "everything!", ignoredSuccess).fillMode);
    EXPECT_EQ(defaultFillMode, applyTimingInputString("fill", "backwardsandforwards", ignoredSuccess).fillMode);
    EXPECT_EQ(defaultFillMode, applyTimingInputNumber("fill", 2, ignoredSuccess).fillMode);
}

TEST_F(AnimationTimingInputTest, TimingInputIterationStart)
{
    bool success;
    EXPECT_EQ(1.1, applyTimingInputNumber("iterationStart", 1.1, success).iterationStart);
    EXPECT_TRUE(success);

    applyTimingInputNumber("iterationStart", -1, success);
    EXPECT_FALSE(success);

    applyTimingInputString("iterationStart", "Infinity", success);
    EXPECT_FALSE(success);

    applyTimingInputString("iterationStart", "-Infinity", success);
    EXPECT_FALSE(success);

    applyTimingInputString("iterationStart", "NaN", success);
    EXPECT_FALSE(success);

    applyTimingInputString("iterationStart", "rubbish", success);
    EXPECT_FALSE(success);
}

TEST_F(AnimationTimingInputTest, TimingInputIterationCount)
{
    bool success;
    EXPECT_EQ(2.1, applyTimingInputNumber("iterations", 2.1, success).iterationCount);
    EXPECT_TRUE(success);

    Timing timing = applyTimingInputString("iterations", "Infinity", success);
    EXPECT_TRUE(success);
    EXPECT_TRUE(std::isinf(timing.iterationCount));
    EXPECT_GT(timing.iterationCount, 0);

    applyTimingInputNumber("iterations", -1, success);
    EXPECT_FALSE(success);

    applyTimingInputString("iterations", "-Infinity", success);
    EXPECT_FALSE(success);

    applyTimingInputString("iterations", "NaN", success);
    EXPECT_FALSE(success);

    applyTimingInputString("iterations", "rubbish", success);
    EXPECT_FALSE(success);
}

TEST_F(AnimationTimingInputTest, TimingInputIterationDuration)
{
    bool success;
    EXPECT_EQ(1.1, applyTimingInputNumber("duration", 1100, success).iterationDuration);
    EXPECT_TRUE(success);

    Timing timing = applyTimingInputNumber("duration", std::numeric_limits<double>::infinity(), success);
    EXPECT_TRUE(success);
    EXPECT_TRUE(std::isinf(timing.iterationDuration));
    EXPECT_GT(timing.iterationDuration, 0);

    EXPECT_TRUE(std::isnan(applyTimingInputString("duration", "auto", success).iterationDuration));
    EXPECT_TRUE(success);

    applyTimingInputString("duration", "1000", success);
    EXPECT_FALSE(success);

    applyTimingInputNumber("duration", -1000, success);
    EXPECT_FALSE(success);

    applyTimingInputString("duration", "-Infinity", success);
    EXPECT_FALSE(success);

    applyTimingInputString("duration", "NaN", success);
    EXPECT_FALSE(success);

    applyTimingInputString("duration", "rubbish", success);
    EXPECT_FALSE(success);
}

TEST_F(AnimationTimingInputTest, TimingInputPlaybackRate)
{
    bool ignoredSuccess;
    EXPECT_EQ(2.1, applyTimingInputNumber("playbackRate", 2.1, ignoredSuccess).playbackRate);
    EXPECT_EQ(-1, applyTimingInputNumber("playbackRate", -1, ignoredSuccess).playbackRate);
    EXPECT_EQ(1, applyTimingInputString("playbackRate", "Infinity", ignoredSuccess).playbackRate);
    EXPECT_EQ(1, applyTimingInputString("playbackRate", "-Infinity", ignoredSuccess).playbackRate);
    EXPECT_EQ(1, applyTimingInputString("playbackRate", "NaN", ignoredSuccess).playbackRate);
    EXPECT_EQ(1, applyTimingInputString("playbackRate", "rubbish", ignoredSuccess).playbackRate);
}

TEST_F(AnimationTimingInputTest, TimingInputDirection)
{
    Timing::PlaybackDirection defaultPlaybackDirection = Timing::PlaybackDirectionNormal;
    bool ignoredSuccess;

    EXPECT_EQ(Timing::PlaybackDirectionNormal, applyTimingInputString("direction", "normal", ignoredSuccess).direction);
    EXPECT_EQ(Timing::PlaybackDirectionReverse, applyTimingInputString("direction", "reverse", ignoredSuccess).direction);
    EXPECT_EQ(Timing::PlaybackDirectionAlternate, applyTimingInputString("direction", "alternate", ignoredSuccess).direction);
    EXPECT_EQ(Timing::PlaybackDirectionAlternateReverse, applyTimingInputString("direction", "alternate-reverse", ignoredSuccess).direction);
    EXPECT_EQ(defaultPlaybackDirection, applyTimingInputString("direction", "rubbish", ignoredSuccess).direction);
    EXPECT_EQ(defaultPlaybackDirection, applyTimingInputNumber("direction", 2, ignoredSuccess).direction);
}

TEST_F(AnimationTimingInputTest, TimingInputTimingFunction)
{
    const RefPtr<TimingFunction> defaultTimingFunction = LinearTimingFunction::shared();
    bool success;

    EXPECT_EQ(*CubicBezierTimingFunction::preset(CubicBezierTimingFunction::Ease), *applyTimingInputString("easing", "ease", success).timingFunction);
    EXPECT_TRUE(success);
    EXPECT_EQ(*CubicBezierTimingFunction::preset(CubicBezierTimingFunction::EaseIn), *applyTimingInputString("easing", "ease-in", success).timingFunction);
    EXPECT_TRUE(success);
    EXPECT_EQ(*CubicBezierTimingFunction::preset(CubicBezierTimingFunction::EaseOut), *applyTimingInputString("easing", "ease-out", success).timingFunction);
    EXPECT_TRUE(success);
    EXPECT_EQ(*CubicBezierTimingFunction::preset(CubicBezierTimingFunction::EaseInOut), *applyTimingInputString("easing", "ease-in-out", success).timingFunction);
    EXPECT_TRUE(success);
    EXPECT_EQ(*LinearTimingFunction::shared(), *applyTimingInputString("easing", "linear", success).timingFunction);
    EXPECT_TRUE(success);
    EXPECT_EQ(*StepsTimingFunction::preset(StepsTimingFunction::Start), *applyTimingInputString("easing", "step-start", success).timingFunction);
    EXPECT_TRUE(success);
    EXPECT_EQ(*StepsTimingFunction::preset(StepsTimingFunction::Middle), *applyTimingInputString("easing", "step-middle", success).timingFunction);
    EXPECT_TRUE(success);
    EXPECT_EQ(*StepsTimingFunction::preset(StepsTimingFunction::End), *applyTimingInputString("easing", "step-end", success).timingFunction);
    EXPECT_TRUE(success);
    EXPECT_EQ(*CubicBezierTimingFunction::create(1, 1, 0.3, 0.3), *applyTimingInputString("easing", "cubic-bezier(1, 1, 0.3, 0.3)", success).timingFunction);
    EXPECT_TRUE(success);
    EXPECT_EQ(*StepsTimingFunction::create(3, StepsTimingFunction::Start), *applyTimingInputString("easing", "steps(3, start)", success).timingFunction);
    EXPECT_TRUE(success);
    EXPECT_EQ(*StepsTimingFunction::create(5, StepsTimingFunction::Middle), *applyTimingInputString("easing", "steps(5, middle)", success).timingFunction);
    EXPECT_TRUE(success);
    EXPECT_EQ(*StepsTimingFunction::create(5, StepsTimingFunction::End), *applyTimingInputString("easing", "steps(5, end)", success).timingFunction);
    EXPECT_TRUE(success);

    applyTimingInputString("easing", "", success);
    EXPECT_FALSE(success);
    applyTimingInputString("easing", "steps(5.6, end)", success);
    EXPECT_FALSE(success);
    applyTimingInputString("easing", "cubic-bezier(2, 2, 0.3, 0.3)", success);
    EXPECT_FALSE(success);
    applyTimingInputString("easing", "rubbish", success);
    EXPECT_FALSE(success);
    applyTimingInputNumber("easing", 2, success);
    EXPECT_FALSE(success);
    applyTimingInputString("easing", "initial", success);
    EXPECT_FALSE(success);
}

TEST_F(AnimationTimingInputTest, TimingInputEmpty)
{
    TrackExceptionState exceptionState;
    Timing controlTiming;
    Timing updatedTiming;
    bool success = TimingInput::convert(KeyframeEffectOptions(), updatedTiming, nullptr, exceptionState);
    EXPECT_TRUE(success);
    EXPECT_FALSE(exceptionState.hadException());

    EXPECT_EQ(controlTiming.startDelay, updatedTiming.startDelay);
    EXPECT_EQ(controlTiming.fillMode, updatedTiming.fillMode);
    EXPECT_EQ(controlTiming.iterationStart, updatedTiming.iterationStart);
    EXPECT_EQ(controlTiming.iterationCount, updatedTiming.iterationCount);
    EXPECT_TRUE(std::isnan(updatedTiming.iterationDuration));
    EXPECT_EQ(controlTiming.playbackRate, updatedTiming.playbackRate);
    EXPECT_EQ(controlTiming.direction, updatedTiming.direction);
    EXPECT_EQ(*controlTiming.timingFunction, *updatedTiming.timingFunction);
}

} // namespace blink
