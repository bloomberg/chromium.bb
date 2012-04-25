// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/logging.h>
#include <gtest/gtest.h>

#include <deque>
#include <vector>

#include "gestures/include/gestures.h"
#include "gestures/include/semi_mt_correcting_filter_interpreter.h"

using std::deque;
using std::make_pair;
using std::pair;
using std::vector;

namespace gestures {

class SemiMtCorrectingFilterInterpreterTest : public ::testing::Test {};

class SemiMtCorrectingFilterInterpreterTestInterpreter : public Interpreter {
 public:
  SemiMtCorrectingFilterInterpreterTestInterpreter()
        : sync_interpret_cnt_(0) {
  }

  virtual Gesture* SyncInterpret(HardwareState* hwstate, stime_t* timeout) {
    sync_interpret_cnt_++;

    if (!expected_finger_cnt_.empty()) {
      EXPECT_EQ(expected_finger_cnt_.front(), hwstate->finger_cnt);
      expected_finger_cnt_.pop_front();
    }

    if (!expected_touch_cnt_.empty()) {
      EXPECT_EQ(expected_touch_cnt_.front(), hwstate->touch_cnt);
      expected_touch_cnt_.pop_front();
    }

    if (!unexpected_tracking_id_.empty() && (hwstate->finger_cnt > 0)) {
      EXPECT_NE(unexpected_tracking_id_.front(),
                hwstate->fingers[0].tracking_id);
      unexpected_tracking_id_.pop_front();
    }

    if (!expected_tracking_id_.empty()) {
      vector<short>& expected = expected_tracking_id_.front();
      for (size_t i = 0; i < hwstate->finger_cnt; i++) {
        EXPECT_EQ(expected[i], hwstate->fingers[i].tracking_id)
            << "i = " << i;
      }
      expected_tracking_id_.pop_front();
    }
    return NULL;
  }

  virtual Gesture* HandleTimer(stime_t now, stime_t* timeout) { return NULL; }

  int sync_interpret_cnt_;
  deque<float> expected_pressures_;
  deque<int> expected_finger_cnt_;
  deque<int> expected_touch_cnt_;
  deque<int> unexpected_tracking_id_;
  deque<vector<short> > expected_tracking_id_;
};

TEST(SemiMtCorrectingFilterInterpreterTest, LowPressureTest) {
  SemiMtCorrectingFilterInterpreterTestInterpreter* base_interpreter =
      new SemiMtCorrectingFilterInterpreterTestInterpreter;
  SemiMtCorrectingFilterInterpreter interpreter(NULL, base_interpreter);

  FingerState fs[] = {
    // TM, Tm, WM, Wm, Press, Orientation, X, Y, TrIDm, flags
    { 0, 0, 0, 0, 40, 0, 1, 1, 5, 0 },
    { 0, 0, 0, 0, 28, 0, 2, 2, 5, 0 },
    { 0, 0, 0, 0, 20, 0, 2, 2, 5, 0 },
    { 0, 0, 0, 0, 40, 0, 3, 3, 5, 0 },
    { 0, 0, 0, 0, 20, 0, 1, 1, 5, 0 },
  };
  HardwareState hs[] = {
    { 0.000, 0, 1, 1, &fs[0] },
    { 0.010, 0, 1, 1, &fs[1] },
    { 0.020, 0, 1, 1, &fs[2] },
    { 0.030, 0, 1, 1, &fs[3] },
    { 0.040, 0, 1, 1, &fs[4] },
  };

  HardwareProperties hwprops = {
    0, 0, 100, 60,  // left, top, right, bottom
    1.0, 1.0, 25.4, 25.4,  // x res, y res, x DPI, y DPI
    2, 3, 0, 0, 0  // max_fingers, max_touch, t5r2, semi_mt,
  };

  hwprops.support_semi_mt = true;
  interpreter.SetHardwareProperties(hwprops);

  base_interpreter->expected_finger_cnt_.push_back(1);
  interpreter.SyncInterpret(&hs[0], NULL);
  int current_tracking_id = fs[0].tracking_id;

  base_interpreter->expected_finger_cnt_.push_back(1);
  interpreter.SyncInterpret(&hs[1], NULL);

  base_interpreter->expected_finger_cnt_.push_back(0);
  interpreter.SyncInterpret(&hs[2], NULL);

  base_interpreter->expected_finger_cnt_.push_back(1);
  base_interpreter->unexpected_tracking_id_.push_back(current_tracking_id);
  interpreter.SyncInterpret(&hs[3], NULL);

  base_interpreter->expected_finger_cnt_.push_back(0);
  interpreter.SyncInterpret(&hs[4], NULL);
}

TEST(SemiMtCorrectingFilterInterpreterTest, TrackingIdMappingTest) {
  SemiMtCorrectingFilterInterpreterTestInterpreter* base_interpreter =
      new SemiMtCorrectingFilterInterpreterTestInterpreter;
  SemiMtCorrectingFilterInterpreter interpreter(NULL, base_interpreter);

  FingerState fs[] = {
    // TM, Tm, WM, Wm, Press, Orientation, X, Y, TrID, flags
    { 0, 0, 0, 0, 40, 0, 3, 3, 10, 0 },
    { 0, 0, 0, 0, 40, 0, 3, 3, 10, 0 },

    { 0, 0, 0, 0, 40, 0, 3, 3, 16, 0 },
    { 0, 0, 0, 0, 40, 0, 5, 5, 17, 0 },

    { 0, 0, 0, 0, 40, 0, 3, 3, 16, 0 },
    { 0, 0, 0, 0, 40, 0, 5, 5, 17, 0 },
  };
  HardwareState hs[] = {
    { 0.000, 0, 0, 0, &fs[0] },
    { 0.010, 0, 1, 1, &fs[0] },
    { 0.020, 0, 1, 1, &fs[1] },
    { 0.030, 0, 0, 0, &fs[1] },
    { 0.040, 0, 2, 2, &fs[2] },
    { 0.050, 0, 2, 2, &fs[4] },
  };

  HardwareProperties hwprops = {
    0, 0, 100, 60,  // left, top, right, bottom
    1.0, 1.0, 25.4, 25.4,  // x res, y res, x DPI, y DPI
    2, 3, 0, 0, 0  // max_fingers, max_touch, t5r2, semi_mt,
  };

  hwprops.support_semi_mt = true;
  interpreter.SetHardwareProperties(hwprops);

  interpreter.SyncInterpret(&hs[0], NULL);
  interpreter.SyncInterpret(&hs[1], NULL);

  vector<short>  result;
  result.push_back(hs[1].fingers[0].tracking_id);
  base_interpreter->expected_tracking_id_.push_back(result);
  short original_id = hs[2].fingers[0].tracking_id;
  base_interpreter->unexpected_tracking_id_.push_back(original_id);
  interpreter.SyncInterpret(&hs[2], NULL);

  interpreter.SyncInterpret(&hs[3], NULL);
  interpreter.SyncInterpret(&hs[4], NULL);
  result.clear();
  result.push_back(hs[4].fingers[0].tracking_id);
  result.push_back(hs[4].fingers[1].tracking_id);
  base_interpreter->expected_tracking_id_.push_back(result);
  interpreter.SyncInterpret(&hs[5], NULL);
}
}  // namespace gestures
