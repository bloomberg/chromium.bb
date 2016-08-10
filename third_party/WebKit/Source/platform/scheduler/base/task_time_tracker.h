// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_BASE_TASK_TIME_TRACKER_H_
#define THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_BASE_TASK_TIME_TRACKER_H_

#include "base/time/time.h"
#include "public/platform/WebCommon.h"

namespace blink {
namespace scheduler {

class BLINK_PLATFORM_EXPORT TaskTimeTracker {
 public:
  TaskTimeTracker() {}
  virtual ~TaskTimeTracker() {}

  virtual void ReportTaskTime(base::TimeTicks startTime,
                              base::TimeTicks endTime) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(TaskTimeTracker);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_BASE_TASK_TIME_TRACKER_H_
