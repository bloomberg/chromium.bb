// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/gestures/blink/web_gesture_curve_impl.h"

#include <limits.h>

#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "third_party/WebKit/public/platform/WebFloatSize.h"
#include "third_party/WebKit/public/platform/WebGestureCurveTarget.h"
#include "ui/events/gestures/fixed_velocity_curve.h"
#include "ui/events/gestures/fling_curve.h"
#include "ui/gfx/geometry/safe_integer_conversions.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/geometry/vector2d_f.h"

#if defined(OS_ANDROID)
#include "ui/events/android/scroller.h"
#endif

using blink::WebGestureCurve;

namespace ui {
namespace {

std::unique_ptr<GestureCurve> CreateDefaultPlatformCurve(
    blink::WebGestureDevice device_source,
    const gfx::Vector2dF& initial_velocity) {
  if (device_source == blink::kWebGestureDeviceSyntheticAutoscroll) {
    return base::MakeUnique<FixedVelocityCurve>(initial_velocity,
                                                base::TimeTicks());
  }

#if defined(OS_ANDROID)
  auto scroller = base::MakeUnique<Scroller>(Scroller::Config());
  scroller->Fling(0,
                  0,
                  initial_velocity.x(),
                  initial_velocity.y(),
                  INT_MIN,
                  INT_MAX,
                  INT_MIN,
                  INT_MAX,
                  base::TimeTicks());
  return std::move(scroller);
#else
  return base::MakeUnique<FlingCurve>(initial_velocity, base::TimeTicks());
#endif
}

}  // namespace

// static
std::unique_ptr<WebGestureCurve>
WebGestureCurveImpl::CreateFromDefaultPlatformCurve(
    blink::WebGestureDevice device_source,
    const gfx::Vector2dF& initial_velocity,
    const gfx::Vector2dF& initial_offset,
    bool on_main_thread) {
  return std::unique_ptr<WebGestureCurve>(new WebGestureCurveImpl(
      CreateDefaultPlatformCurve(device_source, initial_velocity),
      initial_offset, on_main_thread ? ThreadType::MAIN : ThreadType::IMPL));
}

// static
std::unique_ptr<WebGestureCurve>
WebGestureCurveImpl::CreateFromUICurveForTesting(
    std::unique_ptr<GestureCurve> curve,
    const gfx::Vector2dF& initial_offset) {
  return std::unique_ptr<WebGestureCurve>(new WebGestureCurveImpl(
      std::move(curve), initial_offset, ThreadType::TEST));
}

WebGestureCurveImpl::WebGestureCurveImpl(std::unique_ptr<GestureCurve> curve,
                                         const gfx::Vector2dF& initial_offset,
                                         ThreadType animating_thread_type)
    : curve_(std::move(curve)),
      last_offset_(initial_offset),
      ticks_since_first_animate_(0),
      first_animate_time_(0),
      last_animate_time_(0) {}

WebGestureCurveImpl::~WebGestureCurveImpl() {}

bool WebGestureCurveImpl::Apply(double time,
                                blink::WebGestureCurveTarget* target) {
  // If the fling has yet to start, simply return and report true to prevent
  // fling termination.
  if (time <= 0)
    return true;

  if (!first_animate_time_) {
    first_animate_time_ = last_animate_time_ = time;
  } else if (time != last_animate_time_) {
    // Animation can occur multiple times a frame, but with the same timestamp.
    // Suppress recording of such redundant animate calls, avoiding artificially
    // inflated FPS computation.
    last_animate_time_ = time;
    ++ticks_since_first_animate_;
  }

  const base::TimeTicks time_ticks =
      base::TimeTicks() + base::TimeDelta::FromSecondsD(time);
  gfx::Vector2dF offset, velocity;
  bool still_active =
      curve_->ComputeScrollOffset(time_ticks, &offset, &velocity);

  gfx::Vector2dF delta = offset - last_offset_;
  last_offset_ = offset;

  // As successive timestamps can be arbitrarily close (but monotonic!), don't
  // assume that a zero delta means the curve has terminated.
  if (delta.IsZero())
    return still_active;

  // scrollBy() could delete this curve if the animation is over, so don't touch
  // any member variables after making that call.
  bool did_scroll = target->ScrollBy(blink::WebFloatSize(delta),
                                     blink::WebFloatSize(velocity));
  return did_scroll && still_active;
}

}  // namespace ui
