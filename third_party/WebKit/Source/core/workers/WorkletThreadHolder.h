// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WorkletThreadHolder_h
#define WorkletThreadHolder_h

#include "core/CoreExport.h"
#include "core/workers/WorkerBackingThread.h"
#include "core/workers/WorkerBackingThreadStartupData.h"
#include "platform/WaitableEvent.h"
#include "platform/WebThreadSupportingGC.h"
#include "platform/wtf/PtrUtil.h"

namespace blink {

// WorkletThreadHolder is a template class which is designed for singleton
// instance of DerivedWorkletThread (i.e. AnimationWorkletThread,
// AudioWorkletThread).
template <class DerivedWorkletThread>
class WorkletThreadHolder {
 public:
  static WorkletThreadHolder<DerivedWorkletThread>* GetInstance() {
    MutexLocker locker(HolderInstanceMutex());
    return thread_holder_instance_;
  }

  static void EnsureInstance(WebThread* thread) {
    DCHECK(IsMainThread());
    MutexLocker locker(HolderInstanceMutex());
    if (thread_holder_instance_)
      return;
    thread_holder_instance_ = new WorkletThreadHolder<DerivedWorkletThread>;
    thread_holder_instance_->Initialize(WorkerBackingThread::Create(thread));
  }

  static void CreateForTest(const WebThreadCreationParams& params) {
    MutexLocker locker(HolderInstanceMutex());
    DCHECK(!thread_holder_instance_);
    thread_holder_instance_ = new WorkletThreadHolder<DerivedWorkletThread>;
    thread_holder_instance_->Initialize(
        WorkerBackingThread::CreateForTest(params));
  }

  static void CreateForTest(WebThread* thread) {
    MutexLocker locker(HolderInstanceMutex());
    DCHECK(!thread_holder_instance_);
    thread_holder_instance_ = new WorkletThreadHolder<DerivedWorkletThread>;
    thread_holder_instance_->Initialize(
        WorkerBackingThread::CreateForTest(thread));
  }

  static void ClearInstance() {
    DCHECK(IsMainThread());
    MutexLocker locker(HolderInstanceMutex());
    if (thread_holder_instance_) {
      thread_holder_instance_->ShutdownAndWait();
      delete thread_holder_instance_;
      thread_holder_instance_ = nullptr;
    }
  }

  WorkerBackingThread* GetThread() { return thread_.get(); }

 private:
  WorkletThreadHolder() = default;
  ~WorkletThreadHolder() = default;

  static Mutex& HolderInstanceMutex() {
    DEFINE_THREAD_SAFE_STATIC_LOCAL(Mutex, holder_mutex, ());
    return holder_mutex;
  }

  void Initialize(std::unique_ptr<WorkerBackingThread> backing_thread) {
    thread_ = std::move(backing_thread);
    thread_->BackingThread().PostTask(
        FROM_HERE,
        CrossThreadBind(&WorkletThreadHolder::InitializeOnWorkletThread,
                        CrossThreadUnretained(this)));
  }

  void InitializeOnWorkletThread() {
    MutexLocker locker(HolderInstanceMutex());
    DCHECK(!initialized_);
    thread_->InitializeOnBackingThread(
        WorkerBackingThreadStartupData::CreateDefault());
    initialized_ = true;
  }

  void ShutdownAndWait() {
    DCHECK(IsMainThread());
    WaitableEvent waitable_event;
    thread_->BackingThread().PostTask(
        FROM_HERE,
        CrossThreadBind(&WorkletThreadHolder::ShutdownOnWorlketThread,
                        CrossThreadUnretained(this),
                        CrossThreadUnretained(&waitable_event)));
    waitable_event.Wait();
  }

  void ShutdownOnWorlketThread(WaitableEvent* waitable_event) {
    thread_->ShutdownOnBackingThread();
    waitable_event->Signal();
  }

  std::unique_ptr<WorkerBackingThread> thread_;
  bool initialized_ = false;

  static WorkletThreadHolder<DerivedWorkletThread>* thread_holder_instance_;
};

template <class DerivedWorkletThread>
WorkletThreadHolder<DerivedWorkletThread>*
    WorkletThreadHolder<DerivedWorkletThread>::thread_holder_instance_ =
        nullptr;

}  // namespace blink

#endif  // WorkletBackingThreadHolder_h
