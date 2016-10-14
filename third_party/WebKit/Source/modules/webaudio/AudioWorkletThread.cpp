// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webaudio/AudioWorkletThread.h"

#include "core/workers/WorkerBackingThread.h"
#include "core/workers/WorkerThreadStartupData.h"
#include "core/workers/WorkletBackingThreadHolder.h"
#include "modules/webaudio/AudioWorkletGlobalScope.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/WaitableEvent.h"
#include "platform/WebThreadSupportingGC.h"
#include "platform/tracing/TraceEvent.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/Platform.h"
#include "wtf/Assertions.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

namespace {

// TODO(hongchan): consider refactoring static methods in this class into
// a template class.
class AudioWorkletThreadHolder final : public WorkletBackingThreadHolder {
 public:
  static AudioWorkletThreadHolder* instance() {
    MutexLocker locker(holderInstanceMutex());
    return s_instance;
  }

  static void ensureInstance() {
    if (!s_instance)
      s_instance = new AudioWorkletThreadHolder;
  }

  static void clear() {
    MutexLocker locker(holderInstanceMutex());
    if (s_instance) {
      s_instance->shutdownAndWait();
      delete s_instance;
      s_instance = nullptr;
    }
  }

  static void createForTest() {
    MutexLocker locker(holderInstanceMutex());
    DCHECK_EQ(nullptr, s_instance);
    s_instance =
        new AudioWorkletThreadHolder(WorkerBackingThread::createForTest(
            "AudioWorkletThread", BlinkGC::PerThreadHeapMode));
  }

 private:
  AudioWorkletThreadHolder(
      std::unique_ptr<WorkerBackingThread> backingThread = nullptr)
      : WorkletBackingThreadHolder(
            backingThread
                ? std::move(backingThread)
                : WorkerBackingThread::create("AudioWorkletThread",
                                              BlinkGC::PerThreadHeapMode)) {}

  static Mutex& holderInstanceMutex() {
    DEFINE_THREAD_SAFE_STATIC_LOCAL(Mutex, holderMutex, new Mutex);
    return holderMutex;
  }

  void initializeOnThread() {
    MutexLocker locker(holderInstanceMutex());
    DCHECK(!m_initialized);
    m_thread->initialize();
    m_initialized = true;
  }

  static AudioWorkletThreadHolder* s_instance;
};

AudioWorkletThreadHolder* AudioWorkletThreadHolder::s_instance = nullptr;

}  // namespace

std::unique_ptr<AudioWorkletThread> AudioWorkletThread::create(
    PassRefPtr<WorkerLoaderProxy> workerLoaderProxy,
    WorkerReportingProxy& workerReportingProxy) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("audio-worklet"),
               "AudioWorkletThread::create");
  DCHECK(isMainThread());
  return wrapUnique(new AudioWorkletThread(std::move(workerLoaderProxy),
                                           workerReportingProxy));
}

AudioWorkletThread::AudioWorkletThread(
    PassRefPtr<WorkerLoaderProxy> workerLoaderProxy,
    WorkerReportingProxy& workerReportingProxy)
    : WorkerThread(std::move(workerLoaderProxy), workerReportingProxy) {}

AudioWorkletThread::~AudioWorkletThread() {}

WorkerBackingThread& AudioWorkletThread::workerBackingThread() {
  return *AudioWorkletThreadHolder::instance()->thread();
}

void collectAllGarbageOnAudioWorkletThread(WaitableEvent* doneEvent) {
  blink::ThreadState::current()->collectAllGarbage();
  doneEvent->signal();
}

void AudioWorkletThread::collectAllGarbage() {
  DCHECK(isMainThread());
  WaitableEvent doneEvent;
  AudioWorkletThreadHolder* instance = AudioWorkletThreadHolder::instance();
  if (!instance)
    return;
  instance->thread()->backingThread().postTask(
      BLINK_FROM_HERE, crossThreadBind(&collectAllGarbageOnAudioWorkletThread,
                                       crossThreadUnretained(&doneEvent)));
  doneEvent.wait();
}

void AudioWorkletThread::ensureSharedBackingThread() {
  DCHECK(isMainThread());
  AudioWorkletThreadHolder::ensureInstance();
}

void AudioWorkletThread::clearSharedBackingThread() {
  DCHECK(isMainThread());
  AudioWorkletThreadHolder::clear();
}

void AudioWorkletThread::createSharedBackingThreadForTest() {
  AudioWorkletThreadHolder::createForTest();
}

WorkerOrWorkletGlobalScope* AudioWorkletThread::createWorkerGlobalScope(
    std::unique_ptr<WorkerThreadStartupData> startupData) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("audio-worklet"),
               "AudioWorkletThread::createWorkerGlobalScope");

  RefPtr<SecurityOrigin> securityOrigin =
      SecurityOrigin::create(startupData->m_scriptURL);
  if (startupData->m_starterOriginPrivilegeData) {
    securityOrigin->transferPrivilegesFrom(
        std::move(startupData->m_starterOriginPrivilegeData));
  }

  return AudioWorkletGlobalScope::create(
      startupData->m_scriptURL, startupData->m_userAgent,
      securityOrigin.release(), this->isolate(), this);
}

}  // namespace blink
