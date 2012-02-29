// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include <base/logging.h>
#include <base/string_util.h>
#include <gtest/gtest.h>

#include "gestures/include/gestures.h"
#include "gestures/include/immediate_interpreter.h"

namespace gestures {

using std::string;
using std::vector;

class ImmediateInterpreterTest : public ::testing::Test {};

TEST(ImmediateInterpreterTest, MoveDownTest) {
  ImmediateInterpreter ii(NULL);
  HardwareProperties hwprops = {
    0,  // left edge
    0,  // top edge
    1000,  // right edge
    1000,  // bottom edge
    500,  // pixels/TP width
    500,  // pixels/TP height
    96,  // screen DPI x
    96,  // screen DPI y
    2,  // max fingers
    5,  // max touch
    0,  // tripletap
    0,  // semi-mt
    1  // is button pad
  };

  FingerState finger_states[] = {
    // TM, Tm, WM, Wm, Press, Orientation, X, Y, TrID
    {0, 0, 0, 0, 1, 0, 10, 10, 1},
    {0, 0, 0, 0, 1, 0, 10, 20, 1},
    {0, 0, 0, 0, 1, 0, 20, 20, 1}
  };
  HardwareState hardware_states[] = {
    // time, buttons down, finger count, finger states pointer
    { 200000, 0, 1, 1, &finger_states[0] },
    { 210000, 0, 1, 1, &finger_states[1] },
    { 220000, 0, 1, 1, &finger_states[2] },
    { 230000, 0, 0, 0, NULL },
    { 240000, 0, 0, 0, NULL }
  };

  // Should fail w/o hardware props set
  EXPECT_EQ(NULL, ii.SyncInterpret(&hardware_states[0], NULL));

  ii.SetHardwareProperties(hwprops);

  EXPECT_EQ(NULL, ii.SyncInterpret(&hardware_states[0], NULL));

  Gesture* gs = ii.SyncInterpret(&hardware_states[1], NULL);
  ASSERT_NE(reinterpret_cast<Gesture*>(NULL), gs);
  EXPECT_EQ(kGestureTypeMove, gs->type);
  EXPECT_EQ(0, gs->details.move.dx);
  EXPECT_EQ(10, gs->details.move.dy);
  EXPECT_EQ(200000, gs->start_time);
  EXPECT_EQ(210000, gs->end_time);

  gs = ii.SyncInterpret(&hardware_states[2], NULL);
  EXPECT_NE(reinterpret_cast<Gesture*>(NULL), gs);
  EXPECT_EQ(kGestureTypeMove, gs->type);
  EXPECT_EQ(10, gs->details.move.dx);
  EXPECT_EQ(0, gs->details.move.dy);
  EXPECT_EQ(210000, gs->start_time);
  EXPECT_EQ(220000, gs->end_time);

  EXPECT_EQ(reinterpret_cast<Gesture*>(NULL),
            ii.SyncInterpret(&hardware_states[3], NULL));
  EXPECT_EQ(reinterpret_cast<Gesture*>(NULL),
            ii.SyncInterpret(&hardware_states[4], NULL));
}

TEST(ImmediateInterpreterTest, MoveUpWithRestingThumbTest) {
  ImmediateInterpreter ii(NULL);
  HardwareProperties hwprops = {
    0,  // left edge
    0,  // top edge
    1000,  // right edge
    1000,  // bottom edge
    50,  // pixels/TP width
    50,  // pixels/TP height
    96,  // screen DPI x
    96,  // screen DPI y
    2,  // max fingers
    5,  // max touch
    0,  // tripletap
    0,  // semi-mt
    1  // is button pad
  };

  FingerState finger_states[] = {
    // TM, Tm, WM, Wm, Press, Orientation, X, Y, TrID
    {0, 0, 0, 0, 10, 0, 500, 999, 1},
    {0, 0, 0, 0, 10, 0, 500, 950, 2},
    {0, 0, 0, 0, 10, 0, 500, 999, 1},
    {0, 0, 0, 0, 10, 0, 500, 940, 2},
    {0, 0, 0, 0, 10, 0, 500, 999, 1},
    {0, 0, 0, 0, 10, 0, 500, 930, 2}
  };
  HardwareState hardware_states[] = {
    // time, buttons down, finger count, finger states pointer
    { 200000, 0, 2, 2, &finger_states[0] },
    { 210000, 0, 2, 2, &finger_states[2] },
    { 220000, 0, 2, 2, &finger_states[4] },
    { 230000, 0, 0, 0, NULL },
    { 240000, 0, 0, 0, NULL }
  };

  // Should fail w/o hardware props set
  EXPECT_EQ(NULL, ii.SyncInterpret(&hardware_states[0], NULL));

  ii.SetHardwareProperties(hwprops);

  EXPECT_EQ(NULL, ii.SyncInterpret(&hardware_states[0], NULL));

  Gesture* gs = ii.SyncInterpret(&hardware_states[1], NULL);
  ASSERT_NE(reinterpret_cast<Gesture*>(NULL), gs);
  EXPECT_EQ(kGestureTypeMove, gs->type);
  EXPECT_EQ(0, gs->details.move.dx);
  EXPECT_EQ(-10, gs->details.move.dy);
  EXPECT_EQ(200000, gs->start_time);
  EXPECT_EQ(210000, gs->end_time);

  gs = ii.SyncInterpret(&hardware_states[2], NULL);
  EXPECT_NE(reinterpret_cast<Gesture*>(NULL), gs);
  EXPECT_EQ(kGestureTypeMove, gs->type);
  EXPECT_EQ(0, gs->details.move.dx);
  EXPECT_EQ(-10, gs->details.move.dy);
  EXPECT_EQ(210000, gs->start_time);
  EXPECT_EQ(220000, gs->end_time);

  EXPECT_EQ(reinterpret_cast<Gesture*>(NULL),
            ii.SyncInterpret(&hardware_states[3], NULL));
  EXPECT_EQ(reinterpret_cast<Gesture*>(NULL),
            ii.SyncInterpret(&hardware_states[4], NULL));
}

void ScrollUpTest(float pressure_a, float pressure_b) {
  ImmediateInterpreter ii(NULL);
  HardwareProperties hwprops = {
    0,  // left edge
    0,  // top edge
    1000,  // right edge
    1000,  // bottom edge
    20,  // pixels/TP width
    20,  // pixels/TP height
    96,  // x screen DPI
    96,  // y screen DPI
    2,  // max fingers
    5,  // max touch
    0,  // tripletap
    0,  // semi-mt
    1  // is button pad
  };

  float p_a = pressure_a;
  float p_b = pressure_b;

  FingerState finger_states[] = {
    // TM, Tm, WM, Wm, Press, Orientation, X, Y, TrID
    {0, 0, 0, 0, p_a, 0, 400, 900, 1},
    {0, 0, 0, 0, p_b, 0, 415, 900, 2},

    {0, 0, 0, 0, p_a, 0, 400, 800, 1},
    {0, 0, 0, 0, p_b, 0, 415, 800, 2},

    {0, 0, 0, 0, p_a, 0, 400, 700, 1},
    {0, 0, 0, 0, p_b, 0, 415, 700, 2},
  };
  HardwareState hardware_states[] = {
    // time, buttons, finger count, touch count, finger states pointer
    { 0.200000, 0, 2, 2, &finger_states[0] },
    { 0.250000, 0, 2, 2, &finger_states[2] },
    { 0.300000, 0, 2, 2, &finger_states[4] }
  };

  ii.SetHardwareProperties(hwprops);

  EXPECT_EQ(NULL, ii.SyncInterpret(&hardware_states[0], NULL));

  Gesture* gs = ii.SyncInterpret(&hardware_states[1], NULL);
  ASSERT_NE(reinterpret_cast<Gesture*>(NULL), gs);
  EXPECT_EQ(kGestureTypeScroll, gs->type);
  EXPECT_FLOAT_EQ(0, gs->details.move.dx);
  EXPECT_FLOAT_EQ(-100, gs->details.move.dy);
  EXPECT_DOUBLE_EQ(0.200000, gs->start_time);
  EXPECT_DOUBLE_EQ(0.250000, gs->end_time);

  gs = ii.SyncInterpret(&hardware_states[2], NULL);
  ASSERT_NE(reinterpret_cast<Gesture*>(NULL), gs);
  EXPECT_EQ(kGestureTypeScroll, gs->type);
  EXPECT_FLOAT_EQ(0, gs->details.move.dx);
  EXPECT_FLOAT_EQ(-100, gs->details.move.dy);
  EXPECT_DOUBLE_EQ(0.250000, gs->start_time);
  EXPECT_DOUBLE_EQ(0.300000, gs->end_time);
}

TEST(ImmediateInterpreterTest, ScrollUpTest) {
  ScrollUpTest(24, 92);
}

TEST(ImmediateInterpreterTest, FatFingerScrollUpTest) {
  ScrollUpTest(125, 185);
}

// Tests that a tap immediately after a scroll doesn't generate a click.
// Such a tap would be unrealistic to come from a human.
TEST(ImmediateInterpreterTest, ScrollThenFalseTapTest) {
  ImmediateInterpreter ii(NULL);
  HardwareProperties hwprops = {
    0,  // left edge
    0,  // top edge
    1000,  // right edge
    1000,  // bottom edge
    20,  // pixels/TP width
    20,  // pixels/TP height
    96,  // x screen DPI
    96,  // y screen DPI
    2,  // max fingers
    5,  // max touch
    0,  // tripletap
    0,  // semi-mt
    1  // is button pad
  };

  FingerState finger_states[] = {
    // TM, Tm, WM, Wm, Press, Orientation, X, Y, TrID
    {0, 0, 0, 0, 20, 0, 400, 900, 1},
    {0, 0, 0, 0, 20, 0, 415, 900, 2},

    {0, 0, 0, 0, 20, 0, 400, 800, 1},
    {0, 0, 0, 0, 20, 0, 415, 800, 2},

    {0, 0, 0, 0, 20, 0, 400, 700, 1},
    {0, 0, 0, 0, 20, 0, 415, 700, 2},

    {0, 0, 0, 0, 20, 0, 400, 600, 3},
  };
  HardwareState hardware_states[] = {
    // time, buttons, finger count, touch count, finger states pointer
    { 0.200000, 0, 2, 2, &finger_states[0] },
    { 0.250000, 0, 2, 2, &finger_states[2] },
    { 0.300000, 0, 2, 2, &finger_states[4] },
    { 0.310000, 0, 0, 0, NULL },
    { 0.320000, 0, 1, 1, &finger_states[6] },
    { 0.330000, 0, 0, 0, NULL }
  };

  ii.tap_enable_.val_ = 1;
  ii.SetHardwareProperties(hwprops);

  EXPECT_EQ(NULL, ii.SyncInterpret(&hardware_states[0], NULL));

  Gesture* gs = ii.SyncInterpret(&hardware_states[1], NULL);
  ASSERT_NE(reinterpret_cast<Gesture*>(NULL), gs);
  EXPECT_EQ(kGestureTypeScroll, gs->type);
  gs = ii.SyncInterpret(&hardware_states[2], NULL);
  ASSERT_NE(reinterpret_cast<Gesture*>(NULL), gs);
  EXPECT_EQ(kGestureTypeScroll, gs->type);
  gs = ii.SyncInterpret(&hardware_states[3], NULL);
  ASSERT_EQ(reinterpret_cast<Gesture*>(NULL), gs);
  gs = ii.SyncInterpret(&hardware_states[4], NULL);
  ASSERT_EQ(reinterpret_cast<Gesture*>(NULL), gs);
  stime_t timeout = -1.0;
  gs = ii.SyncInterpret(&hardware_states[5], &timeout);
  ASSERT_EQ(reinterpret_cast<Gesture*>(NULL), gs);
  // If it were a tap, timeout would be > 0, but this shouldn't be a tap,
  // so timeout should be negative still.
  EXPECT_LT(timeout, 0.0);
}

TEST(ImmediateInterpreterTest, ThumbRetainTest) {
  ImmediateInterpreter ii(NULL);
  HardwareProperties hwprops = {
    0,  // left edge
    0,  // top edge
    10,  // right edge
    10,  // bottom edge
    1,  // pixels/TP width
    1,  // pixels/TP height
    1,  // x screen DPI
    1,  // y screen DPI
    2,  // max fingers
    5,  // max touch
    0,  // tripletap
    0,  // semi-mt
    1  // is button pad
  };

  FingerState finger_states[] = {
    // TM, Tm, WM, Wm, Press, Orientation, X, Y, TrID
    // id 1 = finger, 2 = thumb
    {0, 0, 0, 0, 24, 0, 3, 3, 1},
    {0, 0, 0, 0, 58, 0, 3, 5, 2},

    // thumb, post-move
    {0, 0, 0, 0, 58, 0, 5, 5, 2},
  };
  HardwareState hardware_states[] = {
    // time, buttons, finger count, touch count, finger states pointer
    { 0.000, 0, 2, 2, &finger_states[0] },
    { 0.100, 0, 2, 2, &finger_states[0] },
    { 0.110, 0, 1, 1, &finger_states[1] },  // finger goes away
    { 0.210, 0, 1, 1, &finger_states[1] },
    { 0.220, 0, 1, 1, &finger_states[2] },  // thumb moves
  };

  ii.SetHardwareProperties(hwprops);
  ii.tap_enable_.val_ = 0;

  for (size_t i = 0; i < arraysize(hardware_states); i++) {
    Gesture* gs = ii.SyncInterpret(&hardware_states[i], NULL);
    EXPECT_TRUE(!gs ||
                (gs->type == kGestureTypeMove &&
                 gs->details.move.dx == 0.0 &&
                 gs->details.move.dy == 0.0));
  }
}

TEST(ImmediateInterpreterTest, ThumbRetainReevaluateTest) {
  ImmediateInterpreter ii(NULL);
  HardwareProperties hwprops = {
    0,  // left edge
    0,  // top edge
    100,  // right edge
    100,  // bottom edge
    1,  // pixels/TP width
    1,  // pixels/TP height
    1,  // x screen DPI
    1,  // y screen DPI
    2,  // max fingers
    5,  // max touch
    0,  // tripletap
    0,  // semi-mt
    1  // is button pad
  };

  FingerState finger_states[] = {
    // TM, Tm, WM, Wm, Press, Orientation, X, Y, TrID
    // one thumb, one finger (it seems)
    {0, 0, 0, 0, 24, 0, 3.0, 3, 3},
    {0, 0, 0, 0, 58, 0, 13.5, 3, 4},
    // two big fingers, it turns out!
    {0, 0, 0, 0, 27, 0, 3.0, 6, 3},
    {0, 0, 0, 0, 58, 0, 13.5, 6, 4},
    // they  move
    {0, 0, 0, 0, 27, 0, 3.0, 7, 3},
    {0, 0, 0, 0, 58, 0, 13.5, 7, 4},
  };
  HardwareState hardware_states[] = {
    // time, buttons, finger count, touch count, finger states pointer
    { 1.000, 0, 2, 2, &finger_states[0] },  // next 2 fingers arrive
    { 1.010, 0, 2, 2, &finger_states[2] },  // pressures fix
    { 1.100, 0, 2, 2, &finger_states[4] },  // they move
  };

  ii.SetHardwareProperties(hwprops);
  ii.tap_enable_.val_ = 0;

  for (size_t i = 0; i < arraysize(hardware_states); i++) {
    Gesture* gs = ii.SyncInterpret(&hardware_states[i], NULL);
    EXPECT_TRUE(!gs || gs->type == kGestureTypeScroll);
  }
}

TEST(ImmediateInterpreterTest, SetHardwarePropertiesTwiceTest) {
  ImmediateInterpreter ii(NULL);
  HardwareProperties hwprops = {
    0,  // left edge
    0,  // top edge
    1000,  // right edge
    1000,  // bottom edge
    500,  // pixels/TP width
    500,  // pixels/TP height
    96,  // screen DPI x
    96,  // screen DPI y
    2,  // max fingers
    5,  // max touch
    0,  // tripletap
    0,  // semi-mt
    1  // is button pad
  };
  ii.SetHardwareProperties(hwprops);
  hwprops.max_finger_cnt = 3;
  ii.SetHardwareProperties(hwprops);

  FingerState finger_states[] = {
    // TM, Tm, WM, Wm, Press, Orientation, X, Y, TrID
    {0, 0, 0, 0, 1, 0, 0, 0, 1},
    {0, 0, 0, 0, 1, 0, 0, 0, 2},
    {0, 0, 0, 0, 1, 0, 0, 0, 3},
    {0, 0, 0, 0, 0, 0, 0, 0, 4},
    {0, 0, 0, 0, 0, 0, 0, 0, 5}
  };
  HardwareState hardware_state = {
    // time, buttons, finger count, touch count, finger states pointer
    200000, 0, 5, 5, &finger_states[0]
  };
  // This used to cause a crash:
  Gesture* gs = ii.SyncInterpret(&hardware_state, NULL);
  EXPECT_EQ(reinterpret_cast<Gesture*>(NULL), gs);
}

TEST(ImmediateInterpreterTest, PalmTest) {
  ImmediateInterpreter ii(NULL);
  HardwareProperties hwprops = {
    0,  // left edge
    0,  // top edge
    1000,  // right edge
    1000,  // bottom edge
    500,  // x pixels/TP width
    500,  // y pixels/TP height
    96,  // x screen DPI
    96,  // y screen DPI
    2,  // max fingers
    5,  // max touch
    0,  // t5r2
    0,  // semi-mt
    1  // is button pad
  };
  ii.SetHardwareProperties(hwprops);

  const int kBig = ii.palm_pressure_.val_ + 1;  // big (palm) pressure
  const int kSml = ii.palm_pressure_.val_ - 1;  // low pressure

  FingerState finger_states[] = {
    // TM, Tm, WM, Wm, Press, Orientation, X, Y, TrID
    {0, 0, 0, 0, kSml, 0, 600, 500, 1},
    {0, 0, 0, 0, kSml, 0, 500, 500, 2},

    {0, 0, 0, 0, kSml, 0, 600, 500, 1},
    {0, 0, 0, 0, kBig, 0, 500, 500, 2},

    {0, 0, 0, 0, kSml, 0, 600, 500, 1},
    {0, 0, 0, 0, kSml, 0, 500, 500, 2},

    {0, 0, 0, 0, kSml, 0, 600, 500, 3},
    {0, 0, 0, 0, kBig, 0, 500, 500, 4},

    {0, 0, 0, 0, kSml, 0, 600, 500, 3},
    {0, 0, 0, 0, kSml, 0, 500, 500, 4}
  };
  HardwareState hardware_state[] = {
    // time, buttons, finger count, touch count, finger states pointer
    { 200000, 0, 2, 2, &finger_states[0] },
    { 200001, 0, 2, 2, &finger_states[2] },
    { 200002, 0, 2, 2, &finger_states[4] },
    { 200003, 0, 2, 2, &finger_states[6] },
    { 200004, 0, 2, 2, &finger_states[8] },
  };

  for (size_t i = 0; i < 5; ++i) {
    ii.SyncInterpret(&hardware_state[i], NULL);
    switch (i) {
      case 0:
        EXPECT_TRUE(SetContainsValue(ii.pointing_, 1));
        EXPECT_FALSE(SetContainsValue(ii.pending_palm_, 1));
        EXPECT_FALSE(SetContainsValue(ii.palm_, 1));
        EXPECT_TRUE(SetContainsValue(ii.pointing_, 2));
        EXPECT_FALSE(SetContainsValue(ii.pending_palm_, 2));
        EXPECT_FALSE(SetContainsValue(ii.palm_, 2));
        break;
      case 1:  // fallthrough
      case 2:
        EXPECT_TRUE(SetContainsValue(ii.pointing_, 1));
        EXPECT_FALSE(SetContainsValue(ii.pending_palm_, 1));
        EXPECT_FALSE(SetContainsValue(ii.palm_, 1));
        EXPECT_FALSE(SetContainsValue(ii.pointing_, 2));
        EXPECT_FALSE(SetContainsValue(ii.pending_palm_, 2));
        EXPECT_TRUE(SetContainsValue(ii.palm_, 2));
        break;
      case 3:  // fallthrough
      case 4:
        EXPECT_TRUE(SetContainsValue(ii.pointing_, 3)) << "i=" << i;
        EXPECT_FALSE(SetContainsValue(ii.pending_palm_, 3));
        EXPECT_FALSE(SetContainsValue(ii.palm_, 3));
        EXPECT_FALSE(SetContainsValue(ii.pointing_, 4));
        EXPECT_FALSE(SetContainsValue(ii.pending_palm_, 4));
        EXPECT_TRUE(SetContainsValue(ii.palm_, 4));
        break;
    }
  }

  ii.ResetSameFingersState(0);
  EXPECT_TRUE(ii.pointing_.empty());
  EXPECT_TRUE(ii.pending_palm_.empty());
  EXPECT_TRUE(ii.palm_.empty());
}

TEST(ImmediateInterpreterTest, PalmAtEdgeTest) {
  scoped_ptr<ImmediateInterpreter> ii(new ImmediateInterpreter(NULL));
  HardwareProperties hwprops = {
    0,  // left edge
    0,  // top edge
    100,  // right edge
    100,  // bottom edge
    1,  // x pixels/mm
    1,  // y pixels/mm
    1,  // x screen px/mm
    1,  // y screen px/mm
    5,  // max fingers
    5,  // max touch
    0,  // t5r2
    0,  // semi-mt
    1  // is button pad
  };

  const float kBig = ii->palm_pressure_.val_ + 1.0;  // palm pressure
  const float kSml = ii->palm_pressure_.val_ - 1.0;  // small, low pressure
  const float kMid = ii->palm_pressure_.val_ / 2.0;
  const float kMidWidth =
      (ii->palm_edge_min_width_.val_ + ii->palm_edge_width_.val_) / 2.0;

  FingerState finger_states[] = {
    // TM, Tm, WM, Wm, Press, Orientation, X, Y, TrID
    // small contact movement in edge
    {0, 0, 0, 0, kSml, 0, 1, 40, 1},
    {0, 0, 0, 0, kSml, 0, 1, 50, 1},
    // small contact movement in middle
    {0, 0, 0, 0, kSml, 0, 50, 40, 1},
    {0, 0, 0, 0, kSml, 0, 50, 50, 1},
    // large contact movment in middle
    {0, 0, 0, 0, kBig, 0, 50, 40, 1},
    {0, 0, 0, 0, kBig, 0, 50, 50, 1},
    // under mid-pressure contact move at mid-width
    {0, 0, 0, 0, kMid - 1.0, 0, kMidWidth, 40, 1},
    {0, 0, 0, 0, kMid - 1.0, 0, kMidWidth, 50, 1},
    // over mid-pressure contact move at mid-width
    {0, 0, 0, 0, kMid + 1.0, 0, kMidWidth, 40, 1},
    {0, 0, 0, 0, kMid + 1.0, 0, kMidWidth, 50, 1},
  };
  HardwareState hardware_state[] = {
    // time, buttons, finger count, touch count, finger states pointer
    // slow movement at edge
    { 0.0, 0, 1, 1, &finger_states[0] },
    { 1.0, 0, 1, 1, &finger_states[1] },
    // slow small contact movement in middle
    { 0.0, 0, 1, 1, &finger_states[2] },
    { 1.0, 0, 1, 1, &finger_states[3] },
    // slow large contact movement in middle
    { 0.0, 0, 1, 1, &finger_states[4] },
    { 1.0, 0, 1, 1, &finger_states[5] },
    // under mid-pressure at mid-width
    { 0.0, 0, 1, 1, &finger_states[6] },
    { 1.0, 0, 1, 1, &finger_states[7] },
    // over mid-pressure at mid-width
    { 0.0, 0, 1, 1, &finger_states[8] },
    { 1.0, 0, 1, 1, &finger_states[9] },
  };

  for (size_t i = 0; i < arraysize(hardware_state); ++i) {
    if ((i % 2) == 0) {
      ii.reset(new ImmediateInterpreter(NULL));
      ii->SetHardwareProperties(hwprops);
      ii->change_timeout_.val_ = 0.0;
    }
    Gesture* result = ii->SyncInterpret(&hardware_state[i], NULL);
    if ((i % 2) == 0) {
      EXPECT_FALSE(result);
      continue;
    }
    switch (i) {
      case 3:  // fallthough
      case 7:
        ASSERT_TRUE(result) << "i=" << i;
        EXPECT_EQ(kGestureTypeMove, result->type);
        break;
      case 1:  // fallthrough
      case 5:
      case 9:
        EXPECT_FALSE(result);
        break;
      default:
        ADD_FAILURE() << "Should be unreached.";
        break;
    }
  }
}

TEST(ImmediateInterpreterTest, PressureChangeMoveTest) {
  ImmediateInterpreter ii(NULL);
  HardwareProperties hwprops = {
    0,  // left edge
    0,  // top edge
    1000,  // right edge
    1000,  // bottom edge
    500,  // x pixels/TP width
    500,  // y pixels/TP height
    96,  // x screen DPI
    96,  // y screen DPI
    2,  // max fingers
    5,  // max touch
    0,  // t5r2
    0,  // semi-mt
    1  // is button pad
  };
  ii.SetHardwareProperties(hwprops);

  const int kBig = 81;  // large pressure
  const int kSml = 50;  // small pressure

  FingerState finger_states[] = {
    // TM, Tm, WM, Wm, Press, Orientation, X, Y, TrID
    {0, 0, 0, 0, kSml, 0, 600, 300, 1},
    {0, 0, 0, 0, kSml, 0, 600, 400, 1},
    {0, 0, 0, 0, kBig, 0, 600, 500, 1},
    {0, 0, 0, 0, kBig, 0, 600, 600, 1},
  };
  HardwareState hardware_state[] = {
    // time, buttons, finger count, touch count, finger states pointer
    { 200000, 0, 1, 1, &finger_states[0] },
    { 200001, 0, 1, 1, &finger_states[1] },
    { 200002, 0, 1, 1, &finger_states[2] },
    { 200003, 0, 1, 1, &finger_states[3] },
  };

  for (size_t i = 0; i < arraysize(hardware_state); ++i) {
    Gesture* result = ii.SyncInterpret(&hardware_state[i], NULL);
    switch (i) {
      case 0:
      case 2:
        EXPECT_FALSE(result);
        break;
      case 1:  // fallthrough
      case 3:
        ASSERT_TRUE(result);
        EXPECT_EQ(kGestureTypeMove, result->type);
        break;
    }
  }
}

TEST(ImmediateInterpreterTest, GetGesturingFingersTest) {
  ImmediateInterpreter ii(NULL);
  HardwareProperties hwprops = {
    0,  // left edge
    0,  // top edge
    1000,  // right edge
    1000,  // bottom edge
    500,  // pixels/TP width
    500,  // pixels/TP height
    96,  // screen DPI x
    96,  // screen DPI y
    2,  // max fingers
    5,  // max touch
    0,  // t5r2
    0,  // semi-mt
    1  // is button pad
  };
  ii.SetHardwareProperties(hwprops);

  FingerState finger_states[] = {
    // TM, Tm, WM, Wm, Press, Orientation, X, Y, TrID
    {0, 0, 0, 0, 1, 0, 61, 70, 91},
    {0, 0, 0, 0, 1, 0, 62, 65,  92},
    {0, 0, 0, 0, 1, 0, 62, 69,  93},
    {0, 0, 0, 0, 1, 0, 62, 61,  94}
  };
  HardwareState hardware_state[] = {
    // time, buttons, finger count, finger states pointer
    { 200000, 0, 0, 0, NULL },
    { 200001, 0, 1, 1, &finger_states[0] },
    { 200002, 0, 2, 2, &finger_states[0] },
    { 200002, 0, 3, 3, &finger_states[0] },
    { 200002, 0, 4, 4, &finger_states[0] }
  };

  // few pointing fingers
  ii.ResetSameFingersState(0);
  ii.UpdatePalmState(hardware_state[0]);
  EXPECT_TRUE(ii.GetGesturingFingers(hardware_state[0]).empty());

  ii.ResetSameFingersState(0);
  ii.UpdatePalmState(hardware_state[1]);
  set<short, kMaxGesturingFingers> ids =
      ii.GetGesturingFingers(hardware_state[1]);
  EXPECT_EQ(1, ids.size());
  EXPECT_TRUE(ids.end() != ids.find(91));

  ii.ResetSameFingersState(0);
  ii.UpdatePalmState(hardware_state[2]);
  ids = ii.GetGesturingFingers(hardware_state[2]);
  EXPECT_EQ(2, ids.size());
  EXPECT_TRUE(ids.end() != ids.find(91));
  EXPECT_TRUE(ids.end() != ids.find(92));

  ii.ResetSameFingersState(0);
  ii.UpdatePalmState(hardware_state[3]);
  ids = ii.GetGesturingFingers(hardware_state[3]);
  EXPECT_EQ(2, ids.size());
  EXPECT_TRUE(ids.end() != ids.find(92));
  EXPECT_TRUE(ids.end() != ids.find(93));

  ii.ResetSameFingersState(0);
  ii.UpdatePalmState(hardware_state[4]);
  ids = ii.GetGesturingFingers(hardware_state[4]);
  EXPECT_EQ(2, ids.size());
  EXPECT_TRUE(ids.end() != ids.find(92));
  EXPECT_TRUE(ids.end() != ids.find(94));

  // T5R2 test
  hwprops.supports_t5r2 = 1;
  ii.SetHardwareProperties(hwprops);
  ii.ResetSameFingersState(0);
  ii.UpdatePalmState(hardware_state[2]);
  ids = ii.GetGesturingFingers(hardware_state[2]);
  EXPECT_EQ(2, ids.size());
  EXPECT_TRUE(ids.end() != ids.find(91));
  EXPECT_TRUE(ids.end() != ids.find(92));
}

namespace {
set<short, kMaxGesturingFingers> MkSet() {
  return set<short, kMaxGesturingFingers>();
}
set<short, kMaxGesturingFingers> MkSet(short the_id) {
  set<short, kMaxGesturingFingers> ret;
  ret.insert(the_id);
  return ret;
}
set<short, kMaxGesturingFingers> MkSet(short id1, short id2) {
  set<short, kMaxGesturingFingers> ret;
  ret.insert(id1);
  ret.insert(id2);
  return ret;
}
}  // namespace{}

TEST(ImmediateInterpreterTest, TapRecordTest) {
  ImmediateInterpreter ii(NULL);
  TapRecord tr(&ii);
  EXPECT_FALSE(tr.TapComplete());
  // two finger IDs:
  const short kF1 = 91;
  const short kF2 = 92;
  const float kTapMoveDist = 1.0;  // mm
  ii.tap_min_pressure_.val_ = 25;

  FingerState fs[] = {
    // TM, Tm, WM, Wm, Press, Orientation, X, Y, TrID
    {0, 0, 0, 0, 50, 0, 4, 4, kF1},
    {0, 0, 0, 0, 75, 0, 4, 9, kF2},
    {0, 0, 0, 0, 50, 0, 7, 4, kF1}
  };
  HardwareState nullstate = { 0.0, 0, 0, 0, NULL };
  HardwareState hw[] = {
    // time, buttons, finger count, finger states pointer
    { 0.0, 0, 1, 1, &fs[0] },
    { 0.1, 0, 2, 2, &fs[0] },
    { 0.2, 0, 1, 1, &fs[1] },
    { 0.3, 0, 2, 2, &fs[0] },
    { 0.4, 0, 1, 1, &fs[1] },
    { 0.5, 0, 1, 1, &fs[2] }
  };

  tr.Update(hw[0], nullstate, MkSet(kF1), MkSet(), MkSet());
  EXPECT_FALSE(tr.Moving(hw[0], kTapMoveDist));
  EXPECT_FALSE(tr.TapComplete());
  tr.Update(hw[1], hw[0], MkSet(), MkSet(), MkSet());
  EXPECT_FALSE(tr.Moving(hw[1], kTapMoveDist));
  EXPECT_FALSE(tr.TapComplete());
  tr.Update(hw[2], hw[1], MkSet(), MkSet(kF1), MkSet());
  EXPECT_FALSE(tr.Moving(hw[2], kTapMoveDist));
  EXPECT_TRUE(tr.TapComplete());
  EXPECT_EQ(GESTURES_BUTTON_LEFT, tr.TapType());

  tr.Clear();
  EXPECT_FALSE(tr.TapComplete());
  tr.Update(hw[2], hw[1], MkSet(kF2), MkSet(), MkSet());
  EXPECT_FALSE(tr.Moving(hw[2], kTapMoveDist));
  EXPECT_FALSE(tr.TapComplete());
  tr.Update(hw[3], hw[2], MkSet(kF1), MkSet(), MkSet(kF2));
  EXPECT_FALSE(tr.Moving(hw[3], kTapMoveDist));
  EXPECT_FALSE(tr.TapComplete());
  tr.Update(hw[4], hw[3], MkSet(), MkSet(kF1), MkSet());
  EXPECT_FALSE(tr.Moving(hw[4], kTapMoveDist));
  EXPECT_TRUE(tr.TapComplete());

  tr.Clear();
  EXPECT_FALSE(tr.TapComplete());
  tr.Update(hw[0], nullstate, MkSet(kF1), MkSet(), MkSet());
  tr.Update(hw[5], hw[4], MkSet(), MkSet(), MkSet());
  EXPECT_TRUE(tr.Moving(hw[5], kTapMoveDist));
  EXPECT_FALSE(tr.TapComplete());

  // This should log an error
  tr.Clear();
  tr.Update(hw[2], hw[1], MkSet(), MkSet(kF1), MkSet());
}

struct HWStateGs {
  HardwareState hws;
  stime_t callback_now;
  set<short, kMaxGesturingFingers> gs;
  unsigned expected_down;
  unsigned expected_up;
  ImmediateInterpreter::TapToClickState expected_state;
  bool timeout;
};

// Shorter names so that HWStateGs defs take only 1 line each
typedef ImmediateInterpreter::TapToClickState TapState;
const TapState kIdl = ImmediateInterpreter::kTtcIdle;
const TapState kFTB = ImmediateInterpreter::kTtcFirstTapBegan;
const TapState kTpC = ImmediateInterpreter::kTtcTapComplete;
const TapState kSTB = ImmediateInterpreter::kTtcSubsequentTapBegan;
const TapState kDrg = ImmediateInterpreter::kTtcDrag;
const TapState kDRl = ImmediateInterpreter::kTtcDragRelease;
const TapState kDRt = ImmediateInterpreter::kTtcDragRetouch;
const unsigned kBL = GESTURES_BUTTON_LEFT;
const unsigned kBR = GESTURES_BUTTON_RIGHT;

TEST(ImmediateInterpreterTest, TapToClickStateMachineTest) {
  scoped_ptr<ImmediateInterpreter> ii;

  HardwareProperties hwprops = {
    0,  // left edge
    0,  // top edge
    200,  // right edge
    200,  // bottom edge
    1.0,  // pixels/TP width
    1.0,  // pixels/TP height
    1.0,  // screen DPI x
    1.0,  // screen DPI y
    5,  // max fingers
    5,  // max touch
    0,  // t5r2
    0,  // semi-mt
    1  // is button pad
  };

  FingerState fs[] = {
    // TM, Tm, WM, Wm, Press, Orientation, X, Y, TrID
    {0, 0, 0, 0, 50, 0, 4, 4, 91},
    {0, 0, 0, 0, 75, 0, 4, 9, 92},
    {0, 0, 0, 0, 50, 0, 4, 4, 93},
    {0, 0, 0, 0, 50, 0, 4, 4, 94},
    {0, 0, 0, 0, 50, 0, 4, 4, 95},
    {0, 0, 0, 0, 50, 0, 6, 4, 95},
    {0, 0, 0, 0, 50, 0, 8, 4, 95},
    {0, 0, 0, 0, 50, 0, 4, 4, 96},
    {0, 0, 0, 0, 50, 0, 6, 4, 96},
    {0, 0, 0, 0, 50, 0, 8, 4, 96},

    {0, 0, 0, 0, 50, 0, 4, 1, 97},  // 10
    {0, 0, 0, 0, 50, 0, 9, 1, 98},
    {0, 0, 0, 0, 50, 0, 4, 5, 97},
    {0, 0, 0, 0, 50, 0, 9, 5, 98},
    {0, 0, 0, 0, 50, 0, 4, 9, 97},
    {0, 0, 0, 0, 50, 0, 9, 9, 98},

    {0, 0, 0, 0, 80, 0, 5, 9, 70},  // 16; thumb
    {0, 0, 0, 0, 50, 0, 4, 4, 91},
    {0, 0, 0, 0, 80, 0, 5, 9, 71},  // thumb-new id

    {0, 0, 0, 0, 50, 0, 8.0, 4, 95},  // 19; very close together fingers:
    {0, 0, 0, 0, 50, 0, 8.1, 4, 96},
    {0, 0, 0, 0, 10, 0, 9.5, 4, 95},  // very light pressure
    {0, 0, 0, 0, 10, 0, 11,  4, 96},

    {0, 0, 0, 0, 50, 0, 20, 4, 95},  // 23; Two fingers very far apart
    {0, 0, 0, 0, 50, 0, 90, 4, 96},
  };
  HWStateGs hwsgs[] = {
    // Simple 1-finger tap
    {{ 0.00, 0, 1, 1, &fs[0] }, -1,  MkSet(91),   0,   0, kFTB, false },
    {{ 0.01, 0, 0, 0, NULL   }, -1,  MkSet(),     0,   0, kTpC, true },
    {{ 0.07, 0, 0, 0, NULL   }, .07, MkSet(),   kBL, kBL, kIdl, false },
    // 1-finger tap with click
    {{ 0.00, kBL, 1, 1, &fs[0] }, -1,  MkSet(91),   0,   0, kIdl, false },  // 3
    {{ 0.01,   0, 0, 0, NULL   }, -1,  MkSet(),     0,   0, kIdl, false },
    {{ 0.07,   0, 0, 0, NULL   }, .07, MkSet(),     0,   0, kIdl, false },
    // 1-finger swipe
    {{ 0.00, 0, 1, 1, &fs[4] }, -1, MkSet(95),   0,   0, kFTB, false },  // 6
    {{ 0.01, 0, 1, 1, &fs[5] }, -1, MkSet(95),   0,   0, kIdl, false },
    {{ 0.02, 0, 1, 1, &fs[6] }, -1, MkSet(95),   0,   0, kIdl, false },
    {{ 0.03, 0, 0, 0, NULL   }, -1, MkSet(),     0,   0, kIdl, false },
    // Double 1-finger tap
    {{ 0.00, 0, 1, 1, &fs[0] }, -1,  MkSet(91),   0,   0, kFTB, false },  // 10
    {{ 0.01, 0, 0, 0, NULL   }, -1,  MkSet(),     0,   0, kTpC, true },
    {{ 0.02, 0, 1, 1, &fs[2] }, -1,  MkSet(93), kBL,   0, kSTB, false },
    {{ 0.03, 0, 0, 0, NULL   }, -1,  MkSet(),     0, kBL, kTpC, true },
    {{ 0.09, 0, 0, 0, NULL   }, .09, MkSet(),   kBL, kBL, kIdl, false },
    // Triple 1-finger tap
    {{ 0.00, 0, 1, 1, &fs[0] }, -1,  MkSet(91),   0,   0, kFTB, false },  // 15
    {{ 0.01, 0, 0, 0, NULL   }, -1,  MkSet(),     0,   0, kTpC, true },
    {{ 0.02, 0, 1, 1, &fs[2] }, -1,  MkSet(93), kBL,   0, kSTB, false },
    {{ 0.03, 0, 0, 0, NULL   }, -1,  MkSet(),     0, kBL, kTpC, true },
    {{ 0.04, 0, 1, 1, &fs[3] }, -1,  MkSet(94), kBL,   0, kSTB, false },
    {{ 0.05, 0, 0, 0, NULL   }, -1,  MkSet(),     0, kBL, kTpC, true },
    {{ 0.11, 0, 0, 0, NULL   }, .11, MkSet(),   kBL, kBL, kIdl, false },
    // 1-finger tap + move
    {{ 0.00, 0, 1, 1, &fs[0] }, -1,  MkSet(91),   0,   0, kFTB, false },  // 22
    {{ 0.01, 0, 0, 0, NULL   }, -1,  MkSet(),     0,   0, kTpC, true },
    {{ 0.02, 0, 1, 1, &fs[4] }, -1,  MkSet(95), kBL,   0, kSTB, false },
    {{ 0.03, 0, 1, 1, &fs[5] }, -1,  MkSet(95),   0,   0, kDrg, false },
    {{ 0.04, 0, 1, 1, &fs[6] }, -1,  MkSet(95),   0,   0, kDrg, false },
    {{ 0.05, 0, 0, 0, NULL   }, -1,  MkSet(),     0,   0, kDRl, true },
    {{ 0.11, 0, 0, 0, NULL   }, .11, MkSet(),     0, kBL, kIdl, false },
    // 1-finger tap, move, release, move again (drag lock)
    {{ 0.00, 0, 1, 1, &fs[0] }, -1,  MkSet(91),   0,   0, kFTB, false },  // 29
    {{ 0.01, 0, 0, 0, NULL   }, -1,  MkSet(),     0,   0, kTpC, true },
    {{ 0.02, 0, 1, 1, &fs[4] }, -1,  MkSet(95), kBL,   0, kSTB, false },
    {{ 0.03, 0, 1, 1, &fs[5] }, -1,  MkSet(95),   0,   0, kDrg, false },
    {{ 0.04, 0, 1, 1, &fs[6] }, -1,  MkSet(95),   0,   0, kDrg, false },
    {{ 0.05, 0, 0, 0, NULL   }, -1,  MkSet(),     0,   0, kDRl, true },
    {{ 0.06, 0, 1, 1, &fs[7] }, -1,  MkSet(96),   0,   0, kDRt, false },
    {{ 0.07, 0, 1, 1, &fs[8] }, -1,  MkSet(96),   0,   0, kDrg, false },
    {{ 0.08, 0, 1, 1, &fs[9] }, -1,  MkSet(96),   0,   0, kDrg, false },
    {{ 0.09, 0, 0, 0, NULL   }, -1,  MkSet(),     0,   0, kDRl, true },
    {{ 0.15, 0, 0, 0, NULL   }, .15, MkSet(),     0, kBL, kIdl, false },
    // 1-finger long press
    {{ 0.00, 0, 1, 1, &fs[0] }, -1, MkSet(91),   0,   0, kFTB, false },  // 40
    {{ 0.02, 0, 1, 1, &fs[0] }, -1, MkSet(91),   0,   0, kFTB, false },
    {{ 0.04, 0, 1, 1, &fs[0] }, -1, MkSet(91),   0,   0, kFTB, false },
    {{ 0.06, 0, 1, 1, &fs[0] }, -1, MkSet(91),   0,   0, kIdl, false },
    {{ 0.07, 0, 0, 0, NULL   }, -1, MkSet(),     0,   0, kIdl, false },
    // 1-finger tap then long press
    {{ 0.00, 0, 1, 1, &fs[0] }, -1,  MkSet(91),   0,   0, kFTB, false },  // 45
    {{ 0.01, 0, 0, 0, NULL   }, -1,  MkSet(),     0,   0, kTpC, true },
    {{ 0.02, 0, 1, 1, &fs[4] }, -1,  MkSet(95), kBL,   0, kSTB, false },
    {{ 0.04, 0, 1, 1, &fs[4] }, -1,  MkSet(95),   0,   0, kSTB, false },
    {{ 0.06, 0, 1, 1, &fs[4] }, -1,  MkSet(95),   0,   0, kSTB, false },
    {{ 0.08, 0, 1, 1, &fs[4] }, -1,  MkSet(95),   0,   0, kDrg, false },
    {{ 0.09, 0, 0, 0, NULL   }, -1,  MkSet(),     0,   0, kDRl, true },
    {{ 0.15, 0, 0, 0, NULL   }, .15, MkSet(),     0, kBL, kIdl, false },
    // 2-finger tap (right click)
    {{ 0.00, 0, 2, 2, &fs[10] }, -1,  MkSet(97, 98), 0, 0, kFTB, false },  // 53
    {{ 0.01, 0, 0, 0, NULL    }, -1,  MkSet(),       kBR, kBR, kIdl, false },
    {{ 0.07, 0, 0, 0, NULL    }, .07, MkSet(),         0,   0, kIdl, false },
    // 2-finger scroll
    {{ 0.00, 0, 2, 2, &fs[10] }, -1, MkSet(97, 98),  0, 0, kFTB, false },  // 56
    {{ 0.01, 0, 2, 2, &fs[12] }, -1, MkSet(97, 98),   0,   0, kIdl, false },
    {{ 0.02, 0, 2, 2, &fs[14] }, -1, MkSet(97, 98),   0,   0, kIdl, false },
    {{ 0.03, 0, 0, 0, NULL    }, -1, MkSet(),         0,   0, kIdl, false },
    // left tap, right tap
    {{ 0.00, 0, 1, 1, &fs[0] },  -1,  MkSet(91),     0, 0, kFTB, false },  // 60
    {{ 0.01, 0, 0, 0, NULL   },  -1,  MkSet(),         0,   0, kTpC, true },
    {{ 0.02, 0, 2, 2, &fs[10] }, -1,  MkSet(97, 98), kBL,   0, kSTB, false },
    {{ 0.03, 0, 0, 0, NULL   },  -1,  MkSet(),         0, kBL, kTpC, true },
    {{ 0.09, 0, 0, 0, NULL    }, .09, MkSet(),       kBR, kBR, kIdl, false },
    // left tap, multi-frame right tap
    {{ 0.00, 0, 1, 1, &fs[0] },  -1,  MkSet(91),   0,   0, kFTB, false },  // 65
    {{ 0.01, 0, 0, 0, NULL   },  -1,  MkSet(),     0,   0, kTpC, true },
    {{ 0.02, 0, 1, 1, &fs[10] }, -1,  MkSet(97), kBL,   0, kSTB, false },
    {{ 0.03, 0, 1, 1, &fs[11] }, -1,  MkSet(98),   0,   0, kSTB, false },
    {{ 0.04, 0, 0, 0, NULL   },  -1,  MkSet(),     0, kBL, kTpC, true },
    {{ 0.10, 0, 0, 0, NULL    }, .10, MkSet(),   kBR, kBR, kIdl, false },
    // right tap, left tap
    {{ 0.00, 0, 2, 2, &fs[10] }, -1,  MkSet(97, 98),  0, 0, kFTB, false }, // 71
    {{ 0.01, 0, 0, 0, NULL   },  -1,  MkSet(),       kBR, kBR, kIdl, false },
    {{ 0.02, 0, 1, 1, &fs[0] },  -1,  MkSet(91),       0,   0, kFTB, false },
    {{ 0.03, 0, 0, 0, NULL   },  -1,  MkSet(),         0,   0, kTpC, true },
    {{ 0.09, 0, 0, 0, NULL    }, .09, MkSet(),       kBL, kBL, kIdl, false },
    // right double-tap
    {{ 0.00, 0, 2, 2, &fs[6] },  -1,  MkSet(95, 96),  0, 0, kFTB, false }, // 76
    {{ 0.01, 0, 0, 0, NULL   },  -1,  MkSet(),       kBR, kBR, kIdl, false },
    {{ 0.02, 0, 2, 2, &fs[10] }, -1,  MkSet(97, 98),   0,   0, kFTB, false },
    {{ 0.03, 0, 0, 0, NULL   },  -1,  MkSet(),       kBR, kBR, kIdl, false },
    {{ 0.09, 0, 0, 0, NULL    }, .09, MkSet(),         0,   0, kIdl, false },
    // left tap, right-drag
    {{ 0.00, 0, 1, 1, &fs[0] },  -1, MkSet(91),       0, 0, kFTB, false }, // 81
    {{ 0.01, 0, 0, 0, NULL   },  -1, MkSet(),         0,   0, kTpC, true },
    {{ 0.02, 0, 2, 2, &fs[10] }, -1, MkSet(97, 98), kBL,   0, kSTB, false },
    {{ 0.03, 0, 2, 2, &fs[12] }, -1, MkSet(97, 98),   0, kBL, kIdl, false },
    {{ 0.04, 0, 2, 2, &fs[14] }, -1, MkSet(97, 98),   0,   0, kIdl, false },
    {{ 0.05, 0, 0, 0, NULL   },  -1, MkSet(),         0,   0, kIdl, false },
    // left tap, right multi-frame join + drag
    {{ 0.00, 0, 1, 1, &fs[0] },  -1, MkSet(91),       0, 0, kFTB, false }, // 87
    {{ 0.01, 0, 0, 0, NULL   },  -1, MkSet(),         0,   0, kTpC, true },
    {{ 0.02, 0, 1, 1, &fs[10] }, -1, MkSet(97),     kBL,   0, kSTB, false },
    {{ 0.03, 0, 1, 1, &fs[12] }, -1, MkSet(97),       0,   0, kDrg, false },
    {{ 0.04, 0, 2, 2, &fs[14] }, -1, MkSet(97, 98),   0, kBL, kIdl, false },
    {{ 0.05, 0, 0, 0, NULL   },  -1, MkSet(),         0,   0, kIdl, false },
    // right tap, left-drag
    {{ 0.00, 0, 2, 2, &fs[14] }, -1, MkSet(97, 98),  0, 0, kFTB, false },  // 93
    {{ 0.01, 0, 0, 0, NULL   }, -1,  MkSet(),       kBR, kBR, kIdl, false },
    {{ 0.02, 0, 1, 1, &fs[4] }, -1,  MkSet(95),       0,   0, kFTB, false },
    {{ 0.03, 0, 1, 1, &fs[5] }, -1,  MkSet(95),       0,   0, kIdl, false },
    {{ 0.04, 0, 1, 1, &fs[6] }, -1,  MkSet(95),       0,   0, kIdl, false },
    {{ 0.05, 0, 0, 0, NULL   }, -1,  MkSet(),         0,   0, kIdl, false },
    // right tap, right-drag
    {{ 0.00, 0, 2, 2, &fs[6] },  -1,  MkSet(95, 96),  0, 0, kFTB, false }, // 99
    {{ 0.01, 0, 0, 0, NULL   },  -1,  MkSet(),       kBR, kBR, kIdl, false },
    {{ 0.02, 0, 2, 2, &fs[10] }, -1,  MkSet(97, 98),   0,   0, kFTB, false },
    {{ 0.03, 0, 2, 2, &fs[12] }, -1,  MkSet(97, 98),   0,   0, kIdl, false },
    {{ 0.04, 0, 2, 2, &fs[14] }, -1,  MkSet(97, 98),   0,   0, kIdl, false },
    {{ 0.05, 0, 0, 0, NULL   },  -1,  MkSet(),         0,   0, kIdl, false },
    // drag then right-tap
    {{ 0.00, 0, 1, 1, &fs[0] }, -1,  MkSet(91),     0,  0, kFTB, false }, // 105
    {{ 0.01, 0, 0, 0, NULL   }, -1,  MkSet(),       0,   0, kTpC, true },
    {{ 0.02, 0, 1, 1, &fs[4] }, -1,  MkSet(95),   kBL,   0, kSTB, false },
    {{ 0.03, 0, 1, 1, &fs[5] }, -1,  MkSet(95),     0,   0, kDrg, false },
    {{ 0.04, 0, 1, 1, &fs[6] }, -1,  MkSet(95),     0,   0, kDrg, false },
    {{ 0.05, 0, 0, 0, NULL   }, -1,  MkSet(),       0,   0, kDRl, true },
    {{ 0.06, 0, 2, 2, &fs[10] }, -1, MkSet(97, 98), 0,   0, kDRt, false },
    {{ 0.07, 0, 0, 0, NULL   }, -1,  MkSet(),       0, kBL, kTpC, true },
    {{ 0.13, 0, 0, 0, NULL   }, .13, MkSet(),     kBR, kBR, kIdl, false },
    // slow double tap
    {{ 0.00, 0, 1, 1, &fs[0] }, -1, MkSet(91),   0,   0, kFTB, false },  // 114
    {{ 0.10, 0, 0, 0, NULL   }, -1, MkSet(),     0,   0, kTpC, true },
    {{ 0.12, 0, 1, 1, &fs[2] }, -1, MkSet(93), kBL,   0, kSTB, false },
    {{ 0.22, 0, 0, 0, NULL   }, -1, MkSet(),     0, kBL, kTpC, true },
    {{ 0.90, 0, 0, 0, NULL   }, .9, MkSet(),   kBL, kBL, kIdl, false },
    // right tap, very close fingers - shouldn't tap
    {{ 0.00, 0, 2, 2, &fs[19] }, -1, MkSet(95, 96), 0, 0, kIdl, false },  // 119
    {{ 0.01, 0, 0, 0, NULL    }, -1, MkSet(),       0, 0, kIdl, false },
    // very light left tap - shouldn't tap
    {{ 0.00, 0, 1, 1, &fs[21] }, -1, MkSet(95), 0, 0, kFTB, false },  // 121
    {{ 0.01, 0, 0, 0, NULL    }, -1, MkSet(),   0, 0, kIdl, false },
    // very light right tap - shouldn't tap
    {{ 0.00, 0, 2, 2, &fs[21] }, -1, MkSet(95, 96), 0, 0, kFTB, false },  // 123
    {{ 0.01, 0, 0, 0, NULL    }, -1, MkSet(),       0, 0, kIdl, false },
    // half very light right tap - should tap
    {{ 0.00, 0, 2, 2, &fs[20] }, -1, MkSet(95, 96), 0, 0, kFTB, false },  // 125
    {{ 0.01, 0, 0, 0, NULL    }, -1, MkSet(),   kBR, kBR, kIdl, false },
    // Right tap, w/ fingers too far apart - shouldn't right tap
    {{ 0.00, 0, 2, 2, &fs[23] }, -1,  MkSet(95, 96), 0, 0, kFTB, false }, // 127
    {{ 0.01, 0, 0, 0, NULL    }, -1,  MkSet(),       0, 0, kTpC, true },
    {{ 0.07, 0, 0, 0, NULL    }, .07, MkSet(),   kBL, kBL, kIdl, false },
    // Two fingers merge into one, then leave - shouldn't tap
    {{ 0.00, 0, 2, 2, &fs[6] },  -1, MkSet(95, 96), 0, 0, kFTB, false },  // 130
    {{ 1.00, 0, 2, 2, &fs[6] },  -1, MkSet(95, 96), 0, 0, kIdl, false },
    {{ 1.01, 0, 1, 1, &fs[17] }, -1, MkSet(91),     0, 0, kIdl, false },
    {{ 1.02, 0, 0, 0, NULL },    -1, MkSet(),       0, 0, kIdl, false },
    // T5R2 tap tests:
    // (1f and 2f tap w/o resting thumb and 1f w/ resting thumb are the same as
    // above)
    // 2f tap w/ resting thumb
    {{ 0.00, 0, 1, 1, &fs[16] }, -1, MkSet(70),   0,   0, kFTB, false },  // 134
    {{ 1.00, 0, 1, 1, &fs[16] }, -1, MkSet(70),   0,   0, kIdl, false },
    {{ 1.01, 0, 1, 3, &fs[16] }, -1, MkSet(70),   0,   0, kFTB, false },
    {{ 1.02, 0, 2, 3, &fs[16] }, -1, MkSet(70, 91), 0, 0, kFTB, false },
    {{ 1.03, 0, 0, 2, NULL    }, -1, MkSet(),     0,   0, kFTB, false },
    {{ 1.04, 0, 1, 1, &fs[18] }, -1, MkSet(71), kBR, kBR, kIdl, false },
    // 3f tap w/o resting thumb
    {{ 0.00, 0, 2, 3, &fs[0] }, -1,  MkSet(91, 92), 0, 0, kFTB, false },  // 140
    {{ 0.01, 0, 0, 1, NULL   }, -1,  MkSet(),       0, 0, kFTB, false },
    {{ 0.02, 0, 0, 0, NULL   }, -1,  MkSet(),   kBR, kBR, kIdl, false },
    // 3f tap w/o resting thumb (slightly different)
    {{ 0.00, 0, 2, 3, &fs[0] }, -1,  MkSet(91, 92), 0, 0, kFTB, false },  // 143
    {{ 0.01, 0, 2, 3, &fs[0] }, -1,  MkSet(91, 92), 0, 0, kFTB, false },
    {{ 0.02, 0, 0, 0, NULL   }, -1,  MkSet(),   kBR, kBR, kIdl, false },
    // 3f tap w/ resting thumb
    {{ 0.00, 0, 1, 1, &fs[16] }, -1, MkSet(70),   0,   0, kFTB, false },  // 146
    {{ 1.00, 0, 1, 1, &fs[16] }, -1, MkSet(70),   0,   0, kIdl, false },
    {{ 1.01, 0, 1, 4, &fs[16] }, -1, MkSet(70),   0,   0, kFTB, false },
    {{ 1.02, 0, 2, 4, &fs[16] }, -1, MkSet(70, 91), 0, 0, kFTB, false },
    {{ 1.03, 0, 1, 1, &fs[16] }, -1, MkSet(70), kBR, kBR, kIdl, false },
    // 4f tap w/o resting thumb
    {{ 0.00, 0, 2, 3, &fs[0] }, -1,  MkSet(91, 92), 0, 0, kFTB, false },  // 152
    {{ 0.01, 0, 1, 4, &fs[0] }, -1,  MkSet(91),     0, 0, kFTB, false },
    {{ 0.02, 0, 2, 4, &fs[0] }, -1,  MkSet(91, 92), 0, 0, kFTB, false },
    {{ 0.03, 0, 0, 0, NULL   }, -1,  MkSet(),   kBR, kBR, kIdl, false },
    // 4f tap w/ resting thumb
    {{ 0.00, 0, 1, 1, &fs[16] }, -1, MkSet(70),   0,   0, kFTB, false },  // 155
    {{ 1.00, 0, 1, 1, &fs[16] }, -1, MkSet(70),   0,   0, kIdl, false },
    {{ 1.01, 0, 1, 5, &fs[16] }, -1, MkSet(70),   0,   0, kFTB, false },
    {{ 1.02, 0, 1, 1, &fs[16] }, -1, MkSet(70), kBR, kBR, kIdl, false },
    // 4f tap w/ resting thumb (slightly different)
    {{ 0.00, 0, 1, 1, &fs[16] }, -1, MkSet(70),   0,   0, kFTB, false },  // 159
    {{ 1.00, 0, 1, 1, &fs[16] }, -1, MkSet(70),   0,   0, kIdl, false },
    {{ 1.01, 0, 1, 5, &fs[16] }, -1, MkSet(70),   0,   0, kFTB, false },
    {{ 1.02, 0, 2, 5, &fs[16] }, -1, MkSet(70, 91), 0, 0, kFTB, false },
    {{ 1.03, 0, 1, 1, &fs[16] }, -1, MkSet(70), kBR, kBR, kIdl, false },
    // 3f letting go, shouldn't tap at all
    {{ 0.00, 0, 2, 3, &fs[0] },  -1, MkSet(91, 92), 0, 0, kFTB, false },  // 166
    {{ 1.01, 0, 2, 3, &fs[0] },  -1, MkSet(91, 92), 0, 0, kIdl, false },
    {{ 1.02, 0, 0, 2, NULL   },  -1, MkSet(),       0, 0, kIdl, false },
    {{ 1.03, 0, 2, 2, &fs[10] }, -1, MkSet(97, 98), 0, 0, kIdl, false },
    {{ 1.04, 0, 0, 0, NULL   },  -1, MkSet(),       0, 0, kIdl, false },
  };
  const size_t kSlowDoubleTapStartIndex = 114;
  const size_t kT5R2TestFirstIndex = 134;

  // Algorithmically add a resting thumb to a copy of all above cases
  HWStateGs hwsgs_full[arraysize(hwsgs) + kT5R2TestFirstIndex];
  std::copy(hwsgs, hwsgs + arraysize(hwsgs), hwsgs_full);
  std::copy(hwsgs, hwsgs + kT5R2TestFirstIndex, hwsgs_full + arraysize(hwsgs));

  vector<vector<FingerState> > thumb_fs(arraysize(hwsgs));
  const FingerState& fs_thumb = fs[18];
  for (size_t i = 0; i < kT5R2TestFirstIndex; ++i) {
    HardwareState* hs = &hwsgs_full[i + arraysize(hwsgs)].hws;
    vector<FingerState>& newfs = thumb_fs[i];
    newfs.resize(hs->finger_cnt + 1);
    newfs[0] = fs_thumb;
    for (size_t j = 0; j < hs->finger_cnt; ++j)
      newfs[j + 1] = hs->fingers[j];
    set<short, kMaxGesturingFingers>& gs = hwsgs_full[i + arraysize(hwsgs)].gs;
    if (gs.empty())
      gs.insert(fs_thumb.tracking_id);
    hs->fingers = &thumb_fs[i][0];
    hs->finger_cnt++;
    hs->touch_cnt++;
  }

  for (size_t i = 0; i < arraysize(hwsgs_full); ++i) {
    string desc;
    if (i < arraysize(hwsgs))
      desc = StringPrintf("State %zu", i);
    else
      desc = StringPrintf("State %zu (resting thumb)", i - arraysize(hwsgs));

    unsigned bdown = 0;
    unsigned bup = 0;
    stime_t tm = -1.0;
    bool same_fingers = false;
    HardwareState* hwstate = &hwsgs_full[i].hws;
    stime_t now = hwsgs_full[i].callback_now;
    if (hwsgs_full[i].callback_now >= 0.0) {
      hwstate = NULL;
    } else {
      now = hwsgs_full[i].hws.timestamp;
    }

    if (hwstate && hwstate->timestamp == 0.0) {
      // Reset imm interpreter
      LOG(INFO) << "Resetting imm interpreter, i = " << i;
      ii.reset(new ImmediateInterpreter(NULL));
      ii->SetHardwareProperties(hwprops);
      ii->drag_lock_enable_.val_ = 1;
      ii->motion_tap_prevent_timeout_.val_ = 0;
      ii->tapping_finger_min_separation_.val_ = 1.0;
      ii->tap_drag_timeout_.val_ = 0.05;
      ii->tap_enable_.val_ = 1;
      ii->tap_move_dist_.val_ = 1.0;
      ii->tap_timeout_.val_ = ii->inter_tap_timeout_.val_ = 0.05;
      // For the slow tap case, we need to make tap_timeout_ bigger
      if (i % arraysize(hwsgs) == kSlowDoubleTapStartIndex)
        ii->tap_timeout_.val_ = 0.11;
      EXPECT_EQ(kIdl, ii->tap_to_click_state_);
    } else {
      same_fingers = ii->prev_state_.SameFingersAs(hwsgs_full[i].hws);
    }

    ii->UpdateTapState(
        hwstate, hwsgs_full[i].gs, same_fingers, now, &bdown, &bup, &tm);
    ii->prev_gs_fingers_ = hwsgs_full[i].gs;
    if (hwstate)
      ii->SetPrevState(*hwstate);
    EXPECT_EQ(hwsgs_full[i].expected_down, bdown) << desc;
    EXPECT_EQ(hwsgs_full[i].expected_up, bup) << desc;
    if (hwsgs_full[i].timeout)
      EXPECT_GT(tm, 0.0) << desc;
    else
      EXPECT_DOUBLE_EQ(-1.0, tm) << desc;
    EXPECT_EQ(hwsgs_full[i].expected_state, ii->tap_to_click_state_)
        << desc;
  }
}

// Does two tap gestures, one with keyboard interference.
TEST(ImmediateInterpreterTest, TapToClickKeyboardTest) {
  scoped_ptr<ImmediateInterpreter> ii;

  HardwareProperties hwprops = {
    0,  // left edge
    0,  // top edge
    200,  // right edge
    200,  // bottom edge
    1.0,  // pixels/TP width
    1.0,  // pixels/TP height
    1.0,  // screen DPI x
    1.0,  // screen DPI y
    5,  // max fingers
    5,  // max touch
    0,  // t5r2
    0,  // semi-mt
    1  // is button pad
  };

  FingerState fs = {
    // TM, Tm, WM, Wm, Press, Orientation, X, Y, TrID
    0, 0, 0, 0, 50, 0, 4, 4, 91
  };
  HardwareState hwstates[] = {
    // Simple 1-finger tap
    { 0.01, 0, 1, 1, &fs },
    { 0.02, 0, 0, 0, NULL },
    { 0.30, 0, 0, 0, NULL }
  };

  enum {
    kWithoutKeyboard = 0,
    kWithKeyboard,
    kMaxTests
  };
  for (size_t test = 0; test != kMaxTests; test++) {
    ii.reset(new ImmediateInterpreter(NULL));
    ii->SetHardwareProperties(hwprops);
    ii->motion_tap_prevent_timeout_.val_ = 0;
    ii->tap_enable_.val_ = 1;

    if (test == kWithKeyboard)
      ii->keyboard_touched_ = 0.001;

    unsigned down = 0;
    unsigned up = 0;
    for (size_t i = 0; i < arraysize(hwstates); i++) {
      down = 0;
      up = 0;
      stime_t timeout = -1.0;
      set<short, kMaxGesturingFingers> gs =
          hwstates[i].finger_cnt == 1 ? MkSet(91) : MkSet();
      ii->UpdateTapState(
          &hwstates[i],
          gs,
          false,  // same fingers
          hwstates[i].timestamp,
          &down,
          &up,
          &timeout);
    }
    EXPECT_EQ(down, up);
    if (test == kWithoutKeyboard)
      EXPECT_EQ(GESTURES_BUTTON_LEFT, down);
    else
      EXPECT_EQ(0, down);
  }
}

TEST(ImmediateInterpreterTest, TapToClickEnableTest) {
  scoped_ptr<ImmediateInterpreter> ii;

  HardwareProperties hwprops = {
    0,  // left edge
    0,  // top edge
    200,  // right edge
    200,  // bottom edge
    1.0,  // pixels/TP width
    1.0,  // pixels/TP height
    1.0,  // screen DPI x
    1.0,  // screen DPI y
    5,  // max fingers
    5,  // max touch
    0,  // t5r2
    0,  // semi-mt
    1  // is button pad
  };

  FingerState fs[] = {
    // TM, Tm, WM, Wm, Press, Orientation, X, Y, TrID
    {0, 0, 0, 0, 50, 0, 4, 4, 91},
    {0, 0, 0, 0, 50, 0, 4, 4, 92},
    {0, 0, 0, 0, 50, 0, 6, 4, 92},
    {0, 0, 0, 0, 50, 0, 8, 4, 92},
    {0, 0, 0, 0, 50, 0, 4, 4, 93},
    {0, 0, 0, 0, 50, 0, 6, 4, 93},
    {0, 0, 0, 0, 50, 0, 8, 4, 93},
  };

  HWStateGs hwsgs_list[] = {
    // 1-finger tap, move, release, move again (drag lock)
    {{ 0.00, 0, 1, 1, &fs[0] }, -1,  MkSet(91),   0,   0, kFTB, false },
    {{ 0.01, 0, 0, 0, NULL   }, -1,  MkSet(),     0,   0, kTpC, true },
    {{ 0.02, 0, 1, 1, &fs[1] }, -1,  MkSet(92), kBL,   0, kSTB, false },
    {{ 0.03, 0, 1, 1, &fs[2] }, -1,  MkSet(92),   0,   0, kDrg, false },
    {{ 0.04, 0, 1, 1, &fs[3] }, -1,  MkSet(92),   0,   0, kDrg, false },
    {{ 0.05, 0, 0, 0, NULL   }, -1,  MkSet(),     0,   0, kDRl, true },
    {{ 0.06, 0, 1, 1, &fs[4] }, -1,  MkSet(93),   0,   0, kDRt, false },
    {{ 0.07, 0, 1, 1, &fs[5] }, -1,  MkSet(93),   0,   0, kDrg, false },
    {{ 0.08, 0, 1, 1, &fs[6] }, -1,  MkSet(93),   0,   0, kDrg, false },
    {{ 0.09, 0, 0, 0, NULL   }, -1,  MkSet(),     0,   0, kDRl, true },
    {{ 0.15, 0, 0, 0, NULL   }, .15, MkSet(),     0, kBL, kIdl, false }
  };

  for (int iter = 0; iter < 3; ++iter) {
    for (size_t i = 0; i < arraysize(hwsgs_list); ++i) {
      string desc;
      stime_t disable_time = 0.0;
      switch (iter) {
        case 0:  // test with tap enabled
          desc = StringPrintf("State %zu (tap enabled)", i);
          disable_time = -1;  // unreachable time
          break;
        case 1:  // test with tap disabled during gesture
          desc = StringPrintf("State %zu (tap disabled during gesture)", i);
          disable_time = 0.02;
          break;
        case 2:  // test with tap disabled before gesture (while Idle)
          desc = StringPrintf("State %zu (tap disabled while Idle)", i);
          disable_time = 0.00;
          break;
      }

      HWStateGs &hwsgs = hwsgs_list[i];
      HardwareState* hwstate = &hwsgs.hws;
      stime_t now = hwsgs.callback_now;
      if (hwsgs.callback_now >= 0.0)
        hwstate = NULL;
      else
        now = hwsgs.hws.timestamp;

      bool same_fingers = false;
      if (hwstate && hwstate->timestamp == 0.0) {
        // Reset imm interpreter
        LOG(INFO) << "Resetting imm interpreter, i = " << i;
        ii.reset(new ImmediateInterpreter(NULL));
        ii->SetHardwareProperties(hwprops);
        ii->drag_lock_enable_.val_ = 1;
        ii->motion_tap_prevent_timeout_.val_ = 0;
        ii->tap_drag_timeout_.val_ = 0.05;
        ii->tap_enable_.val_ = 1;
        ii->tap_move_dist_.val_ = 1.0;
        ii->tap_timeout_.val_ = 0.05;
        EXPECT_EQ(kIdl, ii->tap_to_click_state_);
        EXPECT_TRUE(ii->tap_enable_.val_);
      } else {
        same_fingers = ii->prev_state_.SameFingersAs(hwsgs.hws);
      }

      // Disable tap in the middle of the gesture
      if (hwstate && hwstate->timestamp == disable_time)
        ii->tap_enable_.val_ = false;

      unsigned bdown = 0;
      unsigned bup = 0;
      stime_t tm = -1.0;
      ii->UpdateTapState(
          hwstate, hwsgs.gs, same_fingers, now, &bdown, &bup, &tm);
      ii->prev_gs_fingers_ = hwsgs.gs;
      if (hwstate)
        ii->SetPrevState(*hwstate);

      switch (iter) {
        case 0:  // tap should be enabled
        case 1:
          EXPECT_EQ(hwsgs.expected_down, bdown) << desc;
          EXPECT_EQ(hwsgs.expected_up, bup) << desc;
          if (hwsgs.timeout)
            EXPECT_GT(tm, 0.0) << desc;
          else
            EXPECT_DOUBLE_EQ(-1.0, tm) << desc;
          EXPECT_EQ(hwsgs.expected_state, ii->tap_to_click_state_) << desc;
          break;
        case 2:  // tap should be disabled
          EXPECT_EQ(0, bdown) << desc;
          EXPECT_EQ(0, bup) << desc;
          EXPECT_DOUBLE_EQ(-1.0, tm) << desc;
          EXPECT_EQ(kIdl, ii->tap_to_click_state_) << desc;
          break;
      }
    }
  }
}

TEST(ImmediateInterpreterTest, WiggleSuppressTest) {
  ImmediateInterpreter ii(NULL);
  HardwareProperties hwprops = {
    0,  // left edge
    0,  // top edge
    92,  // right edge
    61,  // bottom edge
    1,  // x pixels/TP width
    1,  // y pixels/TP height
    26,  // x screen DPI
    26,  // y screen DPI
    2,  // max fingers
    5,  // max touch
    0,  // t5r2
    0,  // semi-mt
    0  // is button pad
  };
  ii.SetHardwareProperties(hwprops);

  // These values come from a recording of my finger
  FingerState finger_states[] = {
    // TM, Tm, WM, Wm, Press, Orientation, X, Y, TrID
    {0, 0, 0, 0, 38.299999, 0, 43.195655, 32.814815, 1},
    {0, 0, 0, 0, 39.820442, 0, 43.129665, 32.872276, 1},
    {0, 0, 0, 0, 44.924972, 0, 42.881202, 33.077861, 1},
    {0, 0, 0, 0, 52.412372, 0, 42.476348, 33.405296, 1},
    {0, 0, 0, 0, 59.623386, 0, 42.064849, 33.772129, 1},
    {0, 0, 0, 0, 65.317642, 0, 41.741107, 34.157428, 1},
    {0, 0, 0, 0, 69.175155, 0, 41.524814, 34.531333, 1},
    {0, 0, 0, 0, 71.559425, 0, 41.390705, 34.840869, 1},
    {0, 0, 0, 0, 73.018020, 0, 41.294445, 35.082786, 1},
    {0, 0, 0, 0, 73.918144, 0, 41.210456, 35.280235, 1},
    {0, 0, 0, 0, 74.453460, 0, 41.138065, 35.426036, 1},
    {0, 0, 0, 0, 74.585144, 0, 41.084125, 35.506179, 1},
    {0, 0, 0, 0, 74.297470, 0, 41.052356, 35.498870, 1},
    {0, 0, 0, 0, 73.479888, 0, 41.064708, 35.364994, 1},
    {0, 0, 0, 0, 71.686737, 0, 41.178459, 35.072589, 1},
    {0, 0, 0, 0, 68.128448, 0, 41.473480, 34.566291, 1},
    {0, 0, 0, 0, 62.086532, 0, 42.010086, 33.763534, 1},
    {0, 0, 0, 0, 52.739898, 0, 42.745056, 32.644023, 1},
  };
  HardwareState hardware_state[] = {
    // time, buttons, finger count, touch count, finger states pointer
    { 1319735240.654559, 1, 1, 1, &finger_states[0] },
    { 1319735240.667746, 1, 1, 1, &finger_states[1] },
    { 1319735240.680153, 1, 1, 1, &finger_states[2] },
    { 1319735240.693717, 1, 1, 1, &finger_states[3] },
    { 1319735240.707821, 1, 1, 1, &finger_states[4] },
    { 1319735240.720633, 1, 1, 1, &finger_states[5] },
    { 1319735240.733183, 1, 1, 1, &finger_states[6] },
    { 1319735240.746131, 1, 1, 1, &finger_states[7] },
    { 1319735240.758622, 1, 1, 1, &finger_states[8] },
    { 1319735240.772690, 1, 1, 1, &finger_states[9] },
    { 1319735240.785556, 1, 1, 1, &finger_states[10] },
    { 1319735240.798524, 1, 1, 1, &finger_states[11] },
    { 1319735240.811093, 1, 1, 1, &finger_states[12] },
    { 1319735240.824775, 1, 1, 1, &finger_states[13] },
    { 1319735240.837738, 0, 1, 1, &finger_states[14] },
    { 1319735240.850482, 0, 1, 1, &finger_states[15] },
    { 1319735240.862749, 0, 1, 1, &finger_states[16] },
    { 1319735240.876571, 0, 1, 1, &finger_states[17] },
    { 1319735240.888128, 0, 0, 0, NULL }
  };

  for (size_t i = 0; i < arraysize(hardware_state); ++i) {
    Gesture* result = ii.SyncInterpret(&hardware_state[i], NULL);
    if (result)
      EXPECT_NE(kGestureTypeMove, result->type);
  }
}

struct ClickTestHardwareStateAndExpectations {
  HardwareState hs;
  unsigned expected_down;
  unsigned expected_up;
};

TEST(ImmediateInterpreterTest, ClickTest) {
  ImmediateInterpreter ii(NULL);
  HardwareProperties hwprops = {
    0,  // left edge
    0,  // top edge
    100,  // right edge
    100,  // bottom edge
    1,  // x pixels/mm
    1,  // y pixels/mm
    96,  // x screen DPI
    96,  // y screen DPI
    2,  // max fingers
    5,  // max touch
    1,  // t5r2
    0,  // semi-mt
    1  // is button pad
  };
  ii.SetHardwareProperties(hwprops);
  EXPECT_FLOAT_EQ(10.0, ii.tapping_finger_min_separation_.val_);

  FingerState finger_states[] = {
    // TM, Tm, WM, Wm, Press, Orientation, X, Y, TrID
    {0, 0, 0, 0, 10, 0, 50, 50, 1},
    {0, 0, 0, 0, 10, 0, 70, 50, 2},
    // Fingers very close together - shouldn't right click
    {0, 0, 0, 0, 10, 0, 50, 50, 1},
    {0, 0, 0, 0, 10, 0, 55, 50, 2},
  };
  ClickTestHardwareStateAndExpectations records[] = {
    { { 0,    0, 0, 0, NULL },              0, 0 },
    { { 1,    1, 0, 0, NULL },              0, 0 },
    { { 1.01, 1, 2, 3, &finger_states[0] }, GESTURES_BUTTON_RIGHT, 0 },
    { { 3,    0, 0, 0, NULL },              0, GESTURES_BUTTON_RIGHT },
    { { 4,    1, 0, 0, NULL },              0, 0 },
    { { 4.01, 1, 2, 2, &finger_states[0] }, GESTURES_BUTTON_RIGHT, 0 },
    { { 6,    0, 0, 0, NULL },              0, GESTURES_BUTTON_RIGHT },
    { { 7,    1, 0, 0, NULL },              0, 0 },
    { { 7.01, 1, 2, 2, &finger_states[2] }, 0, 0 },
    { { 7.05, 1, 2, 2, &finger_states[2] }, GESTURES_BUTTON_LEFT, 0 },
    { { 8,    0, 0, 0, NULL },              0, GESTURES_BUTTON_LEFT },
  };

  for (size_t i = 0; i < arraysize(records); ++i) {
    Gesture* result = ii.SyncInterpret(&records[i].hs, NULL);
    if (records[i].expected_down == 0 && records[i].expected_up == 0) {
      EXPECT_EQ(static_cast<Gesture*>(NULL), result) << "i=" << i;
    } else {
      ASSERT_NE(static_cast<Gesture*>(NULL), result) << "i=" << i;
      EXPECT_EQ(records[i].expected_down, result->details.buttons.down);
      EXPECT_EQ(records[i].expected_up, result->details.buttons.up);
    }
  }
}

struct BigHandsRightClickInputAndExpectations {
  HardwareState hs;
  unsigned out_buttons_down;
  unsigned out_buttons_up;
  FingerState fs[2];
};

// This was recorded from Ian Fette, who had trouble right-clicking.
// This test plays back part of his log to ensure that it generates a
// right click.
TEST(ImmediateInterpreterTest, BigHandsRightClickTest) {
  HardwareProperties hwprops = {
    0,  // left edge
    0,  // top edge
    106.666672,  // right edge
    68.000000,  // bottom edge
    1,  // pixels/TP width
    1,  // pixels/TP height
    25.4,  // screen DPI x
    25.4,  // screen DPI y
    15,  // max fingers
    5,  // max touch
    0,  // t5r2
    0,  // semi-mt
    true  // is button pad
  };
  BigHandsRightClickInputAndExpectations records[] = {
    { { 1329527921.327647, 0, 2, 2, NULL }, 0, 0,
      { { 0, 0, 0, 0, 50.013428, 0, 20.250002, 59.400002, 130 },
        { 0, 0, 0, 0, 41.862095, 0, 57.458694, 43.700001, 131 } } },
    { { 1329527921.344421, 0, 2, 2, NULL }, 0, 0,
      { { 0, 0, 0, 0, 50.301102, 0, 20.250002, 59.400002, 130 },
        { 0, 0, 0, 0, 42.007469, 0, 57.479977, 43.700001, 131 } } },
    { { 1329527921.361196, 1, 2, 2, NULL }, GESTURES_BUTTON_RIGHT, 0,
      { { 0, 0, 0, 0, 50.608433, 0, 20.250002, 59.400002, 130 },
        { 0, 0, 0, 0, 42.065464, 0, 57.494164, 43.700001, 131 } } },
    { { 1329527921.372364, 1, 2, 2, NULL }, 0, 0,
      { { 0, 0, 0, 0, 50.840954, 0, 20.250002, 59.400002, 130 },
        { 0, 0, 0, 0, 42.071739, 0, 57.507217, 43.700001, 131 } } },
    { { 1329527921.383517, 1, 2, 2, NULL }, 0, 0,
      { { 0, 0, 0, 0, 51.047310, 0, 20.250002, 59.400002, 130 },
        { 0, 0, 0, 0, 42.054974, 0, 57.527523, 43.700001, 131 } } },
    { { 1329527921.394680, 1, 2, 2, NULL }, 0, 0,
      { { 0, 0, 0, 0, 51.355824, 0, 20.250002, 59.400002, 130 },
        { 0, 0, 0, 0, 42.066948, 0, 57.550964, 43.700001, 131 } } },
    { { 1329527921.405842, 1, 2, 2, NULL }, 0, 0,
      { { 0, 0, 0, 0, 51.791901, 0, 20.250002, 59.400002, 130 },
        { 0, 0, 0, 0, 42.188736, 0, 57.569374, 43.700001, 131 } } },
    { { 1329527921.416791, 1, 2, 2, NULL }, 0, 0,
      { { 0, 0, 0, 0, 52.264156, 0, 20.250002, 59.400002, 130 },
        { 0, 0, 0, 0, 42.424179, 0, 57.586361, 43.700001, 131 } } },
    { { 1329527921.427937, 1, 2, 2, NULL }, 0, 0,
      { { 0, 0, 0, 0, 52.725105, 0, 20.250002, 59.400002, 130 },
        { 0, 0, 0, 0, 42.676739, 0, 57.609421, 43.700001, 131 } } },
    { { 1329527921.439094, 1, 2, 2, NULL }, 0, 0,
      { { 0, 0, 0, 0, 53.191925, 0, 20.250002, 59.400002, 130 },
        { 0, 0, 0, 0, 42.868217, 0, 57.640007, 43.700001, 131 } } },
    { { 1329527921.461392, 1, 2, 2, NULL }, 0, 0,
      { { 0, 0, 0, 0, 53.602665, 0, 20.250002, 59.400002, 130 },
        { 0, 0, 0, 0, 43.016544, 0, 57.676689, 43.700001, 131 } } },
    { { 1329527921.483690, 1, 2, 2, NULL }, 0, 0,
      { { 0, 0, 0, 0, 53.879429, 0, 20.250002, 59.400002, 130 },
        { 0, 0, 0, 0, 43.208221, 0, 57.711613, 43.700001, 131 } } },
    { { 1329527921.511815, 1, 2, 2, NULL }, 0, 0,
      { { 0, 0, 0, 0, 54.059937, 0, 20.250002, 59.400002, 130 },
        { 0, 0, 0, 0, 43.467258, 0, 57.736385, 43.700001, 131 } } },
    { { 1329527921.539940, 1, 2, 2, NULL }, 0, 0,
      { { 0, 0, 0, 0, 54.253189, 0, 20.250002, 59.400002, 130 },
        { 0, 0, 0, 0, 43.717934, 0, 57.750286, 43.700001, 131 } } },
    { { 1329527921.556732, 1, 2, 2, NULL }, 0, 0,
      { { 0, 0, 0, 0, 54.500740, 0, 20.250002, 59.400002, 130 },
        { 0, 0, 0, 0, 43.863792, 0, 57.758759, 43.700001, 131 } } },
    { { 1329527921.573523, 1, 2, 2, NULL }, 0, 0,
      { { 0, 0, 0, 0, 54.737640, 0, 20.250002, 59.400002, 130 },
        { 0, 0, 0, 0, 43.825844, 0, 57.771137, 43.700001, 131 } } },
    { { 1329527921.584697, 1, 2, 2, NULL }, 0, 0,
      { { 0, 0, 0, 0, 54.906223, 0, 20.250002, 59.400002, 130 },
        { 0, 0, 0, 0, 43.654804, 0, 57.790218, 43.700001, 131 } } },
    { { 1329527921.595872, 1, 2, 2, NULL }, 0, 0,
      { { 0, 0, 0, 0, 55.001118, 0, 20.250002, 59.400002, 130 },
        { 0, 0, 0, 0, 43.542431, 0, 57.809731, 43.700001, 131 } } },
    { { 1329527921.618320, 1, 2, 2, NULL }, 0, 0,
      { { 0, 0, 0, 0, 55.039989, 0, 20.252811, 59.400002, 130 },
        { 0, 0, 0, 0, 43.585777, 0, 57.824154, 43.700001, 131 } } },
    { { 1329527921.640768, 1, 2, 2, NULL }, 0, 0,
      { { 0, 0, 0, 0, 55.045246, 0, 20.264456, 59.400002, 130 },
        { 0, 0, 0, 0, 43.715435, 0, 57.832584, 43.700001, 131 } } },
    { { 1329527921.691161, 1, 2, 2, NULL }, 0, 0,
      { { 0, 0, 0, 0, 55.068935, 0, 20.285036, 59.400002, 130 },
        { 0, 0, 0, 0, 43.845741, 0, 57.836266, 43.700001, 131 } } },
    { { 1329527921.741554, 1, 2, 2, NULL }, 0, 0,
      { { 0, 0, 0, 0, 55.195026, 0, 20.306564, 59.400002, 130 },
        { 0, 0, 0, 0, 43.941154, 0, 57.836994, 43.700001, 131 } } },
    { { 1329527921.758389, 1, 2, 2, NULL }, 0, 0,
      { { 0, 0, 0, 0, 55.430550, 0, 20.322674, 59.400002, 130 },
        { 0, 0, 0, 0, 43.962692, 0, 57.836308, 43.700001, 131 } } },
    { { 1329527921.775225, 1, 2, 2, NULL }, 0, 0,
      { { 0, 0, 0, 0, 55.681423, 0, 20.332201, 59.400002, 130 },
        { 0, 0, 0, 0, 43.846741, 0, 57.835224, 43.700001, 131 } } },
    { { 1329527921.786418, 1, 2, 2, NULL }, 0, 0,
      { { 0, 0, 0, 0, 55.803486, 0, 20.336439, 59.400002, 130 },
        { 0, 0, 0, 0, 43.604134, 0, 57.834267, 43.700001, 131 } } },
    { { 1329527921.803205, 1, 2, 2, NULL }, 0, 0,
      { { 0, 0, 0, 0, 55.738258, 0, 20.337351, 59.396629, 130 },
        { 0, 0, 0, 0, 43.340977, 0, 57.833622, 43.700001, 131 } } },
    { { 1329527921.819993, 1, 2, 2, NULL }, 0, 0,
      { { 0, 0, 0, 0, 55.647045, 0, 20.336643, 59.382656, 130 },
        { 0, 0, 0, 0, 43.140343, 0, 57.833279, 43.700001, 131 } } },
    { { 1329527921.831121, 1, 2, 2, NULL }, 0, 0,
      { { 0, 0, 0, 0, 55.670898, 0, 20.335459, 59.357960, 130 },
        { 0, 0, 0, 0, 43.019653, 0, 57.827530, 43.700001, 131 } } },
    { { 1329527921.842232, 1, 2, 2, NULL }, 0, 0,
      { { 0, 0, 0, 0, 55.769543, 0, 20.334396, 59.332127, 130 },
        { 0, 0, 0, 0, 42.964531, 0, 57.807049, 43.700001, 131 } } },
    { { 1329527921.853342, 1, 2, 2, NULL }, 0, 0,
      { { 0, 0, 0, 0, 55.872444, 0, 20.333672, 59.312794, 130 },
        { 0, 0, 0, 0, 42.951347, 0, 57.771957, 43.700001, 131 } } },
    { { 1329527921.864522, 1, 2, 2, NULL }, 0, 0,
      { { 0, 0, 0, 0, 55.949341, 0, 20.333281, 59.301361, 130 },
        { 0, 0, 0, 0, 42.959034, 0, 57.729061, 43.700001, 131 } } },
    { { 1329527921.875702, 1, 2, 2, NULL }, 0, 0,
      { { 0, 0, 0, 0, 55.994751, 0, 20.333134, 59.296276, 130 },
        { 0, 0, 0, 0, 42.973259, 0, 57.683277, 43.700001, 131 } } },
    { { 1329527921.886840, 1, 2, 2, NULL }, 0, 0,
      { { 0, 0, 0, 0, 56.014912, 0, 20.333128, 59.295181, 130 },
        { 0, 0, 0, 0, 42.918892, 0, 57.640221, 43.700001, 131 } } },
    { { 1329527921.898031, 1, 2, 2, NULL }, 0, 0,
      { { 0, 0, 0, 0, 55.951756, 0, 20.333181, 59.296028, 130 },
        { 0, 0, 0, 0, 42.715969, 0, 57.601479, 43.700001, 131 } } },
    { { 1329527921.909149, 1, 2, 2, NULL }, 0, 0,
      { { 0, 0, 0, 0, 55.736336, 0, 20.333244, 59.297451, 130 },
        { 0, 0, 0, 0, 42.304108, 0, 57.563725, 43.700001, 131 } } },
    { { 1329527921.920301, 0, 2, 2, NULL }, 0, GESTURES_BUTTON_RIGHT,
      { { 0, 0, 0, 0, 55.448730, 0, 20.333294, 59.298725, 130 },
        { 0, 0, 0, 0, 41.444939, 0, 57.525326, 43.700001, 131 } } }
  };
  ImmediateInterpreter ii(NULL);
  ii.SetHardwareProperties(hwprops);
  for (size_t i = 0; i < arraysize(records); i++) {
    // Make the hwstate point to the fingers
    HardwareState* hs = &records[i].hs;
    hs->fingers = records[i].fs;
    Gesture* gs_out = ii.SyncInterpret(hs, NULL);
    if (!gs_out || gs_out->type != kGestureTypeButtonsChange) {
      // We got no output buttons gesture. Make sure we expected that
      EXPECT_EQ(0, records[i].out_buttons_down);
      EXPECT_EQ(0, records[i].out_buttons_up);
    } else if (gs_out) {
      // We got a buttons gesture
      EXPECT_EQ(gs_out->details.buttons.down, records[i].out_buttons_down);
      EXPECT_EQ(gs_out->details.buttons.up, records[i].out_buttons_up);
    } else {
      ADD_FAILURE();  // This should be unreachable
    }
  }
}

TEST(ImmediateInterpreterTest, ChangeTimeoutTest) {
  ImmediateInterpreter ii(NULL);
  HardwareProperties hwprops = {
    0,  // left edge
    0,  // top edge
    1000,  // right edge
    1000,  // bottom edge
    500,  // pixels/TP width
    500,  // pixels/TP height
    96,  // screen DPI x
    96,  // screen DPI y
    2,  // max fingers
    5,  // max touch
    0,  // tripletap
    0,  // semi-mt
    1  // is button pad
  };

  FingerState finger_states[] = {
    // TM, Tm, WM, Wm, Press, Orientation, X, Y, TrID
    {0, 0, 0, 0, 1, 0, 30, 30, 2},
    {0, 0, 0, 0, 1, 0, 10, 10, 1},
    {0, 0, 0, 0, 1, 0, 10, 20, 1},
    {0, 0, 0, 0, 1, 0, 20, 20, 1}
  };
  HardwareState hardware_states[] = {
    // time, buttons down, finger count, finger states pointer
    // One finger moves
    { 0.10, 0, 1, 1, &finger_states[1] },
    { 0.12, 0, 1, 1, &finger_states[2] },
    { 0.16, 0, 1, 1, &finger_states[3] },
    { 0.5,  0, 0, 0, NULL },
    // One finger moves after another finger leaves
    { 1.09, 0, 2, 2, &finger_states[0] },
    { 1.10, 0, 1, 1, &finger_states[1] },
    { 1.12, 0, 1, 1, &finger_states[2] },
    { 1.16, 0, 1, 1, &finger_states[3] },
    { 1.5,  0, 0, 0, NULL },
  };

  ii.SetHardwareProperties(hwprops);

  EXPECT_EQ(NULL, ii.SyncInterpret(&hardware_states[0], NULL));

  // One finger moves, change_timeout_ is not applied.
  Gesture* gs = ii.SyncInterpret(&hardware_states[1], NULL);
  ASSERT_NE(reinterpret_cast<Gesture*>(NULL), gs);
  EXPECT_EQ(kGestureTypeMove, gs->type);
  EXPECT_EQ(0, gs->details.move.dx);
  EXPECT_EQ(10, gs->details.move.dy);
  EXPECT_EQ(0.10, gs->start_time);
  EXPECT_EQ(0.12, gs->end_time);

  // One finger moves, change_timeout_ does not block the gesture.
  gs = ii.SyncInterpret(&hardware_states[2], NULL);
  EXPECT_NE(reinterpret_cast<Gesture*>(NULL), gs);
  EXPECT_EQ(kGestureTypeMove, gs->type);
  EXPECT_EQ(10, gs->details.move.dx);
  EXPECT_EQ(0, gs->details.move.dy);
  EXPECT_EQ(0.12, gs->start_time);
  EXPECT_EQ(0.16, gs->end_time);

  // No finger.
  EXPECT_EQ(NULL, ii.SyncInterpret(&hardware_states[3], NULL));

  // Start with 2 fingers.
  EXPECT_EQ(NULL, ii.SyncInterpret(&hardware_states[4], NULL));
  // One finger leaves, finger_leave_time_ recorded.
  EXPECT_EQ(NULL, ii.SyncInterpret(&hardware_states[5], NULL));
  EXPECT_EQ(ii.finger_leave_time_, 1.10);
  // One finger moves, change_timeout_ blocks the gesture.
  EXPECT_EQ(NULL, ii.SyncInterpret(&hardware_states[6], NULL));

  // One finger moves, finger_leave_time_ + change_timeout_
  // has been passed.
  gs = ii.SyncInterpret(&hardware_states[7], NULL);
  EXPECT_NE(reinterpret_cast<Gesture*>(NULL), gs);
  EXPECT_EQ(kGestureTypeMove, gs->type);
  EXPECT_EQ(10, gs->details.move.dx);
  EXPECT_EQ(0, gs->details.move.dy);
  EXPECT_EQ(1.12, gs->start_time);
  EXPECT_EQ(1.16, gs->end_time);

  EXPECT_EQ(NULL, ii.SyncInterpret(&hardware_states[8], NULL));
}

}  // namespace gestures
