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
  IntegralGestureFilterInterpreter interpreter(base_interpreter);

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

  FingerState fs = { 0, 0, 0, 0, 1, 0, 0, 0, 1 };
  HardwareState hs = { 10000, 0, 1, &fs };

  GestureType expected_types[] = {
    kGestureTypeMove,
    kGestureTypeMove,
    kGestureTypeMove,
    kGestureTypeMove,
    kGestureTypeScroll,
    kGestureTypeScroll,
    kGestureTypeScroll,
    kGestureTypeScroll
  };
  float expected_x[] = {
    3, 0, 0, 1, -20, 0, 0, -1
  };
  float expected_y[] = {
    1, -1, 0, 0, 4, 1, 3, 0
  };

  ASSERT_EQ(arraysize(expected_types), arraysize(expected_x));
  ASSERT_EQ(arraysize(expected_types), arraysize(expected_y));

  for (size_t i = 0; i < arraysize(expected_x); i++) {
    Gesture* out = interpreter.SyncInterpret(&hs, NULL);
    ASSERT_NE(reinterpret_cast<Gesture*>(NULL), out) << "i = " << i;
    EXPECT_EQ(expected_types[i], out->type) << "i = " << i;
    if (out->type == kGestureTypeMove) {
      EXPECT_FLOAT_EQ(expected_x[i], out->details.move.dx) << "i = " << i;
      EXPECT_FLOAT_EQ(expected_y[i], out->details.move.dy) << "i = " << i;
    } else {
      EXPECT_FLOAT_EQ(expected_x[i], out->details.scroll.dx) << "i = " << i;
      EXPECT_FLOAT_EQ(expected_y[i], out->details.scroll.dy) << "i = " << i;
    }
  }
}

TEST(IntegralGestureFilterInterpreterTest, SetHwpropsTest) {
  HardwareProperties hwprops = {
    0, 0, 1, 1,  // left, top, right, bottom
    1,  // x res (pixels/mm)
    1,  // y res (pixels/mm)
    133, 133, 2, 0, 0, 0  // scrn DPI X, Y, max fingers, t5r2, semi, button pad
  };
  IntegralGestureFilterInterpreterTestInterpreter* base_interpreter =
      new IntegralGestureFilterInterpreterTestInterpreter;
  IntegralGestureFilterInterpreter interpreter(base_interpreter);
  interpreter.SetHardwareProperties(hwprops);
  EXPECT_TRUE(base_interpreter->set_hwprops_called_);
}

}  // namespace gestures
