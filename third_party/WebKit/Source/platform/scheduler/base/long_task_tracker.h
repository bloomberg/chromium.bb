// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_BASE_LONG_TASK_TRACKER_H_
#define THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_BASE_LONG_TASK_TRACKER_H_

#include <vector>

#include "base/macros.h"
#include "base/time/time.h"
#include "public/platform/WebCommon.h"

namespace blink {
namespace scheduler {

// Records and reports long task times, above the threshold.
// The threshold is set to 50ms in the implementation.
class BLINK_PLATFORM_EXPORT LongTaskTracker {
 public:
  using LongTaskTiming =
      std::vector<std::pair<base::TimeTicks, base::TimeDelta>>;

  LongTaskTracker();
  ~LongTaskTracker();

  LongTaskTiming GetLongTaskTiming();
  void RecordLongTask(base::TimeTicks startTime, base::TimeDelta duration);

 private:
  friend class LongTaskTrackerTest;

  LongTaskTiming long_task_times_;

  DISALLOW_COPY_AND_ASSIGN(LongTaskTracker);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_BASE_LONG_TASK_TRACKER_H_
