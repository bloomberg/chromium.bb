// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_IDLE_DETECTOR_H_
#define CHROME_BROWSER_CHROMEOS_IDLE_DETECTOR_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/base/user_activity/user_activity_observer.h"

namespace chromeos {

class IdleDetector : public ui::UserActivityObserver {
 public:
  IdleDetector(const base::RepeatingClosure& on_idle_callback,
               const base::TickClock* tick_clock);

  ~IdleDetector() override;

  void Start(const base::TimeDelta& timeout);

 private:
  // ui::UserActivityObserver overrides:
  void OnUserActivity(const ui::Event* event) override;

  // Resets |timer_| to fire when we reach our idle timeout.
  void ResetTimer();

  base::OneShotTimer timer_;

  base::RepeatingClosure idle_callback_;

  base::TimeDelta timeout_;

  DISALLOW_COPY_AND_ASSIGN(IdleDetector);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_IDLE_DETECTOR_H_
