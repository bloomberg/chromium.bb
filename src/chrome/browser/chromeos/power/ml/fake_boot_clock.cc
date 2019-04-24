// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/ml/fake_boot_clock.h"

namespace chromeos {
namespace power {
namespace ml {

FakeBootClock::FakeBootClock(base::test::ScopedTaskEnvironment* env,
                             base::TimeDelta initial_time_since_boot)
    : env_(env), initial_time_since_boot_(initial_time_since_boot) {
  DCHECK_GE(initial_time_since_boot, base::TimeDelta());
  initial_time_ticks_ = env_->NowTicks();
}

FakeBootClock::~FakeBootClock() = default;

base::TimeDelta FakeBootClock::GetTimeSinceBoot() {
  base::TimeTicks now = env_->NowTicks();
  return (now - initial_time_ticks_) + initial_time_since_boot_;
}

}  // namespace ml
}  // namespace power
}  // namespace chromeos
