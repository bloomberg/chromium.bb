// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AnimationWorkletThread_h
#define AnimationWorkletThread_h

#include <memory>
#include "core/workers/WorkerThread.h"
#include "modules/ModulesExport.h"

namespace blink {

class ThreadableLoadingContext;
class WorkerReportingProxy;

// Represents the shared backing thread that is used by all animation worklets
// and participates in the Blink garbage collection process.
class MODULES_EXPORT AnimationWorkletThread final : public WorkerThread {
 public:
  static std::unique_ptr<AnimationWorkletThread> Create(
      ThreadableLoadingContext*,
      WorkerReportingProxy&);
  ~AnimationWorkletThread() override;

  WorkerBackingThread& GetWorkerBackingThread() override;

  // The backing thread is cleared by ClearSharedBackingThread().
  void ClearWorkerBackingThread() override {}

  // This may block the main thread.
  static void CollectAllGarbage();

  static void EnsureSharedBackingThread();
  static void ClearSharedBackingThread();

  static void CreateSharedBackingThreadForTest();

  // This only can be called after EnsureSharedBackingThread() is performed.
  // Currently AnimationWorkletThread owns only one thread and it is shared
  // by all the customers.
  static WebThread* GetSharedBackingThread();

 private:
  AnimationWorkletThread(ThreadableLoadingContext*, WorkerReportingProxy&);

  WorkerOrWorkletGlobalScope* CreateWorkerGlobalScope(
      std::unique_ptr<GlobalScopeCreationParams>) final;

  bool IsOwningBackingThread() const override { return false; }

  WebThreadType GetThreadType() const override {
    return WebThreadType::kAnimationWorkletThread;
  }
};

}  // namespace blink

#endif  // AnimationWorkletThread_h
