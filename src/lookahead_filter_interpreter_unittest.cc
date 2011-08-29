// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <deque>
#include <math.h>
#include <utility>
#include <vector>

#include <base/logging.h>
#include <gtest/gtest.h>

#include "gestures/include/gestures.h"
#include "gestures/include/lookahead_filter_interpreter.h"

using std::deque;
using std::pair;
using std::vector;

namespace gestures {

class LookaheadFilterInterpreterTest : public ::testing::Test {};

class LookaheadFilterInterpreterTestInterpreter : public Interpreter {
 public:
  LookaheadFilterInterpreterTestInterpreter()
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

  virtual void SetHardwareProperties(const HardwareProperties& hw_props) {
    set_hwprops_called_ = true;
  };

  Gesture return_value_;
  deque<Gesture> return_values_;
  bool set_hwprops_called_;
};

TEST(LookaheadFilterInterpreterTest, SimpleTest) {
  LookaheadFilterInterpreterTestInterpreter* base_interpreter = NULL;
  scoped_ptr<LookaheadFilterInterpreter> interpreter;

  HardwareProperties initial_hwprops = {
    0, 0, 100, 100,  // left, top, right, bottom
    10,  // x res (pixels/mm)
    10,  // y res (pixels/mm)
    133, 133, 2, 5,  // scrn DPI X, Y, max fingers, max_touch,
    1, 0, 0  // t5r2, semi, button pad
  };

  FingerState fs[] = {
    // TM, Tm, WM, Wm, pr, orient, x, y, id
    { 0, 0, 0, 0, 1, 0, 10, 1, 1 },
    { 0, 0, 0, 0, 1, 0, 10, 2, 1 },
    { 0, 0, 0, 0, 1, 0, 10, 3, 1 }
  };
  HardwareState hs[] = {
    // Expect movement to take
    { 1.01, 0, 1, 1, &fs[0] },
    { 1.02, 0, 1, 1, &fs[1] },
    { 1.03, 0, 1, 1, &fs[2] },

    // Expect no movement
    { 1.01, 0, 1, 1, &fs[0] },
    { 1.03, 0, 1, 1, &fs[1] },
    { 1.03, 0, 0, 0, NULL },

    // Expect movement b/c it's moving really fast
    { 1.010, 0, 1, 1, &fs[0] },
    { 1.011, 0, 1, 1, &fs[1] },
    { 1.030, 0, 0, 0, NULL }
  };

  stime_t expected_timeout = 0.0;
  Gesture expected_movement;
  for (size_t i = 3; i < arraysize(hs); ++i) {
    if (i % 3 == 0) {
      base_interpreter = new LookaheadFilterInterpreterTestInterpreter;

      for (size_t j = 0; j < 2; ++j) {
        if (hs[i + j + 1].finger_cnt == 0)
          break;
        expected_movement = Gesture(kGestureMove,
                                    hs[i + j].timestamp,  // start time
                                    hs[i + j + 1].timestamp,  // end time
                                    hs[i + j + 1].fingers[0].position_x -
                                    hs[i + j].fingers[0].position_x,  // dx
                                    hs[i + j + 1].fingers[0].position_y -
                                    hs[i + j].fingers[0].position_y);  // dy
        base_interpreter->return_values_.push_back(expected_movement);
      }

      interpreter.reset(new LookaheadFilterInterpreter(base_interpreter));
      interpreter->SetHardwareProperties(initial_hwprops);
      EXPECT_TRUE(base_interpreter->set_hwprops_called_);
      expected_timeout = interpreter->delay_;
    }
    stime_t timeout = -1.0;
    Gesture* out = interpreter->SyncInterpret(&hs[i], &timeout);
    EXPECT_EQ(reinterpret_cast<Gesture*>(NULL), out);
    EXPECT_LT(fabs(expected_timeout - timeout), 0.0000001);
    if ((i % 3) != 2) {
      expected_timeout -= hs[i + 1].timestamp - hs[i].timestamp;
    } else {
      stime_t newtimeout = -1.0;
      out = interpreter->HandleTimer(hs[i].timestamp + timeout, &newtimeout);
      if (newtimeout < 0.0)
        EXPECT_DOUBLE_EQ(-1.0, newtimeout);
      if (i == 5) {
        EXPECT_EQ(reinterpret_cast<Gesture*>(NULL), out);
      } else {
        // Expect movement
        EXPECT_EQ(kGestureTypeMove, out->type);
        EXPECT_EQ(expected_movement.start_time, out->start_time);
        EXPECT_EQ(expected_movement.end_time, out->end_time);
        EXPECT_EQ(expected_movement.details.move.dx, out->details.move.dx);
        EXPECT_EQ(expected_movement.details.move.dy, out->details.move.dy);
      }
      // Run through rest of interpreter timeouts, makeing sure we get
      // reasonable timeout values
      int cnt = 0;
      stime_t now = hs[i].timestamp + timeout;
      while (newtimeout >= 0.0) {
        if (cnt++ == 10)
          break;
        timeout = newtimeout;
        newtimeout = -1.0;
        now += timeout;
        out = interpreter->HandleTimer(now, &newtimeout);
        if (newtimeout >= 0.0)
          EXPECT_LT(newtimeout, 1.0);
        else
          EXPECT_DOUBLE_EQ(-1.0, newtimeout);
      }
    }
  }
}

}  // namespace gestures
