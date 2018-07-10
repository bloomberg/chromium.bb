// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/base/test/fake_task.h"

namespace base {
namespace sequence_manager {

FakeTask::FakeTask()
    : TaskQueue::Task(TaskQueue::PostedTask(base::OnceClosure(), FROM_HERE),
                      base::TimeTicks()) {}

FakeTaskTiming::FakeTaskTiming()
    : TaskTiming(false /* has_wall_time */, false /* has_thread_time */) {}

FakeTaskTiming::FakeTaskTiming(base::TimeTicks start, base::TimeTicks end)
    : FakeTaskTiming() {
  has_wall_time_ = true;
  start_time_ = start;
  end_time_ = end;
}

FakeTaskTiming::FakeTaskTiming(base::TimeTicks start,
                               base::TimeTicks end,
                               base::ThreadTicks thread_start,
                               base::ThreadTicks thread_end)
    : FakeTaskTiming(start, end) {
  has_thread_time_ = true;
  start_thread_time_ = thread_start;
  end_thread_time_ = thread_end;
}

}  // namespace sequence_manager
}  // namespace base
