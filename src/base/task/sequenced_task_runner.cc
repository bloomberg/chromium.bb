// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/sequenced_task_runner.h"

#include <utility>

#include "base/bind.h"
#include "base/task/default_delayed_task_handle_delegate.h"

namespace base {

bool SequencedTaskRunner::PostNonNestableTask(const Location& from_here,
                                              OnceClosure task) {
  return PostNonNestableDelayedTask(from_here, std::move(task),
                                    base::TimeDelta());
}

DelayedTaskHandle SequencedTaskRunner::PostCancelableDelayedTask(
    const Location& from_here,
    OnceClosure task,
    TimeDelta delay) {
  auto delayed_task_handle_delegate =
      base::MakeRefCounted<DefaultDelayedTaskHandleDelegate>();

  task = delayed_task_handle_delegate->BindCallback(std::move(task));

  DelayedTaskHandle delayed_task_handle(
      std::move(delayed_task_handle_delegate));

  // If the task fails to be posted, the handle will automatically be
  // invalidated upon destruction of the callback object.
  if (!PostDelayedTask(from_here, std::move(task), delay))
    DCHECK(!delayed_task_handle.IsValid());

  return delayed_task_handle;
}

bool SequencedTaskRunner::DeleteOrReleaseSoonInternal(
    const Location& from_here,
    void (*deleter)(const void*),
    const void* object) {
  return PostNonNestableTask(from_here, BindOnce(deleter, object));
}

OnTaskRunnerDeleter::OnTaskRunnerDeleter(
    scoped_refptr<SequencedTaskRunner> task_runner)
    : task_runner_(std::move(task_runner)) {
}

OnTaskRunnerDeleter::~OnTaskRunnerDeleter() = default;

OnTaskRunnerDeleter::OnTaskRunnerDeleter(OnTaskRunnerDeleter&&) = default;

OnTaskRunnerDeleter& OnTaskRunnerDeleter::operator=(
    OnTaskRunnerDeleter&&) = default;

}  // namespace base
