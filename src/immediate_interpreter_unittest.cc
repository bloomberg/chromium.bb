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

TEST(ImmediateInterpreterTest, ScrollUpTest) {
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
    {0, 0, 0, 0, 24, 0, 400, 900, 1},
    {0, 0, 0, 0, 52, 0, 405, 900, 2},

    {0, 0, 0, 0, 24, 0, 400, 800, 1},
    {0, 0, 0, 0, 52, 0, 405, 800, 2},

    {0, 0, 0, 0, 24, 0, 400, 700, 1},
    {0, 0, 0, 0, 52, 0, 405, 700, 2},
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

  const int kBig = 70;  // large pressure
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

TEST(ImmediateInterpreter, TapRecordTest) {
  TapRecord tr;
  EXPECT_FALSE(tr.TapComplete());
  // two finger IDs:
  const short kF1 = 91;
  const short kF2 = 92;
  const float kTapMoveDist = 1.0;  // mm

  FingerState fs[] = {
    // TM, Tm, WM, Wm, Press, Orientation, X, Y, TrID
    {0, 0, 0, 0, 50, 0, 4, 4, kF1},
    {0, 0, 0, 0, 75, 0, 4, 9, kF2},
    {0, 0, 0, 0, 50, 0, 7, 4, kF1}
  };
  HardwareState hw[] = {
    // time, buttons, finger count, finger states pointer
    { 0.0, 0, 1, 1, &fs[0] },
    { 0.1, 0, 2, 2, &fs[0] },
    { 0.2, 0, 1, 1, &fs[1] },
    { 0.3, 0, 2, 2, &fs[0] },
    { 0.4, 0, 1, 1, &fs[1] },
    { 0.5, 0, 1, 1, &fs[2] }
  };

  tr.Update(hw[0], MkSet(kF1), MkSet(), MkSet());
  EXPECT_FALSE(tr.Moving(hw[0], kTapMoveDist));
  EXPECT_FALSE(tr.TapComplete());
  tr.Update(hw[1], MkSet(), MkSet(), MkSet());
  EXPECT_FALSE(tr.Moving(hw[1], kTapMoveDist));
  EXPECT_FALSE(tr.TapComplete());
  tr.Update(hw[2], MkSet(), MkSet(kF1), MkSet());
  EXPECT_FALSE(tr.Moving(hw[2], kTapMoveDist));
  EXPECT_TRUE(tr.TapComplete());
  EXPECT_EQ(GESTURES_BUTTON_LEFT, tr.TapType());

  tr.Clear();
  EXPECT_FALSE(tr.TapComplete());
  tr.Update(hw[2], MkSet(kF2), MkSet(), MkSet());
  EXPECT_FALSE(tr.Moving(hw[2], kTapMoveDist));
  EXPECT_FALSE(tr.TapComplete());
  tr.Update(hw[3], MkSet(kF1), MkSet(), MkSet(kF2));
  EXPECT_FALSE(tr.Moving(hw[3], kTapMoveDist));
  EXPECT_FALSE(tr.TapComplete());
  tr.Update(hw[4], MkSet(), MkSet(kF1), MkSet());
  EXPECT_FALSE(tr.Moving(hw[4], kTapMoveDist));
  EXPECT_TRUE(tr.TapComplete());

  tr.Clear();
  EXPECT_FALSE(tr.TapComplete());
  tr.Update(hw[0], MkSet(kF1), MkSet(), MkSet());
  tr.Update(hw[5], MkSet(), MkSet(), MkSet());
  EXPECT_TRUE(tr.Moving(hw[5], kTapMoveDist));
  EXPECT_FALSE(tr.TapComplete());

  // This should log an error
  tr.Clear();
  tr.Update(hw[2], MkSet(), MkSet(kF1), MkSet());
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

    {0, 0, 0, 0, 50, 0, 4, 1, 97},
    {0, 0, 0, 0, 50, 0, 9, 1, 98},
    {0, 0, 0, 0, 50, 0, 4, 5, 97},
    {0, 0, 0, 0, 50, 0, 9, 5, 98},
    {0, 0, 0, 0, 50, 0, 4, 9, 97},
    {0, 0, 0, 0, 50, 0, 9, 9, 98},

    {0, 0, 0, 0, 80, 0, 5, 9, 70},  // thumb
  };
  HWStateGs hwsgs[] = {
    // Simple 1-finger tap
    {{ 0.00, 0, 1, 1, &fs[0] }, -1,  MkSet(91),   0,   0, kFTB, false },
    {{ 0.01, 0, 0, 0, NULL   }, -1,  MkSet(),     0,   0, kTpC, true },
    {{ 0.07, 0, 0, 0, NULL   }, .07, MkSet(),   kBL, kBL, kIdl, false },
    // 1-finger tap with click
    {{ 0.00, kBL, 1, 1, &fs[0] }, -1,  MkSet(91),   0,  0, kIdl, false },  // 3
    {{ 0.01,   0, 0, 0, NULL   }, -1,  MkSet(),     0,   0, kIdl, false },
    {{ 0.07,   0, 0, 0, NULL   }, .07, MkSet(),     0,   0, kIdl, false },
    // 1-finger swipe
    {{ 0.00, 0, 1, 1, &fs[4] }, -1,  MkSet(95),   0,   0, kFTB, false },  // 6
    {{ 0.01, 0, 1, 1, &fs[5] }, -1,  MkSet(95),   0,   0, kIdl, false },
    {{ 0.02, 0, 1, 1, &fs[6] }, -1,  MkSet(95),   0,   0, kIdl, false },
    {{ 0.03, 0, 0, 0, NULL   }, -1,  MkSet(),     0,   0, kIdl, false },
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
    {{ 0.00, 0, 1, 1, &fs[0] }, -1,  MkSet(91),   0,   0, kFTB, false },  // 40
    {{ 0.02, 0, 1, 1, &fs[0] }, -1,  MkSet(91),   0,   0, kFTB, false },
    {{ 0.04, 0, 1, 1, &fs[0] }, -1,  MkSet(91),   0,   0, kFTB, false },
    {{ 0.06, 0, 1, 1, &fs[0] }, -1,  MkSet(91),   0,   0, kIdl, false },
    {{ 0.07, 0, 0, 0, NULL   }, -1,  MkSet(),     0,   0, kIdl, false },
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
    {{ 0.00, 0, 2, 2, &fs[10] }, -1, MkSet(97, 98), 0, 0, kFTB, false },  // 53
    {{ 0.01, 0, 0, 0, NULL    }, -1,  MkSet(),       kBR, kBR, kIdl, false },
    {{ 0.07, 0, 0, 0, NULL    }, .07, MkSet(),         0,   0, kIdl, false },
    // 2-finger scroll
    {{ 0.00, 0, 2, 2, &fs[10] }, -1, MkSet(97, 98), 0, 0, kFTB, false },  // 56
    {{ 0.01, 0, 2, 2, &fs[12] }, -1,  MkSet(97, 98),   0,   0, kIdl, false },
    {{ 0.02, 0, 2, 2, &fs[14] }, -1,  MkSet(97, 98),   0,   0, kIdl, false },
    {{ 0.03, 0, 0, 0, NULL    }, -1,  MkSet(),         0,   0, kIdl, false },
    // left tap, right tap
    {{ 0.00, 0, 1, 1, &fs[0] },  -1,  MkSet(91),    0, 0, kFTB, false },  // 60
    {{ 0.01, 0, 0, 0, NULL   },  -1,  MkSet(),         0,   0, kTpC, true },
    {{ 0.02, 0, 2, 2, &fs[10] }, -1,  MkSet(97, 98), kBL,   0, kSTB, false },
    {{ 0.03, 0, 0, 0, NULL   },  -1,  MkSet(),         0, kBL, kTpC, true },
    {{ 0.09, 0, 0, 0, NULL    }, .09, MkSet(),       kBR, kBR, kIdl, false },
    // left tap, multi-frame right tap
    {{ 0.00, 0, 1, 1, &fs[0] },  -1,  MkSet(91),   0,  0, kFTB, false },  // 65
    {{ 0.01, 0, 0, 0, NULL   },  -1,  MkSet(),     0,   0, kTpC, true },
    {{ 0.02, 0, 1, 1, &fs[10] }, -1,  MkSet(97), kBL,   0, kSTB, false },
    {{ 0.03, 0, 1, 1, &fs[11] }, -1,  MkSet(98),   0,   0, kSTB, false },
    {{ 0.04, 0, 0, 0, NULL   },  -1,  MkSet(),     0, kBL, kTpC, true },
    {{ 0.10, 0, 0, 0, NULL    }, .10, MkSet(),   kBR, kBR, kIdl, false },
    // right tap, left tap
    {{ 0.00, 0, 2, 2, &fs[10] }, -1,  MkSet(97, 98), 0, 0, kFTB, false }, // 71
    {{ 0.01, 0, 0, 0, NULL   },  -1,  MkSet(),       kBR, kBR, kIdl, false },
    {{ 0.02, 0, 1, 1, &fs[0] },  -1,  MkSet(91),       0,   0, kFTB, false },
    {{ 0.03, 0, 0, 0, NULL   },  -1,  MkSet(),         0,   0, kTpC, true },
    {{ 0.09, 0, 0, 0, NULL    }, .09, MkSet(),       kBL, kBL, kIdl, false },
    // right double-tap
    {{ 0.00, 0, 2, 2, &fs[6] },  -1,  MkSet(95, 96), 0, 0, kFTB, false }, // 76
    {{ 0.01, 0, 0, 0, NULL   },  -1,  MkSet(),       kBR, kBR, kIdl, false },
    {{ 0.02, 0, 2, 2, &fs[10] }, -1,  MkSet(97, 98),   0,   0, kFTB, false },
    {{ 0.03, 0, 0, 0, NULL   },  -1,  MkSet(),       kBR, kBR, kIdl, false },
    {{ 0.09, 0, 0, 0, NULL    }, .09, MkSet(),         0,   0, kIdl, false },
    // left tap, right-drag
    {{ 0.00, 0, 1, 1, &fs[0] },  -1,  MkSet(91),     0, 0, kFTB, false }, // 81
    {{ 0.01, 0, 0, 0, NULL   },  -1,  MkSet(),         0,   0, kTpC, true },
    {{ 0.02, 0, 2, 2, &fs[10] }, -1,  MkSet(97, 98), kBL,   0, kSTB, false },
    {{ 0.03, 0, 2, 2, &fs[12] }, -1,  MkSet(97, 98),   0, kBL, kIdl, false },
    {{ 0.04, 0, 2, 2, &fs[14] }, -1,  MkSet(97, 98),   0,   0, kIdl, false },
    {{ 0.05, 0, 0, 0, NULL   },  -1,  MkSet(),         0,   0, kIdl, false },
    // left tap, right multi-frame join + drag
    {{ 0.00, 0, 1, 1, &fs[0] },  -1,  MkSet(91),     0, 0, kFTB, false }, // 87
    {{ 0.01, 0, 0, 0, NULL   },  -1,  MkSet(),         0,   0, kTpC, true },
    {{ 0.02, 0, 1, 1, &fs[10] }, -1,  MkSet(97),     kBL,   0, kSTB, false },
    {{ 0.03, 0, 1, 1, &fs[12] }, -1,  MkSet(97),       0,   0, kDrg, false },
    {{ 0.04, 0, 2, 2, &fs[14] }, -1,  MkSet(97, 98),   0, kBL, kIdl, false },
    {{ 0.05, 0, 0, 0, NULL   },  -1,  MkSet(),         0,   0, kIdl, false },
    // right tap, left-drag
    {{ 0.00, 0, 2, 2, &fs[14] }, -1, MkSet(97, 98), 0, 0, kFTB, false },  // 93
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
    {{ 0.00, 0, 1, 1, &fs[0] }, -1, MkSet(91),    0,   0, kFTB, false }, // 105
    {{ 0.01, 0, 0, 0, NULL   }, -1,  MkSet(),         0,   0, kTpC, true },
    {{ 0.02, 0, 1, 1, &fs[4] }, -1,  MkSet(95),     kBL,   0, kSTB, false },
    {{ 0.03, 0, 1, 1, &fs[5] }, -1,  MkSet(95),       0,   0, kDrg, false },
    {{ 0.04, 0, 1, 1, &fs[6] }, -1,  MkSet(95),       0,   0, kDrg, false },
    {{ 0.05, 0, 0, 0, NULL   }, -1,  MkSet(),         0,   0, kDRl, true },
    {{ 0.06, 0, 2, 2, &fs[10] }, -1, MkSet(97, 98),   0,   0, kDRt, false },
    {{ 0.07, 0, 0, 0, NULL   }, -1,  MkSet(),         0, kBL, kTpC, true },
    {{ 0.13, 0, 0, 0, NULL   }, .13, MkSet(),       kBR, kBR, kIdl, false },
  };

  // Algorithmically add a resting thumb to a copy of all above cases
  HWStateGs hwsgs_full[arraysize(hwsgs) * 2];
  std::copy(hwsgs, hwsgs + arraysize(hwsgs), hwsgs_full);
  std::copy(hwsgs, hwsgs + arraysize(hwsgs), hwsgs_full + arraysize(hwsgs));

  vector<vector<FingerState> > thumb_fs(arraysize(hwsgs));
  const FingerState& fs_thumb = fs[arraysize(fs) - 1];
  for (size_t i = 0; i < arraysize(hwsgs); ++i) {
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
      ii->tap_timeout_.val_ = 0.05;
      ii->tap_drag_timeout_.val_ = 0.05;
      ii->tap_move_dist_.val_ = 1.0;
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
        ii->tap_timeout_.val_ = 0.05;
        ii->tap_drag_timeout_.val_ = 0.05;
        ii->tap_move_dist_.val_ = 1.0;
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

}  // namespace gestures
