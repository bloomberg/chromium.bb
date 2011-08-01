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
    500,  // pixels/TP width
    500,  // pixels/TP height
    96,  // screen DPI x
    96,  // screen DPI y
    2,  // max fingers
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
    { 200000, 0, 1, &finger_states[0] },
    { 210000, 0, 1, &finger_states[1] },
    { 220000, 0, 1, &finger_states[2] },
    { 230000, 0, 0, NULL },
    { 240000, 0, 0, NULL }
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

TEST(ImmediateInterpreterTest, ScrollUpTest) {
  ImmediateInterpreter ii;
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
    0,  // tripletap
    0,  // semi-mt
    1  // is button pad
  };

  FingerState finger_states[] = {
    // TM, Tm, WM, Wm, Press, Orientation, X, Y, TrID
    {0, 0, 0, 0, 1, 0, 400, 900, 1},
    {0, 0, 0, 0, 1, 0, 405, 900, 2},

    {0, 0, 0, 0, 1, 0, 400, 800, 1},
    {0, 0, 0, 0, 1, 0, 405, 800, 2},

    {0, 0, 0, 0, 1, 0, 400, 700, 1},
    {0, 0, 0, 0, 1, 0, 405, 700, 2},
  };
  HardwareState hardware_states[] = {
    // time, buttons down, finger count, finger states pointer
    { 0.200000, 0, 2, &finger_states[0] },
    { 0.250000, 0, 2, &finger_states[2] },
    { 0.300000, 0, 2, &finger_states[4] }
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

TEST(ImmediateInterpreterTest, SetHardwarePropertiesTwiceTest) {
  ImmediateInterpreter ii;
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
    // time, buttons, finger count, finger states pointer
    200000, 0, 5, &finger_states[0]
  };
  // This used to cause a crash:
  Gesture* gs = ii.SyncInterpret(&hardware_state, NULL);
  EXPECT_EQ(reinterpret_cast<Gesture*>(NULL), gs);
}

TEST(ImmediateInterpreterTest, SameFingersTest) {
  ImmediateInterpreter ii;
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
    0,  // t5r2
    0,  // semi-mt
    1  // is button pad
  };
  ii.SetHardwareProperties(hwprops);

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
    { 200000, 0, 1, &finger_states[0] },
    { 200001, 0, 1, &finger_states[1] },
    { 200001, 0, 2, &finger_states[1] },
    { 200001, 0, 2, &finger_states[2] },
  };

  ii.SetPrevState(hardware_state[0]);
  EXPECT_TRUE(ii.SameFingers(hardware_state[1]));
  EXPECT_FALSE(ii.SameFingers(hardware_state[2]));
  ii.SetPrevState(hardware_state[2]);
  EXPECT_TRUE(ii.SameFingers(hardware_state[2]));
  EXPECT_FALSE(ii.SameFingers(hardware_state[3]));
}

TEST(ImmediateInterpreterTest, PalmTest) {
  ImmediateInterpreter ii;
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
    0,  // t5r2
    0,  // semi-mt
    1  // is button pad
  };
  ii.SetHardwareProperties(hwprops);

  const int kBig = 100;  // palm pressure
  const int kSml = 1;  // small, low pressure

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
    // time, buttons, finger count, finger states pointer
    { 200000, 0, 2, &finger_states[0] },
    { 200001, 0, 2, &finger_states[2] },
    { 200002, 0, 2, &finger_states[4] },
    { 200003, 0, 2, &finger_states[6] },
    { 200004, 0, 2, &finger_states[8] },
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
        EXPECT_TRUE(SetContainsValue(ii.pointing_, 3));
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

TEST(ImmediateInterpreterTest, GetGesturingFingersTest) {
  ImmediateInterpreter ii;
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
    0,  // t5r2
    0,  // semi-mt
    1  // is button pad
  };
  ii.SetHardwareProperties(hwprops);

  FingerState finger_states[] = {
    // TM, Tm, WM, Wm, Press, Orientation, X, Y, TrID
    {0, 0, 0, 0, 1, 0, 1, 10, 91},
    {0, 0, 0, 0, 1, 0, 2, 5,  92},
    {0, 0, 0, 0, 1, 0, 2, 9,  93},
    {0, 0, 0, 0, 1, 0, 2, 1,  94}
  };
  HardwareState hardware_state[] = {
    // time, buttons, finger count, finger states pointer
    { 200000, 0, 0, NULL },
    { 200001, 0, 1, &finger_states[0] },
    { 200002, 0, 2, &finger_states[0] },
    { 200002, 0, 3, &finger_states[0] },
    { 200002, 0, 4, &finger_states[0] }
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
  ii.UpdatePalmState(hardware_state[3]);
  ids = ii.GetGesturingFingers(hardware_state[3]);
  EXPECT_EQ(3, ids.size());
  EXPECT_TRUE(ids.end() != ids.find(91));
  EXPECT_TRUE(ids.end() != ids.find(92));
  EXPECT_TRUE(ids.end() != ids.find(93));
}

namespace {
set<short, kMaxFingers> MkSet() { return set<short, kMaxFingers>(); }
set<short, kMaxFingers> MkSet(short the_id) {
  set<short, kMaxFingers> ret;
  ret.insert(the_id);
  return ret;
}
set<short, kMaxFingers> MkSet(short id1, short id2) {
  set<short, kMaxFingers> ret;
  ret.insert(id1);
  ret.insert(id2);
  return ret;
}
}  // namespace{}

TEST(ImmediateInterpreter, TapRecordTest) {
  TapRecord tr;
  EXPECT_FALSE(tr.IsTap());
  // two finger IDs:
  const short kF1 = 91;
  const short kF2 = 92;

  FingerState fs[] = {
    // TM, Tm, WM, Wm, Press, Orientation, X, Y, TrID
    {0, 0, 0, 0, 50, 0, 4, 4, kF1},
    {0, 0, 0, 0, 75, 0, 4, 9, kF2},
    {0, 0, 0, 0, 50, 0, 7, 4, kF1}
  };
  HardwareState hw[] = {
    // time, buttons, finger count, finger states pointer
    { 0.0, 0, 1, &fs[0] },
    { 0.1, 0, 2, &fs[0] },
    { 0.2, 0, 1, &fs[1] },
    { 0.3, 0, 2, &fs[0] },
    { 0.4, 0, 1, &fs[1] },
    { 0.5, 0, 1, &fs[2] }
  };

  tr.Update(hw[0], MkSet(kF1), MkSet(), MkSet());
  EXPECT_FALSE(tr.Moving(hw[0]));
  EXPECT_FALSE(tr.IsTap());
  tr.Update(hw[1], MkSet(), MkSet(), MkSet());
  EXPECT_FALSE(tr.Moving(hw[1]));
  EXPECT_FALSE(tr.IsTap());
  tr.Update(hw[2], MkSet(), MkSet(kF1), MkSet());
  EXPECT_FALSE(tr.Moving(hw[2]));
  EXPECT_TRUE(tr.IsTap());
  EXPECT_EQ(GESTURES_BUTTON_LEFT, tr.TapType());

  tr.Clear();
  EXPECT_FALSE(tr.IsTap());
  tr.Update(hw[2], MkSet(kF2), MkSet(), MkSet());
  EXPECT_FALSE(tr.Moving(hw[2]));
  EXPECT_FALSE(tr.IsTap());
  tr.Update(hw[3], MkSet(kF1), MkSet(), MkSet(kF2));
  EXPECT_FALSE(tr.Moving(hw[3]));
  EXPECT_FALSE(tr.IsTap());
  tr.Update(hw[4], MkSet(), MkSet(kF1), MkSet());
  EXPECT_FALSE(tr.Moving(hw[4]));
  EXPECT_TRUE(tr.IsTap());

  tr.Clear();
  EXPECT_FALSE(tr.IsTap());
  tr.Update(hw[0], MkSet(kF1), MkSet(), MkSet());
  tr.Update(hw[5], MkSet(), MkSet(), MkSet());
  EXPECT_TRUE(tr.Moving(hw[5]));
  EXPECT_FALSE(tr.IsTap());

  // This should log an error
  tr.Clear();
  tr.Update(hw[2], MkSet(), MkSet(kF1), MkSet());
}

}  // namespace gestures
