// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/child/task_runner_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "third_party/blink/renderer/platform/scheduler/base/task_queue.h"

namespace blink {
namespace scheduler {

scoped_refptr<TaskRunnerImpl> TaskRunnerImpl::Create(
    scoped_refptr<TaskQueue> task_queue,
    TaskType task_type) {
  return base::WrapRefCounted(
      new TaskRunnerImpl(std::move(task_queue), task_type));
}

bool TaskRunnerImpl::RunsTasksInCurrentSequence() const {
  return task_queue_->RunsTasksInCurrentSequence();
}

TaskRunnerImpl::TaskRunnerImpl(scoped_refptr<TaskQueue> task_queue,
                               TaskType task_type)
    : task_queue_(std::move(task_queue)), task_type_(task_type) {}

TaskRunnerImpl::~TaskRunnerImpl() = default;

bool TaskRunnerImpl::PostDelayedTask(const base::Location& location,
                                     base::OnceClosure task,
                                     base::TimeDelta delay) {
  return task_queue_->PostTaskWithMetadata(TaskQueue::PostedTask(
      std::move(task), location, delay, base::Nestable::kNestable,
      static_cast<int>(task_type_)));
}

bool TaskRunnerImpl::PostNonNestableDelayedTask(const base::Location& location,
                                                base::OnceClosure task,
                                                base::TimeDelta delay) {
  return task_queue_->PostTaskWithMetadata(TaskQueue::PostedTask(
      std::move(task), location, delay, base::Nestable::kNonNestable,
      static_cast<int>(task_type_)));
}

}  // namespace scheduler
}  // namespace blink
