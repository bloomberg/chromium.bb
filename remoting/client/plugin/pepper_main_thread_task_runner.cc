// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/plugin/pepper_main_thread_task_runner.h"

#include <memory>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/core.h"
#include "ppapi/cpp/module.h"

namespace remoting {
namespace {

void RunAndDestroy(void* task_ptr, int32_t) {
  std::unique_ptr<base::Closure> task(static_cast<base::Closure*>(task_ptr));
  task->Run();
}

}  // namespace

PepperMainThreadTaskRunner::PepperMainThreadTaskRunner()
    : core_(pp::Module::Get()->core()), weak_ptr_factory_(this) {
  DCHECK(core_->IsMainThread());
  weak_ptr_ = weak_ptr_factory_.GetWeakPtr();
}

bool PepperMainThreadTaskRunner::PostDelayedTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeDelta delay) {
  auto task_ptr = base::MakeUnique<base::Closure>(
      base::Bind(&PepperMainThreadTaskRunner::RunTask, weak_ptr_, task));
  core_->CallOnMainThread(
      delay.InMillisecondsRoundedUp(),
      pp::CompletionCallback(&RunAndDestroy, task_ptr.release()));
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

void PepperMainThreadTaskRunner::RunTask(const base::Closure& task) {
  task.Run();
}

}  // namespace remoting
