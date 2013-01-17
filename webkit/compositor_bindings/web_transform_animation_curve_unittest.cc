// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "cc/timing_function.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebTransformationMatrix.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebTransformOperations.h"
#include "webkit/compositor_bindings/web_transform_animation_curve_impl.h"
#include "webkit/compositor_bindings/web_transform_operations_impl.h"

using namespace WebKit;

#if WEB_TRANSFORM_OPERATIONS_IS_VIRTUAL
using webkit::WebTransformOperationsImpl;
#endif

namespace {

// Tests that a transform animation with one keyframe works as expected.
TEST(WebTransformAnimationCurveTest, OneTransformKeyframe)
{
    scoped_ptr<WebTransformAnimationCurve> curve(new WebTransformAnimationCurveImpl);
#if WEB_TRANSFORM_OPERATIONS_IS_VIRTUAL
    scoped_ptr<WebTransformOperations> operations(new WebTransformOperationsImpl());
    operations->appendTranslate(2, 0, 0);
    curve->add(WebTransformKeyframe(0, operations.release()), WebAnimationCurve::TimingFunctionTypeLinear);
#else
    WebTransformOperations operations;
    operations.appendTranslate(2, 0, 0);
    curve->add(WebTransformKeyframe(0, operations), WebAnimationCurve::TimingFunctionTypeLinear);
#endif

    EXPECT_FLOAT_EQ(2, curve->getValue(-1).m41());
    EXPECT_FLOAT_EQ(2, curve->getValue(0).m41());
    EXPECT_FLOAT_EQ(2, curve->getValue(0.5).m41());
    EXPECT_FLOAT_EQ(2, curve->getValue(1).m41());
    EXPECT_FLOAT_EQ(2, curve->getValue(2).m41());
}

// Tests that a transform animation with two keyframes works as expected.
TEST(WebTransformAnimationCurveTest, TwoTransformKeyframe)
{
    scoped_ptr<WebTransformAnimationCurve> curve(new WebTransformAnimationCurveImpl);
#if WEB_TRANSFORM_OPERATIONS_IS_VIRTUAL
    scoped_ptr<WebTransformOperations> operations1(new WebTransformOperationsImpl());
    operations1->appendTranslate(2, 0, 0);
    scoped_ptr<WebTransformOperations> operations2(new WebTransformOperationsImpl());
    operations2->appendTranslate(4, 0, 0);
    curve->add(WebTransformKeyframe(0, operations1.release()), WebAnimationCurve::TimingFunctionTypeLinear);
    curve->add(WebTransformKeyframe(1, operations2.release()), WebAnimationCurve::TimingFunctionTypeLinear);
#else
    WebTransformOperations operations1;
    operations1.appendTranslate(2, 0, 0);
    WebTransformOperations operations2;
    operations2.appendTranslate(4, 0, 0);
    curve->add(WebTransformKeyframe(0, operations1), WebAnimationCurve::TimingFunctionTypeLinear);
    curve->add(WebTransformKeyframe(1, operations2), WebAnimationCurve::TimingFunctionTypeLinear);
#endif
    EXPECT_FLOAT_EQ(2, curve->getValue(-1).m41());
    EXPECT_FLOAT_EQ(2, curve->getValue(0).m41());
    EXPECT_FLOAT_EQ(3, curve->getValue(0.5).m41());
    EXPECT_FLOAT_EQ(4, curve->getValue(1).m41());
    EXPECT_FLOAT_EQ(4, curve->getValue(2).m41());
}

// Tests that a transform animation with three keyframes works as expected.
TEST(WebTransformAnimationCurveTest, ThreeTransformKeyframe)
{
    scoped_ptr<WebTransformAnimationCurve> curve(new WebTransformAnimationCurveImpl);
#if WEB_TRANSFORM_OPERATIONS_IS_VIRTUAL
    scoped_ptr<WebTransformOperations> operations1(new WebTransformOperationsImpl());
    operations1->appendTranslate(2, 0, 0);
    scoped_ptr<WebTransformOperations> operations2(new WebTransformOperationsImpl());
    operations2->appendTranslate(4, 0, 0);
    scoped_ptr<WebTransformOperations> operations3(new WebTransformOperationsImpl());
    operations3->appendTranslate(8, 0, 0);
    curve->add(WebTransformKeyframe(0, operations1.release()), WebAnimationCurve::TimingFunctionTypeLinear);
    curve->add(WebTransformKeyframe(1, operations2.release()), WebAnimationCurve::TimingFunctionTypeLinear);
    curve->add(WebTransformKeyframe(2, operations3.release()), WebAnimationCurve::TimingFunctionTypeLinear);
#else
    WebTransformOperations operations1;
    operations1.appendTranslate(2, 0, 0);
    WebTransformOperations operations2;
    operations2.appendTranslate(4, 0, 0);
    WebTransformOperations operations3;
    operations3.appendTranslate(8, 0, 0);
    curve->add(WebTransformKeyframe(0, operations1), WebAnimationCurve::TimingFunctionTypeLinear);
    curve->add(WebTransformKeyframe(1, operations2), WebAnimationCurve::TimingFunctionTypeLinear);
    curve->add(WebTransformKeyframe(2, operations3), WebAnimationCurve::TimingFunctionTypeLinear);
#endif
    EXPECT_FLOAT_EQ(2, curve->getValue(-1).m41());
    EXPECT_FLOAT_EQ(2, curve->getValue(0).m41());
    EXPECT_FLOAT_EQ(3, curve->getValue(0.5).m41());
    EXPECT_FLOAT_EQ(4, curve->getValue(1).m41());
    EXPECT_FLOAT_EQ(6, curve->getValue(1.5).m41());
    EXPECT_FLOAT_EQ(8, curve->getValue(2).m41());
    EXPECT_FLOAT_EQ(8, curve->getValue(3).m41());
}

// Tests that a transform animation with multiple keys at a given time works sanely.
TEST(WebTransformAnimationCurveTest, RepeatedTransformKeyTimes)
{
    // A step function.
    scoped_ptr<WebTransformAnimationCurve> curve(new WebTransformAnimationCurveImpl);
#if WEB_TRANSFORM_OPERATIONS_IS_VIRTUAL
    scoped_ptr<WebTransformOperations> operations1(new WebTransformOperationsImpl());
    operations1->appendTranslate(4, 0, 0);
    scoped_ptr<WebTransformOperations> operations2(new WebTransformOperationsImpl());
    operations2->appendTranslate(4, 0, 0);
    scoped_ptr<WebTransformOperations> operations3(new WebTransformOperationsImpl());
    operations3->appendTranslate(6, 0, 0);
    scoped_ptr<WebTransformOperations> operations4(new WebTransformOperationsImpl());
    operations4->appendTranslate(6, 0, 0);
    curve->add(WebTransformKeyframe(0, operations1.release()), WebAnimationCurve::TimingFunctionTypeLinear);
    curve->add(WebTransformKeyframe(1, operations2.release()), WebAnimationCurve::TimingFunctionTypeLinear);
    curve->add(WebTransformKeyframe(1, operations3.release()), WebAnimationCurve::TimingFunctionTypeLinear);
    curve->add(WebTransformKeyframe(2, operations4.release()), WebAnimationCurve::TimingFunctionTypeLinear);
#else
    WebTransformOperations operations1;
    operations1.appendTranslate(4, 0, 0);
    WebTransformOperations operations2;
    operations2.appendTranslate(4, 0, 0);
    WebTransformOperations operations3;
    operations3.appendTranslate(6, 0, 0);
    WebTransformOperations operations4;
    operations4.appendTranslate(6, 0, 0);
    curve->add(WebTransformKeyframe(0, operations1), WebAnimationCurve::TimingFunctionTypeLinear);
    curve->add(WebTransformKeyframe(1, operations2), WebAnimationCurve::TimingFunctionTypeLinear);
    curve->add(WebTransformKeyframe(1, operations3), WebAnimationCurve::TimingFunctionTypeLinear);
    curve->add(WebTransformKeyframe(2, operations4), WebAnimationCurve::TimingFunctionTypeLinear);
#endif

    EXPECT_FLOAT_EQ(4, curve->getValue(-1).m41());
    EXPECT_FLOAT_EQ(4, curve->getValue(0).m41());
    EXPECT_FLOAT_EQ(4, curve->getValue(0.5).m41());

    // There is a discontinuity at 1. Any value between 4 and 6 is valid.
    WebTransformationMatrix value = curve->getValue(1);
    EXPECT_TRUE(value.m41() >= 4 && value.m41() <= 6);

    EXPECT_FLOAT_EQ(6, curve->getValue(1.5).m41());
    EXPECT_FLOAT_EQ(6, curve->getValue(2).m41());
    EXPECT_FLOAT_EQ(6, curve->getValue(3).m41());
}

// Tests that the keyframes may be added out of order.
TEST(WebTransformAnimationCurveTest, UnsortedKeyframes)
{
    scoped_ptr<WebTransformAnimationCurve> curve(new WebTransformAnimationCurveImpl);
#if WEB_TRANSFORM_OPERATIONS_IS_VIRTUAL
    scoped_ptr<WebTransformOperations> operations1(new WebTransformOperationsImpl());
    operations1->appendTranslate(2, 0, 0);
    scoped_ptr<WebTransformOperations> operations2(new WebTransformOperationsImpl());
    operations2->appendTranslate(4, 0, 0);
    scoped_ptr<WebTransformOperations> operations3(new WebTransformOperationsImpl());
    operations3->appendTranslate(8, 0, 0);
    curve->add(WebTransformKeyframe(2, operations3.release()), WebAnimationCurve::TimingFunctionTypeLinear);
    curve->add(WebTransformKeyframe(0, operations1.release()), WebAnimationCurve::TimingFunctionTypeLinear);
    curve->add(WebTransformKeyframe(1, operations2.release()), WebAnimationCurve::TimingFunctionTypeLinear);
#else
    WebTransformOperations operations1;
    operations1.appendTranslate(2, 0, 0);
    WebTransformOperations operations2;
    operations2.appendTranslate(4, 0, 0);
    WebTransformOperations operations3;
    operations3.appendTranslate(8, 0, 0);
    curve->add(WebTransformKeyframe(2, operations3), WebAnimationCurve::TimingFunctionTypeLinear);
    curve->add(WebTransformKeyframe(0, operations1), WebAnimationCurve::TimingFunctionTypeLinear);
    curve->add(WebTransformKeyframe(1, operations2), WebAnimationCurve::TimingFunctionTypeLinear);
#endif

    EXPECT_FLOAT_EQ(2, curve->getValue(-1).m41());
    EXPECT_FLOAT_EQ(2, curve->getValue(0).m41());
    EXPECT_FLOAT_EQ(3, curve->getValue(0.5).m41());
    EXPECT_FLOAT_EQ(4, curve->getValue(1).m41());
    EXPECT_FLOAT_EQ(6, curve->getValue(1.5).m41());
    EXPECT_FLOAT_EQ(8, curve->getValue(2).m41());
    EXPECT_FLOAT_EQ(8, curve->getValue(3).m41());
}

// Tests that a cubic bezier timing function works as expected.
TEST(WebTransformAnimationCurveTest, CubicBezierTimingFunction)
{
    scoped_ptr<WebTransformAnimationCurve> curve(new WebTransformAnimationCurveImpl);
#if WEB_TRANSFORM_OPERATIONS_IS_VIRTUAL
    scoped_ptr<WebTransformOperations> operations1(new WebTransformOperationsImpl());
    operations1->appendTranslate(0, 0, 0);
    scoped_ptr<WebTransformOperations> operations2(new WebTransformOperationsImpl());
    operations2->appendTranslate(1, 0, 0);
    curve->add(WebTransformKeyframe(0, operations1.release()), 0.25, 0, 0.75, 1);
    curve->add(WebTransformKeyframe(1, operations2.release()), WebAnimationCurve::TimingFunctionTypeLinear);
#else
    WebTransformOperations operations1;
    operations1.appendTranslate(0, 0, 0);
    WebTransformOperations operations2;
    operations2.appendTranslate(1, 0, 0);
    curve->add(WebTransformKeyframe(0, operations1), 0.25, 0, 0.75, 1);
    curve->add(WebTransformKeyframe(1, operations2), WebAnimationCurve::TimingFunctionTypeLinear);
#endif
    EXPECT_FLOAT_EQ(0, curve->getValue(0).m41());
    EXPECT_LT(0, curve->getValue(0.25).m41());
    EXPECT_GT(0.25, curve->getValue(0.25).m41());
    EXPECT_NEAR(curve->getValue(0.5).m41(), 0.5, 0.00015);
    EXPECT_LT(0.75, curve->getValue(0.75).m41());
    EXPECT_GT(1, curve->getValue(0.75).m41());
    EXPECT_FLOAT_EQ(1, curve->getValue(1).m41());
}

// Tests that an ease timing function works as expected.
TEST(WebTransformAnimationCurveTest, EaseTimingFunction)
{
    scoped_ptr<WebTransformAnimationCurve> curve(new WebTransformAnimationCurveImpl);
#if WEB_TRANSFORM_OPERATIONS_IS_VIRTUAL
    scoped_ptr<WebTransformOperations> operations1(new WebTransformOperationsImpl());
    operations1->appendTranslate(0, 0, 0);
    scoped_ptr<WebTransformOperations> operations2(new WebTransformOperationsImpl());
    operations2->appendTranslate(1, 0, 0);
    curve->add(WebTransformKeyframe(0, operations1.release()), WebAnimationCurve::TimingFunctionTypeEase);
    curve->add(WebTransformKeyframe(1, operations2.release()), WebAnimationCurve::TimingFunctionTypeLinear);
#else
    WebTransformOperations operations1;
    operations1.appendTranslate(0, 0, 0);
    WebTransformOperations operations2;
    operations2.appendTranslate(1, 0, 0);
    curve->add(WebTransformKeyframe(0, operations1), WebAnimationCurve::TimingFunctionTypeEase);
    curve->add(WebTransformKeyframe(1, operations2), WebAnimationCurve::TimingFunctionTypeLinear);
#endif

    scoped_ptr<cc::TimingFunction> timingFunction(cc::EaseTimingFunction::create());
    for (int i = 0; i <= 4; ++i) {
        const double time = i * 0.25;
        EXPECT_FLOAT_EQ(timingFunction->getValue(time), curve->getValue(time).m41());
    }
}

// Tests using a linear timing function.
TEST(WebTransformAnimationCurveTest, LinearTimingFunction)
{
    scoped_ptr<WebTransformAnimationCurve> curve(new WebTransformAnimationCurveImpl);
#if WEB_TRANSFORM_OPERATIONS_IS_VIRTUAL
    scoped_ptr<WebTransformOperations> operations1(new WebTransformOperationsImpl());
    operations1->appendTranslate(0, 0, 0);
    scoped_ptr<WebTransformOperations> operations2(new WebTransformOperationsImpl());
    operations2->appendTranslate(1, 0, 0);
    curve->add(WebTransformKeyframe(0, operations1.release()), WebAnimationCurve::TimingFunctionTypeLinear);
    curve->add(WebTransformKeyframe(1, operations2.release()), WebAnimationCurve::TimingFunctionTypeLinear);
#else
    WebTransformOperations operations1;
    operations1.appendTranslate(0, 0, 0);
    WebTransformOperations operations2;
    operations2.appendTranslate(1, 0, 0);
    curve->add(WebTransformKeyframe(0, operations1), WebAnimationCurve::TimingFunctionTypeLinear);
    curve->add(WebTransformKeyframe(1, operations2), WebAnimationCurve::TimingFunctionTypeLinear);
#endif

    for (int i = 0; i <= 4; ++i) {
        const double time = i * 0.25;
        EXPECT_FLOAT_EQ(time, curve->getValue(time).m41());
    }
}

// Tests that an ease in timing function works as expected.
TEST(WebTransformAnimationCurveTest, EaseInTimingFunction)
{
    scoped_ptr<WebTransformAnimationCurve> curve(new WebTransformAnimationCurveImpl);
#if WEB_TRANSFORM_OPERATIONS_IS_VIRTUAL
    scoped_ptr<WebTransformOperations> operations1(new WebTransformOperationsImpl());
    operations1->appendTranslate(0, 0, 0);
    scoped_ptr<WebTransformOperations> operations2(new WebTransformOperationsImpl());
    operations2->appendTranslate(1, 0, 0);
    curve->add(WebTransformKeyframe(0, operations1.release()), WebAnimationCurve::TimingFunctionTypeEaseIn);
    curve->add(WebTransformKeyframe(1, operations2.release()), WebAnimationCurve::TimingFunctionTypeLinear);
#else
    WebTransformOperations operations1;
    operations1.appendTranslate(0, 0, 0);
    WebTransformOperations operations2;
    operations2.appendTranslate(1, 0, 0);
    curve->add(WebTransformKeyframe(0, operations1), WebAnimationCurve::TimingFunctionTypeEaseIn);
    curve->add(WebTransformKeyframe(1, operations2), WebAnimationCurve::TimingFunctionTypeLinear);
#endif

    scoped_ptr<cc::TimingFunction> timingFunction(cc::EaseInTimingFunction::create());
    for (int i = 0; i <= 4; ++i) {
        const double time = i * 0.25;
        EXPECT_FLOAT_EQ(timingFunction->getValue(time), curve->getValue(time).m41());
    }
}

// Tests that an ease in timing function works as expected.
TEST(WebTransformAnimationCurveTest, EaseOutTimingFunction)
{
    scoped_ptr<WebTransformAnimationCurve> curve(new WebTransformAnimationCurveImpl);
#if WEB_TRANSFORM_OPERATIONS_IS_VIRTUAL
    scoped_ptr<WebTransformOperations> operations1(new WebTransformOperationsImpl());
    operations1->appendTranslate(0, 0, 0);
    scoped_ptr<WebTransformOperations> operations2(new WebTransformOperationsImpl());
    operations2->appendTranslate(1, 0, 0);
    curve->add(WebTransformKeyframe(0, operations1.release()), WebAnimationCurve::TimingFunctionTypeEaseOut);
    curve->add(WebTransformKeyframe(1, operations2.release()), WebAnimationCurve::TimingFunctionTypeLinear);
#else
    WebTransformOperations operations1;
    operations1.appendTranslate(0, 0, 0);
    WebTransformOperations operations2;
    operations2.appendTranslate(1, 0, 0);
    curve->add(WebTransformKeyframe(0, operations1), WebAnimationCurve::TimingFunctionTypeEaseOut);
    curve->add(WebTransformKeyframe(1, operations2), WebAnimationCurve::TimingFunctionTypeLinear);
#endif

    scoped_ptr<cc::TimingFunction> timingFunction(cc::EaseOutTimingFunction::create());
    for (int i = 0; i <= 4; ++i) {
        const double time = i * 0.25;
        EXPECT_FLOAT_EQ(timingFunction->getValue(time), curve->getValue(time).m41());
    }
}

// Tests that an ease in timing function works as expected.
TEST(WebTransformAnimationCurveTest, EaseInOutTimingFunction)
{
    scoped_ptr<WebTransformAnimationCurve> curve(new WebTransformAnimationCurveImpl);
#if WEB_TRANSFORM_OPERATIONS_IS_VIRTUAL
    scoped_ptr<WebTransformOperations> operations1(new WebTransformOperationsImpl());
    operations1->appendTranslate(0, 0, 0);
    scoped_ptr<WebTransformOperations> operations2(new WebTransformOperationsImpl());
    operations2->appendTranslate(1, 0, 0);
    curve->add(WebTransformKeyframe(0, operations1.release()), WebAnimationCurve::TimingFunctionTypeEaseInOut);
    curve->add(WebTransformKeyframe(1, operations2.release()), WebAnimationCurve::TimingFunctionTypeLinear);
#else
    WebTransformOperations operations1;
    operations1.appendTranslate(0, 0, 0);
    WebTransformOperations operations2;
    operations2.appendTranslate(1, 0, 0);
    curve->add(WebTransformKeyframe(0, operations1), WebAnimationCurve::TimingFunctionTypeEaseInOut);
    curve->add(WebTransformKeyframe(1, operations2), WebAnimationCurve::TimingFunctionTypeLinear);
#endif

    scoped_ptr<cc::TimingFunction> timingFunction(cc::EaseInOutTimingFunction::create());
    for (int i = 0; i <= 4; ++i) {
        const double time = i * 0.25;
        EXPECT_FLOAT_EQ(timingFunction->getValue(time), curve->getValue(time).m41());
    }
}

// Tests that an ease in timing function works as expected.
TEST(WebTransformAnimationCurveTest, CustomBezierTimingFunction)
{
    scoped_ptr<WebTransformAnimationCurve> curve(new WebTransformAnimationCurveImpl);
    double x1 = 0.3;
    double y1 = 0.2;
    double x2 = 0.8;
    double y2 = 0.7;
#if WEB_TRANSFORM_OPERATIONS_IS_VIRTUAL
    scoped_ptr<WebTransformOperations> operations1(new WebTransformOperationsImpl());
    operations1->appendTranslate(0, 0, 0);
    scoped_ptr<WebTransformOperations> operations2(new WebTransformOperationsImpl());
    operations2->appendTranslate(1, 0, 0);
    curve->add(WebTransformKeyframe(0, operations1.release()), x1, y1, x2, y2);
    curve->add(WebTransformKeyframe(1, operations2.release()), WebAnimationCurve::TimingFunctionTypeLinear);
#else
    WebTransformOperations operations1;
    operations1.appendTranslate(0, 0, 0);
    WebTransformOperations operations2;
    operations2.appendTranslate(1, 0, 0);
    curve->add(WebTransformKeyframe(0, operations1), x1, y1, x2, y2);
    curve->add(WebTransformKeyframe(1, operations2), WebAnimationCurve::TimingFunctionTypeLinear);
#endif

    scoped_ptr<cc::TimingFunction> timingFunction(cc::CubicBezierTimingFunction::create(x1, y1, x2, y2));
    for (int i = 0; i <= 4; ++i) {
        const double time = i * 0.25;
        EXPECT_FLOAT_EQ(timingFunction->getValue(time), curve->getValue(time).m41());
    }
}

// Tests that the default timing function is indeed ease.
TEST(WebTransformAnimationCurveTest, DefaultTimingFunction)
{
    scoped_ptr<WebTransformAnimationCurve> curve(new WebTransformAnimationCurveImpl);
#if WEB_TRANSFORM_OPERATIONS_IS_VIRTUAL
    scoped_ptr<WebTransformOperations> operations1(new WebTransformOperationsImpl());
    operations1->appendTranslate(0, 0, 0);
    scoped_ptr<WebTransformOperations> operations2(new WebTransformOperationsImpl());
    operations2->appendTranslate(1, 0, 0);
    curve->add(WebTransformKeyframe(0, operations1.release()));
    curve->add(WebTransformKeyframe(1, operations2.release()), WebAnimationCurve::TimingFunctionTypeLinear);
#else
    WebTransformOperations operations1;
    operations1.appendTranslate(0, 0, 0);
    WebTransformOperations operations2;
    operations2.appendTranslate(1, 0, 0);
    curve->add(WebTransformKeyframe(0, operations1));
    curve->add(WebTransformKeyframe(1, operations2), WebAnimationCurve::TimingFunctionTypeLinear);
#endif

    scoped_ptr<cc::TimingFunction> timingFunction(cc::EaseTimingFunction::create());
    for (int i = 0; i <= 4; ++i) {
        const double time = i * 0.25;
        EXPECT_FLOAT_EQ(timingFunction->getValue(time), curve->getValue(time).m41());
    }
}

} // namespace
