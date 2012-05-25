// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <deque>

#include <base/logging.h>
#include <gtest/gtest.h>

#include "gestures/include/gestures.h"
#include "gestures/include/click_wiggle_filter_interpreter.h"

using std::deque;

namespace gestures {

class ClickWiggleFilterInterpreterTest : public ::testing::Test {};

class ClickWiggleFilterInterpreterTestInterpreter : public Interpreter {
 public:
  ClickWiggleFilterInterpreterTestInterpreter()
      : set_hwprops_called_(false), expect_warp_(true) {}

  virtual Gesture* SyncInterpret(HardwareState* hwstate, stime_t* timeout) {
    if (hwstate->finger_cnt > 0 && expect_warp_) {
      EXPECT_EQ(1, hwstate->finger_cnt);
      EXPECT_TRUE(hwstate->fingers[0].flags & GESTURES_FINGER_WARP_X);
      EXPECT_TRUE(hwstate->fingers[0].flags & GESTURES_FINGER_WARP_Y);
    }
    return NULL;
  }

  virtual Gesture* HandleTimer(stime_t now, stime_t* timeout) {
    EXPECT_TRUE(false);
    return NULL;
  }

  virtual void SetHardwareProperties(const HardwareProperties& hw_props) {
    set_hwprops_called_ = true;
  };

  bool set_hwprops_called_;
  bool expect_warp_;
};

TEST(ClickWiggleFilterInterpreterTest, WiggleSuppressTest) {
  ClickWiggleFilterInterpreterTestInterpreter* base_interpreter =
      new ClickWiggleFilterInterpreterTestInterpreter;
  ClickWiggleFilterInterpreter interpreter(NULL, base_interpreter);
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
  EXPECT_FALSE(base_interpreter->set_hwprops_called_);
  interpreter.SetHardwareProperties(hwprops);
  EXPECT_TRUE(base_interpreter->set_hwprops_called_);

  // These values come from a recording of my finger
  FingerState finger_states[] = {
    // TM, Tm, WM, Wm, Press, Orientation, X, Y, TrID
    {0, 0, 0, 0, 38.299999, 0, 43.195655, 32.814815, 1, 0},
    {0, 0, 0, 0, 39.820442, 0, 43.129665, 32.872276, 1, 0},
    {0, 0, 0, 0, 44.924972, 0, 42.881202, 33.077861, 1, 0},
    {0, 0, 0, 0, 52.412372, 0, 42.476348, 33.405296, 1, 0},
    {0, 0, 0, 0, 59.623386, 0, 42.064849, 33.772129, 1, 0},
    {0, 0, 0, 0, 65.317642, 0, 41.741107, 34.157428, 1, 0},
    {0, 0, 0, 0, 69.175155, 0, 41.524814, 34.531333, 1, 0},
    {0, 0, 0, 0, 71.559425, 0, 41.390705, 34.840869, 1, 0},
    {0, 0, 0, 0, 73.018020, 0, 41.294445, 35.082786, 1, 0},
    {0, 0, 0, 0, 73.918144, 0, 41.210456, 35.280235, 1, 0},
    {0, 0, 0, 0, 74.453460, 0, 41.138065, 35.426036, 1, 0},
    {0, 0, 0, 0, 74.585144, 0, 41.084125, 35.506179, 1, 0},
    {0, 0, 0, 0, 74.297470, 0, 41.052356, 35.498870, 1, 0},
    {0, 0, 0, 0, 73.479888, 0, 41.064708, 35.364994, 1, 0},
    {0, 0, 0, 0, 71.686737, 0, 41.178459, 35.072589, 1, 0},
    {0, 0, 0, 0, 68.128448, 0, 41.473480, 34.566291, 1, 0},
    {0, 0, 0, 0, 62.086532, 0, 42.010086, 33.763534, 1, 0},
    {0, 0, 0, 0, 52.739898, 0, 42.745056, 32.644023, 1, 0},
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

  for (size_t i = 0; i < arraysize(hardware_state); ++i)
    // Assertions happen in the base interpreter
    interpreter.SyncInterpret(&hardware_state[i], NULL);
}

TEST(ClickWiggleFilterInterpreterTest, OneFingerClickSuppressTest) {
  ClickWiggleFilterInterpreterTestInterpreter* base_interpreter =
      new ClickWiggleFilterInterpreterTestInterpreter;
  ClickWiggleFilterInterpreter interpreter(NULL, base_interpreter);
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
  EXPECT_FALSE(base_interpreter->set_hwprops_called_);
  interpreter.SetHardwareProperties(hwprops);
  EXPECT_TRUE(base_interpreter->set_hwprops_called_);

  // These values come from a recording of my finger
  FingerState finger_states[] = {
    // TM, Tm, WM, Wm, Press, Orientation, X, Y, TrID, flags
    {0, 0, 0, 0, 38, 0, 43, 45, 1, 0},  // 0
    {0, 0, 0, 0, 37, 0, 43, 48, 1, 0},  // 1
    {0, 0, 0, 0, 38, 0, 43, 49, 1, 0},  // 2
    {0, 0, 0, 0, 38, 0, 43, 49, 1, 0},  // 3 (same as 2)
  };
  HardwareState hardware_state[] = {
    // time, buttons, finger count, touch count, finger states pointer
    { 1.0, 1, 1, 1, &finger_states[0] },  // 0
    { 1.1, 1, 1, 1, &finger_states[1] },  // 1
    { 1.11, 1, 1, 1, &finger_states[2] },  // 2
    { 1.25, 1, 1, 1, &finger_states[3] },  // 3, stable & > Timeout => no warp
  };

  interpreter.one_finger_click_wiggle_timeout_.val_ = 0.2;

  for (size_t i = 0; i < arraysize(hardware_state); ++i) {
    // Assertions happen in the base interpreter
    base_interpreter->expect_warp_ = (i != 3);  // No warp after timeout
    interpreter.SyncInterpret(&hardware_state[i], NULL);
  }
}

namespace {
struct ThumbClickTestInput {
  stime_t timestamp_;
  float x_, y_, pressure_;
  short buttons_down_;
};
}  // namespace {}

// This tests uses actual data from a log from Ryan Tabone, where he clicked
// with his thumb and the cursor moved.
TEST(ClickWiggleFilterInterpreter, ThumbClickTest) {
  ClickWiggleFilterInterpreterTestInterpreter* base_interpreter =
    new ClickWiggleFilterInterpreterTestInterpreter;
  ClickWiggleFilterInterpreter interpreter(NULL, base_interpreter);
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

  interpreter.SetHardwareProperties(hwprops);

  ThumbClickTestInput inputs[] = {
    { 1.089467, 27.83, 21.20, 11.48, 0 },
    { 1.102349, 28.08, 21.20, 17.30, 0 },
    { 1.115390, 28.83, 19.80, 19.24, 0 },
    { 1.128652, 29.91, 18.60, 19.24, 0 },
    { 1.141682, 30.50, 17.30, 19.24, 0 },
    { 1.154569, 31.33, 16.80, 19.24, 1 },
    { 1.168041, 31.91, 16.20, 21.18, 1 },
    { 1.181294, 32.58, 15.90, 09.54, 1 },
  };
  for (size_t i = 0; i < arraysize(inputs); i++) {
    const ThumbClickTestInput& input = inputs[i];
    FingerState fs = {
      0, 0, 0, 0,  // touch/width major/minor
      input.pressure_,
      0,  // orientation
      input.x_, input.y_,
      1,  // tracking id
      0  // flags
    };
    HardwareState hs = { input.timestamp_, input.buttons_down_, 1, 1, &fs };
    interpreter.SyncInterpret(&hs, NULL);
    // Assertions tested in base interpreter
  }
}

// This test makes sure we can do 1-finger movement if the clock goes
// backwards.
TEST(ClickWiggleFilterInterpreter, TimeBackwardsTest) {
  ClickWiggleFilterInterpreterTestInterpreter* base_interpreter =
    new ClickWiggleFilterInterpreterTestInterpreter;
  ClickWiggleFilterInterpreter interpreter(NULL, base_interpreter);
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

  interpreter.SetHardwareProperties(hwprops);

  const short kInitialId = 1;
  FingerState fs = { 0, 0, 0, 0, 50, 0, 20, 20, kInitialId, 0 };

  HardwareState hs[] = {
    // click
    { 9.00, 1, 1, 1, &fs },
    { 9.01, 0, 1, 1, &fs },
    // time goes backwards
    { 1.00, 0, 1, 1, &fs },
    // long time passes, shouldn't be wobbling anymore
    { 2.01, 0, 1, 1, &fs },
  };

  for (size_t i = 0; i < arraysize(hs); i++) {
    fs.flags = 0;
    if (hs[i].timestamp < hs[0].timestamp) {
      fs.tracking_id = kInitialId + 1;
    }
    if (i == arraysize(hs) - 1)
      base_interpreter->expect_warp_ = false;
    interpreter.SyncInterpret(&hs[i], NULL);
    if (i == arraysize(hs) - 1)
      EXPECT_EQ(0, fs.flags);
  }
}

}  // namespace gestures
