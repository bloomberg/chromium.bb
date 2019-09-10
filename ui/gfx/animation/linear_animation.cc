// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/animation/linear_animation.h"

#include <math.h>
#include <stdint.h>

#include <algorithm>

#include "base/numerics/ranges.h"
#include "ui/gfx/animation/animation_container.h"
#include "ui/gfx/animation/animation_delegate.h"

namespace {

double g_duration_scale_factor = 1.0;

}  // namespace

namespace gfx {

static base::TimeDelta CalculateInterval(int frame_rate) {
  int timer_interval = 1000000 / frame_rate;
  if (timer_interval < 10000)
    timer_interval = 10000;
  return base::TimeDelta::FromMicroseconds(timer_interval);
}

const int LinearAnimation::kDefaultFrameRate = 60;

LinearAnimation::LinearAnimation(AnimationDelegate* delegate, int frame_rate)
    : LinearAnimation({}, frame_rate, delegate) {}

LinearAnimation::LinearAnimation(base::TimeDelta duration,
                                 int frame_rate,
                                 AnimationDelegate* delegate)
    : Animation(CalculateInterval(frame_rate)), state_(0.0), in_end_(false) {
  set_delegate(delegate);
  SetDuration(duration);
}

double LinearAnimation::GetCurrentValue() const {
  // Default is linear relationship, subclass to adapt.
  return state_;
}

void LinearAnimation::SetCurrentValue(double new_value) {
  new_value = base::ClampToRange(new_value, 0.0, 1.0);
  base::TimeDelta time_delta = base::TimeDelta::FromMicroseconds(
      static_cast<int64_t>(duration_.InMicroseconds() * (new_value - state_)));
  SetStartTime(start_time() - time_delta);
  state_ = new_value;
}

void LinearAnimation::End() {
  if (!is_animating())
    return;

  // NOTE: We don't use AutoReset here as Stop may end up deleting us (by way
  // of the delegate).
  in_end_ = true;
  Stop();
}

void LinearAnimation::SetDuration(base::TimeDelta duration) {
  duration_ = duration * g_duration_scale_factor;
  if (duration_ < timer_interval())
    duration_ = timer_interval();
  if (is_animating())
    SetStartTime(container()->last_tick_time());
}

// static
void LinearAnimation::SetDurationScale(const double scale_factor) {
  if (scale_factor >= 0.0)
    g_duration_scale_factor = scale_factor;
}

void LinearAnimation::Step(base::TimeTicks time_now) {
  base::TimeDelta elapsed_time = time_now - start_time();
  state_ = static_cast<double>(elapsed_time.InMicroseconds()) /
           static_cast<double>(duration_.InMicroseconds());
  if (state_ >= 1.0)
    state_ = 1.0;

  AnimateToState(state_);

  if (delegate())
    delegate()->AnimationProgressed(this);

  if (state_ == 1.0)
    Stop();
}

void LinearAnimation::AnimationStarted() {
  state_ = 0.0;
}

void LinearAnimation::AnimationStopped() {
  if (!in_end_)
    return;

  in_end_ = false;
  // Set state_ to ensure we send ended to delegate and not canceled.
  state_ = 1;
  AnimateToState(1.0);
}

bool LinearAnimation::ShouldSendCanceledFromStop() {
  return state_ != 1;
}

}  // namespace gfx
