// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include "gestures/include/gestures.h"
#include "gestures/include/immediate_interpreter.h"

namespace gestures {

class ImmediateInterpreterTest : public ::testing::Test {};

TEST(ImmediateInterpreterTest, MoveDownTest) {
  ImmediateInterpreter ii;
  HardwareProperties hwprops = {
    0,  // left edge
    0,  // top edge
    1000,  // right edge
    1000,  // bottom edge
    2,  // TP width
    2,  // TP height
    96,  // screen DPI
    0,  // tripletap
    2  // max fingers
  };

  FingerState finger_states[] = {
    // TM, Tm, WM, Wm, Press, Orientation, X, Y, TrID
    {0, 0, 0, 0, 1, 0, 10, 10, 0},
    {0, 0, 0, 0, 1, 0, 10, 20, 0},
    {0, 0, 0, 0, 1, 0, 20, 20, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0}
  };
  HardwareState hardware_states[] = {
    // time, finger count, finger states pointer
    { 200000, 0, 1, &finger_states[0] },
    { 210000, 0, 1, &finger_states[1] },
    { 220000, 0, 1, &finger_states[2] },
    { 230000, 0, 1, &finger_states[3] },
    { 240000, 0, 1, &finger_states[4] }
  };

  // Should fail w/o hardware props set
  EXPECT_EQ(0, ii.Push(&hardware_states[0]));

  ii.SetHardwareProperties(&hwprops);

  EXPECT_EQ(0, ii.Push(&hardware_states[0]));

  ustime_t next = ii.Push(&hardware_states[1]);
  EXPECT_NE(0, next);
  const SpeculativeGestures* sgs = ii.Back(next);
  const SpeculativeGesture* sg = sgs->GetHighestConfidence();
  EXPECT_EQ(kPointingKind, sg->kind);
  EXPECT_EQ(100, sg->confidence);
  EXPECT_EQ(0, sg->gesture.dx);
  EXPECT_EQ(10, sg->gesture.dy);
  EXPECT_EQ(0, sg->gesture.hscroll);
  EXPECT_EQ(0, sg->gesture.vscroll);
  ii.Pop(next);

  next = ii.Push(&hardware_states[2]);
  EXPECT_NE(0, next);
  sgs = ii.Back(next);
  sg = sgs->GetHighestConfidence();
  EXPECT_EQ(kPointingKind, sg->kind);
  EXPECT_EQ(100, sg->confidence);
  EXPECT_EQ(10, sg->gesture.dx);
  EXPECT_EQ(0, sg->gesture.dy);
  EXPECT_EQ(0, sg->gesture.hscroll);
  EXPECT_EQ(0, sg->gesture.vscroll);
  ii.Pop(next);

  EXPECT_EQ(0, ii.Push(&hardware_states[3]));
  EXPECT_EQ(0, ii.Push(&hardware_states[4]));
}

}  // namespace gestures
