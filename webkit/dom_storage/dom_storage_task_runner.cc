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

bool DomStorageTaskRunner::PostTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task) {
  return message_loop_->PostTask(from_here, task);
}

bool DomStorageTaskRunner::PostDelayedTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeDelta delay) {
  return message_loop_->PostDelayedTask(from_here, task, delay);
}

// DomStorageWorkerPoolTaskRunner

DomStorageWorkerPoolTaskRunner::DomStorageWorkerPoolTaskRunner(
    base::SequencedWorkerPool* sequenced_worker_pool,
    base::MessageLoopProxy* delayed_task_loop)
    : DomStorageTaskRunner(delayed_task_loop),
      sequenced_worker_pool_(sequenced_worker_pool),
      sequence_token_(
          sequenced_worker_pool->GetNamedSequenceToken("dom_storage_token")) {
}

DomStorageWorkerPoolTaskRunner::~DomStorageWorkerPoolTaskRunner() {
}

bool DomStorageWorkerPoolTaskRunner::PostTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task) {
  // We can skip on shutdown as the destructor of DomStorageArea will ensure
  // that any remaining data is committed to disk.
  return sequenced_worker_pool_->PostSequencedWorkerTaskWithShutdownBehavior(
      sequence_token_, from_here, task,
      base::SequencedWorkerPool::SKIP_ON_SHUTDOWN);
}

bool DomStorageWorkerPoolTaskRunner::PostDelayedTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeDelta delay) {
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
  // Don't wait in unit tests.
  return PostTask(from_here, task);
}

}  // namespace dom_storage
