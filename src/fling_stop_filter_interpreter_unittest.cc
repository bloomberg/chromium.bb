// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/logging.h>
#include <gtest/gtest.h>

#include "gestures/include/gestures.h"
#include "gestures/include/fling_stop_filter_interpreter.h"

namespace gestures {

class FlingStopFilterInterpreterTest : public ::testing::Test {};

namespace {

struct SimpleTestInputs {
  bool finger_added;
  stime_t now;
  GestureType gs_input;
  bool expected_fling_stop;
};

Gesture GestureOfType(GestureType type) {
  switch (type) {
    case kGestureTypeNull: return Gesture();
    case kGestureTypeMove: return Gesture(kGestureMove, 0.0, 0.0, 0.0, 1);
    case kGestureTypeScroll: return Gesture(kGestureScroll, 0.0, 0.0, 0.0, 1);
    default: ADD_FAILURE();
  }
  return Gesture();
}

}  // namespace {}

TEST(FlingStopFilterInterpreterTest, SimpleTest) {
  FlingStopFilterInterpreter interpreter(NULL, NULL);

  SimpleTestInputs inputs[] = {
    // timeout case
    { true,  0.01, kGestureTypeNull, false },
    { false, 0.02, kGestureTypeNull, false },
    { false, 0.03, kGestureTypeNull, false },
    { false, 0.10, kGestureTypeNull, true },

    // multiple fingers come down, timeout
    { true,  3.01,  kGestureTypeNull, false },
    { true,  3.02,  kGestureTypeNull, false },
    { false, 3.03,  kGestureTypeNull, false },
    { false, 3.095, kGestureTypeNull, true },

    // Scroll happens
    { true,  6.01, kGestureTypeNull, false },
    { false, 6.02, kGestureTypeNull, false },
    { false, 6.03, kGestureTypeScroll, true },

    // Move happens
    { true,  9.01, kGestureTypeNull, false },
    { false, 9.02, kGestureTypeNull, false },
    { false, 9.03, kGestureTypeMove, true }
  };

  for (size_t i = 0; i < arraysize(inputs); i++) {
    SimpleTestInputs& input = inputs[i];
    Gesture gs = GestureOfType(input.gs_input);
    interpreter.EvaluateFlingStop(input.finger_added, input.now, &gs);
    if (input.expected_fling_stop) {
      if (gs.type == kGestureTypeScroll) {
        EXPECT_NE(0, gs.details.scroll.stop_fling);
      } else {
        EXPECT_EQ(kGestureTypeFling, gs.type);
        EXPECT_FLOAT_EQ(0.0, gs.details.fling.vx);
        EXPECT_FLOAT_EQ(0.0, gs.details.fling.vy);
        EXPECT_EQ(GESTURES_FLING_TAP_DOWN, gs.details.fling.fling_state);
      }
    } else {
      if (gs.type == kGestureTypeScroll) {
        EXPECT_EQ(0, gs.details.scroll.stop_fling);
      } else {
        EXPECT_NE(kGestureTypeFling, gs.type);
      }
    }
  }
}

}  // namespace gestures
