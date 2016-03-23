/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/animation/UnitBezier.h"

#include "base/memory/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(UnitBezierTest, BasicUse)
{
    UnitBezier bezier(0.5, 1.0, 0.5, 1.0);
    EXPECT_EQ(0.875, bezier.solve(0.5));
}

TEST(UnitBezierTest, Overshoot)
{
    UnitBezier bezier(0.5, 2.0, 0.5, 2.0);
    EXPECT_EQ(1.625, bezier.solve(0.5));
}

TEST(UnitBezierTest, Undershoot)
{
    UnitBezier bezier(0.5, -1.0, 0.5, -1.0);
    EXPECT_EQ(-0.625, bezier.solve(0.5));
}

TEST(UnitBezierTest, InputAtEdgeOfRange)
{
    UnitBezier bezier(0.5, 1.0, 0.5, 1.0);
    EXPECT_EQ(0.0, bezier.solve(0.0));
    EXPECT_EQ(1.0, bezier.solve(1.0));
}

TEST(UnitBezierTest, InputOutOfRange)
{
    UnitBezier bezier(0.5, 1.0, 0.5, 1.0);
    EXPECT_EQ(-2.0, bezier.solve(-1.0));
    EXPECT_EQ(1.0, bezier.solve(2.0));
}

TEST(UnitBezierTest, InputOutOfRangeLargeEpsilon)
{
    UnitBezier bezier(0.5, 1.0, 0.5, 1.0);
    EXPECT_EQ(-2.0, bezier.solveWithEpsilon(-1.0, 1.0));
    EXPECT_EQ(1.0, bezier.solveWithEpsilon(2.0, 1.0));
}

TEST(UnitBezierTest, InputOutOfRangeCoincidentEndpoints)
{
    UnitBezier bezier(0.0, 0.0, 1.0, 1.0);
    EXPECT_EQ(-1.0, bezier.solve(-1.0));
    EXPECT_EQ(2.0, bezier.solve(2.0));
}

TEST(UnitBezierTest, InputOutOfRangeVerticalGradient)
{
    UnitBezier bezier(0.0, 1.0, 1.0, 0.0);
    EXPECT_EQ(0.0, bezier.solve(-1.0));
    EXPECT_EQ(1.0, bezier.solve(2.0));
}

TEST(UnitBezierTest, InputOutOfRangeDistinctEndpoints)
{
    UnitBezier bezier(0.1, 0.2, 0.8, 0.8);
    EXPECT_EQ(-2.0, bezier.solve(-1.0));
    EXPECT_EQ(2.0, bezier.solve(2.0));
}

TEST(UnitBezierTest, Range)
{
    double epsilon = 0.00015;
    double min, max;

    // Derivative is a constant.
    scoped_ptr<UnitBezier> bezier(
        new UnitBezier(0.25, (1.0 / 3.0), 0.75, (2.0 / 3.0)));
    bezier->range(&min, &max);
    EXPECT_EQ(0, min);
    EXPECT_EQ(1, max);

    // Derivative is linear.
    bezier.reset(new UnitBezier(0.25, -0.5, 0.75, (-1.0 / 6.0)));
    bezier->range(&min, &max);
    EXPECT_NEAR(min, -0.225, epsilon);
    EXPECT_EQ(1, max);

    // Derivative has no real roots.
    bezier.reset(new UnitBezier(0.25, 0.25, 0.75, 0.5));
    bezier->range(&min, &max);
    EXPECT_EQ(0, min);
    EXPECT_EQ(1, max);

    // Derivative has exactly one real root.
    bezier.reset(new UnitBezier(0.0, 1.0, 1.0, 0.0));
    bezier->range(&min, &max);
    EXPECT_EQ(0, min);
    EXPECT_EQ(1, max);

    // Derivative has one root < 0 and one root > 1.
    bezier.reset(new UnitBezier(0.25, 0.1, 0.75, 0.9));
    bezier->range(&min, &max);
    EXPECT_EQ(0, min);
    EXPECT_EQ(1, max);

    // Derivative has two roots in [0,1].
    bezier.reset(new UnitBezier(0.25, 2.5, 0.75, 0.5));
    bezier->range(&min, &max);
    EXPECT_EQ(0, min);
    EXPECT_NEAR(max, 1.28818, epsilon);
    bezier.reset(new UnitBezier(0.25, 0.5, 0.75, -1.5));
    bezier->range(&min, &max);
    EXPECT_NEAR(min, -0.28818, epsilon);
    EXPECT_EQ(1, max);

    // Derivative has one root < 0 and one root in [0,1].
    bezier.reset(new UnitBezier(0.25, 0.1, 0.75, 1.5));
    bezier->range(&min, &max);
    EXPECT_EQ(0, min);
    EXPECT_NEAR(max, 1.10755, epsilon);

    // Derivative has one root in [0,1] and one root > 1.
    bezier.reset(new UnitBezier(0.25, -0.5, 0.75, 0.9));
    bezier->range(&min, &max);
    EXPECT_NEAR(min, -0.10755, epsilon);
    EXPECT_EQ(1, max);

    // Derivative has two roots < 0.
    bezier.reset(new UnitBezier(0.25, 0.3, 0.75, 0.633));
    bezier->range(&min, &max);
    EXPECT_EQ(0, min);
    EXPECT_EQ(1, max);

    // Derivative has two roots > 1.
    bezier.reset(new UnitBezier(0.25, 0.367, 0.75, 0.7));
    bezier->range(&min, &max);
    EXPECT_EQ(0.f, min);
    EXPECT_EQ(1.f, max);
}

TEST(UnitBezierTest, Slope)
{
    UnitBezier bezier(0.25, 0.0, 0.75, 1.0);

    double epsilon = 0.00015;

    EXPECT_NEAR(bezier.slope(0), 0, epsilon);
    EXPECT_NEAR(bezier.slope(0.05), 0.42170, epsilon);
    EXPECT_NEAR(bezier.slope(0.1), 0.69778, epsilon);
    EXPECT_NEAR(bezier.slope(0.15), 0.89121, epsilon);
    EXPECT_NEAR(bezier.slope(0.2), 1.03184, epsilon);
    EXPECT_NEAR(bezier.slope(0.25), 1.13576, epsilon);
    EXPECT_NEAR(bezier.slope(0.3), 1.21239, epsilon);
    EXPECT_NEAR(bezier.slope(0.35), 1.26751, epsilon);
    EXPECT_NEAR(bezier.slope(0.4), 1.30474, epsilon);
    EXPECT_NEAR(bezier.slope(0.45), 1.32628, epsilon);
    EXPECT_NEAR(bezier.slope(0.5), 1.33333, epsilon);
    EXPECT_NEAR(bezier.slope(0.55), 1.32628, epsilon);
    EXPECT_NEAR(bezier.slope(0.6), 1.30474, epsilon);
    EXPECT_NEAR(bezier.slope(0.65), 1.26751, epsilon);
    EXPECT_NEAR(bezier.slope(0.7), 1.21239, epsilon);
    EXPECT_NEAR(bezier.slope(0.75), 1.13576, epsilon);
    EXPECT_NEAR(bezier.slope(0.8), 1.03184, epsilon);
    EXPECT_NEAR(bezier.slope(0.85), 0.89121, epsilon);
    EXPECT_NEAR(bezier.slope(0.9), 0.69778, epsilon);
    EXPECT_NEAR(bezier.slope(0.95), 0.42170, epsilon);
    EXPECT_NEAR(bezier.slope(1), 0, epsilon);
}

} // namespace blink
