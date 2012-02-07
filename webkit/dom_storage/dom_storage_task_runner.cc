// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/dom_storage/dom_storage_task_runner.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/message_loop_proxy.h"

namespace dom_storage {

// DomStorageTaskRunner

DomStorageTaskRunner::DomStorageTaskRunner(
    base::MessageLoopProxy* message_loop)
    : message_loop_(message_loop) {
}

DomStorageTaskRunner::~DomStorageTaskRunner() {
}

void DomStorageTaskRunner::PostTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task) {
  message_loop_->PostTask(from_here, task);
}

void DomStorageTaskRunner::PostDelayedTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeDelta delay) {
  message_loop_->PostDelayedTask(from_here, task, delay.InMilliseconds());
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

void DomStorageWorkerPoolTaskRunner::PostTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task) {
  // TODO(michaeln): Do all tasks need to be run prior to shutdown?
  // Maybe make better use of the SHUTDOWN_BEHAVIOR.
  sequenced_worker_pool_->PostSequencedWorkerTask(
      sequence_token_, from_here, task);
}

void DomStorageWorkerPoolTaskRunner::PostDelayedTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeDelta delay) {
  // Post a task to call this->PostTask() after the delay.
  message_loop_->PostDelayedTask(
      FROM_HERE,
      base::Bind(&DomStorageWorkerPoolTaskRunner::PostTask, this,
                 from_here, task),
      delay.InMilliseconds());
}

}  // namespace dom_storage
