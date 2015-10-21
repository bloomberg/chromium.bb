// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/plugin/pepper_main_thread_task_runner.h"

#include "base/bind.h"
#include "ppapi/cpp/core.h"

namespace remoting {

PepperMainThreadTaskRunner::PepperMainThreadTaskRunner()
    : core_(pp::Module::Get()->core()), callback_factory_(this) {}

bool PepperMainThreadTaskRunner::PostDelayedTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeDelta delay) {
  core_->CallOnMainThread(delay.InMillisecondsRoundedUp(),
                          callback_factory_.NewCallback(
                              &PepperMainThreadTaskRunner::RunTask, task));
  return true;
}

bool PepperMainThreadTaskRunner::PostNonNestableDelayedTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeDelta delay) {
  return PostDelayedTask(from_here, task, delay);
}

bool PepperMainThreadTaskRunner::RunsTasksOnCurrentThread() const {
  return core_->IsMainThread();
}

PepperMainThreadTaskRunner::~PepperMainThreadTaskRunner() {}

void PepperMainThreadTaskRunner::RunTask(int32_t result,
                                         const base::Closure& task) {
  task.Run();
}

}  // namespace remoting
