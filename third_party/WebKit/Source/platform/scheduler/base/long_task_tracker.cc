// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/base/long_task_tracker.h"

namespace blink {
namespace scheduler {

namespace {
int kLongTaskThresholdMillis = 50;
}

LongTaskTracker::LongTaskTracker() {}

LongTaskTracker::~LongTaskTracker() {}

void LongTaskTracker::RecordLongTask(base::TimeTicks startTime,
                                     base::TimeDelta duration) {
  if (duration.InMilliseconds() > kLongTaskThresholdMillis) {
    long_task_times_.push_back(std::make_pair(startTime, duration));
  }
}

LongTaskTracker::LongTaskTiming LongTaskTracker::GetLongTaskTiming() {
  return std::move(long_task_times_);
}

}  // namespace scheduler
}  // namespace blink
