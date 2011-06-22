// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/memory/scoped_ptr.h>
#include <gtest/gtest.h>

#include "gestures/include/gestures.h"

namespace gestures {

class GesturesTest : public ::testing::Test {};

TEST(GesturesTest, SimpleTest) {
  // Simple allocate/free test
  scoped_ptr<GestureInterpreter> gs(NewGestureInterpreter());
  EXPECT_NE(static_cast<GestureInterpreter*>(NULL), gs.get());
}

TEST(GesturesTest, CtorTest) {
  Gesture move_gs(kGestureMove, 2, 3, 4.0, 5.0);
  EXPECT_EQ(move_gs.type, kGestureTypeMove);
  EXPECT_EQ(move_gs.start_time, 2);
  EXPECT_EQ(move_gs.end_time, 3);
  EXPECT_EQ(move_gs.details.move.dx, 4.0);
  EXPECT_EQ(move_gs.details.move.dy, 5.0);

  Gesture scroll_gs(kGestureScroll, 2, 3, 4.0, 5.0);
  EXPECT_EQ(scroll_gs.type, kGestureTypeScroll);
  EXPECT_EQ(scroll_gs.start_time, 2);
  EXPECT_EQ(scroll_gs.end_time, 3);
  EXPECT_EQ(scroll_gs.details.scroll.dx, 4.0);
  EXPECT_EQ(scroll_gs.details.scroll.dy, 5.0);

  Gesture bdown_gs(kGestureButtonsChange, 2, 3, GESTURES_BUTTON_LEFT, 0);
  EXPECT_EQ(bdown_gs.type, kGestureTypeButtonsChange);
  EXPECT_EQ(bdown_gs.start_time, 2);
  EXPECT_EQ(bdown_gs.end_time, 3);
  EXPECT_EQ(bdown_gs.details.buttons.down, GESTURES_BUTTON_LEFT);
  EXPECT_EQ(bdown_gs.details.buttons.up, 0);

  Gesture bup_gs(kGestureButtonsChange, 2, 3, 0, GESTURES_BUTTON_LEFT);
  EXPECT_EQ(bup_gs.type, kGestureTypeButtonsChange);
  EXPECT_EQ(bup_gs.start_time, 2);
  EXPECT_EQ(bup_gs.end_time, 3);
  EXPECT_EQ(bup_gs.details.buttons.down, 0);
  EXPECT_EQ(bup_gs.details.buttons.up, GESTURES_BUTTON_LEFT);

  Gesture bdownup_gs(
      kGestureButtonsChange, 2, 3, GESTURES_BUTTON_LEFT, GESTURES_BUTTON_LEFT);
  EXPECT_EQ(bdownup_gs.type, kGestureTypeButtonsChange);
  EXPECT_EQ(bdownup_gs.start_time, 2);
  EXPECT_EQ(bdownup_gs.end_time, 3);
  EXPECT_EQ(bdownup_gs.details.buttons.down, GESTURES_BUTTON_LEFT);
  EXPECT_EQ(bdownup_gs.details.buttons.up, GESTURES_BUTTON_LEFT);
}

TEST(GesturesTest, StimeFromTimevalTest) {
  struct timeval tv;
  tv.tv_sec = 3;
  tv.tv_usec = 88;
  EXPECT_EQ(3.000088, StimeFromTimeval(&tv));
  tv.tv_sec = 2000000000;
  tv.tv_usec = 999999;
  EXPECT_EQ(2000000000.999999, StimeFromTimeval(&tv));
}


}  // namespace gestures
