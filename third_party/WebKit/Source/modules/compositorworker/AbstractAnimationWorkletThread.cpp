// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/compositorworker/AbstractAnimationWorkletThread.h"

#include <memory>
#include "core/loader/ThreadableLoadingContext.h"
#include "core/workers/WorkerBackingThread.h"
#include "core/workers/WorkletThreadHolder.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/WebThreadSupportingGC.h"
#include "platform/wtf/Assertions.h"
#include "platform/wtf/PtrUtil.h"
#include "public/platform/Platform.h"

namespace blink {

template class WorkletThreadHolder<AbstractAnimationWorkletThread>;

AbstractAnimationWorkletThread::AbstractAnimationWorkletThread(
    ThreadableLoadingContext* loading_context,
    WorkerReportingProxy& worker_reporting_proxy)
    : WorkerThread(loading_context, worker_reporting_proxy) {}

AbstractAnimationWorkletThread::~AbstractAnimationWorkletThread() {}

WorkerBackingThread& AbstractAnimationWorkletThread::GetWorkerBackingThread() {
  return *WorkletThreadHolder<AbstractAnimationWorkletThread>::GetInstance()
              ->GetThread();
}

void CollectAllGarbageOnThread(WaitableEvent* done_event) {
  blink::ThreadState::Current()->CollectAllGarbage();
  done_event->Signal();
}

void AbstractAnimationWorkletThread::CollectAllGarbage() {
  DCHECK(IsMainThread());
  WaitableEvent done_event;
  WorkletThreadHolder<AbstractAnimationWorkletThread>* worklet_thread_holder =
      WorkletThreadHolder<AbstractAnimationWorkletThread>::GetInstance();
  if (!worklet_thread_holder)
    return;
  worklet_thread_holder->GetThread()->BackingThread().PostTask(
      BLINK_FROM_HERE, CrossThreadBind(&CollectAllGarbageOnThread,
                                       CrossThreadUnretained(&done_event)));
  done_event.Wait();
}

void AbstractAnimationWorkletThread::EnsureSharedBackingThread() {
  DCHECK(IsMainThread());
  WorkletThreadHolder<AbstractAnimationWorkletThread>::EnsureInstance(
      Platform::Current()->CompositorThread());
}

void AbstractAnimationWorkletThread::ClearSharedBackingThread() {
  DCHECK(IsMainThread());
  WorkletThreadHolder<AbstractAnimationWorkletThread>::ClearInstance();
}

void AbstractAnimationWorkletThread::CreateSharedBackingThreadForTest() {
  WorkletThreadHolder<AbstractAnimationWorkletThread>::CreateForTest(
      Platform::Current()->CompositorThread());
}

}  // namespace blink
