// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/child/worker_scheduler_proxy.h"

#include "platform/scheduler/child/worker_scheduler_impl.h"
#include "platform/scheduler/renderer/web_frame_scheduler_impl.h"

namespace blink {
namespace scheduler {

WorkerSchedulerProxy::WorkerSchedulerProxy(WebFrameScheduler* frame_scheduler) {
  throttling_observer_handle_ = frame_scheduler->AddThrottlingObserver(
      WebFrameScheduler::ObserverType::kWorkerScheduler, this);
}

WorkerSchedulerProxy::~WorkerSchedulerProxy() {
  DCHECK(WTF::IsMainThread());
}

void WorkerSchedulerProxy::OnWorkerSchedulerCreated(
    base::WeakPtr<WorkerSchedulerImpl> worker_scheduler) {
  DCHECK(!WTF::IsMainThread())
      << "OnWorkerSchedulerCreated should be called from the worker thread";
  DCHECK(!worker_scheduler_) << "OnWorkerSchedulerCreated is called twice";
  DCHECK(worker_scheduler) << "WorkerScheduler is expected to exist";
  worker_scheduler_ = std::move(worker_scheduler);
  worker_thread_task_runner_ = worker_scheduler_->ControlTaskQueue();
  initialized_ = true;
}

void WorkerSchedulerProxy::OnThrottlingStateChanged(
    WebFrameScheduler::ThrottlingState throttling_state) {
  DCHECK(WTF::IsMainThread());
  if (throttling_state_ == throttling_state)
    return;
  throttling_state_ = throttling_state;

  if (!initialized_)
    return;

  worker_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&WorkerSchedulerImpl::OnThrottlingStateChanged,
                                worker_scheduler_, throttling_state));
}

}  // namespace scheduler
}  // namespace blink
