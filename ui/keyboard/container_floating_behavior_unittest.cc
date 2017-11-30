// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/keyboard/container_floating_behavior.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d.h"

namespace keyboard {

class ContainerFloatingBehaviorTest : public testing::Test {
 public:
  ContainerFloatingBehaviorTest() {}
  ~ContainerFloatingBehaviorTest() override {}
};

TEST(ContainerFloatingBehaviorTest, AdjustSetBoundsRequest) {
  ContainerFloatingBehavior floating_behavior(nullptr);

  const int keyboard_width = 600;
  const int keyboard_height = 70;

  gfx::Rect workspace(0, 0, 1000, 600);
  gfx::Rect center(100, 100, keyboard_width, keyboard_height);
  gfx::Rect top_left_overlap(-30, -30, keyboard_width, keyboard_height);
  gfx::Rect bottom_right_overlap(workspace.width() - 30,
                                 workspace.height() - 30, keyboard_width,
                                 keyboard_height);

  // Save an arbitrary position so that default location will not be used.
  floating_behavior.SavePosition(gfx::Point(0, 0));

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
  ContainerFloatingBehavior floating_behavior(nullptr);

  const int keyboard_width = 600;
  const int keyboard_height = 70;

  gfx::Rect workspace(0, 0, 1000, 600);
  gfx::Rect top_left(0, 0, keyboard_width, keyboard_height);
  gfx::Rect center(100, 100, keyboard_width, keyboard_height);
  gfx::Rect initial_default(
      workspace.width() - keyboard_width - kDefaultDistanceFromScreenRight,
      workspace.height() - keyboard_height - kDefaultDistanceFromScreenBottom,
      keyboard_width, keyboard_height);

  // Adjust bounds to the arbitrary load location. Floating Behavior should use
  // the UX-chosen default location instead.
  gfx::Rect result =
      floating_behavior.AdjustSetBoundsRequest(workspace, top_left);
  ASSERT_EQ(initial_default, result);

  // Doing the same thing again should result in the same behavior, since the
  // values should not have been preserved.
  result = floating_behavior.AdjustSetBoundsRequest(workspace, top_left);
  ASSERT_EQ(initial_default, result);

  // Simulate the user clicking and moving the keyboard to some arbitrary
  // location (it doesn't matter where). Now that the coordinate is known to be
  // user-determined.
  floating_behavior.SavePosition(gfx::Point(10, 10));

  // Move the keyboard somewhere else. The coordinates should be taken as-is
  // without being adjusted.
  result = floating_behavior.AdjustSetBoundsRequest(workspace, center);
  ASSERT_EQ(center, result);
}

TEST(ContainerFloatingBehaviorTest, SetDraggableArea) {
  ContainerFloatingBehavior floating_behavior(nullptr);
  ASSERT_FALSE(
      floating_behavior.IsDragHandle(gfx::Vector2d(2, 2), gfx::Size(600, 600)));
  floating_behavior.SetDraggableArea(gfx::Rect(0, 0, 10, 10));
  ASSERT_TRUE(
      floating_behavior.IsDragHandle(gfx::Vector2d(2, 2), gfx::Size(600, 600)));
}

}  // namespace keyboard
