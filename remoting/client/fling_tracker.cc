// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/fling_tracker.h"

#include <cmath>

namespace remoting {

namespace {

// TODO(yuweih): May need to tweak these numbers to get better smoothness.

// Stop flinging if the speed drops below this.
// 1px per 16ms. i.e. 1px/frame.
// TODO(yuweih): The screen unit may not be in pixel. This needs to be
//               normalized with the DPI.
const float kMinTrackSpeed = 0.0625f;

// The minimum fling duration (ms) needed to trigger the fling animation. This
// is to prevent unintentional fling with low velocity.
const float kMinFlingTime = 500.f;

float GetExpFactor(float time_constant, float time_elapsed) {
  return std::exp(-time_elapsed / time_constant);
}

float GetDisplacement(float initial_speed_rate, float exp_factor) {
  // x = v0 * (1 - e^(-t / T))
  // This comes from a solution to the linear drag equation F=-kv
  return initial_speed_rate * (1.f - exp_factor);
}

float GetSpeed(float initial_speed_rate,
               float time_constant,
               float exp_factor) {
  // v = (1 / T) * v0 * e^(-t / T).
  // Derivative of the displacement.
  return (1.f / time_constant) * initial_speed_rate * exp_factor;
}

}  // namespace

FlingTracker::FlingTracker(float time_constant)
    : time_constant_(time_constant) {}

FlingTracker::~FlingTracker() {}

void FlingTracker::StartFling(float velocity_x, float velocity_y) {
  start_time_ = base::TimeTicks::Now();

  initial_speed_rate_ =
      std::sqrt(velocity_x * velocity_x + velocity_y * velocity_y);

  if (GetSpeed(initial_speed_rate_, time_constant_,
               GetExpFactor(time_constant_, kMinFlingTime)) < kMinTrackSpeed) {
    StopFling();
    return;
  }

  velocity_ratio_x_ = velocity_x / initial_speed_rate_;
  velocity_ratio_y_ = velocity_y / initial_speed_rate_;

  previous_position_x_ = 0;
  previous_position_y_ = 0;
}

void FlingTracker::StopFling() {
  initial_speed_rate_ = 0.f;
}

bool FlingTracker::IsFlingInProgress() const {
  return initial_speed_rate_ > 0;
}

bool FlingTracker::TrackMovement(base::TimeDelta time_elapsed,
                                 float* dx,
                                 float* dy) {
  if (!IsFlingInProgress()) {
    return false;
  }

  float time_elapsed_ms = time_elapsed.InMilliseconds();

  float exp_factor = GetExpFactor(time_constant_, time_elapsed_ms);

  float speed = GetSpeed(initial_speed_rate_, time_constant_, exp_factor);

  if (speed < kMinTrackSpeed) {
    StopFling();
    return false;
  }

  float displacement = GetDisplacement(initial_speed_rate_, exp_factor);

  float position_x = displacement * velocity_ratio_x_;
  float position_y = displacement * velocity_ratio_y_;

  *dx = position_x - previous_position_x_;
  *dy = position_y - previous_position_y_;

  previous_position_x_ = position_x;
  previous_position_y_ = position_y;

  return true;
}

}  // namespace remoting
