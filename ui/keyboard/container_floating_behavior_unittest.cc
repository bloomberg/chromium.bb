// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/keyboard/container_floating_behavior.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"

namespace keyboard {

class ContainerFloatingBehaviorTest : public testing::Test {
 public:
  ContainerFloatingBehaviorTest() {}
  ~ContainerFloatingBehaviorTest() override {}
};

TEST(ContainerFloatingBehaviorTest, AdjustSetBoundsRequest) {
  ContainerFloatingBehavior floating_behavior;

  const int keyboard_width = 600;
  const int keyboard_height = 70;

  gfx::Rect workspace(0, 0, 1000, 600);
  gfx::Rect center(100, 100, keyboard_width, keyboard_height);
  gfx::Rect top_left_overlap(-30, -30, keyboard_width, keyboard_height);
  gfx::Rect bottom_right_overlap(workspace.width() - 30,
                                 workspace.height() - 30, keyboard_width,
                                 keyboard_height);

  gfx::Rect result =
      floating_behavior.AdjustSetBoundsRequest(workspace, center);
  ASSERT_EQ(center, result);
  result =
      floating_behavior.AdjustSetBoundsRequest(workspace, top_left_overlap);
  ASSERT_EQ(gfx::Rect(0, 0, keyboard_width, keyboard_height), result);

  result =
      floating_behavior.AdjustSetBoundsRequest(workspace, bottom_right_overlap);
  ASSERT_EQ(gfx::Rect(workspace.width() - keyboard_width,
                      workspace.height() - keyboard_height, keyboard_width,
                      keyboard_height),
            result);
}

TEST(ContainerFloatingBehaviorTest, DontSaveCoordinatesUntilKeyboardMoved) {
  ContainerFloatingBehavior floating_behavior;

  const int keyboard_width = 600;
  const int keyboard_height = 70;

  gfx::Rect workspace(0, 0, 1000, 600);
  gfx::Rect default_load_location(0, 0, keyboard_width, keyboard_height);
  gfx::Rect center(100, 100, keyboard_width, keyboard_height);
  gfx::Rect initial_default(
      workspace.width() - keyboard_width - kDefaultDistanceFromScreenRight,
      workspace.height() - keyboard_height - kDefaultDistanceFromScreenBottom,
      keyboard_width, keyboard_height);

  // Adjust bounds to the default load location. Floating Behavior should use
  // the default location instead.
  gfx::Rect result = floating_behavior.AdjustSetBoundsRequest(
      workspace, default_load_location);
  ASSERT_EQ(initial_default, result);

  // Doing the same thing again should result in the same behavior, since the
  // values should not have been preserved.
  result = floating_behavior.AdjustSetBoundsRequest(workspace,
                                                    default_load_location);
  ASSERT_EQ(initial_default, result);

  // Move the keyboard somewhere else. This is clearly a user-driven move so the
  // value should be saved for later use.
  result = floating_behavior.AdjustSetBoundsRequest(workspace, center);
  ASSERT_EQ(center, result);

  // Move the keyboard to the default load location. Since the keyboard has
  // already been moved by the user, it is clear that this isn't a default
  // location and only matches by coincidence. The result is that the location
  // should be used as-is, not the default location.
  result = floating_behavior.AdjustSetBoundsRequest(workspace,
                                                    default_load_location);
  ASSERT_EQ(default_load_location, result);
}

}  // namespace keyboard
