// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gestures/include/finger_metrics.h"

namespace gestures {

FingerMetrics::FingerMetrics(PropRegistry* prop_reg)
    : two_finger_close_horizontal_distance_thresh_(
          prop_reg,
          "Two Finger Horizontal Close Distance Thresh",
          50.0),
      two_finger_close_vertical_distance_thresh_(
          prop_reg,
          "Two Finger Vertical Close Distance Thresh",
          30.0) {}

bool FingerMetrics::FingersCloseEnoughToGesture(
    const FingerState& finger_a,
    const FingerState& finger_b) const {
  float horiz_axis_sq = two_finger_close_horizontal_distance_thresh_.val_ *
      two_finger_close_horizontal_distance_thresh_.val_;
  float vert_axis_sq = two_finger_close_vertical_distance_thresh_.val_ *
      two_finger_close_vertical_distance_thresh_.val_;
  float dx = finger_b.position_x - finger_a.position_x;
  float dy = finger_b.position_y - finger_a.position_y;
  // Equation of ellipse:
  //    ,.--+--..
  //  ,'   V|    `.   x^2   y^2
  // |      +------|  --- + --- < 1
  //  \        H  /   H^2   V^2
  //   `-..__,,.-'
  return vert_axis_sq * dx * dx + horiz_axis_sq * dy * dy <
      vert_axis_sq * horiz_axis_sq;
}

}  // namespace gestures
