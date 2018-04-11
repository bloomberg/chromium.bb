// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_CHILD_TASK_RUNNER_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_CHILD_TASK_RUNNER_IMPL_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/single_thread_task_runner.h"
#include "base/time/time.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace blink {
namespace scheduler {
class TaskQueue;

class PLATFORM_EXPORT TaskRunnerImpl : public base::SingleThreadTaskRunner {
 public:
  static scoped_refptr<TaskRunnerImpl> Create(
      scoped_refptr<TaskQueue> task_queue,
      TaskType task_type);

  // base::SingleThreadTaskRunner implementation:
  bool RunsTasksInCurrentSequence() const override;

  TaskQueue* GetTaskQueue() const { return task_queue_.get(); }

 protected:
  bool PostDelayedTask(const base::Location&,
                       base::OnceClosure,
                       base::TimeDelta) override;
  bool PostNonNestableDelayedTask(const base::Location&,
                                  base::OnceClosure,
                                  base::TimeDelta) override;

 private:
  TaskRunnerImpl(scoped_refptr<TaskQueue> task_queue, TaskType task_type);
  ~TaskRunnerImpl() override;

  scoped_refptr<TaskQueue> task_queue_;
  TaskType task_type_;

  DISALLOW_COPY_AND_ASSIGN(TaskRunnerImpl);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_CHILD_TASK_RUNNER_IMPL_H_
