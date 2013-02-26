// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/compositor_bindings/web_animation_impl.h"
#include "webkit/compositor_bindings/web_float_animation_curve_impl.h"

using namespace WebKit;

namespace {

TEST(WebAnimationTest, DefaultSettings) {
  scoped_ptr<WebAnimationCurve> curve(new WebFloatAnimationCurveImpl());
  scoped_ptr<WebAnimation> animation(
      new WebAnimationImpl(*curve, WebAnimation::TargetPropertyOpacity, 1));

  // Ensure that the defaults are correct.
  EXPECT_EQ(1, animation->iterations());
  EXPECT_EQ(0, animation->startTime());
  EXPECT_EQ(0, animation->timeOffset());
  EXPECT_FALSE(animation->alternatesDirection());
}

TEST(WebAnimationTest, ModifiedSettings) {
  scoped_ptr<WebFloatAnimationCurve> curve(new WebFloatAnimationCurveImpl());
  scoped_ptr<WebAnimation> animation(
      new WebAnimationImpl(*curve, WebAnimation::TargetPropertyOpacity, 1));
  animation->setIterations(2);
  animation->setStartTime(2);
  animation->setTimeOffset(2);
  animation->setAlternatesDirection(true);

  EXPECT_EQ(2, animation->iterations());
  EXPECT_EQ(2, animation->startTime());
  EXPECT_EQ(2, animation->timeOffset());
  EXPECT_TRUE(animation->alternatesDirection());
}

}  // namespace
