// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webaudio/AudioWorkletThread.h"

#include <memory>
#include "core/workers/GlobalScopeCreationParams.h"
#include "core/workers/WorkerBackingThread.h"
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

std::unique_ptr<AudioWorkletThread> AudioWorkletThread::Create(
    ThreadableLoadingContext* loading_context,
    WorkerReportingProxy& worker_reporting_proxy) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("audio-worklet"),
               "AudioWorkletThread::create");
  DCHECK(IsMainThread());
  return WTF::WrapUnique(
      new AudioWorkletThread(loading_context, worker_reporting_proxy));
}

AudioWorkletThread::AudioWorkletThread(
    ThreadableLoadingContext* loading_context,
    WorkerReportingProxy& worker_reporting_proxy)
    : WorkerThread(loading_context, worker_reporting_proxy) {}

AudioWorkletThread::~AudioWorkletThread() {}

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
  WorkletThreadHolder<AudioWorkletThread>::EnsureInstance("AudioWorkletThread");
}

void AudioWorkletThread::ClearSharedBackingThread() {
  DCHECK(IsMainThread());
  WorkletThreadHolder<AudioWorkletThread>::ClearInstance();
}

WebThread* AudioWorkletThread::GetSharedBackingThread() {
  DCHECK(IsMainThread());
  WorkletThreadHolder<AudioWorkletThread>* instance =
      WorkletThreadHolder<AudioWorkletThread>::GetInstance();
  if (!instance)
    return nullptr;
  return &(instance->GetThread()->BackingThread().PlatformThread());
}

void AudioWorkletThread::CreateSharedBackingThreadForTest() {
  WorkletThreadHolder<AudioWorkletThread>::CreateForTest("AudioWorkletThread");
}

WorkerOrWorkletGlobalScope* AudioWorkletThread::CreateWorkerGlobalScope(
    std::unique_ptr<GlobalScopeCreationParams> creation_params) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("audio-worklet"),
               "AudioWorkletThread::createWorkerGlobalScope");

  RefPtr<SecurityOrigin> security_origin =
      SecurityOrigin::Create(creation_params->script_url);
  if (creation_params->starter_origin_privilege_data) {
    security_origin->TransferPrivilegesFrom(
        std::move(creation_params->starter_origin_privilege_data));
  }

  return AudioWorkletGlobalScope::Create(
      creation_params->script_url, creation_params->user_agent,
      std::move(security_origin), this->GetIsolate(), this,
      creation_params->worker_clients);
}

}  // namespace blink
