// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/gestures/fling_curve.h"

#include <algorithm>
#include <cmath>

#include "base/logging.h"

namespace {

const float kDefaultAlpha = -5.70762e+03f;
const float kDefaultBeta = 1.72e+02f;
const float kDefaultGamma = 3.7e+00f;

inline double GetPositionAtTime(double t) {
  return kDefaultAlpha * exp(-kDefaultGamma * t) - kDefaultBeta * t -
         kDefaultAlpha;
}

inline double GetVelocityAtTime(double t) {
  return -kDefaultAlpha * kDefaultGamma * exp(-kDefaultGamma * t) -
         kDefaultBeta;
}

inline double GetTimeAtVelocity(double v) {
  return -log((v + kDefaultBeta) / (-kDefaultAlpha * kDefaultGamma)) /
         kDefaultGamma;
}

}  // namespace

namespace ui {

FlingCurve::FlingCurve(const gfx::Vector2dF& velocity,
                       base::TimeTicks start_timestamp)
    : curve_duration_(GetTimeAtVelocity(0)),
      start_timestamp_(start_timestamp),
      time_offset_(0),
      position_offset_(0) {
  float max_start_velocity = std::max(fabs(velocity.x()), fabs(velocity.y()));
  if (max_start_velocity > GetVelocityAtTime(0))
    max_start_velocity = GetVelocityAtTime(0);
  CHECK_GT(max_start_velocity, 0);

  displacement_ratio_ = gfx::Vector2dF(velocity.x() / max_start_velocity,
                                       velocity.y() / max_start_velocity);
  time_offset_ = GetTimeAtVelocity(max_start_velocity);
  position_offset_ = GetPositionAtTime(time_offset_);
  last_timestamp_ = start_timestamp_ + base::TimeDelta::FromSecondsD(
                                           curve_duration_ - time_offset_);
}

FlingCurve::~FlingCurve() {
}

gfx::Vector2dF FlingCurve::GetScrollAmountAtTime(base::TimeTicks current) {
  if (current < start_timestamp_)
    return gfx::Vector2dF();

  float displacement = 0;
  if (current < last_timestamp_) {
    float time = time_offset_ + (current - start_timestamp_).InSecondsF();
    CHECK_LT(time, curve_duration_);
    displacement = GetPositionAtTime(time) - position_offset_;
  } else {
    displacement = GetPositionAtTime(curve_duration_) - position_offset_;
  }

  gfx::Vector2dF scroll(displacement * displacement_ratio_.x(),
                        displacement * displacement_ratio_.y());
  gfx::Vector2dF scroll_increment(scroll.x() - cumulative_scroll_.x(),
                                  scroll.y() - cumulative_scroll_.y());
  cumulative_scroll_ = scroll;
  return scroll_increment;
}

}  // namespace ui
