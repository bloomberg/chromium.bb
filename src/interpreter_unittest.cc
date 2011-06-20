// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include "gestures/include/gestures.h"
#include "gestures/include/interpreter.h"

namespace gestures {

class InterpreterTest : public ::testing::Test {};

TEST(InterpreterTest, AddMovementTest) {
  SpeculativeGestures sgs;
  sgs.AddMovement(1,  // first actor
                  2,  // second actor
                  12,  // dx
                  3,  // dy
                  87,  // confidence
                  reinterpret_cast<struct HardwareState*>(5));
  const SpeculativeGesture* sg  = sgs.GetHighestConfidence();

  ASSERT_NE(reinterpret_cast<const SpeculativeGesture*>(NULL), sg);
  EXPECT_EQ(1, sg->first_actor);
  EXPECT_EQ(2, sg->second_actor);
  EXPECT_EQ(kPointingKind, sg->kind);
  EXPECT_EQ(87, sg->confidence);

  EXPECT_EQ(12, sg->gesture.dx);
  EXPECT_EQ(3, sg->gesture.dy);
  EXPECT_EQ(0, sg->gesture.vscroll);
  EXPECT_EQ(0, sg->gesture.hscroll);
  EXPECT_EQ(1, sg->gesture.fingers_present);
  EXPECT_EQ(0, sg->gesture.buttons_down);
  EXPECT_EQ(reinterpret_cast<struct HardwareState*>(5), sg->gesture.hwstate);
}

TEST(InterpreterTest, EmptyLookupTest) {
  SpeculativeGestures sgs;
  const SpeculativeGesture* sg  = sgs.GetHighestConfidence();
  EXPECT_EQ(reinterpret_cast<const SpeculativeGesture*>(NULL), sg);
}

}  // namespace gestures
