// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/animation/CompositorFloatAnimationCurve.h"

#include "base/memory/scoped_ptr.h"
#include "cc/animation/timing_function.h"
#include "testing/gtest/include/gtest/gtest.h"

using blink::CompositorAnimationCurve;
using blink::CompositorFloatAnimationCurve;
using blink::CompositorFloatKeyframe;

namespace blink {

// Tests that a float animation with one keyframe works as expected.
TEST(WebFloatAnimationCurveTest, OneFloatKeyframe)
{
    scoped_ptr<CompositorFloatAnimationCurve> curve(new CompositorFloatAnimationCurve);
    curve->add(CompositorFloatKeyframe(0, 2),
        CompositorAnimationCurve::TimingFunctionTypeLinear);
    EXPECT_FLOAT_EQ(2, curve->getValue(-1));
    EXPECT_FLOAT_EQ(2, curve->getValue(0));
    EXPECT_FLOAT_EQ(2, curve->getValue(0.5));
    EXPECT_FLOAT_EQ(2, curve->getValue(1));
    EXPECT_FLOAT_EQ(2, curve->getValue(2));
}

// Tests that a float animation with two keyframes works as expected.
TEST(WebFloatAnimationCurveTest, TwoFloatKeyframe)
{
    scoped_ptr<CompositorFloatAnimationCurve> curve(new CompositorFloatAnimationCurve);
    curve->add(CompositorFloatKeyframe(0, 2),
        CompositorAnimationCurve::TimingFunctionTypeLinear);
    curve->add(CompositorFloatKeyframe(1, 4),
        CompositorAnimationCurve::TimingFunctionTypeLinear);
    EXPECT_FLOAT_EQ(2, curve->getValue(-1));
    EXPECT_FLOAT_EQ(2, curve->getValue(0));
    EXPECT_FLOAT_EQ(3, curve->getValue(0.5));
    EXPECT_FLOAT_EQ(4, curve->getValue(1));
    EXPECT_FLOAT_EQ(4, curve->getValue(2));
}

// Tests that a float animation with three keyframes works as expected.
TEST(WebFloatAnimationCurveTest, ThreeFloatKeyframe)
{
    scoped_ptr<CompositorFloatAnimationCurve> curve(new CompositorFloatAnimationCurve);
    curve->add(CompositorFloatKeyframe(0, 2),
        CompositorAnimationCurve::TimingFunctionTypeLinear);
    curve->add(CompositorFloatKeyframe(1, 4),
        CompositorAnimationCurve::TimingFunctionTypeLinear);
    curve->add(CompositorFloatKeyframe(2, 8),
        CompositorAnimationCurve::TimingFunctionTypeLinear);
    EXPECT_FLOAT_EQ(2, curve->getValue(-1));
    EXPECT_FLOAT_EQ(2, curve->getValue(0));
    EXPECT_FLOAT_EQ(3, curve->getValue(0.5));
    EXPECT_FLOAT_EQ(4, curve->getValue(1));
    EXPECT_FLOAT_EQ(6, curve->getValue(1.5));
    EXPECT_FLOAT_EQ(8, curve->getValue(2));
    EXPECT_FLOAT_EQ(8, curve->getValue(3));
}

// Tests that a float animation with multiple keys at a given time works sanely.
TEST(WebFloatAnimationCurveTest, RepeatedFloatKeyTimes)
{
    scoped_ptr<CompositorFloatAnimationCurve> curve(new CompositorFloatAnimationCurve);
    curve->add(CompositorFloatKeyframe(0, 4),
        CompositorAnimationCurve::TimingFunctionTypeLinear);
    curve->add(CompositorFloatKeyframe(1, 4),
        CompositorAnimationCurve::TimingFunctionTypeLinear);
    curve->add(CompositorFloatKeyframe(1, 6),
        CompositorAnimationCurve::TimingFunctionTypeLinear);
    curve->add(CompositorFloatKeyframe(2, 6),
        CompositorAnimationCurve::TimingFunctionTypeLinear);

    EXPECT_FLOAT_EQ(4, curve->getValue(-1));
    EXPECT_FLOAT_EQ(4, curve->getValue(0));
    EXPECT_FLOAT_EQ(4, curve->getValue(0.5));

    // There is a discontinuity at 1. Any value between 4 and 6 is valid.
    float value = curve->getValue(1);
    EXPECT_TRUE(value >= 4 && value <= 6);

    EXPECT_FLOAT_EQ(6, curve->getValue(1.5));
    EXPECT_FLOAT_EQ(6, curve->getValue(2));
    EXPECT_FLOAT_EQ(6, curve->getValue(3));
}

// Tests that the keyframes may be added out of order.
TEST(WebFloatAnimationCurveTest, UnsortedKeyframes)
{
    scoped_ptr<CompositorFloatAnimationCurve> curve(new CompositorFloatAnimationCurve);
    curve->add(CompositorFloatKeyframe(2, 8),
        CompositorAnimationCurve::TimingFunctionTypeLinear);
    curve->add(CompositorFloatKeyframe(0, 2),
        CompositorAnimationCurve::TimingFunctionTypeLinear);
    curve->add(CompositorFloatKeyframe(1, 4),
        CompositorAnimationCurve::TimingFunctionTypeLinear);

    EXPECT_FLOAT_EQ(2, curve->getValue(-1));
    EXPECT_FLOAT_EQ(2, curve->getValue(0));
    EXPECT_FLOAT_EQ(3, curve->getValue(0.5));
    EXPECT_FLOAT_EQ(4, curve->getValue(1));
    EXPECT_FLOAT_EQ(6, curve->getValue(1.5));
    EXPECT_FLOAT_EQ(8, curve->getValue(2));
    EXPECT_FLOAT_EQ(8, curve->getValue(3));
}

// Tests that a cubic bezier timing function works as expected.
TEST(WebFloatAnimationCurveTest, CubicBezierTimingFunction)
{
    scoped_ptr<CompositorFloatAnimationCurve> curve(new CompositorFloatAnimationCurve);
    curve->add(CompositorFloatKeyframe(0, 0), 0.25, 0, 0.75, 1);
    curve->add(CompositorFloatKeyframe(1, 1),
        CompositorAnimationCurve::TimingFunctionTypeLinear);

    EXPECT_FLOAT_EQ(0, curve->getValue(0));
    EXPECT_LT(0, curve->getValue(0.25));
    EXPECT_GT(0.25, curve->getValue(0.25));
    EXPECT_NEAR(curve->getValue(0.5), 0.5, 0.00015);
    EXPECT_LT(0.75, curve->getValue(0.75));
    EXPECT_GT(1, curve->getValue(0.75));
    EXPECT_FLOAT_EQ(1, curve->getValue(1));
}

// Tests that an ease timing function works as expected.
TEST(WebFloatAnimationCurveTest, EaseTimingFunction)
{
    scoped_ptr<CompositorFloatAnimationCurve> curve(new CompositorFloatAnimationCurve);
    curve->add(CompositorFloatKeyframe(0, 0),
        CompositorAnimationCurve::TimingFunctionTypeEase);
    curve->add(CompositorFloatKeyframe(1, 1),
        CompositorAnimationCurve::TimingFunctionTypeLinear);

    scoped_ptr<cc::TimingFunction> timingFunction(
        cc::EaseTimingFunction::Create());
    for (int i = 0; i <= 4; ++i) {
        const double time = i * 0.25;
        EXPECT_FLOAT_EQ(timingFunction->GetValue(time), curve->getValue(time));
    }
}

// Tests using a linear timing function.
TEST(WebFloatAnimationCurveTest, LinearTimingFunction)
{
    scoped_ptr<CompositorFloatAnimationCurve> curve(new CompositorFloatAnimationCurve);
    curve->add(CompositorFloatKeyframe(0, 0),
        CompositorAnimationCurve::TimingFunctionTypeLinear);
    curve->add(CompositorFloatKeyframe(1, 1),
        CompositorAnimationCurve::TimingFunctionTypeLinear);

    for (int i = 0; i <= 4; ++i) {
        const double time = i * 0.25;
        EXPECT_FLOAT_EQ(time, curve->getValue(time));
    }
}

// Tests that an ease in timing function works as expected.
TEST(WebFloatAnimationCurveTest, EaseInTimingFunction)
{
    scoped_ptr<CompositorFloatAnimationCurve> curve(new CompositorFloatAnimationCurve);
    curve->add(CompositorFloatKeyframe(0, 0),
        CompositorAnimationCurve::TimingFunctionTypeEaseIn);
    curve->add(CompositorFloatKeyframe(1, 1),
        CompositorAnimationCurve::TimingFunctionTypeLinear);

    scoped_ptr<cc::TimingFunction> timingFunction(
        cc::EaseInTimingFunction::Create());
    for (int i = 0; i <= 4; ++i) {
        const double time = i * 0.25;
        EXPECT_FLOAT_EQ(timingFunction->GetValue(time), curve->getValue(time));
    }
}

// Tests that an ease in timing function works as expected.
TEST(WebFloatAnimationCurveTest, EaseOutTimingFunction)
{
    scoped_ptr<CompositorFloatAnimationCurve> curve(new CompositorFloatAnimationCurve);
    curve->add(CompositorFloatKeyframe(0, 0),
        CompositorAnimationCurve::TimingFunctionTypeEaseOut);
    curve->add(CompositorFloatKeyframe(1, 1),
        CompositorAnimationCurve::TimingFunctionTypeLinear);

    scoped_ptr<cc::TimingFunction> timingFunction(
        cc::EaseOutTimingFunction::Create());
    for (int i = 0; i <= 4; ++i) {
        const double time = i * 0.25;
        EXPECT_FLOAT_EQ(timingFunction->GetValue(time), curve->getValue(time));
    }
}

// Tests that an ease in timing function works as expected.
TEST(WebFloatAnimationCurveTest, EaseInOutTimingFunction)
{
    scoped_ptr<CompositorFloatAnimationCurve> curve(new CompositorFloatAnimationCurve);
    curve->add(CompositorFloatKeyframe(0, 0),
        CompositorAnimationCurve::TimingFunctionTypeEaseInOut);
    curve->add(CompositorFloatKeyframe(1, 1),
        CompositorAnimationCurve::TimingFunctionTypeLinear);

    scoped_ptr<cc::TimingFunction> timingFunction(
        cc::EaseInOutTimingFunction::Create());
    for (int i = 0; i <= 4; ++i) {
        const double time = i * 0.25;
        EXPECT_FLOAT_EQ(timingFunction->GetValue(time), curve->getValue(time));
    }
}

// Tests that an ease in timing function works as expected.
TEST(WebFloatAnimationCurveTest, CustomBezierTimingFunction)
{
    scoped_ptr<CompositorFloatAnimationCurve> curve(new CompositorFloatAnimationCurve);
    double x1 = 0.3;
    double y1 = 0.2;
    double x2 = 0.8;
    double y2 = 0.7;
    curve->add(CompositorFloatKeyframe(0, 0), x1, y1, x2, y2);
    curve->add(CompositorFloatKeyframe(1, 1),
        CompositorAnimationCurve::TimingFunctionTypeLinear);

    scoped_ptr<cc::TimingFunction> timingFunction(
        cc::CubicBezierTimingFunction::Create(x1, y1, x2, y2));
    for (int i = 0; i <= 4; ++i) {
        const double time = i * 0.25;
        EXPECT_FLOAT_EQ(timingFunction->GetValue(time), curve->getValue(time));
    }
}

// Tests that the default timing function is indeed ease.
TEST(WebFloatAnimationCurveTest, DefaultTimingFunction)
{
    scoped_ptr<CompositorFloatAnimationCurve> curve(new CompositorFloatAnimationCurve);
    curve->add(CompositorFloatKeyframe(0, 0));
    curve->add(CompositorFloatKeyframe(1, 1),
        CompositorAnimationCurve::TimingFunctionTypeLinear);

    scoped_ptr<cc::TimingFunction> timingFunction(
        cc::EaseTimingFunction::Create());
    for (int i = 0; i <= 4; ++i) {
        const double time = i * 0.25;
        EXPECT_FLOAT_EQ(timingFunction->GetValue(time), curve->getValue(time));
    }
}

} // namespace blink
