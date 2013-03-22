// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gestures/include/accel_filter_interpreter.h"

#include <algorithm>
#include <math.h>

#include "gestures/include/gestures.h"
#include "gestures/include/interpreter.h"
#include "gestures/include/logging.h"
#include "gestures/include/tracer.h"

namespace gestures {

// Takes ownership of |next|:
AccelFilterInterpreter::AccelFilterInterpreter(PropRegistry* prop_reg,
                                               Interpreter* next,
                                               Tracer* tracer)
    : FilterInterpreter(NULL, next, tracer, false),
      pointer_sensitivity_(prop_reg, "Pointer Sensitivity", 3),
      scroll_sensitivity_(prop_reg, "Scroll Sensitivity", 3),
      custom_point_str_(prop_reg, "Pointer Accel Curve", ""),
      custom_scroll_str_(prop_reg, "Scroll Accel Curve", ""),
      point_x_out_scale_(prop_reg, "Point X Out Scale", 1.0),
      point_y_out_scale_(prop_reg, "Point Y Out Scale", 1.0),
      scroll_x_out_scale_(prop_reg, "Scroll X Out Scale", 3.0),
      scroll_y_out_scale_(prop_reg, "Scroll Y Out Scale", 3.0),
      use_mouse_point_curves_(prop_reg, "Mouse Accel Curves", 0) {
  InitName();
  // Set up default curves.

  // Our pointing curves are the following.
  // x = input speed of movement (mm/s, always >= 0), y = output speed (mm/s)
  // 1: y = x (No acceleration)
  // 2: y = 32x/60   (x < 32), x^2/60   (x < 150), linear with same slope after
  // 3: y = 32x/37.5 (x < 32), x^2/37.5 (x < 150), linear with same slope after
  // 4: y = 32x/30   (x < 32), x^2/30   (x < 150), linear with same slope after
  // 5: y = 32x/25   (x < 32), x^2/25   (x < 150), linear with same slope after

  const float point_divisors[] = { 0.0, // unused
                                   60.0, 37.5, 30.0, 25.0 };  // used


  // i starts as 1 b/c we skip the first slot, since the default is fine for it.
  for (size_t i = 1; i < kMaxAccelCurves; ++i) {
    const float divisor = point_divisors[i];
    const float linear_until_x = 32.0;
    const float init_slope = linear_until_x / divisor;
    point_curves_[i][0] = CurveSegment(linear_until_x, 0, init_slope, 0);
    const float x_border = 150;
    point_curves_[i][1] = CurveSegment(x_border, 1 / divisor, 0, 0);
    const float slope = x_border * 2 / divisor;
    const float y_at_border = x_border * x_border / divisor;
    const float icept = y_at_border - slope * x_border;
    point_curves_[i][2] = CurveSegment(INFINITY, 0, slope, icept);
  }

  const float mouse_speed_straight_cutoff[] = { 5.0, 5.0, 5.0, 8.0, 8.0 };
  const float mouse_speed_accel[] = { 1.0, 1.4, 1.8, 2.0, 2.2 };

  for (size_t i = 0; i < kMaxAccelCurves; ++i) {
    const float kParabolaA = 1.3;
    const float kParabolaB = 0.2;
    const float cutoff_x = mouse_speed_straight_cutoff[i];
    const float cutoff_y =
        kParabolaA * cutoff_x * cutoff_x + kParabolaB * cutoff_x;
    const float line_m = 2.0 * kParabolaA * cutoff_x + kParabolaB;
    const float line_b = cutoff_y - cutoff_x * line_m;
    const float kOutMult = mouse_speed_accel[i];

    mouse_point_curves_[i][0] =
        CurveSegment(cutoff_x * 25.4, kParabolaA * kOutMult / 25.4,
                     kParabolaB * kOutMult, 0.0);
    mouse_point_curves_[i][1] = CurveSegment(INFINITY, 0.0, line_m * kOutMult,
                                             line_b * kOutMult * 25.4);
  }

  const float scroll_divisors[] = { 0.0, // unused
                                    150, 75.0, 70.0, 65.0 };  // used
  // Our scrolling curves are the following.
  // x = input speed of movement (mm/s, always >= 0), y = output speed (mm/s)
  // 1: y = x (No acceleration)
  // 2: y = 75x/150   (x < 75), x^2/150   (x < 600), linear (initial slope).
  // 3: y = 75x/75    (x < 75), x^2/75    (x < 600), linear (initial slope).
  // 4: y = 75x/70    (x < 75), x^2/70    (x < 600), linear (initial slope).
  // 5: y = 75x/65    (x < 75), x^2/65    (x < 600), linear (initial slope).
  // i starts as 1 b/c we skip the first slot, since the default is fine for it.
  for (size_t i = 1; i < kMaxAccelCurves; ++i) {
    const float divisor = scroll_divisors[i];
    const float linear_until_x = 75.0;
    const float init_slope = linear_until_x / divisor;
    scroll_curves_[i][0] = CurveSegment(linear_until_x, 0, init_slope, 0);
    const float x_border = 600;
    scroll_curves_[i][1] = CurveSegment(x_border, 1 / divisor, 0, 0);
    // For scrolling / flinging we level off the speed.
    const float slope = init_slope;
    const float y_at_border = x_border * x_border / divisor;
    const float icept = y_at_border - slope * x_border;
    scroll_curves_[i][2] = CurveSegment(INFINITY, 0, slope, icept);
  }
}

Gesture* AccelFilterInterpreter::SyncInterpretImpl(HardwareState* hwstate,
                                                   stime_t* timeout) {
  Gesture* fg = next_->SyncInterpret(hwstate, timeout);
  for (Gesture* it = fg; it; it = it->next)
    ScaleGesture(it);
  return fg;
}

Gesture* AccelFilterInterpreter::HandleTimerImpl(stime_t now,
                                                 stime_t* timeout) {
  Gesture* gs = next_->HandleTimer(now, timeout);
  for (Gesture* it = gs; it; it=it->next)
    ScaleGesture(it);
  return gs;
}

void AccelFilterInterpreter::ParseCurveString(const char* input,
                                              char* cache,
                                              CurveSegment* out_segs) {
  if (!strncmp(input, cache, strlen(input)))
    return;  // cache hit
  memset(cache, 0, kCacheStrLen);
  strncpy(cache, input, kCacheStrLen - 1);
  // input must be a space-separated list of x, y coord pairs
  float prev_x = 0.0;
  float prev_y = 0.0;
  const char* ptr = input;
  size_t i = 0;
  while (i < kMaxCustomCurveSegs) {
    float cur_x = static_cast<float>(atof(ptr));
    ptr = strchr(ptr, ' ');
    if (!ptr)
      break;
    ++ptr;
    float cur_y = static_cast<float>(atof(ptr));
    float dx = cur_x - prev_x;
    float dy = cur_y - prev_y;
    float slope = dy / dx;
    float icept = cur_y - cur_x * slope;
    out_segs[i] = CurveSegment(cur_x, 0.0, slope, icept);
    ++i;
    ptr = strchr(ptr, ' ');
    if (!ptr)
      break;
    ++ptr;
    prev_x = cur_x;
    prev_y = cur_y;
  }
  if (i == 0)
    out_segs[0] = CurveSegment(INFINITY, 0.0, 1.0, 0.0);  // Sane default
  else
    out_segs[i - 1].x_ = INFINITY;  // Extend final segment
}

void AccelFilterInterpreter::ScaleGesture(Gesture* gs) {
  CurveSegment* segs = NULL;
  float* dx = NULL;
  float* dy = NULL;
  float dt = gs->end_time - gs->start_time;
  size_t max_segs = kMaxCurveSegs;
  float x_scale = 1.0;
  float y_scale = 1.0;
  float mag = 0.0;
  // The quantities to scale:
  float* scale_out_x = NULL;
  float* scale_out_y = NULL;

  switch (gs->type) {
    case kGestureTypeMove:
    case kGestureTypeSwipe:
      if (gs->type == kGestureTypeMove) {
        scale_out_x = dx = &gs->details.move.dx;
        scale_out_y = dy = &gs->details.move.dy;
      } else {
        scale_out_x = dx = &gs->details.swipe.dx;
        scale_out_y = dy = &gs->details.swipe.dy;
      }
      if (pointer_sensitivity_.val_ >= 1 && pointer_sensitivity_.val_ <= 5) {
        if (use_mouse_point_curves_.val_)
          segs = mouse_point_curves_[pointer_sensitivity_.val_ - 1];
        else
          segs = point_curves_[pointer_sensitivity_.val_ - 1];
      } else {
        segs = custom_point_;
        ParseCurveString(custom_point_str_.val_,
                         last_parsed_custom_point_str_,
                         custom_point_);
        max_segs = kMaxCustomCurveSegs;
      }
      x_scale = point_x_out_scale_.val_;
      y_scale = point_y_out_scale_.val_;
      break;
    case kGestureTypeFling:  // fall through
    case kGestureTypeScroll:
      if (gs->type == kGestureTypeFling) {
        float vx = gs->details.fling.vx;
        float vy = gs->details.fling.vy;
        mag = sqrtf(vx * vx + vy * vy);
        scale_out_x = &gs->details.fling.vx;
        scale_out_y = &gs->details.fling.vy;
      } else {
        scale_out_x = dx = &gs->details.scroll.dx;
        scale_out_y = dy = &gs->details.scroll.dy;
      }
      if (scroll_sensitivity_.val_ >= 1 && scroll_sensitivity_.val_ <= 5) {
        segs = scroll_curves_[scroll_sensitivity_.val_ - 1];
      } else {
        segs = custom_scroll_;
        ParseCurveString(custom_scroll_str_.val_,
                         last_parsed_custom_scroll_str_,
                         custom_scroll_);
        max_segs = kMaxCustomCurveSegs;
      }
      x_scale = scroll_x_out_scale_.val_;
      y_scale = scroll_y_out_scale_.val_;
      break;
    default:  // Nothing to accelerate
      return;
  }

  if (dx != NULL && dy != NULL) {
    if (dt < 0.00001)
      return;  // Avoid division by 0
    mag = sqrtf(*dx * *dx + *dy * *dy) / dt;
  }
  if (mag < 0.00001)
    return;  // Avoid division by 0
  for (size_t i = 0; i < max_segs; ++i) {
    if (mag > segs[i].x_)
      continue;
    float ratio = segs[i].sqr_ * mag + segs[i].mul_ + segs[i].int_ / mag;
    *scale_out_x *= ratio * x_scale;
    *scale_out_y *= ratio * y_scale;

    return;
  }
  Err("Overflowed acceleration curve!");
}

}  // namespace gestures
