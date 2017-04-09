// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webaudio/AudioWorkletThread.h"

#include "core/workers/WorkerBackingThread.h"
#include "core/workers/WorkerThreadStartupData.h"
#include "modules/webaudio/AudioWorkletGlobalScope.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/WaitableEvent.h"
#include "platform/WebThreadSupportingGC.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/Platform.h"
#include "wtf/Assertions.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

template class WorkletThreadHolder<AudioWorkletThread>;

std::unique_ptr<AudioWorkletThread> AudioWorkletThread::Create(
    PassRefPtr<WorkerLoaderProxy> worker_loader_proxy,
    WorkerReportingProxy& worker_reporting_proxy) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("audio-worklet"),
               "AudioWorkletThread::create");
  DCHECK(IsMainThread());
  return WTF::WrapUnique(new AudioWorkletThread(std::move(worker_loader_proxy),
                                                worker_reporting_proxy));
}

AudioWorkletThread::AudioWorkletThread(
    PassRefPtr<WorkerLoaderProxy> worker_loader_proxy,
    WorkerReportingProxy& worker_reporting_proxy)
    : WorkerThread(std::move(worker_loader_proxy), worker_reporting_proxy) {}

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

void AudioWorkletThread::CreateSharedBackingThreadForTest() {
  WorkletThreadHolder<AudioWorkletThread>::CreateForTest("AudioWorkletThread");
}

WorkerOrWorkletGlobalScope* AudioWorkletThread::CreateWorkerGlobalScope(
    std::unique_ptr<WorkerThreadStartupData> startup_data) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("audio-worklet"),
               "AudioWorkletThread::createWorkerGlobalScope");

  RefPtr<SecurityOrigin> security_origin =
      SecurityOrigin::Create(startup_data->script_url_);
  if (startup_data->starter_origin_privilege_data_) {
    security_origin->TransferPrivilegesFrom(
        std::move(startup_data->starter_origin_privilege_data_));
  }

  return AudioWorkletGlobalScope::Create(
      startup_data->script_url_, startup_data->user_agent_,
      security_origin.Release(), this->GetIsolate(), this);
}

}  // namespace blink
