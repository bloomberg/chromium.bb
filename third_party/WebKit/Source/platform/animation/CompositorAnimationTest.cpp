// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/animation/CompositorAnimation.h"

#include "platform/animation/CompositorFloatAnimationCurve.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(WebCompositorAnimationTest, DefaultSettings)
{
    scoped_ptr<CompositorAnimationCurve> curve(new CompositorFloatAnimationCurve());
    scoped_ptr<CompositorAnimation> animation(new CompositorAnimation(
        *curve, CompositorAnimation::TargetPropertyOpacity, 1, 0));

    // Ensure that the defaults are correct.
    EXPECT_EQ(1, animation->iterations());
    EXPECT_EQ(0, animation->startTime());
    EXPECT_EQ(0, animation->timeOffset());
    EXPECT_EQ(CompositorAnimation::DirectionNormal, animation->direction());
}

TEST(WebCompositorAnimationTest, ModifiedSettings)
{
    scoped_ptr<CompositorFloatAnimationCurve> curve(new CompositorFloatAnimationCurve());
    scoped_ptr<CompositorAnimation> animation(new CompositorAnimation(
        *curve, CompositorAnimation::TargetPropertyOpacity, 1, 0));
    animation->setIterations(2);
    animation->setStartTime(2);
    animation->setTimeOffset(2);
    animation->setDirection(CompositorAnimation::DirectionReverse);

    EXPECT_EQ(2, animation->iterations());
    EXPECT_EQ(2, animation->startTime());
    EXPECT_EQ(2, animation->timeOffset());
    EXPECT_EQ(CompositorAnimation::DirectionReverse, animation->direction());
}

} // namespace blink
