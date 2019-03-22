// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/perfetto/task_runner.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_local_storage.h"

namespace tracing {

PerfettoTaskRunner::PerfettoTaskRunner(
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : task_runner_(std::move(task_runner)) {}

PerfettoTaskRunner::~PerfettoTaskRunner() = default;

void PerfettoTaskRunner::PostTask(std::function<void()> task) {
  // If we're blocked from PostTasking, we defer the task until
  // later. If we're not blocked, but there's tasks that have previously been
  // deferred, we PostTask them now; this is important to preserve ordering,
  // in case the previously deferred tasks have been posted from the same
  // sequence as we're now posting a new task from.
  {
    base::AutoLock lock(lock_);
    if (posttask_is_blocked_for_thread_.Get()) {
      deferred_tasks_.emplace_back(std::move(task));
      return;
    }

    while (!deferred_tasks_.empty()) {
      task_runner_->PostTask(
          FROM_HERE, base::BindOnce([](std::function<void()> task) { task(); },
                                    deferred_tasks_.front()));
      deferred_tasks_.pop_front();
    }
  }

  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce([](std::function<void()> task) { task(); }, task));
}

void PerfettoTaskRunner::PostDelayedTask(std::function<void()> task,
                                         uint32_t delay_ms) {
  if (delay_ms == 0) {
    PostTask(std::move(task));
    return;
  }

  // There's currently nothing which uses PostDelayedTask on the ProducerClient
  // side, where PostTask sometimes requires blocking. If this DCHECK ever
  // triggers, support for deferring delayed tasks need to be added.
  DCHECK(!posttask_is_blocked_for_thread_.Get());
  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce([](std::function<void()> task) { task(); }, task),
      base::TimeDelta::FromMilliseconds(delay_ms));
}

bool PerfettoTaskRunner::RunsTasksOnCurrentThread() const {
  return task_runner_->RunsTasksInCurrentSequence();
}

void PerfettoTaskRunner::AddFileDescriptorWatch(int fd, std::function<void()>) {
  NOTREACHED();
}

void PerfettoTaskRunner::RemoveFileDescriptorWatch(int fd) {
  NOTREACHED();
}

void PerfettoTaskRunner::ResetTaskRunnerForTesting(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  task_runner_ = std::move(task_runner);
}

void PerfettoTaskRunner::BlockPostTaskForThread() {
  DCHECK(!posttask_is_blocked_for_thread_.Get());
  posttask_is_blocked_for_thread_.Set(true);
}

void PerfettoTaskRunner::UnblockPostTaskForThread() {
  DCHECK(posttask_is_blocked_for_thread_.Get());
  posttask_is_blocked_for_thread_.Set(false);
}

void PerfettoTaskRunner::StartDeferredTasksDrainTimer() {
  DCHECK(!posttask_is_blocked_for_thread_.Get());
  // The deferred tasks will generally be posted by another task being
  // posted when PostTask isn't blocked; this timer is a fallback for the
  // rare case where we're *only* getting trace events when PostTask is
  // blocked, and hence doesn't need to run very often (just often enough so
  // the SMB doesn't get filled up with uncommitted chunks).
  deferred_tasks_timer_.Start(FROM_HERE, base::TimeDelta::FromSeconds(1), this,
                              &PerfettoTaskRunner::OnDeferredTasksDrainTimer);
}

void PerfettoTaskRunner::StopDeferredTasksDrainTimer() {
  DCHECK(!posttask_is_blocked_for_thread_.Get());

  deferred_tasks_timer_.Stop();
  OnDeferredTasksDrainTimer();
}

void PerfettoTaskRunner::OnDeferredTasksDrainTimer() {
  DCHECK(!posttask_is_blocked_for_thread_.Get());

  base::AutoLock lock(lock_);
  while (!deferred_tasks_.empty()) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce([](std::function<void()> task) { task(); },
                                  deferred_tasks_.front()));
    deferred_tasks_.pop_front();
  }
}

}  // namespace tracing
