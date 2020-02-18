// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_WEB_SCHEDULING_TASK_QUEUE_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_WEB_SCHEDULING_TASK_QUEUE_IMPL_H_

#include "third_party/blink/renderer/platform/scheduler/public/web_scheduling_task_queue.h"

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/scheduler/public/web_scheduling_priority.h"

namespace blink {
namespace scheduler {

class MainThreadTaskQueue;

class PLATFORM_EXPORT WebSchedulingTaskQueueImpl
    : public WebSchedulingTaskQueue {
 public:
  WebSchedulingTaskQueueImpl(base::WeakPtr<MainThreadTaskQueue>);
  ~WebSchedulingTaskQueueImpl() override = default;

  void SetPriority(WebSchedulingPriority) override;

  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner() override;

 private:
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  base::WeakPtr<MainThreadTaskQueue> task_queue_;
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_WEB_SCHEDULING_TASK_QUEUE_IMPL_H_
