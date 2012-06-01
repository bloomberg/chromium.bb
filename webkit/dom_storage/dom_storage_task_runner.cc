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

bool DomStorageTaskRunner::RunsTasksOnCurrentThread() const {
  return IsRunningOnSequence(PRIMARY_SEQUENCE);
}

// DomStorageWorkerPoolTaskRunner

DomStorageWorkerPoolTaskRunner::DomStorageWorkerPoolTaskRunner(
    base::SequencedWorkerPool* sequenced_worker_pool,
    base::SequencedWorkerPool::SequenceToken primary_sequence_token,
    base::SequencedWorkerPool::SequenceToken commit_sequence_token,
    base::MessageLoopProxy* delayed_task_loop)
    : message_loop_(delayed_task_loop),
      sequenced_worker_pool_(sequenced_worker_pool),
      primary_sequence_token_(primary_sequence_token),
      commit_sequence_token_(commit_sequence_token) {
}

DomStorageWorkerPoolTaskRunner::~DomStorageWorkerPoolTaskRunner() {
}

bool DomStorageWorkerPoolTaskRunner::PostDelayedTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeDelta delay) {
  // Note base::TaskRunner implements PostTask in terms of PostDelayedTask
  // with a delay of zero, we detect that usage and avoid the unecessary
  // trip thru the message loop.
  if (delay == base::TimeDelta()) {
    return sequenced_worker_pool_->PostSequencedWorkerTaskWithShutdownBehavior(
        primary_sequence_token_, from_here, task,
        base::SequencedWorkerPool::BLOCK_SHUTDOWN);
  }
  // Post a task to call this->PostTask() after the delay.
  return message_loop_->PostDelayedTask(
      FROM_HERE,
      base::Bind(base::IgnoreResult(&DomStorageWorkerPoolTaskRunner::PostTask),
                 this, from_here, task),
      delay);
}

bool DomStorageWorkerPoolTaskRunner::PostShutdownBlockingTask(
    const tracked_objects::Location& from_here,
    SequenceID sequence_id,
    const base::Closure& task) {
  return sequenced_worker_pool_->PostSequencedWorkerTaskWithShutdownBehavior(
      IDtoToken(sequence_id), from_here, task,
      base::SequencedWorkerPool::BLOCK_SHUTDOWN);
}

bool DomStorageWorkerPoolTaskRunner::IsRunningOnSequence(
    SequenceID sequence_id) const {
  return sequenced_worker_pool_->IsRunningSequenceOnCurrentThread(
      IDtoToken(sequence_id));
}

base::SequencedWorkerPool::SequenceToken
DomStorageWorkerPoolTaskRunner::IDtoToken(SequenceID id) const {
  if (id == PRIMARY_SEQUENCE)
    return primary_sequence_token_;
  DCHECK_EQ(COMMIT_SEQUENCE, id);
  return commit_sequence_token_;
}

// MockDomStorageTaskRunner

MockDomStorageTaskRunner::MockDomStorageTaskRunner(
    base::MessageLoopProxy* message_loop)
    : message_loop_(message_loop) {
}

MockDomStorageTaskRunner::~MockDomStorageTaskRunner() {
}

bool MockDomStorageTaskRunner::PostDelayedTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeDelta delay) {
  return message_loop_->PostTask(from_here, task);
}

bool MockDomStorageTaskRunner::PostShutdownBlockingTask(
    const tracked_objects::Location& from_here,
    SequenceID sequence_id,
    const base::Closure& task) {
  return message_loop_->PostTask(from_here, task);
}

bool MockDomStorageTaskRunner::IsRunningOnSequence(SequenceID) const {
  return message_loop_->RunsTasksOnCurrentThread();
}

}  // namespace dom_storage
