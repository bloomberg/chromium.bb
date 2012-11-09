// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/touch_fling_platform_gesture_curve.h"

#include <cmath>

#include "base/logging.h"
#include "base/debug/trace_event.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebFloatPoint.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebPoint.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebSize.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebGestureCurve.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebGestureCurveTarget.h"

using WebKit::WebGestureCurve;
using WebKit::WebGestureCurveTarget;
using WebKit::WebPoint;
using WebKit::WebSize;
using WebKit::WebFloatPoint;

namespace {

const char* kCurveName = "TouchFlingGestureCurve";

const int kMaxSearchIterations = 20;

// The touchscreen-specific parameters listed below are a matched set,
// and should not be changed independently of one another.
const float kDefaultAlpha = -5.70762e+03f;
const float kDefaultBeta= 1.72e+02f;
const float kDefaultGamma= 3.7e+00f;
const float kMaxCurveDuration = 1.3f;

inline double position(double t, float* p) {
  return p[0] * exp(-p[2] * t) - p[1] * t - p[0];
}

inline double velocity(double t, float* p) {
  return -p[0] * p[2] * exp(-p[2] * t) - p[1];
}

} // namespace


namespace webkit_glue {

// This curve implementation is based on the notion of a single, absolute
// curve, which starts at a large velocity and smoothly decreases to
// zero. For a given input velocity, we find where on the curve this
// velocity occurs, and start the animation at this point---denoted by
// (time_offset_, position_offset_).
//
// This has the effect of automatically determining an animation duration
// that scales with input velocity, as faster initial velocities start
// earlier on the curve and thus take longer to reach the end. No
// complicated time scaling is required.
//
// Since the starting velocity is implicitly determined by our starting
// point, we only store the relative magnitude and direction of both
// initial x- and y-velocities, and use this to scale the computed
// displacement at any point in time. This guarantees that fling
// trajectories are straight lines when viewed in x-y space. Initial
// velocities that lie outside the max velocity are constrained to start
// at zero (and thus are implicitly scaled).
//
// The curve is modelled as a 4th order polynomial, starting at t = 0,
// and ending at t = curve_duration_. Attempts to generate
// position/velocity estimates outside this range are undefined.

WebGestureCurve* TouchFlingGestureCurve::CreateForTouchPad(
    const WebFloatPoint& velocity,
    const WebSize& cumulative_scroll) {
  // The default parameters listed below are a matched set,
  // and should not be changed independently of one another.
  return Create(velocity, kDefaultAlpha, kDefaultBeta, kDefaultGamma,
                kMaxCurveDuration, cumulative_scroll);
}

WebGestureCurve* TouchFlingGestureCurve::CreateForTouchScreen(
    const WebFloatPoint& velocity,
    const WebSize& cumulative_scroll) {
  // The touchscreen-specific parameters listed below are a matched set,
  // and should not be changed independently of one another.
  return Create(velocity, kDefaultAlpha, kDefaultBeta, kDefaultGamma,
                kMaxCurveDuration, cumulative_scroll);
}

WebGestureCurve* TouchFlingGestureCurve::Create(
    const WebFloatPoint& initial_velocity,
    float p0,
    float p1,
    float p2,
    float curve_duration,
    const WebSize& cumulative_scroll) {
  return new TouchFlingGestureCurve(initial_velocity, p0, p1, p2,
                                    curve_duration, cumulative_scroll);
}

TouchFlingGestureCurve::TouchFlingGestureCurve(
    const WebFloatPoint& initial_velocity,
    float alpha,
    float beta,
    float gamma,
    float curve_duration,
    const WebSize& cumulative_scroll)
    : cumulative_scroll_(cumulative_scroll),
      curve_duration_(curve_duration) {
  DCHECK(initial_velocity != WebFloatPoint());

  coefficients_[0] = alpha;
  coefficients_[1] = beta;
  coefficients_[2] = gamma;

  float max_start_velocity = std::max(fabs(initial_velocity.x),
                                      fabs(initial_velocity.y));

  // Force max_start_velocity to lie in the range v(0) to v(curve_duration),
  // and assume that the curve parameters define a monotonically decreasing
  // velocity, or else bisection search may fail.
  if (max_start_velocity > velocity(0, coefficients_))
    max_start_velocity = velocity(0, coefficients_);

  if (max_start_velocity < velocity(curve_duration_, coefficients_))
    max_start_velocity = velocity(curve_duration_, coefficients_);

  // We keep track of relative magnitudes and directions of the
  // velocity/displacement components here.
  displacement_ratio_ = WebFloatPoint(initial_velocity.x / max_start_velocity,
                                      initial_velocity.y / max_start_velocity);

  // Use basic bisection to estimate where we should start on the curve.
  // TODO(wjmaclean): Would Newton's method be better?
  // It is probably good enough to get the start point to within 1 pixel/sec.
  const double epsilon = 1;
  double t0 = 0;
  double t1 = curve_duration;
  int num_iterations = 0;
  while (t0 < t1 && num_iterations < kMaxSearchIterations) {
    num_iterations++;
    time_offset_ = (t0 + t1) * 0.5;
    double vOffset = velocity(time_offset_, coefficients_);
    if (fabs(max_start_velocity - vOffset) < epsilon)
      break;

    if (vOffset > max_start_velocity)
      t0 = time_offset_;
    else
      t1 = time_offset_;
  }

  // Compute curve position at offset time
  position_offset_ = position(time_offset_, coefficients_);
  TRACE_EVENT_ASYNC_BEGIN1("input", "GestureAnimation", this, "curve",
      kCurveName);
}

TouchFlingGestureCurve::~TouchFlingGestureCurve() {
  TRACE_EVENT_ASYNC_END0("input", "GestureAnimation", this);
}

bool TouchFlingGestureCurve::apply(double time, WebGestureCurveTarget* target) {
  float displacement;
  if (time < 0)
    displacement = 0;
  else if (time + time_offset_ < curve_duration_) {
    displacement =
        position(time + time_offset_, coefficients_) - position_offset_;
  } else
    displacement = position(curve_duration_, coefficients_) - position_offset_;

  // Keep track of integer portion of scroll thus far, and prepare increment.
  WebPoint scroll(displacement * displacement_ratio_.x,
                  displacement * displacement_ratio_.y);
  WebPoint scroll_increment(scroll.x - cumulative_scroll_.width,
                            scroll.y - cumulative_scroll_.height);
  cumulative_scroll_ = WebSize(scroll.x, scroll.y);

  if (time + time_offset_ < curve_duration_ || scroll_increment != WebPoint()) {
    target->scrollBy(scroll_increment);
    return true;
  }

  return false;
}

} // namespace webkit_glue
