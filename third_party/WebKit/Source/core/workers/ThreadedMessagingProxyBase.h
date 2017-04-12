// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ThreadedMessagingProxyBase_h
#define ThreadedMessagingProxyBase_h

#include "core/CoreExport.h"
#include "core/frame/UseCounter.h"
#include "core/inspector/ConsoleTypes.h"
#include "core/workers/ParentFrameTaskRunners.h"
#include "core/workers/WorkerLoaderProxy.h"
#include "platform/wtf/Forward.h"

namespace blink {

class ExecutionContext;
class SourceLocation;
class WorkerInspectorProxy;
class WorkerLoaderProxy;
class WorkerThread;
class WorkerThreadStartupData;

class CORE_EXPORT ThreadedMessagingProxyBase
    : private WorkerLoaderProxyProvider {
 public:
  void TerminateGlobalScope();

  virtual void WorkerThreadCreated();

  // This method should be called in the destructor of the object which
  // initially created it. This object could either be a Worker or a Worklet.
  virtual void ParentObjectDestroyed();

  void CountFeature(UseCounter::Feature);
  void CountDeprecation(UseCounter::Feature);

  void ReportConsoleMessage(MessageSource,
                            MessageLevel,
                            const String& message,
                            std::unique_ptr<SourceLocation>);
  void PostMessageToPageInspector(const String&);

  // 'virtual' for testing.
  virtual void WorkerThreadTerminated();

  // Accessed only from the parent thread.
  ExecutionContext* GetExecutionContext() const {
    return execution_context_.Get();
  }

  // Accessed from both the parent thread and the worker.
  ParentFrameTaskRunners* GetParentFrameTaskRunners() {
    return parent_frame_task_runners_.Get();
  }

  // Number of live messaging proxies, used by leak detection.
  static int ProxyCount();

  void SetWorkerThreadForTest(std::unique_ptr<WorkerThread>);

 protected:
  ThreadedMessagingProxyBase(ExecutionContext*);
  ~ThreadedMessagingProxyBase() override;

  void InitializeWorkerThread(std::unique_ptr<WorkerThreadStartupData>);
  virtual std::unique_ptr<WorkerThread> CreateWorkerThread(
      double origin_time) = 0;

  WorkerThread* GetWorkerThread() const { return worker_thread_.get(); }

  bool AskedToTerminate() const { return asked_to_terminate_; }

  PassRefPtr<WorkerLoaderProxy> LoaderProxy() { return loader_proxy_; }
  WorkerInspectorProxy* GetWorkerInspectorProxy() const {
    return worker_inspector_proxy_.Get();
  }

  // Returns true if this is called on the parent context thread.
  bool IsParentContextThread() const;

  // WorkerLoaderProxyProvider
  // These methods are called on different threads to schedule loading
  // requests and to send callbacks back to WorkerGlobalScope.
  void PostTaskToLoader(const WebTraceLocation&,
                        std::unique_ptr<WTF::CrossThreadClosure>) override;
  void PostTaskToWorkerGlobalScope(
      const WebTraceLocation&,
      std::unique_ptr<WTF::CrossThreadClosure>) override;
  ThreadableLoadingContext* GetThreadableLoadingContext() override;

 private:
  friend class InProcessWorkerMessagingProxyForTest;
  friend class ThreadedWorkletMessagingProxyForTest;

  void ParentObjectDestroyedInternal();

  Persistent<ExecutionContext> execution_context_;
  Persistent<ThreadableLoadingContext> loading_context_;
  Persistent<WorkerInspectorProxy> worker_inspector_proxy_;
  // Accessed cross-thread when worker thread posts tasks to the parent.
  CrossThreadPersistent<ParentFrameTaskRunners> parent_frame_task_runners_;

  std::unique_ptr<WorkerThread> worker_thread_;

  RefPtr<WorkerLoaderProxy> loader_proxy_;

  bool may_be_destroyed_;
  bool asked_to_terminate_;
};

}  // namespace blink

#endif  // ThreadedMessagingProxyBase_h
