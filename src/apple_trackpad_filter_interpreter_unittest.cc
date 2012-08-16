// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include "gestures/include/gestures.h"
#include "gestures/include/apple_trackpad_filter_interpreter.h"

namespace gestures {

class AppleTrackpadFilterInterpreterTest : public ::testing::Test {};

class AppleTrackpadFilterInterpreterTestInterpreter : public Interpreter {
 public:
  AppleTrackpadFilterInterpreterTestInterpreter() {}

  virtual Gesture* SyncInterpret(HardwareState* hwstate, stime_t* timeout) {
    fs = hwstate->fingers[0];
    return NULL;
  }

  virtual Gesture* HandleTimer(stime_t now, stime_t* timeout) { return NULL; }

  FingerState fs;
};

TEST(AppleTrackpadFilterInterpreterTest, SimpleTest) {
  AppleTrackpadFilterInterpreterTestInterpreter* base_interpreter =
      new AppleTrackpadFilterInterpreterTestInterpreter;
  AppleTrackpadFilterInterpreter interpreter(NULL, base_interpreter, NULL);

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

  interpreter.SetHardwareProperties(hwprops);

  const int kTouchMajor = 400;
  const int kTouchPressure = 0;

  FingerState fs[] = {
    {kTouchMajor, 0, 0, 0, kTouchPressure, 0, 600, 500, 1, 0},
    {kTouchMajor, 0, 0, 0, kTouchPressure, 0, 600, 500, 1, 0},
  };

  HardwareState hs[] = {
    // time, buttons, finger count, touch count, finger states pointer
    { 0, 0, 1, 1, &fs[0] },
    { 0, 0, 1, 1, &fs[1] },
  };

  FingerState* base_fs = &base_interpreter->fs;

  interpreter.SyncInterpret(&hs[0], NULL);
  EXPECT_EQ(base_fs->pressure, kTouchPressure);

  interpreter.enabled_.val_ = 1;

  interpreter.SyncInterpret(&hs[1], NULL);
  EXPECT_EQ(base_fs->pressure, kTouchMajor);
}

}  // namespace gestures
