// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/animation/CompositorAnimation.h"

#include "platform/animation/CompositorFloatAnimationCurve.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(WebCompositorAnimationTest, DefaultSettings) {
  std::unique_ptr<CompositorAnimationCurve> curve =
      CompositorFloatAnimationCurve::Create();
  std::unique_ptr<CompositorAnimation> animation = CompositorAnimation::Create(
      *curve, CompositorTargetProperty::OPACITY, 1, 0);

  // Ensure that the defaults are correct.
  EXPECT_EQ(1, animation->Iterations());
  EXPECT_EQ(0, animation->StartTime());
  EXPECT_EQ(0, animation->TimeOffset());
  EXPECT_EQ(CompositorAnimation::Direction::NORMAL, animation->GetDirection());
}

TEST(WebCompositorAnimationTest, ModifiedSettings) {
  std::unique_ptr<CompositorFloatAnimationCurve> curve =
      CompositorFloatAnimationCurve::Create();
  std::unique_ptr<CompositorAnimation> animation = CompositorAnimation::Create(
      *curve, CompositorTargetProperty::OPACITY, 1, 0);
  animation->SetIterations(2);
  animation->SetStartTime(2);
  animation->SetTimeOffset(2);
  animation->SetDirection(CompositorAnimation::Direction::REVERSE);

  EXPECT_EQ(2, animation->Iterations());
  EXPECT_EQ(2, animation->StartTime());
  EXPECT_EQ(2, animation->TimeOffset());
  EXPECT_EQ(CompositorAnimation::Direction::REVERSE, animation->GetDirection());
}

}  // namespace blink
