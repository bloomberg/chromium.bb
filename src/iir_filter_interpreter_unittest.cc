// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/logging.h>
#include <gtest/gtest.h>

#include "gestures/include/gestures.h"
#include "gestures/include/iir_filter_interpreter.h"

namespace gestures {

class IirFilterInterpreterTest : public ::testing::Test {};

class IirFilterInterpreterTestInterpreter : public Interpreter {
 public:
  IirFilterInterpreterTestInterpreter() : sync_interpret_cnt_(0) {
    prev_.position_x = 0.0;
    prev_.position_y = 0.0;
  }

  virtual Gesture* SyncInterpret(HardwareState* hwstate, stime_t* timeout) {
    if (sync_interpret_cnt_) {
      EXPECT_GT(hwstate->fingers[0].position_x, prev_.position_x);
      EXPECT_GT(hwstate->fingers[0].position_y, prev_.position_y);
    }
    EXPECT_EQ(1, hwstate->finger_cnt);
    prev_ = hwstate->fingers[0];
    sync_interpret_cnt_++;
    return NULL;
  }

  virtual Gesture* HandleTimer(stime_t now, stime_t* timeout) { return NULL; }

  FingerState prev_;
  size_t sync_interpret_cnt_;
};

TEST(IirFilterInterpreterTest, SimpleTest) {
  IirFilterInterpreterTestInterpreter* base_interpreter =
      new IirFilterInterpreterTestInterpreter;
  IirFilterInterpreter interpreter(NULL, base_interpreter);

  FingerState fs[] = {
    // TM, Tm, WM, Wm, Press, Orientation, X, Y, TrID
    { 0, 0, 0, 0, 30, 0, 1, 1, 1 },
    { 0, 0, 0, 0, 30, 0, 2, 2, 1 },
    { 0, 0, 0, 0, 30, 0, 3, 3, 1 },
    { 0, 0, 0, 0, 30, 0, 5, 5, 1 }
  };
  HardwareState hs[] = {
    { 0.000, 0, 1, 1, &fs[0] },
    { 0.010, 0, 1, 1, &fs[1] },
    { 0.020, 0, 1, 1, &fs[2] },
    { 0.030, 0, 1, 1, &fs[3] }
  };

  for (size_t i = 0; i < arraysize(hs); i++)
    interpreter.SyncInterpret(&hs[i], NULL);
  EXPECT_EQ(arraysize(hs), base_interpreter->sync_interpret_cnt_);
}

}  // namespace gestures
