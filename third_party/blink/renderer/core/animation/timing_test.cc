// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/timing.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class AnimationTimingTest : public testing::Test {
 public:
  Timing::CalculatedTiming CalculateTimings(base::Optional<double> local_time,
                                            double playback_rate) {
    const bool is_keyframe_effect = false;
    Timing::AnimationDirection animation_direction =
        playback_rate < 0 ? Timing::AnimationDirection::kBackwards
                          : Timing::AnimationDirection::kForwards;
    return timing_.CalculateTimings(local_time, animation_direction,
                                    is_keyframe_effect, playback_rate);
  }
  bool IsCurrent(base::Optional<double> local_time, double playback_rate) {
    return CalculateTimings(local_time, playback_rate).is_current;
  }

 private:
  void SetUp() override {
    timing_.iteration_duration = AnimationTimeDelta::FromSecondsD(1);
  }
  Timing timing_;
};

TEST_F(AnimationTimingTest, IsCurrent) {
  // https://drafts.csswg.org/web-animations-1/#animation-effect-phases-and-states
  // Before boundary time = 0, and after boundary time = 1 based on test setup
  // with start delay = 0 and iteration duration = 1.
  // An animation effect is current if any of the following conditions are true:
  //   * the animation effect is in play, or
  //   * the animation effect is associated with an animation with a playback
  //     rate > 0 and the animation effect is in the before phase, or
  //   * the animation effect is associated with an animation with a playback
  //     rate < 0 and the animation effect is in the after phase.

  EXPECT_TRUE(IsCurrent(-1, 1))
      << "Expected 'current' with a positive playback rate in the before phase";
  EXPECT_FALSE(IsCurrent(-1, 0))
      << "Expected 'not current' with a zero playback rate in the before phase";
  EXPECT_FALSE(IsCurrent(-1, -1))
      << "Expected 'not current' with a negative playback rate in the before "
      << "phase";

  EXPECT_TRUE(IsCurrent(0, 1))
      << "Expected 'current' with a positive playback rate at the start of the "
      << " active phase";
  EXPECT_TRUE(IsCurrent(0, 0))
      << "Expected 'current' with a zero playback rate at the start of the "
      << "active phase";
  EXPECT_FALSE(IsCurrent(0, -1))
      << "Expected 'not current' with a negative playback rate at the end of "
      << "the before phase";

  EXPECT_TRUE(IsCurrent(0.5, 1))
      << "Expected 'current' with a positive playback rate in the active phase";
  EXPECT_TRUE(IsCurrent(0.5, 0))
      << "Expected 'current' with a zero playback rate in the active phase";
  EXPECT_TRUE(IsCurrent(0.5, -1))
      << "Expected 'current' with a negative playback rate in the active phase";

  EXPECT_FALSE(IsCurrent(1, 1))
      << "Expected 'not current' with a positive playback rate at the start "
      << "of the after phase";
  EXPECT_FALSE(IsCurrent(1, 0))
      << "Expected 'not current' with a zero playback rate at the start of "
      << "the after phase";
  EXPECT_TRUE(IsCurrent(1, -1))
      << "Expected 'current' with negative playback rate at the end of the "
      << "active phase";

  EXPECT_FALSE(IsCurrent(2, 1))
      << "Expected 'not current' with a positive playback rate in the after "
      << "phase";
  EXPECT_FALSE(IsCurrent(2, 0))
      << "Expected 'not current' with a zero playback rate in the after phase";
  EXPECT_TRUE(IsCurrent(2, -1))
      << "Expected 'current' with a negative playback rate in the after phase";

  EXPECT_FALSE(IsCurrent(base::nullopt, 1))
      << "Expected 'not current' when time is unresolved";
  EXPECT_FALSE(IsCurrent(base::nullopt, 0))
      << "Expected 'not current' when time is unresolved";
  EXPECT_FALSE(IsCurrent(base::nullopt, -1))
      << "Expected 'not current' when time is unresolved";
}

}  // namespace blink
