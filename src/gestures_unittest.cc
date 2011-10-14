// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/logging.h>
#include <base/memory/scoped_ptr.h>
#include <gtest/gtest.h>

#include "gestures/include/gestures.h"

namespace gestures {

class GesturesTest : public ::testing::Test {};

TEST(GesturesTest, SameFingersAsTest) {
  FingerState finger_states[] = {
    // TM, Tm, WM, Wm, Press, Orientation, X, Y, TrID
    {0, 0, 0, 0, 1, 0, 0, 0, 1},
    {0, 0, 0, 0, 1, 0, 0, 0, 1},
    {0, 0, 0, 0, 1, 0, 0, 0, 2},
    {0, 0, 0, 0, 1, 0, 0, 0, 3},
    {0, 0, 0, 0, 1, 0, 0, 0, 4},
    {0, 0, 0, 0, 1, 0, 0, 0, 5}
  };
  HardwareState hardware_state[] = {
    // time, buttons, finger count, finger states pointer
    { 200000, 0, 1, 1, &finger_states[0] },
    { 200001, 0, 1, 1, &finger_states[1] },
    { 200001, 0, 2, 2, &finger_states[1] },
    { 200001, 0, 2, 2, &finger_states[2] },
  };

  EXPECT_TRUE(hardware_state[0].SameFingersAs(hardware_state[1]));
  EXPECT_FALSE(hardware_state[0].SameFingersAs(hardware_state[2]));
  EXPECT_TRUE(hardware_state[2].SameFingersAs(hardware_state[2]));
  EXPECT_FALSE(hardware_state[2].SameFingersAs(hardware_state[3]));
}

TEST(GesturesTest, GestureStringTest) {
  Gesture null;
  EXPECT_TRUE(strstr(null.String().c_str(), "null"));

  Gesture move(kGestureMove, 1.0, 2.0, 3.0, 4.0);
  EXPECT_TRUE(strstr(move.String().c_str(), "1"));
  EXPECT_TRUE(strstr(move.String().c_str(), "2"));
  EXPECT_TRUE(strstr(move.String().c_str(), "3"));
  EXPECT_TRUE(strstr(move.String().c_str(), "4"));

  Gesture scroll(kGestureScroll, 1.0, 2.0, 3.0, 4.0);
  EXPECT_TRUE(strstr(scroll.String().c_str(), "1"));
  EXPECT_TRUE(strstr(scroll.String().c_str(), "2"));
  EXPECT_TRUE(strstr(scroll.String().c_str(), "3"));
  EXPECT_TRUE(strstr(scroll.String().c_str(), "4"));

  Gesture buttons(kGestureButtonsChange, 1.0, 2.0, 3, 4);
  EXPECT_TRUE(strstr(buttons.String().c_str(), "1"));
  EXPECT_TRUE(strstr(buttons.String().c_str(), "2"));
  EXPECT_TRUE(strstr(buttons.String().c_str(), "3"));
  EXPECT_TRUE(strstr(buttons.String().c_str(), "4"));

  Gesture contact_initiated;
  contact_initiated.type = kGestureTypeContactInitiated;
  EXPECT_TRUE(strstr(contact_initiated.String().c_str(), "nitiated"));
}

TEST(GesturesTest, GestureEqTest) {
  Gesture null;
  Gesture null2;
  EXPECT_TRUE(null == null2);
  EXPECT_FALSE(null != null2);

  Gesture move(kGestureMove, 1.0, 2.0, 3.0, 4.0);
  Gesture move2(kGestureMove, 1.0, 2.0, 3.0, 4.0);
  Gesture move_ne0(kGestureMove, 9.0, 2.0, 3.0, 4.0);
  Gesture move_ne1(kGestureMove, 1.0, 9.0, 3.0, 4.0);
  Gesture move_ne2(kGestureMove, 1.0, 2.0, 9.0, 4.0);
  Gesture move_ne3(kGestureMove, 1.0, 2.0, 3.0, 9.0);
  EXPECT_TRUE(move == move2);
  EXPECT_FALSE(move == move_ne0);
  EXPECT_FALSE(move == move_ne1);
  EXPECT_FALSE(move == move_ne2);
  EXPECT_FALSE(move == move_ne3);
  EXPECT_FALSE(move != move2);
  EXPECT_TRUE(move != move_ne0);
  EXPECT_TRUE(move != move_ne1);
  EXPECT_TRUE(move != move_ne2);
  EXPECT_TRUE(move != move_ne3);

  Gesture scroll(kGestureScroll, 1.0, 2.0, 3.0, 4.0);
  Gesture scroll2(kGestureScroll, 1.0, 2.0, 3.0, 4.0);
  Gesture scroll_ne0(kGestureScroll, 9.0, 2.0, 3.0, 4.0);
  Gesture scroll_ne1(kGestureScroll, 1.0, 9.0, 3.0, 4.0);
  Gesture scroll_ne2(kGestureScroll, 1.0, 2.0, 9.0, 4.0);
  Gesture scroll_ne3(kGestureScroll, 1.0, 2.0, 3.0, 9.0);
  EXPECT_TRUE(scroll == scroll2);
  EXPECT_FALSE(scroll == scroll_ne0);
  EXPECT_FALSE(scroll == scroll_ne1);
  EXPECT_FALSE(scroll == scroll_ne2);
  EXPECT_FALSE(scroll == scroll_ne3);
  EXPECT_FALSE(scroll != scroll2);
  EXPECT_TRUE(scroll != scroll_ne0);
  EXPECT_TRUE(scroll != scroll_ne1);
  EXPECT_TRUE(scroll != scroll_ne2);
  EXPECT_TRUE(scroll != scroll_ne3);

  Gesture buttons(kGestureButtonsChange, 1.0, 2.0, 3, 4);
  Gesture buttons2(kGestureButtonsChange, 1.0, 2.0, 3, 4);
  Gesture buttons_ne0(kGestureButtonsChange, 9.0, 2.0, 3, 4);
  Gesture buttons_ne1(kGestureButtonsChange, 1.0, 9.0, 3, 4);
  Gesture buttons_ne2(kGestureButtonsChange, 1.0, 2.0, 9, 4);
  Gesture buttons_ne3(kGestureButtonsChange, 1.0, 2.0, 3, 9);
  EXPECT_TRUE(buttons == buttons2);
  EXPECT_FALSE(buttons == buttons_ne0);
  EXPECT_FALSE(buttons == buttons_ne1);
  EXPECT_FALSE(buttons == buttons_ne2);
  EXPECT_FALSE(buttons == buttons_ne3);
  EXPECT_FALSE(buttons != buttons2);
  EXPECT_TRUE(buttons != buttons_ne0);
  EXPECT_TRUE(buttons != buttons_ne1);
  EXPECT_TRUE(buttons != buttons_ne2);
  EXPECT_TRUE(buttons != buttons_ne3);

  Gesture contact_initiated;
  contact_initiated.type = kGestureTypeContactInitiated;
  Gesture contact_initiated2;
  contact_initiated2.type = kGestureTypeContactInitiated;
  EXPECT_TRUE(contact_initiated == contact_initiated2);
  EXPECT_FALSE(contact_initiated != contact_initiated2);

  // Compare different types, should all fail to equate
  Gesture* gs[] = { &null, &move, &scroll, &buttons, &contact_initiated };
  for (size_t i = 0; i < arraysize(gs); ++i) {
    for (size_t j = 0; j < arraysize(gs); ++j) {
      if (i == j)
        continue;
      EXPECT_FALSE(*gs[i] == *gs[j]) << "i=" << i << ", j=" << j;
      EXPECT_TRUE(*gs[i] != *gs[j]) << "i=" << i << ", j=" << j;
    }
  }
}

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


TEST(GesturesTest, HardwareStateGetFingerStateTest) {
  FingerState fs[] = {
    { 0, 0, 0, 0, 1, 0, 150, 4000, 4 },
    { 0, 0, 0, 0, 1, 0, 550, 2000, 2 },
    { 0, 0, 0, 0, 1, 0, 250, 3000, 7 }
  };
  HardwareState hs = { 10000, 0, 3, 3, &fs[0] };
  EXPECT_EQ(&fs[0], hs.GetFingerState(4));
  EXPECT_EQ(&fs[1], hs.GetFingerState(2));
  EXPECT_EQ(&fs[2], hs.GetFingerState(7));
  EXPECT_EQ(reinterpret_cast<FingerState*>(NULL), hs.GetFingerState(8));

  const HardwareState& const_hs = hs;
  EXPECT_EQ(&fs[0], const_hs.GetFingerState(4));
  EXPECT_EQ(&fs[1], const_hs.GetFingerState(2));
  EXPECT_EQ(&fs[2], const_hs.GetFingerState(7));
  EXPECT_EQ(reinterpret_cast<const FingerState*>(NULL), hs.GetFingerState(8));
}

}  // namespace gestures
