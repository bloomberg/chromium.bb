// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/auto_thread_task_runner.h"

#include "base/logging.h"

namespace remoting {

AutoThreadTaskRunner::AutoThreadTaskRunner(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : task_runner_(task_runner) {
}

AutoThreadTaskRunner::AutoThreadTaskRunner(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    const base::Closure& stop_callback)
    : stop_callback_(stop_callback),
      task_runner_(task_runner) {
}

AutoThreadTaskRunner::AutoThreadTaskRunner(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    scoped_refptr<AutoThreadTaskRunner> parent)
    : parent_(parent),
      task_runner_(task_runner) {
}

AutoThreadTaskRunner::AutoThreadTaskRunner(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    scoped_refptr<AutoThreadTaskRunner> parent,
    const base::Closure& stop_callback)
    : parent_(parent),
      stop_callback_(stop_callback),
      task_runner_(task_runner) {
}

bool AutoThreadTaskRunner::PostDelayedTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeDelta delay) {
  // CHECK() makes sure that |task_runner_| is still running, - the assumption
  // made by |AutoThreadTaskRunner| owners.
  CHECK(task_runner_->PostDelayedTask(from_here, task, delay));
  return true;
}

bool AutoThreadTaskRunner::PostNonNestableDelayedTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeDelta delay) {
  // CHECK() makes sure that |task_runner_| is still running, - the assumption
  // made by |AutoThreadTaskRunner| owners.
  CHECK(task_runner_->PostNonNestableDelayedTask(from_here, task, delay));
  return true;
}

bool AutoThreadTaskRunner::RunsTasksOnCurrentThread() const {
  return task_runner_->RunsTasksOnCurrentThread();
}

AutoThreadTaskRunner::~AutoThreadTaskRunner() {
  if (!stop_callback_.is_null())
    stop_callback_.Run();
}

}  // namespace remoting
