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

      interpreter.reset(new LookaheadFilterInterpreter(NULL, base_interpreter));
      interpreter->SetHardwareProperties(initial_hwprops);
      interpreter->delay_.val_ = 0.05;
      EXPECT_TRUE(base_interpreter->set_hwprops_called_);
      expected_timeout = interpreter->delay_.val_;
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
        ASSERT_TRUE(out);
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

TEST(LookaheadFilterInterpreterTest, TimeGoesBackwardsTest) {
  LookaheadFilterInterpreterTestInterpreter* base_interpreter =
      new LookaheadFilterInterpreterTestInterpreter;
  Gesture expected_movement = Gesture(kGestureMove,
                                      0.0,  // start time
                                      0.0,  // end time
                                      1.0,  // dx
                                      1.0);  // dy
  base_interpreter->return_values_.push_back(expected_movement);
  base_interpreter->return_values_.push_back(expected_movement);
  LookaheadFilterInterpreter interpreter(NULL, base_interpreter);

  HardwareProperties initial_hwprops = {
    0, 0, 100, 100,  // left, top, right, bottom
    1,  // x res (pixels/mm)
    1,  // y res (pixels/mm)
    133, 133, 2, 5,  // scrn DPI X, Y, max fingers, max_touch,
    1, 0, 0  // t5r2, semi, button pad
  };
  interpreter.SetHardwareProperties(initial_hwprops);

  FingerState fs = {
    // TM, Tm, WM, Wm, pr, orient, x, y, id
    0, 0, 0, 0, 1, 0, 20, 20, 1
  };
  HardwareState hs[] = {
    // Initial state
    { 9.00, 0, 1, 1, &fs },
    // Time jumps backwards, then goes forwards
    { 0.01, 0, 1, 1, &fs },
    { 0.02, 0, 1, 1, &fs },
    { 0.03, 0, 1, 1, &fs },
    { 0.04, 0, 1, 1, &fs },
    { 0.05, 0, 1, 1, &fs },
    { 0.06, 0, 1, 1, &fs },
    { 0.07, 0, 1, 1, &fs },
    { 0.08, 0, 1, 1, &fs },
    { 0.09, 0, 1, 1, &fs },
    { 0.10, 0, 1, 1, &fs },
    { 0.11, 0, 1, 1, &fs },
    { 0.12, 0, 1, 1, &fs },
    { 0.13, 0, 1, 1, &fs },
    { 0.14, 0, 1, 1, &fs },
    { 0.15, 0, 1, 1, &fs },
    { 0.16, 0, 1, 1, &fs },
    { 0.17, 0, 1, 1, &fs },
    { 0.18, 0, 1, 1, &fs },
    { 0.19, 0, 1, 1, &fs },
    { 0.20, 0, 1, 1, &fs }
  };
  for (size_t i = 0; i < arraysize(hs); ++i) {
    stime_t timeout_requested = -1.0;
    Gesture* result = interpreter.SyncInterpret(&hs[i], &timeout_requested);
    if (result && result->type == kGestureTypeMove)
      return;  // Success!
  }
  ADD_FAILURE() << "Should have gotten a move gesture";
}

namespace {
struct GesturesRec {
  Gesture gs_;
  Gesture addend_;
  Gesture expected_;
};
}  // namespace {}

TEST(LookaheadFilterInterpreterTest, CombineGesturesTest) {
  Gesture null;
  Gesture move = Gesture(kGestureMove,
                         0,  // start time
                         0,  // end time
                         -4,  // dx
                         2.8);  // dy
  Gesture dbl_move = Gesture(kGestureMove,
                             0,  // start time
                             0,  // end time
                             -8,  // dx
                             5.6);  // dy
  Gesture scroll = Gesture(kGestureScroll,
                           0,  // start time
                           0,  // end time
                           -4,  // dx
                           2.8);  // dy
  Gesture dbl_scroll = Gesture(kGestureScroll,
                               0,  // start time
                               0,  // end time
                               -8,  // dx
                               5.6);  // dy
  Gesture down = Gesture(kGestureButtonsChange,
                         0,  // start time
                         0,  // end time
                         GESTURES_BUTTON_LEFT,  // down
                         0);  // up
  Gesture up = Gesture(kGestureButtonsChange,
                       0,  // start time
                       0,  // end time
                       0,  // down
                       GESTURES_BUTTON_LEFT);  // up
  Gesture click = Gesture(kGestureButtonsChange,
                          0,  // start time
                          0,  // end time
                          GESTURES_BUTTON_LEFT,  // down
                          GESTURES_BUTTON_LEFT);  // up
  Gesture rdown = Gesture(kGestureButtonsChange,
                          0,  // start time
                          0,  // end time
                          GESTURES_BUTTON_RIGHT,  // down
                          0);  // up
  Gesture rup = Gesture(kGestureButtonsChange,
                        0,  // start time
                        0,  // end time
                        0,  // down
                        GESTURES_BUTTON_RIGHT);  // up
  Gesture rclick = Gesture(kGestureButtonsChange,
                           0,  // start time
                           0,  // end time
                           GESTURES_BUTTON_RIGHT,  // down
                           GESTURES_BUTTON_RIGHT);  // up

  GesturesRec recs[] = {
    { null, null, null },
    { null, move, move },
    { null, scroll, scroll },
    { move, null, move },
    { scroll, null, scroll },
    { move, scroll, move },
    { scroll, move, scroll },
    { move, move, dbl_move },
    { scroll, scroll, dbl_scroll },
    { move, down, down },
    { scroll, up, up },
    { rup, move, rup },
    { rdown, scroll, rdown },
    { null, click, click },
    { click, null, click },
    // button only tests:
    { up, down, null },  // the special case
    { up, click, up },
    { down, up, click },
    { click, down, down },
    { click, click, click },
    // with right button:
    { rup, rdown, null },  // the special case
    { rup, rclick, rup },
    { rdown, rup, rclick },
    { rclick, rdown, rdown },
    { rclick, rclick, rclick }
  };
  for (size_t i = 0; i < arraysize(recs); ++i) {
    Gesture gs = recs[i].gs_;
    LookaheadFilterInterpreter::CombineGestures(
        &gs,
        recs[i].addend_.type == kGestureTypeNull ? NULL : &recs[i].addend_);
    EXPECT_TRUE(gs == recs[i].expected_) << "i=" << i;
  }
}

TEST(LookaheadFilterInterpreterTest, InterpolateHwStateTest) {
  // This test takes the first two HardwareStates, Interpolates them, putting
  // the output into the fourth slot. The third slot is the expected output.
  FingerState fs[] = {
    // TM, Tm, WM, Wm, pr, orient, x, y, id
    { 0.1, 0.4, 1.6, 1.2, 10, 3, 102, 102, 1 },
    { 0.2, 0.5, 1.7, 1.3, 11, 4, 4, 4, 2 },
    { 0.3, 0.6, 1.8, 1.4, 12, 5, 4444, 9999, 3 },

    { 0.5, 0.2, 2.0, 1.2, 13, 8, 200, 100, 1 },
    { 0.7, 0.4, 2.3, 1.3, 17, 7, 20, 22, 2 },
    { 1.0, 0.5, 2.4, 1.6, 10, 9, 5000, 5000, 3 },

    { 0.3, 0.3, 1.8, 1.2, 11.5, 5.5, 151, 101, 1 },
    { 0.45, 0.45, 2.0, 1.3, 14, 5.5, 12, 13, 2 },
    { 0.65, 0.55, 2.1, 1.5, 11, 7, 4722, 7499.5, 3 },

    { 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0 }
  };
  HardwareState hs[] = {
    // Expect movement to take
    { 1.011,  2, 3, 3, &fs[0] },
    { 1.022,  2, 3, 3, &fs[3] },
    { 1.0165, 2, 3, 3, &fs[6] },
    { 0, 0, 0, 0, &fs[9] },
  };

  LookaheadFilterInterpreter::Interpolate(hs[0], hs[1], &hs[3]);
  EXPECT_DOUBLE_EQ(hs[2].timestamp, hs[3].timestamp);
  EXPECT_EQ(hs[2].buttons_down, hs[3].buttons_down);
  EXPECT_EQ(hs[2].touch_cnt, hs[3].touch_cnt);
  EXPECT_EQ(hs[2].finger_cnt, hs[3].finger_cnt);
  for (size_t i = 0; i < hs[3].finger_cnt; i++)
    EXPECT_TRUE(hs[2].fingers[i] == hs[3].fingers[i]) << "i=" << i;
}

TEST(LookaheadFilterInterpreterTest, InterpolateTest) {
  LookaheadFilterInterpreterTestInterpreter* base_interpreter = NULL;
  scoped_ptr<LookaheadFilterInterpreter> interpreter;

  HardwareProperties initial_hwprops = {
    0, 0, 100, 100,  // left, top, right, bottom
    10,  // x res (pixels/mm)
    10,  // y res (pixels/mm)
    133, 133, 2, 5,  // scrn DPI X, Y, max fingers, max_touch,
    1, 0, 0  // t5r2, semi, button pad
  };

  FingerState fs = {
    // TM, Tm, WM, Wm, pr, orient, x, y, id
    0, 0, 0, 0, 1, 0, 10, 1, 1
  };
  HardwareState hs[] = {
    // Expect movement to take
    { 1.01, 0, 1, 1, &fs },
    { 1.02, 0, 1, 1, &fs },
    { 1.04, 0, 1, 1, &fs },
  };

  // Tests that we can properly decide when to interpolate two events.
  for (size_t i = 0; i < 2; ++i) {
    bool should_interpolate = i;
    base_interpreter = new LookaheadFilterInterpreterTestInterpreter;
    base_interpreter->return_values_.push_back(
        Gesture(kGestureMove,
                0,  // start time
                1,  // end time
                0,  // dx
                1));  // dy
    base_interpreter->return_values_.push_back(
        Gesture(kGestureMove,
                1,  // start time
                2,  // end time
                0,  // dx
                2));  // dy
    base_interpreter->return_values_.push_back(
        Gesture(kGestureMove,
                2,  // start time
                3,  // end time
                0,  // dx
                3));  // dy
    interpreter.reset(new LookaheadFilterInterpreter(NULL, base_interpreter));
    interpreter->SetHardwareProperties(initial_hwprops);
    interpreter->delay_.val_ = 0.05;

    stime_t timeout = -1.0;
    Gesture* out = interpreter->SyncInterpret(&hs[0], &timeout);
    EXPECT_EQ(reinterpret_cast<Gesture*>(NULL), out);
    EXPECT_GT(timeout, 0);
    const size_t next_idx = should_interpolate ? 2 : 1;
    timeout = -1.0;
    out = interpreter->SyncInterpret(&hs[next_idx], &timeout);
    EXPECT_EQ(reinterpret_cast<Gesture*>(NULL), out);
    EXPECT_GT(timeout, 0);

    // Fetch the gestures
    size_t gs_count = 0;
    stime_t now = hs[next_idx].timestamp + timeout;
    do {
      timeout = -1.0;
      out = interpreter->HandleTimer(now, &timeout);
      EXPECT_NE(reinterpret_cast<Gesture*>(NULL), out);
      gs_count++;
      now += timeout;
    } while(timeout > 0.0);
    EXPECT_EQ(should_interpolate ? 3 : 2, gs_count);
  }
}

}  // namespace gestures
