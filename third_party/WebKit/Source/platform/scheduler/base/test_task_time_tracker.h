// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_BASE_TEST_TASK_TIME_TRACKER_H_
#define THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_BASE_TEST_TASK_TIME_TRACKER_H_

#include "base/time/time.h"
#include "platform/scheduler/base/task_time_tracker.h"

namespace blink {
namespace scheduler {

class TestTaskTimeTracker : public TaskTimeTracker {
 public:
  void ReportTaskTime(base::TimeTicks startTime,
                      base::TimeTicks endTime) override {}
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_BASE_TEST_TASK_TIME_TRACKER_H_
