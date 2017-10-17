// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webaudio/AudioWorkletThread.h"

#include <memory>
#include "core/workers/GlobalScopeCreationParams.h"
#include "core/workers/WorkerBackingThread.h"
#include "modules/webaudio/AudioWorklet.h"
#include "modules/webaudio/AudioWorkletGlobalScope.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/WaitableEvent.h"
#include "platform/WebThreadSupportingGC.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/wtf/Assertions.h"
#include "platform/wtf/PtrUtil.h"
#include "public/platform/Platform.h"

namespace blink {

template class WorkletThreadHolder<AudioWorkletThread>;

WebThread* AudioWorkletThread::s_backing_thread_ = nullptr;

unsigned AudioWorkletThread::s_ref_count_ = 0;

std::unique_ptr<AudioWorkletThread> AudioWorkletThread::Create(
    ThreadableLoadingContext* loading_context,
    WorkerReportingProxy& worker_reporting_proxy) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("audio-worklet"),
               "AudioWorkletThread::create");
  return WTF::WrapUnique(
      new AudioWorkletThread(loading_context, worker_reporting_proxy));
}

AudioWorkletThread::AudioWorkletThread(
    ThreadableLoadingContext* loading_context,
    WorkerReportingProxy& worker_reporting_proxy)
    : WorkerThread(loading_context, worker_reporting_proxy) {
  DCHECK(IsMainThread());
  if (++s_ref_count_ == 1)
    EnsureSharedBackingThread();
}

AudioWorkletThread::~AudioWorkletThread() {
  DCHECK(IsMainThread());
  if (--s_ref_count_ == 0)
    ClearSharedBackingThread();
}

WorkerBackingThread& AudioWorkletThread::GetWorkerBackingThread() {
  return *WorkletThreadHolder<AudioWorkletThread>::GetInstance()->GetThread();
}

void CollectAllGarbageOnAudioWorkletThread(WaitableEvent* done_event) {
  blink::ThreadState::Current()->CollectAllGarbage();
  done_event->Signal();
}

void AudioWorkletThread::CollectAllGarbage() {
  DCHECK(IsMainThread());
  WaitableEvent done_event;
  WorkletThreadHolder<AudioWorkletThread>* worklet_thread_holder =
      WorkletThreadHolder<AudioWorkletThread>::GetInstance();
  if (!worklet_thread_holder)
    return;
  worklet_thread_holder->GetThread()->BackingThread().PostTask(
      BLINK_FROM_HERE, CrossThreadBind(&CollectAllGarbageOnAudioWorkletThread,
                                       CrossThreadUnretained(&done_event)));
  done_event.Wait();
}

void AudioWorkletThread::EnsureSharedBackingThread() {
  DCHECK(IsMainThread());
  if (!s_backing_thread_)
    s_backing_thread_ = Platform::Current()->CreateWebAudioThread().release();
  WorkletThreadHolder<AudioWorkletThread>::EnsureInstance(s_backing_thread_);
}

void AudioWorkletThread::ClearSharedBackingThread() {
  DCHECK(IsMainThread());
  DCHECK(s_backing_thread_);
  DCHECK_EQ(s_ref_count_, 0u);
  WorkletThreadHolder<AudioWorkletThread>::ClearInstance();
  delete s_backing_thread_;
  s_backing_thread_ = nullptr;
}

WebThread* AudioWorkletThread::GetSharedBackingThread() {
  DCHECK(IsMainThread());
  WorkletThreadHolder<AudioWorkletThread>* instance =
      WorkletThreadHolder<AudioWorkletThread>::GetInstance();
  return &(instance->GetThread()->BackingThread().PlatformThread());
}

void AudioWorkletThread::CreateSharedBackingThreadForTest() {
  if (!s_backing_thread_)
    s_backing_thread_ = Platform::Current()->CreateWebAudioThread().release();
  WorkletThreadHolder<AudioWorkletThread>::CreateForTest(s_backing_thread_);
}

WorkerOrWorkletGlobalScope* AudioWorkletThread::CreateWorkerGlobalScope(
    std::unique_ptr<GlobalScopeCreationParams> creation_params) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("audio-worklet"),
               "AudioWorkletThread::createWorkerGlobalScope");
  return AudioWorkletGlobalScope::Create(
      creation_params->script_url, creation_params->user_agent,
      std::move(creation_params->starter_origin), this->GetIsolate(), this,
      creation_params->worker_clients);
}

}  // namespace blink
