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

 protected:
  WorkerOrWorkletGlobalScope* CreateWorkerGlobalScope(
      std::unique_ptr<GlobalScopeCreationParams>) final;

  bool IsOwningBackingThread() const override { return false; }

 private:
  AudioWorkletThread(ThreadableLoadingContext*, WorkerReportingProxy&);
};

}  // namespace blink

#endif  // AudioWorkletThread_h
