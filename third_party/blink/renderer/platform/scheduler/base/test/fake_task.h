// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_BASE_TEST_FAKE_TASK_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_BASE_TEST_FAKE_TASK_H_

#include "base/task/sequence_manager/task_queue.h"

namespace base {
namespace sequence_manager {

class FakeTask : public TaskQueue::Task {
 public:
  FakeTask();
};

class FakeTaskTiming : public TaskQueue::TaskTiming {
 public:
  FakeTaskTiming();
  FakeTaskTiming(base::TimeTicks start, base::TimeTicks end);
  FakeTaskTiming(base::TimeTicks start,
                 base::TimeTicks end,
                 base::ThreadTicks thread_start,
                 base::ThreadTicks thread_end);
};

}  // namespace sequence_manager
}  // namespace base

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_BASE_TEST_FAKE_TASK_H_
