// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/animation/animation_runner.h"

#include <utility>

#include "base/timer/timer.h"

namespace {

// A default AnimationRunner based on base::Timer.
// TODO(https://crbug.com/953585): Remove this altogether.
class DefaultAnimationRunner : public gfx::AnimationRunner {
 public:
  DefaultAnimationRunner() = default;
  ~DefaultAnimationRunner() override = default;

  // gfx::AnimationRunner:
  void Stop() override { timer_.Stop(); }

 protected:
  // gfx::AnimationRunner:
  void OnStart(base::TimeDelta min_interval) override {
    timer_.Start(FROM_HERE, min_interval, this,
                 &DefaultAnimationRunner::OnTimerTick);
  }

 private:
  void OnTimerTick() { Step(base::TimeTicks::Now()); }

  base::RepeatingTimer timer_;
};

}  // namespace

namespace gfx {

// static
std::unique_ptr<AnimationRunner>
AnimationRunner::CreateDefaultAnimationRunner() {
  return std::make_unique<DefaultAnimationRunner>();
}

AnimationRunner::~AnimationRunner() = default;

void AnimationRunner::Start(
    base::TimeDelta min_interval,
    base::RepeatingCallback<void(base::TimeTicks)> step) {
  step_ = std::move(step);
  OnStart(min_interval);
}

AnimationRunner::AnimationRunner() = default;

void AnimationRunner::Step(base::TimeTicks tick) {
  step_.Run(tick);
}

void AnimationRunner::SetAnimationTimeForTesting(base::TimeTicks time) {
  step_.Run(time);
}

}  // namespace gfx
