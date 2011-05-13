// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/quota/quota_task.h"

#include <algorithm>
#include <functional>

#include "base/message_loop.h"
#include "base/message_loop_proxy.h"

using base::MessageLoopProxy;

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
      original_message_loop_(MessageLoopProxy::CreateForCurrentThread()) {
}

void QuotaTask::CallCompleted() {
  DCHECK(original_message_loop_->BelongsToCurrentThread());
  if (observer_) {
    observer_->UnregisterTask(this);
    Completed();
  }
}

void QuotaTask::Abort() {
  DCHECK(original_message_loop_->BelongsToCurrentThread());
  observer_ = NULL;
  Aborted();
}

void QuotaTask::DeleteSoon() {
  MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

// QuotaThreadTask ---------------------------------------------------------

QuotaThreadTask::QuotaThreadTask(
    QuotaTaskObserver* observer,
    scoped_refptr<MessageLoopProxy> target_message_loop)
    : QuotaTask(observer),
      target_message_loop_(target_message_loop) {
}

QuotaThreadTask::~QuotaThreadTask() {
}

void QuotaThreadTask::Run() {
  target_message_loop_->PostTask(FROM_HERE, NewRunnableMethod(
      this, &QuotaThreadTask::CallRunOnTargetThread));
}

void QuotaThreadTask::CallRunOnTargetThread() {
  DCHECK(target_message_loop_->BelongsToCurrentThread());
  RunOnTargetThread();
  original_message_loop()->PostTask(
      FROM_HERE, NewRunnableMethod(this, &QuotaThreadTask::CallCompleted));
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
