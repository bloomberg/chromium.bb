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
#include "gestures/include/scaling_filter_interpreter.h"

using std::deque;
using std::make_pair;
using std::pair;
using std::vector;

namespace gestures {

class ScalingFilterInterpreterTest : public ::testing::Test {};

class ScalingFilterInterpreterTestInterpreter : public Interpreter {
 public:
  ScalingFilterInterpreterTestInterpreter()
      : set_hwprops_called_(false) {}

  virtual Gesture* SyncInterpret(HardwareState* hwstate, stime_t* timeout) {
    if (!expected_coordinates_.empty()) {
      vector<pair<float, float> >& expected = expected_coordinates_.front();
      for (unsigned short i = 0; i < hwstate->finger_cnt; i++) {
        EXPECT_FLOAT_EQ(expected[i].first, hwstate->fingers[i].position_x)
            << "i = " << i;
        EXPECT_FLOAT_EQ(expected[i].second, hwstate->fingers[i].position_y)
            << "i = " << i;
      }
      expected_coordinates_.pop_front();
    }
    if (!expected_pressures_.empty() && hwstate->finger_cnt > 0) {
      EXPECT_FLOAT_EQ(expected_pressures_.front(),
                      hwstate->fingers[0].pressure);
      expected_pressures_.pop_front();
    } else {
      // Test if the low pressure event is dropped
      EXPECT_EQ(expected_finger_cnt_.front(), hwstate->finger_cnt);
      expected_finger_cnt_.pop_front();
      EXPECT_EQ(expected_touch_cnt_.front(), hwstate->touch_cnt);
      expected_touch_cnt_.pop_front();
    }
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
    EXPECT_FLOAT_EQ(expected_hwprops_.left, hw_props.left);
    EXPECT_FLOAT_EQ(expected_hwprops_.top, hw_props.top);
    EXPECT_FLOAT_EQ(expected_hwprops_.right, hw_props.right);
    EXPECT_FLOAT_EQ(expected_hwprops_.bottom, hw_props.bottom);
    EXPECT_FLOAT_EQ(expected_hwprops_.res_x, hw_props.res_x);
    EXPECT_FLOAT_EQ(expected_hwprops_.res_y, hw_props.res_y);
    EXPECT_FLOAT_EQ(expected_hwprops_.screen_x_dpi, hw_props.screen_x_dpi);
    EXPECT_FLOAT_EQ(expected_hwprops_.screen_y_dpi, hw_props.screen_y_dpi);
    EXPECT_EQ(expected_hwprops_.max_finger_cnt, hw_props.max_finger_cnt);
    EXPECT_EQ(expected_hwprops_.max_touch_cnt, hw_props.max_touch_cnt);
    EXPECT_EQ(expected_hwprops_.supports_t5r2, hw_props.supports_t5r2);
    EXPECT_EQ(expected_hwprops_.support_semi_mt, hw_props.support_semi_mt);
    EXPECT_EQ(expected_hwprops_.is_button_pad, hw_props.is_button_pad);
    set_hwprops_called_ = true;
  };

  Gesture return_value_;
  deque<Gesture> return_values_;
  deque<vector<pair<float, float> > > expected_coordinates_;
  deque<float> expected_pressures_;
  deque<int> expected_finger_cnt_;
  deque<int> expected_touch_cnt_;
  HardwareProperties expected_hwprops_;
  bool set_hwprops_called_;
};

TEST(ScalingFilterInterpreterTest, SimpleTest) {
  ScalingFilterInterpreterTestInterpreter* base_interpreter =
      new ScalingFilterInterpreterTestInterpreter;
  ScalingFilterInterpreter interpreter(NULL, base_interpreter);

  HardwareProperties initial_hwprops = {
    133, 728, 10279, 5822,  // left, top, right, bottom
    (10279.0 - 133.0) / 100.0,  // x res (pixels/mm)
    (5822.0 - 728.0) / 60,  // y res (pixels/mm)
    133, 133, 2, 5,  // scrn DPI X, Y, max fingers, max_touch,
    0, 0, 0  //t5r2, semi, button pad
  };
  HardwareProperties expected_hwprops = {
    0, 0, 100, 60,  // left, top, right, bottom
    1.0, 1.0, 25.4, 25.4, // x res, y res, x DPI, y DPI
    2, 5, 0, 0, 0  // max_fingers, max_touch, t5r2, semi_mt,
  };
  base_interpreter->expected_hwprops_ = expected_hwprops;

  interpreter.SetHardwareProperties(initial_hwprops);
  EXPECT_TRUE(base_interpreter->set_hwprops_called_);
  const float kPressureScale = 2.0;
  const float kPressureTranslate = 3.0;
  const float kPressureThreshold = 10.0;
  interpreter.pressure_scale_.val_ = kPressureScale;
  interpreter.pressure_translate_.val_ = kPressureTranslate;

  FingerState fs[] = {
    { 0, 0, 0, 0, 1, 0, 150, 4000, 1 },
    { 0, 0, 0, 0, 2, 0, 550, 2000, 1 },
    { 0, 0, 0, 0, 3, 0, 250, 3000, 1 }
  };
  HardwareState hs[] = {
    { 10000, 0, 1, 1, &fs[0] },
    { 54000, 0, 1, 1, &fs[1] },
    { 98000, 0, 1, 1, &fs[2] }
  };

  // Set up expected translated coordinates
  base_interpreter->expected_coordinates_.push_back(
      vector<pair<float, float> >(1, make_pair(
          static_cast<float>(100.0 * (150.0 - 133.0) / (10279.0 - 133.0)),
          static_cast<float>(60.0 * (4000.0 - 728.0) / (5822.0 - 728.0)))));
  base_interpreter->expected_coordinates_.push_back(
      vector<pair<float, float> >(1, make_pair(
          static_cast<float>(100.0 * (550.0 - 133.0) / (10279.0 - 133.0)),
          static_cast<float>(60.0 * (2000.0 - 728.0) / (5822.0 - 728.0)))));
  base_interpreter->expected_coordinates_.push_back(
      vector<pair<float, float> >(1, make_pair(
          static_cast<float>(100.0 * (250.0 - 133.0) / (10279.0 - 133.0)),
          static_cast<float>(60.0 * (3000.0 - 728.0) / (5822.0 - 728.0)))));

  base_interpreter->expected_pressures_.push_back(
      fs[0].pressure * kPressureScale + kPressureTranslate);
  base_interpreter->expected_pressures_.push_back(
      fs[1].pressure * kPressureScale + kPressureTranslate);
  base_interpreter->expected_pressures_.push_back(
      fs[2].pressure * kPressureScale + kPressureTranslate);


  // Set up gestures to return
  base_interpreter->return_values_.push_back(Gesture());  // Null type
  base_interpreter->return_values_.push_back(Gesture(kGestureMove,
                                                     0,  // start time
                                                     0,  // end time
                                                     -4,  // dx
                                                     2.8));  // dy
  base_interpreter->return_values_.push_back(Gesture(kGestureScroll,
                                                     0,  // start time
                                                     0,  // end time
                                                     4.1,  // dx
                                                     -10.3));  // dy
  base_interpreter->return_values_.push_back(Gesture());  // Null type

  Gesture* out = interpreter.SyncInterpret(&hs[0], NULL);
  ASSERT_EQ(reinterpret_cast<Gesture*>(NULL), out);
  out = interpreter.SyncInterpret(&hs[1], NULL);
  ASSERT_NE(reinterpret_cast<Gesture*>(NULL), out);
  EXPECT_EQ(kGestureTypeMove, out->type);
  EXPECT_FLOAT_EQ(-4.0 * 133.0 / 25.4, out->details.move.dx);
  EXPECT_FLOAT_EQ(2.8 * 133.0 / 25.4, out->details.move.dy);
  out = interpreter.SyncInterpret(&hs[2], NULL);
  ASSERT_NE(reinterpret_cast<Gesture*>(NULL), out);
  EXPECT_EQ(kGestureTypeScroll, out->type);
  EXPECT_FLOAT_EQ(4.1 * 133.0 / 25.4, out->details.scroll.dx);
  EXPECT_FLOAT_EQ(-10.3 * 133.0 / 25.4, out->details.scroll.dy);

  // Test if we will drop the low pressure event.
  FingerState fs2[] = {
    { 0, 0, 0, 0, 1, 0, 150, 4000, 2 },
    { 0, 0, 0, 0, 4, 0, 550, 2000, 2 }
  };
  HardwareState hs2[] = {
    { 110000, 0, 1, 2, &fs2[0] },
    { 154000, 0, 1, 1, &fs2[1] }
  };
  interpreter.pressure_threshold_.val_ = kPressureThreshold;
  base_interpreter->expected_finger_cnt_.push_back(0);
  base_interpreter->expected_touch_cnt_.push_back(1);
  out = interpreter.SyncInterpret(&hs2[0], NULL);

  base_interpreter->expected_pressures_.push_back(
      fs2[1].pressure * kPressureScale + kPressureTranslate);
  out = interpreter.SyncInterpret(&hs2[1], NULL);
}

}  // namespace gestures
