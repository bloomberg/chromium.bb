// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/compositorworker/AbstractAnimationWorkletThread.h"

#include "core/workers/WorkerBackingThread.h"
#include "core/workers/WorkletBackingThreadHolder.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/WebThreadSupportingGC.h"
#include "public/platform/Platform.h"
#include "wtf/Assertions.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

namespace {

// This is a singleton class holding the animation worklet thread in this
// renderer process. WorkletBackingThreadHolder::m_thread is cleared by
// ModulesInitializer::shutdown.
//
// See WorkerThread::terminateAndWaitForAllWorkers for the process shutdown
// case.
//
// TODO(hongchan): consider refactoring static methods in this class into
// a template class.
class AnimationWorkletBackingThreadHolder final
    : public WorkletBackingThreadHolder {
 public:
  static AnimationWorkletBackingThreadHolder* instance() {
    MutexLocker locker(holderInstanceMutex());
    return s_instance;
  }

  static void ensureInstance() {
    if (!s_instance)
      s_instance = new AnimationWorkletBackingThreadHolder;
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
    s_instance = new AnimationWorkletBackingThreadHolder(
        WorkerBackingThread::createForTest(
            Platform::current()->compositorThread()));
  }

 private:
  AnimationWorkletBackingThreadHolder(
      std::unique_ptr<WorkerBackingThread> backingThread = nullptr)
      : WorkletBackingThreadHolder(
            backingThread ? std::move(backingThread)
                          : WorkerBackingThread::create(
                                Platform::current()->compositorThread())) {}

  static Mutex& holderInstanceMutex() {
    DEFINE_THREAD_SAFE_STATIC_LOCAL(Mutex, holderMutex, new Mutex);
    return holderMutex;
  }

  void initializeOnThread() final {
    MutexLocker locker(holderInstanceMutex());
    DCHECK(!m_initialized);
    m_thread->initialize();
    m_initialized = true;
  }

  static AnimationWorkletBackingThreadHolder* s_instance;
};

AnimationWorkletBackingThreadHolder*
    AnimationWorkletBackingThreadHolder::s_instance = nullptr;

}  // namespace

AbstractAnimationWorkletThread::AbstractAnimationWorkletThread(
    PassRefPtr<WorkerLoaderProxy> workerLoaderProxy,
    WorkerReportingProxy& workerReportingProxy)
    : WorkerThread(std::move(workerLoaderProxy), workerReportingProxy) {}

AbstractAnimationWorkletThread::~AbstractAnimationWorkletThread() {}

WorkerBackingThread& AbstractAnimationWorkletThread::workerBackingThread() {
  return *AnimationWorkletBackingThreadHolder::instance()->thread();
}

void collectAllGarbageOnThread(WaitableEvent* doneEvent) {
  blink::ThreadState::current()->collectAllGarbage();
  doneEvent->signal();
}

void AbstractAnimationWorkletThread::collectAllGarbage() {
  DCHECK(isMainThread());
  WaitableEvent doneEvent;
  AnimationWorkletBackingThreadHolder* instance =
      AnimationWorkletBackingThreadHolder::instance();
  if (!instance)
    return;
  instance->thread()->backingThread().postTask(
      BLINK_FROM_HERE, crossThreadBind(&collectAllGarbageOnThread,
                                       crossThreadUnretained(&doneEvent)));
  doneEvent.wait();
}

void AbstractAnimationWorkletThread::ensureSharedBackingThread() {
  DCHECK(isMainThread());
  AnimationWorkletBackingThreadHolder::ensureInstance();
}

void AbstractAnimationWorkletThread::clearSharedBackingThread() {
  DCHECK(isMainThread());
  AnimationWorkletBackingThreadHolder::clear();
}

void AbstractAnimationWorkletThread::createSharedBackingThreadForTest() {
  AnimationWorkletBackingThreadHolder::createForTest();
}

}  // namespace blink
