// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_CHILD_WORKER_SCHEDULER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_CHILD_WORKER_SCHEDULER_H_

#include "base/single_thread_task_runner.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/platform/scheduler/base/task_queue.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_or_worker_scheduler.h"

namespace blink {

namespace scheduler {

class NonMainThreadScheduler;

// A scheduler provides per-global-scope task queues. This is constructed when a
// global scope is created and destructed when it's closed.
//
// Unless stated otherwise, all methods must be called on the worker thread.
class PLATFORM_EXPORT WorkerScheduler : public FrameOrWorkerScheduler {
 public:
  explicit WorkerScheduler(NonMainThreadScheduler* non_main_thread_scheduler);
  ~WorkerScheduler() override;

  std::unique_ptr<ActiveConnectionHandle> OnActiveConnectionCreated() override;

  // Unregisters the task queues and cancels tasks in them.
  void Dispose();

  // Returns a task runner that is suitable with the given task type. This can
  // be called from any thread.
  //
  // This must be called only from WorkerThread::GetTaskRunner().
  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner(TaskType) const;

  // TODO(nhiroki): Add mechanism to throttle/suspend tasks in response to the
  // state of the parent document (https://crbug.com/670534).

 private:
  scoped_refptr<base::sequence_manager::TaskQueue> task_queue_;

  NonMainThreadScheduler* thread_scheduler_;  // NOT OWNED

#if DCHECK_IS_ON()
  bool is_disposed_ = false;
#endif
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_CHILD_WORKER_SCHEDULER_H_
