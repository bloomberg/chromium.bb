// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/animationworklet/AnimationWorkletThread.h"

#include <memory>
#include "core/loader/ThreadableLoadingContext.h"
#include "core/workers/GlobalScopeCreationParams.h"
#include "core/workers/WorkerBackingThread.h"
#include "core/workers/WorkletThreadHolder.h"
#include "modules/animationworklet/AnimationWorkletGlobalScope.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/WebThreadSupportingGC.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/wtf/Assertions.h"
#include "platform/wtf/PtrUtil.h"
#include "public/platform/Platform.h"

namespace blink {

std::unique_ptr<AnimationWorkletThread> AnimationWorkletThread::Create(
    ThreadableLoadingContext* loading_context,
    WorkerReportingProxy& worker_reporting_proxy) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("animation-worklet"),
               "AnimationWorkletThread::create");
  DCHECK(IsMainThread());
  return WTF::WrapUnique(
      new AnimationWorkletThread(loading_context, worker_reporting_proxy));
}

template class WorkletThreadHolder<AnimationWorkletThread>;

AnimationWorkletThread::AnimationWorkletThread(
    ThreadableLoadingContext* loading_context,
    WorkerReportingProxy& worker_reporting_proxy)
    : WorkerThread(loading_context, worker_reporting_proxy) {}

AnimationWorkletThread::~AnimationWorkletThread() {}

WorkerBackingThread& AnimationWorkletThread::GetWorkerBackingThread() {
  return *WorkletThreadHolder<AnimationWorkletThread>::GetInstance()
              ->GetThread();
}

static void CollectAllGarbageOnThread(WaitableEvent* done_event) {
  blink::ThreadState::Current()->CollectAllGarbage();
  done_event->Signal();
}

void AnimationWorkletThread::CollectAllGarbage() {
  DCHECK(IsMainThread());
  WaitableEvent done_event;
  auto* holder = WorkletThreadHolder<AnimationWorkletThread>::GetInstance();
  if (!holder)
    return;
  holder->GetThread()->BackingThread().PostTask(
      BLINK_FROM_HERE, CrossThreadBind(&CollectAllGarbageOnThread,
                                       CrossThreadUnretained(&done_event)));
  done_event.Wait();
}

void AnimationWorkletThread::EnsureSharedBackingThread() {
  DCHECK(IsMainThread());
  WorkletThreadHolder<AnimationWorkletThread>::EnsureInstance(
      Platform::Current()->CompositorThread());
}

void AnimationWorkletThread::ClearSharedBackingThread() {
  DCHECK(IsMainThread());
  WorkletThreadHolder<AnimationWorkletThread>::ClearInstance();
}

void AnimationWorkletThread::CreateSharedBackingThreadForTest() {
  WorkletThreadHolder<AnimationWorkletThread>::CreateForTest(
      Platform::Current()->CompositorThread());
}

WorkerOrWorkletGlobalScope* AnimationWorkletThread::CreateWorkerGlobalScope(
    std::unique_ptr<GlobalScopeCreationParams> creation_params) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("animation-worklet"),
               "AnimationWorkletThread::createWorkerGlobalScope");
  return AnimationWorkletGlobalScope::Create(std::move(creation_params),
                                             GetIsolate(), this);
}

}  // namespace blink
