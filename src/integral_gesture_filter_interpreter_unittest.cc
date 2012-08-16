// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <deque>
#include <math.h>
#include <vector>
#include <utility>

#include <base/logging.h>
#include <gtest/gtest.h>

#include "gestures/include/gestures.h"
#include "gestures/include/integral_gesture_filter_interpreter.h"

using std::deque;
using std::make_pair;
using std::pair;
using std::vector;

namespace gestures {

class IntegralGestureFilterInterpreterTest : public ::testing::Test {};

class IntegralGestureFilterInterpreterTestInterpreter : public Interpreter {
 public:
  IntegralGestureFilterInterpreterTestInterpreter()
      : set_hwprops_called_(false) {}

  virtual Gesture* SyncInterpret(HardwareState* hwstate, stime_t* timeout) {
    if (return_values_.empty())
      return NULL;
    return_value_ = return_values_.front();
    return_values_.pop_front();
    if (return_value_.type == kGestureTypeNull)
      return NULL;
    return &return_value_;
  }

  virtual Gesture* HandleTimer(stime_t now, stime_t* timeout) {
    EXPECT_TRUE(false);
    return NULL;
  }

  virtual void SetHardwareProperties(const HardwareProperties& hwprops) {
    set_hwprops_called_ = true;
  }

  Gesture return_value_;
  deque<Gesture> return_values_;
  deque<vector<pair<float, float> > > expected_coordinates_;
  HardwareProperties expected_hwprops_;
  bool set_hwprops_called_;
};

TEST(IntegralGestureFilterInterpreterTestInterpreter, OverflowTest) {
  IntegralGestureFilterInterpreterTestInterpreter* base_interpreter =
      new IntegralGestureFilterInterpreterTestInterpreter;
  IntegralGestureFilterInterpreter interpreter(base_interpreter, NULL);

  // causing finger, dx, dy, fingers, buttons down, buttons mask, hwstate:
  base_interpreter->return_values_.push_back(
      Gesture(kGestureMove, 0, 0, 3.2, 1.5));
  base_interpreter->return_values_.push_back(
      Gesture(kGestureMove, 0, 0, .2, -1.5));
  base_interpreter->return_values_.push_back(
      Gesture(kGestureMove, 0, 0, .5, 0));
  base_interpreter->return_values_.push_back(
      Gesture(kGestureMove, 0, 0, .2, .9));
  base_interpreter->return_values_.push_back(
      Gesture(kGestureScroll, 0, 0, -20.9, 4.2));
  base_interpreter->return_values_.push_back(
      Gesture(kGestureScroll, 0, 0, .8, 1.7));
  base_interpreter->return_values_.push_back(
      Gesture(kGestureScroll, 0, 0, -0.8, 2.2));
  base_interpreter->return_values_.push_back(
      Gesture(kGestureScroll, 0, 0, -0.2, 0));
  base_interpreter->return_values_.push_back(
      Gesture(kGestureScroll, 0, 0, -0.2, 0));

  base_interpreter->return_values_[base_interpreter->return_values_.size() -
                                   1].details.scroll.stop_fling = 1;

  FingerState fs = { 0, 0, 0, 0, 1, 0, 0, 0, 1, 0 };
  HardwareState hs = { 10000, 0, 1, 1, &fs };

  GestureType expected_types[] = {
    kGestureTypeMove,
    kGestureTypeMove,
    kGestureTypeMove,
    kGestureTypeMove,
    kGestureTypeScroll,
    kGestureTypeScroll,
    kGestureTypeScroll,
    kGestureTypeScroll,
    kGestureTypeFling
  };
  float expected_x[] = {
    3, 0, 0, 1, -20, 0, 0, -1, 0
  };
  float expected_y[] = {
    1, -1, 0, 0, 4, 1, 3, 0, 0
  };

  ASSERT_EQ(arraysize(expected_types), arraysize(expected_x));
  ASSERT_EQ(arraysize(expected_types), arraysize(expected_y));

  for (size_t i = 0; i < arraysize(expected_x); i++) {
    Gesture* out = interpreter.SyncInterpret(&hs, NULL);
    if (out)
      EXPECT_EQ(expected_types[i], out->type) << "i = " << i;
    if (out == NULL) {
      EXPECT_FLOAT_EQ(expected_x[i], 0.0) << "i = " << i;
      EXPECT_FLOAT_EQ(expected_y[i], 0.0) << "i = " << i;
    } else if (out->type == kGestureTypeMove) {
      EXPECT_FLOAT_EQ(expected_x[i], out->details.move.dx) << "i = " << i;
      EXPECT_FLOAT_EQ(expected_y[i], out->details.move.dy) << "i = " << i;
    } else if (out->type == kGestureTypeFling) {
      EXPECT_FLOAT_EQ(GESTURES_FLING_TAP_DOWN, out->details.fling.fling_state)
          << "i = " << i;
    } else {
      EXPECT_FLOAT_EQ(expected_x[i], out->details.scroll.dx) << "i = " << i;
      EXPECT_FLOAT_EQ(expected_y[i], out->details.scroll.dy) << "i = " << i;
    }
  }
}

// This test moves the cursor 3.9 pixels, which causes an output of 3px w/ a
// stored remainder of 0.9 px. Then, all fingers are removed, which should
// reset the remainders. Then the curor is moved 0.2 pixels, which would
// result in a 1px move if the remainders weren't cleared.
TEST(IntegralGestureFilterInterpreterTest, ResetTest) {
  IntegralGestureFilterInterpreterTestInterpreter* base_interpreter =
      new IntegralGestureFilterInterpreterTestInterpreter;
  IntegralGestureFilterInterpreter interpreter(base_interpreter, NULL);

  // causing finger, dx, dy, fingers, buttons down, buttons mask, hwstate:
  base_interpreter->return_values_.push_back(
      Gesture(kGestureMove, 0, 0, 3.9, 0.0));
  base_interpreter->return_values_.push_back(
      Gesture());
  base_interpreter->return_values_.push_back(
      Gesture(kGestureMove, 0, 0, .2, 0.0));

  FingerState fs = { 0, 0, 0, 0, 1, 0, 0, 0, 1, 0 };
  HardwareState hs[] = {
    { 10000.00, 0, 1, 1, &fs },
    { 10000.01, 0, 0, 0, NULL },
    { 10000.02, 0, 1, 1, &fs },
  };

  size_t iter = 0;
  Gesture* out = interpreter.SyncInterpret(&hs[iter++], NULL);
  ASSERT_NE(reinterpret_cast<Gesture*>(NULL), out);
  EXPECT_EQ(kGestureTypeMove, out->type);
  out = interpreter.SyncInterpret(&hs[iter++], NULL);
  EXPECT_EQ(reinterpret_cast<Gesture*>(NULL), out);
  out = interpreter.SyncInterpret(&hs[iter++], NULL);
  EXPECT_EQ(reinterpret_cast<Gesture*>(NULL), out);
}

TEST(IntegralGestureFilterInterpreterTest, SetHwpropsTest) {
  HardwareProperties hwprops = {
    0, 0, 1, 1,  // left, top, right, bottom
    1,  // x res (pixels/mm)
    1,  // y res (pixels/mm)
    133, 133, 2, 5,  // scrn DPI X, Y, max fingers, max_touch
    0, 0, 0  // t5r2, semi_mt, button pad
  };
  IntegralGestureFilterInterpreterTestInterpreter* base_interpreter =
      new IntegralGestureFilterInterpreterTestInterpreter;
  IntegralGestureFilterInterpreter interpreter(base_interpreter, NULL);
  interpreter.SetHardwareProperties(hwprops);
  EXPECT_TRUE(base_interpreter->set_hwprops_called_);
}

}  // namespace gestures
