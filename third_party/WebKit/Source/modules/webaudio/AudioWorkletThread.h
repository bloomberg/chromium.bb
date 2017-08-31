// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AudioWorkletThread_h
#define AudioWorkletThread_h

#include "core/workers/WorkerThread.h"
#include "core/workers/WorkletThreadHolder.h"
#include "modules/ModulesExport.h"
#include <memory>

namespace blink {

class WebThread;
class WorkerReportingProxy;

// AudioWorkletThread is a per-frame singleton object that represents the
// backing thread for the processing of AudioWorkletNode/AudioWorkletProcessor.
// It is supposed to run an instance of V8 isolate.

class MODULES_EXPORT AudioWorkletThread final : public WorkerThread {
 public:
  static std::unique_ptr<AudioWorkletThread> Create(ThreadableLoadingContext*,
                                                    WorkerReportingProxy&);
  ~AudioWorkletThread() override;

  WorkerBackingThread& GetWorkerBackingThread() override;

  // The backing thread is cleared by clearSharedBackingThread().
  void ClearWorkerBackingThread() override {}

  // This may block the main thread.
  static void CollectAllGarbage();

  static void EnsureSharedBackingThread();
  static void ClearSharedBackingThread();

  static void CreateSharedBackingThreadForTest();

  // This only can be called after EnsureSharedBackingThread() is performed.
  // Currently AudioWorkletThread owns only one thread and it is shared by all
  // the customers.
  static WebThread* GetSharedBackingThread();

 protected:
  WorkerOrWorkletGlobalScope* CreateWorkerGlobalScope(
      std::unique_ptr<GlobalScopeCreationParams>) final;

  bool IsOwningBackingThread() const override { return false; }

 private:
  AudioWorkletThread(ThreadableLoadingContext*, WorkerReportingProxy&);

  // This raw pointer gets assigned in EnsureSharedBackingThread() and manually
  // released by ClearSharedBackingThread().
  static WebThread* s_backing_thread_;
};

}  // namespace blink

#endif  // AudioWorkletThread_h
