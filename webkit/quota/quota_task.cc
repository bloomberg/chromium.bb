// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/quota/quota_task.h"

#include <algorithm>
#include <functional>

#include "base/bind.h"
#include "base/location.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/single_thread_task_runner.h"

using base::TaskRunner;

namespace quota {

// QuotaTask ---------------------------------------------------------------

QuotaTask::~QuotaTask() {
}

void QuotaTask::Start() {
  DCHECK(observer_);
  observer()->RegisterTask(this);
  Run();
}

QuotaTask::QuotaTask(QuotaTaskObserver* observer)
    : observer_(observer),
      original_task_runner_(base::MessageLoopProxy::current()) {
}

void QuotaTask::CallCompleted() {
  DCHECK(original_task_runner_->BelongsToCurrentThread());
  if (observer_) {
    observer_->UnregisterTask(this);
    Completed();
  }
}

void QuotaTask::Abort() {
  DCHECK(original_task_runner_->BelongsToCurrentThread());
  observer_ = NULL;
  Aborted();
}

void QuotaTask::DeleteSoon() {
  MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

// QuotaThreadTask ---------------------------------------------------------

QuotaThreadTask::QuotaThreadTask(
    QuotaTaskObserver* observer,
    TaskRunner* target_task_runner)
    : QuotaTask(observer),
      target_task_runner_(target_task_runner) {
}

QuotaThreadTask::~QuotaThreadTask() {
}

void QuotaThreadTask::Run() {
  target_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&QuotaThreadTask::CallRunOnTargetThread, this));
}

void QuotaThreadTask::CallRunOnTargetThread() {
  DCHECK(target_task_runner_->RunsTasksOnCurrentThread());
  if (RunOnTargetThreadAsync())
    original_task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&QuotaThreadTask::CallCompleted, this));
}

bool QuotaThreadTask::RunOnTargetThreadAsync() {
  RunOnTargetThread();
  return true;
}

void QuotaThreadTask::RunOnTargetThread() {
}

// QuotaTaskObserver -------------------------------------------------------

QuotaTaskObserver::~QuotaTaskObserver() {
  std::for_each(running_quota_tasks_.begin(),
                running_quota_tasks_.end(),
                std::mem_fun(&QuotaTask::Abort));
}

QuotaTaskObserver::QuotaTaskObserver() {
}

void QuotaTaskObserver::RegisterTask(QuotaTask* task) {
  running_quota_tasks_.insert(task);
}

void QuotaTaskObserver::UnregisterTask(QuotaTask* task) {
  DCHECK(running_quota_tasks_.find(task) != running_quota_tasks_.end());
  running_quota_tasks_.erase(task);
}

}  // namespace quota
