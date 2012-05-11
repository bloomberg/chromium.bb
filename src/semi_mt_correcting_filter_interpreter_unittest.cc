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

    if (!expected_coordinates_.empty()) {
      vector<FingerPosition>& expected = expected_coordinates_.front();
      for (size_t i = 0; i < hwstate->finger_cnt; i++) {
        EXPECT_FLOAT_EQ(expected[i].x, hwstate->fingers[i].position_x)
            << "i = " << i;
        EXPECT_FLOAT_EQ(expected[i].y, hwstate->fingers[i].position_y)
            << "i = " << i;
      }
      expected_coordinates_.pop_front();
    }
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
  deque<vector<FingerPosition> > expected_coordinates_;
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
  interpreter.interpreter_enabled_.val_ = true;

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
  interpreter.interpreter_enabled_.val_ = true;

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

TEST(SemiMtCorrectingFilterInterpreterTest, CorrectFingerPositionTest) {
  SemiMtCorrectingFilterInterpreterTestInterpreter* base_interpreter =
      new SemiMtCorrectingFilterInterpreterTestInterpreter;
  SemiMtCorrectingFilterInterpreter interpreter(NULL, base_interpreter);
  FingerState fs[] = {
    // TM, Tm, WM, Wm, Press, Orientation, X, Y, TrID
    { 0, 0, 0, 0, 60, 0, 4000, 3300, 5, 0 },
    { 0, 0, 0, 0, 60, 0, 2900, 3300, 5, 0 },
    { 0, 0, 0, 0, 60, 0, 4000, 2700, 6, 0 },
    { 0, 0, 0, 0, 60, 0, 2950, 3200, 5, 0 },
    { 0, 0, 0, 0, 60, 0, 4050, 2750, 6, 0 },
  };
  HardwareState hs[] = {
    { 0.000, 0, 1, 1, &fs[0] },
    { 0.010, 0, 2, 2, &fs[1] },
    { 0.010, 0, 2, 2, &fs[3] },
  };

  HardwareProperties hwprops = {
    1400, 1400, 5600, 4500,  // left, top, right, bottom
    1.0, 1.0, 25.4, 25.4,  // x res, y res, x DPI, y DPI
    2, 3, 0, 0, 0  // max_fingers, max_touch, t5r2, semi_mt,
  };

  hwprops.support_semi_mt = true;
  interpreter.SetHardwareProperties(hwprops);
  interpreter.interpreter_enabled_.val_ = true;

  interpreter.SyncInterpret(&hs[0], NULL);

  // Test if both finger positions are corrected.
  vector<FingerPosition>  result;
  FingerPosition first_finger_position1 = { 4000, 3300 };
  result.push_back(first_finger_position1);  // first finger
  FingerPosition second_finger_position1 = { 2900, 2700 };
  result.push_back(second_finger_position1);  // second finger
  base_interpreter->expected_coordinates_.push_back(result);
  interpreter.SyncInterpret(&hs[1], NULL);

  result.clear();
  FingerPosition first_finger_position2 = { 4050, 3200 };
  result.push_back(first_finger_position2);  // first finger
  FingerPosition second_finger_position2 = { 2950, 2750 };
  result.push_back(second_finger_position2);  // second finger
  base_interpreter->expected_coordinates_.push_back(result);
  interpreter.SyncInterpret(&hs[2], NULL);
}

TEST(SemiMtCorrectingFilterInterpreterTest, FingerCrossOverTest) {
  SemiMtCorrectingFilterInterpreterTestInterpreter* base_interpreter =
      new SemiMtCorrectingFilterInterpreterTestInterpreter;
  SemiMtCorrectingFilterInterpreter interpreter(NULL, base_interpreter);
  FingerState fs[] = {
    // TM, Tm, WM, Wm, Press, Orientation, X, Y, TrID, flags
    { 0, 0, 0, 0, 60, 0, 2969, 3088, 1481, 0},

    // Test if we reflect the pattern change for one-finger vertical
    // scroll, the starting finger pattern is left-bottom-right-top.
    // index 1
    { 0, 0, 0, 0, 60, 0, 2969, 3088, 1481, 0},
    { 0, 0, 0, 0, 60, 0, 4121, 1938, 1482, 0},

    // index 3
    { 0, 0, 0, 0, 60, 0, 2969, 2978, 1481, 0},
    { 0, 0, 0, 0, 60, 0, 4121, 2421, 1482, 0},

    // index 5
    { 0, 0, 0, 0, 60, 0, 2969, 3038, 1481, 0},
    { 0, 0, 0, 0, 60, 0, 4121, 3001, 1482, 0},

    // index 7
    { 0, 0, 0, 0, 60, 0, 2969, 3056, 1481, 0},
    { 0, 0, 0, 0, 60, 0, 4120, 3043, 1482, 0},

    // index 9
    { 0, 0, 0, 0, 60, 0, 2969, 3082, 1481, 0},
    { 0, 0, 0, 0, 60, 0, 4118, 3076, 1482, 0},

    // finger with tid 1481 crosses the finger 1482 vertically
    // we should see the position_y swapped in the results, i.e.
    // finger pattern is left-top-right-bottom.
    // index 11
    { 0, 0, 0, 0, 60, 0, 2969, 3117, 1481, 0},
    { 0, 0, 0, 0, 60, 0, 4118, 3096, 1482, 0},

    // index 13
    { 0, 0, 0, 0, 60, 0, 2969, 3153, 1481, 0},
    { 0, 0, 0, 0, 60, 0, 4118, 3114, 1482, 0},

    // index 15
    { 0, 0, 0, 0, 60, 0, 2969, 3198, 1481, 0},
    { 0, 0, 0, 0, 60, 0, 4118, 3130, 1482, 0},
  };
  HardwareState hs[] = {
    { 0.000, 0, 1, 1, &fs[0] },
    { 0.000, 0, 2, 2, &fs[1] },
    { 0.010, 0, 2, 2, &fs[3] },
    { 0.020, 0, 2, 2, &fs[5] },
    { 0.030, 0, 2, 2, &fs[7] },
    { 0.040, 0, 2, 2, &fs[9] },
    { 0.050, 0, 2, 2, &fs[11] },  // the finger pattern will be swapped
    { 0.060, 0, 2, 2, &fs[13] },  // to left-top-right-bottom and should
    { 0.060, 0, 2, 2, &fs[15] },  // applied for the following reports
  };

  HardwareProperties hwprops = {
    1400, 1400, 5600, 4500,  // left, top, right, bottom
    1.0, 1.0, 25.4, 25.4,  // x res, y res, x DPI, y DPI
    2, 3, 0, 0, 0  // max_fingers, max_touch, t5r2, semi_mt,
  };

  size_t hwstate_index_finger_crossed = 6;

  hwprops.support_semi_mt = true;
  interpreter.SetHardwareProperties(hwprops);
  interpreter.interpreter_enabled_.val_ = true;

  for (size_t i = 0; i < hwstate_index_finger_crossed; i++)
    interpreter.SyncInterpret(&hs[i], NULL);

  // Test if both finger positions are corrected with the new pattern
  // by examining the swapped position_y values.
  vector<FingerPosition>  result;
  FingerPosition first_finger_position1 = { 2969, 3096 };
  result.push_back(first_finger_position1);  // first finger
  FingerPosition second_finger_position1 = { 4118, 3117 };
  result.push_back(second_finger_position1);  // second finger
  base_interpreter->expected_coordinates_.push_back(result);
  interpreter.SyncInterpret(&hs[hwstate_index_finger_crossed], NULL);

  result.clear();
  FingerPosition first_finger_position2 = { 2969, 3114 };
  result.push_back(first_finger_position2);  // first finger
  FingerPosition second_finger_position2 = { 4118, 3153 };
  result.push_back(second_finger_position2);  // second finger
  base_interpreter->expected_coordinates_.push_back(result);
  interpreter.SyncInterpret(&hs[hwstate_index_finger_crossed + 1], NULL);

  result.clear();
  FingerPosition first_finger_position3 = { 2969, 3130 };
  result.push_back(first_finger_position3);  // first finger
  FingerPosition second_finger_position3 =  { 4118, 3198 };
  result.push_back(second_finger_position3);  // second finger
  base_interpreter->expected_coordinates_.push_back(result);
  interpreter.SyncInterpret(&hs[hwstate_index_finger_crossed + 2], NULL);
}

TEST(SemiMtCorrectingFilterInterpreterTest, ClipNonLinearAreaTest) {
  SemiMtCorrectingFilterInterpreterTestInterpreter* base_interpreter =
      new SemiMtCorrectingFilterInterpreterTestInterpreter;
  SemiMtCorrectingFilterInterpreter interpreter(NULL, base_interpreter);
  FingerState fs[] = {
    // TM, Tm, WM, Wm, Press, Orientation, X, Y, TrID
    { 0, 0, 0, 0, 60, 0, 1240, 3088, 1481, 0},
    { 0, 0, 0, 0, 60, 0, 5570, 3088, 1481, 0},
    { 0, 0, 0, 0, 60, 0, 4118, 1210, 1481, 0},
    { 0, 0, 0, 0, 60, 0, 4118, 4580, 1481, 0},
  };
  HardwareState hs[] = {
    { 0.00, 0, 1, 1, &fs[0] },
    { 0.02, 0, 1, 1, &fs[1] },
    { 0.04, 0, 1, 1, &fs[2] },
    { 0.06, 0, 1, 1, &fs[3] },
  };

  HardwareProperties hwprops = {
    1217, 5733, 1061, 4798,  // left, top, right, bottom
    1.0, 1.0, 133, 133,  // x res, y res, x DPI, y DPI
    2, 3, 0, 1, 1  // max_fingers, max_touch, t5r2, semi_mt, is_button_pad
  };

  interpreter.SetHardwareProperties(hwprops);
  interpreter.interpreter_enabled_.val_ = true;

  float non_linear_left = interpreter.non_linear_left_.val_;
  float non_linear_right = interpreter.non_linear_right_.val_;
  float non_linear_top = interpreter.non_linear_top_.val_;
  float non_linear_bottom = interpreter.non_linear_bottom_.val_;

  // Test if finger positions are corrected when a finger is located
  // in the non-linear clipping area.
  vector<FingerPosition> result;
  FingerPosition finger_position1 = { non_linear_left, 3088 };
  result.push_back(finger_position1);
  base_interpreter->expected_coordinates_.push_back(result);
  interpreter.SyncInterpret(&hs[0], NULL);

  result.clear();
  FingerPosition finger_position2 = { non_linear_right, 3088 };
  result.push_back(finger_position2);
  base_interpreter->expected_coordinates_.push_back(result);
  interpreter.SyncInterpret(&hs[1], NULL);

  result.clear();
  FingerPosition finger_position3 = { 4118, non_linear_top };
  result.push_back(finger_position3);
  base_interpreter->expected_coordinates_.push_back(result);
  interpreter.SyncInterpret(&hs[2], NULL);

  result.clear();
  FingerPosition finger_position4 = { 4118, non_linear_bottom };
  result.push_back(finger_position4);
  base_interpreter->expected_coordinates_.push_back(result);
  interpreter.SyncInterpret(&hs[3], NULL);
}

TEST(SemiMtCorrectingFilterInterpreterTest, MovingFingerTest) {
  SemiMtCorrectingFilterInterpreterTestInterpreter* base_interpreter =
      new SemiMtCorrectingFilterInterpreterTestInterpreter;
  SemiMtCorrectingFilterInterpreter interpreter(NULL, base_interpreter);

  // Test one, second finger arrives below the first finger, then move the first
  // finger. Expect moving finger will be the first finger(Y=1942) as the
  // second finger's starting Y(Y=4370) is higher.
  FingerState fs[] = {

    // TM, Tm, WM, Wm, Press, Orientation, X, Y, TrID, flags
    { 0, 0, 0, 0, 80, 0, 3424, 1942, 48, 0},

    // index 1
    { 0, 0, 0, 0, 80, 0, 3426, 4370, 48, 0},
    { 0, 0, 0, 0, 80, 0, 3598, 1938, 49, 0},

    // index 3
    { 0, 0, 0, 0, 80, 0, 3427, 4406, 48, 0},
    { 0, 0, 0, 0, 80, 0, 3502, 1934, 49, 0},

    // index 5
    { 0, 0, 0, 0, 80, 0, 3458, 4449, 48, 0},
    { 0, 0, 0, 0, 80, 0, 3516, 1920, 49, 0},

    // index 7
    { 0, 0, 0, 0, 80, 0, 3480, 4449, 48, 0},
    { 0, 0, 0, 0, 80, 0, 3521, 1920, 49, 0},

    // index 9
    { 0, 0, 0, 0, 80, 0, 3486, 4449, 48, 0},
    { 0, 0, 0, 0, 80, 0, 3527, 1918, 49, 0},

    // index 11
    { 0, 0, 0, 0, 80, 0, 3492, 4449, 48, 0},
    { 0, 0, 0, 0, 80, 0, 3532, 1918, 49, 0},

    // index 13
    { 0, 0, 0, 0, 80, 0, 3510, 4449, 48, 0},
    { 0, 0, 0, 0, 80, 0, 3539, 1918, 49, 0},

    // index 15
    { 0, 0, 0, 0, 80, 0, 3516, 4449, 48, 0},
    { 0, 0, 0, 0, 80, 0, 3546, 1918, 49, 0},

    // index 17
    { 0, 0, 0, 0, 80, 0, 3539, 4449, 48, 0},
    { 0, 0, 0, 0, 80, 0, 3564, 1918, 49, 0},

    // index 19
    { 0, 0, 0, 0, 80, 0, 3546, 4449, 48, 0},
    { 0, 0, 0, 0, 80, 0, 3568, 1918, 49, 0},
  };

  // Test two, have a resting thumb in the center of the touchpad, then the
  // index finger tries to cross the thumb horizontally. Expect the moving
  // finger will be the index finger as its starting position (Y=1873) is lower
  // than the first finger (Y=3325).
  FingerState fs2[] = {
    { 0, 0, 0, 0, 108, 0, 3202, 3325, 99, 0},

    // index 1
    { 0, 0, 0, 0, 108, 0, 3198, 3325, 99, 0},
    { 0, 0, 0, 0, 108, 0, 2424, 1873, 100, 0},

    // index 3
    { 0, 0, 0, 0, 108, 0, 3198, 3325, 99, 0},
    { 0, 0, 0, 0, 108, 0, 2471, 1873, 100, 0},

    // index 5
    { 0, 0, 0, 0, 108, 0, 3196, 3325, 99, 0},
    { 0, 0, 0, 0, 108, 0, 2500, 1867, 100, 0},

    // index 7
    { 0, 0, 0, 0, 108, 0, 3192, 3325, 99, 0},
    { 0, 0, 0, 0, 108, 0, 2546, 1850, 100, 0},

    // index 9
    { 0, 0, 0, 0, 108, 0, 3185, 3325, 99, 0},
    { 0, 0, 0, 0, 108, 0, 2594, 1844, 100, 0},

    // index 11
    { 0, 0, 0, 0, 108, 0, 3177, 3325, 99, 0},
    { 0, 0, 0, 0, 108, 0, 2654, 1822, 100, 0},

    // index 13
    { 0, 0, 0, 0, 108, 0, 3152, 3325, 99, 0},
    { 0, 0, 0, 0, 108, 0, 2707, 1820, 100, 0},

    // index 15
    { 0, 0, 0, 0, 108, 0, 3130, 3325, 99, 0},
    { 0, 0, 0, 0, 108, 0, 2749, 1818, 100, 0},

    // index 17
    { 0, 0, 0, 0, 108, 0, 3098, 3325, 99, 0},
    { 0, 0, 0, 0, 108, 0, 2780, 1801, 100, 0},

    // index 19
    { 0, 0, 0, 0, 108, 0, 3082, 3325, 99, 0},
    { 0, 0, 0, 0, 108, 0, 2804, 1797, 100, 0},
  };

  HardwareState hs[] = {
    { 0.00, 0, 1, 1, &fs[0] },
    { 0.02, 0, 2, 2, &fs[1] },
  };

  HardwareState hs2[] = {
    { 0.00, 0, 1, 1, &fs2[0] },
    { 0.02, 0, 2, 2, &fs2[1] },
  };

  HardwareProperties hwprops = {
    1217, 5733, 1061, 4798,  // left, top, right, bottom
    1.0, 1.0, 133, 133,  // x res, y res, x DPI, y DPI
    2, 3, 0, 1, 1  // max_fingers, max_touch, t5r2, semi_mt, is_button_pad
  };

  interpreter.SetHardwareProperties(hwprops);
  interpreter.interpreter_enabled_.val_ = true;

  // Test one, the moving finger should be the first finger.
  interpreter.SyncInterpret(&hs[0], NULL);
  for (size_t i = 0; i < (arraysize(fs) - 1) / 2; i++) {
    hs[1].fingers = &fs[2 * i + 1];  // update the finger array
    hs[1].timestamp += 0.02;
    interpreter.SyncInterpret(&hs[1], NULL);
    EXPECT_EQ(interpreter.moving_finger_, 0);
  }

  // Test two, the moving finger should be the second finger.
  interpreter.SyncInterpret(&hs2[0], NULL);
  for (size_t i = 0; i < (arraysize(fs2) - 1) / 2; i++) {
    hs2[1].fingers = &fs2[2 * i + 1];  // update the finger array
    hs2[1].timestamp += 0.02;
    interpreter.SyncInterpret(&hs2[1], NULL);
    EXPECT_EQ(interpreter.moving_finger_, 1);
  }
}
}  // namespace gestures
