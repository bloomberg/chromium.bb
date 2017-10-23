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

}  // namespace keyboard
