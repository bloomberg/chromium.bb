// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/plugin_thread_task_runner.h"

#include "base/bind.h"

namespace remoting {

PluginThreadTaskRunner::PluginThreadTaskRunner(Delegate* delegate)
    : plugin_thread_id_(base::PlatformThread::CurrentId()),
      delegate_(delegate) {
}

PluginThreadTaskRunner::~PluginThreadTaskRunner() {
}

void PluginThreadTaskRunner::Detach() {
  base::AutoLock auto_lock(lock_);
  if (delegate_) {
    DCHECK(BelongsToCurrentThread());
    delegate_ = NULL;
  }
}

bool PluginThreadTaskRunner::PostDelayedTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeDelta delay) {
  base::AutoLock auto_lock(lock_);
  if (!delegate_)
    return false;

  base::Closure* springpad_closure = new base::Closure(base::Bind(
      &PluginThreadTaskRunner::RunClosureIf, this, task));
  return delegate_->RunOnPluginThread(
      delay, &PluginThreadTaskRunner::TaskSpringboard, springpad_closure);
}

bool PluginThreadTaskRunner::PostNonNestableDelayedTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeDelta delay) {
  // All tasks running on this message loop are non-nestable.
  return PostDelayedTask(from_here, task, delay);
}

bool PluginThreadTaskRunner::RunsTasksOnCurrentThread() const {
  // In pepper plugins ideally we should use pp::Core::IsMainThread,
  // but it is problematic because we would need to keep reference to
  // Core somewhere, e.g. make the delegate ref-counted.
  return base::PlatformThread::CurrentId() == plugin_thread_id_;
}

// static
void PluginThreadTaskRunner::TaskSpringboard(void* data) {
  base::Closure* task = reinterpret_cast<base::Closure*>(data);
  task->Run();
  delete task;
}

void PluginThreadTaskRunner::RunClosureIf(const base::Closure& task) {
  // |delegate_| can be changed only from our thread, so it's safe to
  // access it without acquiring |lock_|.
  if (delegate_)
    task.Run();
}

}  // namespace remoting
