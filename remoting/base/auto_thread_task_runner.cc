// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/auto_thread_task_runner.h"

#include "base/logging.h"

namespace remoting {

AutoThreadTaskRunner::AutoThreadTaskRunner(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    const base::Closure& stop_task)
    : stop_task_(stop_task),
      task_runner_(task_runner) {
  DCHECK(!stop_task_.is_null());
}

bool AutoThreadTaskRunner::PostDelayedTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeDelta delay) {
  CHECK(task_runner_->PostDelayedTask(from_here, task, delay));
  return true;
}

bool AutoThreadTaskRunner::PostNonNestableDelayedTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeDelta delay) {
  CHECK(task_runner_->PostNonNestableDelayedTask(from_here, task, delay));
  return true;
}

bool AutoThreadTaskRunner::RunsTasksOnCurrentThread() const {
  return task_runner_->RunsTasksOnCurrentThread();
}

AutoThreadTaskRunner::~AutoThreadTaskRunner() {
  CHECK(task_runner_->PostTask(FROM_HERE, stop_task_));
}

}  // namespace remoting
