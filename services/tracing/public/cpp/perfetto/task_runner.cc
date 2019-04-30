// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/perfetto/task_runner.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/no_destructor.h"
#include "base/task/common/checked_lock_impl.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool/thread_pool.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_local.h"
#include "base/threading/thread_local_storage.h"
#include "services/tracing/public/cpp/perfetto/trace_event_data_source.h"

namespace tracing {

namespace {

base::ThreadLocalBoolean* PostTaskIsBlockedForThread() {
  static base::NoDestructor<base::ThreadLocalBoolean> post_task_is_blocked;
  return post_task_is_blocked.get();
}

}  // namespace

ScopedPerfettoPostTaskBlocker::ScopedPerfettoPostTaskBlocker(bool enable)
    : enabled_(enable) {
  if (enabled_) {
    PerfettoTaskRunner::BlockPostTaskForThread();
  } else {
    base::internal::CheckedLockImpl::AssertNoLockHeldOnCurrentThread();
  }
}

ScopedPerfettoPostTaskBlocker::~ScopedPerfettoPostTaskBlocker() {
  if (enabled_) {
    PerfettoTaskRunner::UnblockPostTaskForThread();
  }
}

// static
void PerfettoTaskRunner::BlockPostTaskForThread() {
  DCHECK(!PostTaskIsBlockedForThread()->Get());
  PostTaskIsBlockedForThread()->Set(true);
}

// static
void PerfettoTaskRunner::UnblockPostTaskForThread() {
  DCHECK(PostTaskIsBlockedForThread()->Get());
  PostTaskIsBlockedForThread()->Set(false);
}

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
    if (!base::ThreadPool::GetInstance() ||
        PostTaskIsBlockedForThread()->Get()) {
      deferred_tasks_.emplace_back(std::move(task));
      return;
    }

    while (!deferred_tasks_.empty()) {
      GetOrCreateTaskRunner()->PostTask(
          FROM_HERE, base::BindOnce([](std::function<void()> task) { task(); },
                                    deferred_tasks_.front()));
      deferred_tasks_.pop_front();
    }
  }

  GetOrCreateTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](std::function<void()> task) {
                       // We block any trace events that happens while any
                       // Perfetto task is running, or we'll get deadlocks in
                       // situations where the StartupTraceWriterRegistry tries
                       // to bind a writer which in turn causes a PostTask where
                       // a trace event can be emitted, which then deadlocks as
                       // it needs a new chunk from the same StartupTraceWriter
                       // which we're trying to bind and are keeping the lock
                       // to.
                       // TODO(oysteine): Try to see if we can be more selective
                       // about this.
                       AutoThreadLocalBoolean thread_is_in_trace_event(
                           TraceEventDataSource::GetThreadIsInTraceEventTLS());
                       task();
                     },
                     task));
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
  DCHECK(!PostTaskIsBlockedForThread()->Get());
  GetOrCreateTaskRunner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce([](std::function<void()> task) { task(); }, task),
      base::TimeDelta::FromMilliseconds(delay_ms));
}

bool PerfettoTaskRunner::RunsTasksOnCurrentThread() const {
  DCHECK(task_runner_);
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

void PerfettoTaskRunner::StartDeferredTasksDrainTimer() {
  DCHECK(!PostTaskIsBlockedForThread()->Get());
  // The deferred tasks will generally be posted by another task being
  // posted when PostTask isn't blocked; this timer is a fallback for the
  // rare case where we're *only* getting trace events when PostTask is
  // blocked, and hence doesn't need to run very often (just often enough so
  // the SMB doesn't get filled up with uncommitted chunks).
  deferred_tasks_timer_.Start(FROM_HERE, base::TimeDelta::FromSeconds(1), this,
                              &PerfettoTaskRunner::OnDeferredTasksDrainTimer);
}

void PerfettoTaskRunner::StopDeferredTasksDrainTimer() {
  DCHECK(!PostTaskIsBlockedForThread()->Get());

  deferred_tasks_timer_.Stop();
  OnDeferredTasksDrainTimer();
}

void PerfettoTaskRunner::OnDeferredTasksDrainTimer() {
  DCHECK(!PostTaskIsBlockedForThread()->Get());

  base::AutoLock lock(lock_);
  while (!deferred_tasks_.empty()) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce([](std::function<void()> task) { task(); },
                                  deferred_tasks_.front()));
    deferred_tasks_.pop_front();
  }
}

void PerfettoTaskRunner::SetTaskRunner(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  DCHECK(!task_runner_);
  task_runner_ = std::move(task_runner);
}

scoped_refptr<base::SequencedTaskRunner>
PerfettoTaskRunner::GetOrCreateTaskRunner() {
  if (!task_runner_) {
    DCHECK(base::ThreadPool::GetInstance());
    task_runner_ = base::CreateSequencedTaskRunnerWithTraits(
        {base::MayBlock(), base::TaskPriority::USER_BLOCKING});
  }

  return task_runner_;
}

}  // namespace tracing
