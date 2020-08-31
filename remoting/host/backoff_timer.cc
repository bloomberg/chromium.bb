// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/backoff_timer.h"
#include "base/bind.h"

#include <utility>

namespace remoting {

BackoffTimer::BackoffTimer() : timer_(new base::OneShotTimer()) {}

BackoffTimer::~BackoffTimer() = default;

void BackoffTimer::Start(const base::Location& posted_from,
                         base::TimeDelta delay,
                         base::TimeDelta max_delay,
                         const base::Closure& user_task) {
  backoff_policy_.multiply_factor = 2;
  backoff_policy_.initial_delay_ms = delay.InMilliseconds();
  backoff_policy_.maximum_backoff_ms = max_delay.InMilliseconds();
  backoff_policy_.entry_lifetime_ms = -1;
  backoff_entry_.reset(new net::BackoffEntry(&backoff_policy_));

  posted_from_ = posted_from;
  user_task_ = user_task;
  StartTimer();
}

void BackoffTimer::Stop() {
  timer_->Stop();
  user_task_.Reset();
  backoff_entry_.reset();
}

void BackoffTimer::SetTimerForTest(std::unique_ptr<base::OneShotTimer> timer) {
  timer_ = std::move(timer);
}

void BackoffTimer::StartTimer() {
  timer_->Start(
      posted_from_, backoff_entry_->GetTimeUntilRelease(),
      base::BindOnce(&BackoffTimer::OnTimerFired, base::Unretained(this)));
}

void BackoffTimer::OnTimerFired() {
  DCHECK(IsRunning());
  DCHECK(!user_task_.is_null());
  backoff_entry_->InformOfRequest(false);
  StartTimer();

  // Running the user task may destroy this object, so don't reference
  // any fields of this object after running it.
  base::Closure user_task(user_task_);
  user_task.Run();
}

}  // namespace remoting
