// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/animationworklet/animation_worklet_thread.h"

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/core/workers/global_scope_creation_params.h"
#include "third_party/blink/renderer/core/workers/worker_backing_thread.h"
#include "third_party/blink/renderer/core/workers/worklet_thread_holder.h"
#include "third_party/blink/renderer/modules/animationworklet/animation_worklet_global_scope.h"
#include "third_party/blink/renderer/platform/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/web_thread_supporting_gc.h"

namespace blink {

namespace {
unsigned s_ref_count = 0;
}

std::unique_ptr<AnimationWorkletThread> AnimationWorkletThread::Create(
    WorkerReportingProxy& worker_reporting_proxy) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("animation-worklet"),
               "AnimationWorkletThread::create");
  DCHECK(IsMainThread());
  return base::WrapUnique(new AnimationWorkletThread(worker_reporting_proxy));
}

template class WorkletThreadHolder<AnimationWorkletThread>;

AnimationWorkletThread::AnimationWorkletThread(
    WorkerReportingProxy& worker_reporting_proxy)
    : WorkerThread(worker_reporting_proxy) {
  DCHECK(IsMainThread());
  if (++s_ref_count == 1) {
    EnsureSharedBackingThread();
  }
}

AnimationWorkletThread::~AnimationWorkletThread() {
  DCHECK(IsMainThread());
  if (--s_ref_count == 0) {
    ClearSharedBackingThread();
  }
}

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
      FROM_HERE, CrossThreadBind(&CollectAllGarbageOnThread,
                                 CrossThreadUnretained(&done_event)));
  done_event.Wait();
}

WorkerOrWorkletGlobalScope* AnimationWorkletThread::CreateWorkerGlobalScope(
    std::unique_ptr<GlobalScopeCreationParams> creation_params) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("animation-worklet"),
               "AnimationWorkletThread::CreateWorkerGlobalScope");
  return AnimationWorkletGlobalScope::Create(std::move(creation_params), this);
}

void AnimationWorkletThread::EnsureSharedBackingThread() {
  DCHECK(IsMainThread());
  WorkletThreadHolder<AnimationWorkletThread>::EnsureInstance(
      ThreadCreationParams(WebThreadType::kAnimationWorkletThread));
}

void AnimationWorkletThread::ClearSharedBackingThread() {
  DCHECK(IsMainThread());
  DCHECK_EQ(s_ref_count, 0u);
  WorkletThreadHolder<AnimationWorkletThread>::ClearInstance();
}

// static
WorkletThreadHolder<AnimationWorkletThread>*
AnimationWorkletThread::GetWorkletThreadHolderForTesting() {
  return WorkletThreadHolder<AnimationWorkletThread>::GetInstance();
}

}  // namespace blink
