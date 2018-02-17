// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/IdleDeadline.h"

#include "core/timing/Performance.h"
#include "platform/scheduler/child/web_scheduler.h"
#include "platform/wtf/Time.h"
#include "public/platform/Platform.h"

namespace blink {

IdleDeadline::IdleDeadline(double deadline_seconds, CallbackType callback_type)
    : deadline_seconds_(deadline_seconds), callback_type_(callback_type) {}

double IdleDeadline::timeRemaining() const {
  double time_remaining = deadline_seconds_ - CurrentTimeTicksInSeconds();
  if (time_remaining < 0) {
    return 0;
  } else if (Platform::Current()
                 ->CurrentThread()
                 ->Scheduler()
                 ->ShouldYieldForHighPriorityWork()) {
    return 0;
  }

  return 1000.0 * Performance::ClampTimeResolution(time_remaining);
}

}  // namespace blink
