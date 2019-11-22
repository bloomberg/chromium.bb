// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_WEB_SCHEDULING_TASK_QUEUE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_WEB_SCHEDULING_TASK_QUEUE_H_

#include "base/memory/scoped_refptr.h"
#include "base/single_thread_task_runner.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/scheduler/public/web_scheduling_priority.h"

namespace blink {

// This class is used by the experimental Scheduling API to submit tasks to the
// platform's scheduler through prioritized task queues (see
// https://github.com/WICG/main-thread-scheduling).
class PLATFORM_EXPORT WebSchedulingTaskQueue {
 public:
  virtual ~WebSchedulingTaskQueue() = default;

  virtual void SetPriority(WebSchedulingPriority) = 0;

  // Returns a task runner that is suitable with the web scheduling task type
  // associated with the priority of this task queue.
  virtual scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner() = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_WEB_SCHEDULING_TASK_QUEUE_H_
