// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/plugin_message_loop_proxy.h"

namespace remoting {

PluginMessageLoopProxy::PluginMessageLoopProxy(Delegate* delegate)
    : delegate_(delegate) {
}

PluginMessageLoopProxy::~PluginMessageLoopProxy() {
}

void PluginMessageLoopProxy::Detach() {
  base::AutoLock auto_lock(lock_);
  delegate_ = NULL;
}

// MessageLoopProxy interface implementation.
bool PluginMessageLoopProxy::PostTask(
    const tracked_objects::Location& from_here,
    Task* task) {
  return PostDelayedTask(from_here, task, 0);
}

bool PluginMessageLoopProxy::PostDelayedTask(
    const tracked_objects::Location& from_here,
    Task* task,
    int64 delay_ms) {
  base::AutoLock auto_lock(lock_);
  if (!delegate_) {
    return false;
  } else {
    return delegate_->RunOnPluginThread(
        delay_ms, &PluginMessageLoopProxy::RunTask, task);
  }
}

bool PluginMessageLoopProxy::PostNonNestableTask(
    const tracked_objects::Location& from_here,
    Task* task) {
  // All tasks running on this message loop are non-nestable.
  return PostTask(from_here, task);
}

bool PluginMessageLoopProxy::PostNonNestableDelayedTask(
      const tracked_objects::Location& from_here,
      Task* task,
      int64 delay_ms) {
  // All tasks running on this message loop are non-nestable.
  return PostDelayedTask(from_here, task, delay_ms);
}

bool PluginMessageLoopProxy::PostTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task) {
  return PostDelayedTask(from_here, task, 0);
}

bool PluginMessageLoopProxy::PostDelayedTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    int64 delay_ms) {
  base::AutoLock auto_lock(lock_);
  if (!delegate_) {
    return false;
  } else {
    base::Closure* task_on_heap = new base::Closure(task);
    return delegate_->RunOnPluginThread(
        delay_ms, &PluginMessageLoopProxy::RunClosure, task_on_heap);
  }
}

bool PluginMessageLoopProxy::PostNonNestableTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task) {
  // All tasks running on this message loop are non-nestable.
  return PostTask(from_here, task);
}

bool PluginMessageLoopProxy::PostNonNestableDelayedTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    int64 delay_ms) {
  // All tasks running on this message loop are non-nestable.
  return PostDelayedTask(from_here, task, delay_ms);
}

bool PluginMessageLoopProxy::BelongsToCurrentThread() {
  base::AutoLock auto_lock(lock_);
  if (delegate_) {
    return delegate_->IsPluginThread();
  } else {
    return false;
  }
}

// static
void PluginMessageLoopProxy::RunTask(void* data) {
  Task* task = reinterpret_cast<Task*>(data);
  task->Run();
  delete task;
}

// static
void PluginMessageLoopProxy::RunClosure(void* data) {
  base::Closure* task = reinterpret_cast<base::Closure*>(data);
  task->Run();
  delete task;
}

}  // namespace remoting
