// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/child/worker_scheduler_proxy.h"

#include "platform/scheduler/child/worker_scheduler_impl.h"
#include "platform/scheduler/renderer/web_frame_scheduler_impl.h"

namespace blink {
namespace scheduler {

WorkerSchedulerProxy::WorkerSchedulerProxy(WebFrameScheduler* frame_scheduler)
    : main_thread_ref_(base::PlatformThread::CurrentRef()) {
  throttling_observer_ = frame_scheduler->AddThrottlingObserver(
      WebFrameScheduler::ObserverType::kWorkerScheduler, this);
}

WorkerSchedulerProxy::~WorkerSchedulerProxy() {
  DCHECK(IsMainThread());
}

void WorkerSchedulerProxy::OnWorkerSchedulerCreated(
    WorkerSchedulerImpl* worker_scheduler) {
  DCHECK(!IsMainThread())
      << "OnWorkerSchedulerCreated should be called from the worker thread";
  DCHECK(!worker_scheduler_) << "OnWorkerSchedulerCreated is called twice";
  worker_scheduler_ = worker_scheduler->GetWeakPtr();
  worker_thread_task_runner_ = worker_scheduler->ControlTaskQueue();
  worker_thread_ref_ = base::PlatformThread::CurrentRef();
  initialized_ = true;
}

void WorkerSchedulerProxy::OnThrottlingStateChanged(
    WebFrameScheduler::ThrottlingState throttling_state) {
  DCHECK(IsMainThread());
  if (throttling_state_ == throttling_state)
    return;
  throttling_state_ = throttling_state;

  if (!initialized_)
    return;

  worker_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&WorkerSchedulerImpl::OnThrottlingStateChanged,
                                worker_scheduler_, throttling_state));
}

bool WorkerSchedulerProxy::IsMainThread() const {
  return main_thread_ref_ == base::PlatformThread::CurrentRef();
}

bool WorkerSchedulerProxy::IsWorkerThread() const {
  return worker_thread_ref_ == base::PlatformThread::CurrentRef();
}

}  // namespace scheduler
}  // namespace blink
