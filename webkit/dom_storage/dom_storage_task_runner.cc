// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/dom_storage/dom_storage_task_runner.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/message_loop_proxy.h"
#include "base/tracked_objects.h"

namespace dom_storage {

// DomStorageTaskRunner

DomStorageTaskRunner::DomStorageTaskRunner(
    base::MessageLoopProxy* message_loop)
    : message_loop_(message_loop) {
}

DomStorageTaskRunner::~DomStorageTaskRunner() {
}

bool DomStorageTaskRunner::PostDelayedTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeDelta delay) {
  return message_loop_->PostDelayedTask(from_here, task, delay);
}

bool DomStorageTaskRunner::PostDelayedTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    int64 delay_ms) {
  return PostDelayedTask(
      from_here, task, base::TimeDelta::FromMilliseconds(delay_ms));
}

bool DomStorageTaskRunner::RunsTasksOnCurrentThread() const {
  return true;
}

bool DomStorageTaskRunner::PostNonNestableDelayedTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeDelta delay) {
  return PostDelayedTask(from_here, task, delay);
}

bool DomStorageTaskRunner::PostNonNestableDelayedTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    int64 delay_ms) {
  return PostDelayedTask(
      from_here, task, base::TimeDelta::FromMilliseconds(delay_ms));
}

// DomStorageWorkerPoolTaskRunner

DomStorageWorkerPoolTaskRunner::DomStorageWorkerPoolTaskRunner(
    base::SequencedWorkerPool* sequenced_worker_pool,
    base::SequencedWorkerPool::SequenceToken sequence_token,
    base::MessageLoopProxy* delayed_task_loop)
    : DomStorageTaskRunner(delayed_task_loop),
      sequenced_worker_pool_(sequenced_worker_pool),
      sequence_token_(sequence_token) {
}

DomStorageWorkerPoolTaskRunner::~DomStorageWorkerPoolTaskRunner() {
}

bool DomStorageWorkerPoolTaskRunner::PostDelayedTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeDelta delay) {
  // Note base::TaskRunner implements PostTask in terms of PostDelayedTask
  // with a delay of zero, we detect that usage and avoid the unecessary
  // trip thru the message_loop.
  if (delay == base::TimeDelta()) {
    // We can skip on shutdown as the destructor of DomStorageArea will ensure
    // that any remaining data is committed to disk.
    return sequenced_worker_pool_->PostSequencedWorkerTaskWithShutdownBehavior(
        sequence_token_, from_here, task,
        base::SequencedWorkerPool::SKIP_ON_SHUTDOWN);
  }
  // Post a task to call this->PostTask() after the delay.
  return message_loop_->PostDelayedTask(
      FROM_HERE,
      base::Bind(base::IgnoreResult(&DomStorageWorkerPoolTaskRunner::PostTask),
                 this, from_here, task),
      delay);
}

// MockDomStorageTaskRunner

MockDomStorageTaskRunner::MockDomStorageTaskRunner(
    base::MessageLoopProxy* message_loop)
    : DomStorageTaskRunner(message_loop) {
}

bool MockDomStorageTaskRunner::PostDelayedTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeDelta delay) {
  // Squash all delays to zero in our mock.
  return DomStorageTaskRunner::PostDelayedTask(
      from_here, task, base::TimeDelta());
}

}  // namespace dom_storage
